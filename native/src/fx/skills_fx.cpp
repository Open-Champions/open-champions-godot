// Skill and effect-class visual recipes (RTA): parts 1-3 of tools/ref_fx_skills.py at 19f80a6.
// See champions/fx_skills.hpp.  Handwritten from the reference's documented contract; every
// original address in the comments is the reference's.
#include "champions/fx_skills.hpp"

#include "champions/skills_c.hpp" // the Service numbers of the skills_c call log (enumerators only)

#include <algorithm>
#include <cstring>
#include <memory>
#include <set>
#include <sstream>

namespace champions::rta::fx::skills {
namespace {
using core::add;
using core::cos16;
using core::div;
using core::eq;
using core::fmax;
using core::fmin;
using core::le;
using core::lt;
using core::mul;
using core::neg;
using core::rsqrt;
using core::sin16;
using core::sub;
constexpr Raw ZERO = core::ZERO, HALF = core::HALF, ONE = core::ONE, MINUS_ONE = core::MINUS_ONE;
constexpr Raw F_2M16 = 0x37800000;                    // 1 / 65536
constexpr std::uint64_t M32 = 0xffffffffu;

// globals (ref_creature / ref_fx_skills)
constexpr Addr G_SIM_FRAME = 0x44FDCC, G_WORLD = 0x44FDC8, G_NUM_PLAYERS = 0x4D8AE0;
constexpr Addr G_RAND_SEED = 0x404464;                // shared seed (in the sync hash)
constexpr Addr G_CUTSCENE_ACTIVE = 0x44FCD0;          // s8
constexpr Addr G_LEVEL_MONSTER_LEVEL = 0x4F714C;      // float
// glow textures (resource words; docs/effects.md 6, 14.2)
constexpr Addr G_GLOW_TEX_A = 0x4F848C, G_GLOW_TEX_B = 0x4F8494, G_GLOW_TEX_C = 0x4F8498,
               G_GLOW_TEX_RES = 0x44FE04, G_GLOW_TEX_FIRE = 0x4F84A0, G_GLOW_TEX_FIRE_DIM = 0x4F84A4;
constexpr Raw VT_SPELL = 0x515BA0;                    // the Spell base table

constexpr std::size_t MAX_LOOP = 1000000;             // a loop the reference would not finish refuses
constexpr unsigned MAX_DEPTH = 64;

std::int32_t s32v(std::uint64_t v) { return static_cast<std::int32_t>(static_cast<std::uint32_t>(v)); }
std::int32_t s16v(std::uint64_t v) { return static_cast<std::int16_t>(static_cast<std::uint16_t>(v)); }
Raw fi(std::int64_t v) { return core::from_int(static_cast<std::uint32_t>(v)); } // cvt.s.w of s32(v)
std::int32_t ti(Raw v) { return core::to_int(v); }
Raw u(std::int64_t v) { return static_cast<Raw>(v); }  // & M32
// Python's x >> n on a signed integer (floor)
std::int64_t floor_shift(std::int64_t v, unsigned n) { return v >= 0 ? v >> n : -((-v - 1) >> n) - 1; }
// Python's x % m (the sign of the divisor)
std::int64_t pymod(std::int64_t x, std::int64_t m) {
  if (m == 0) throw Refusal("modulo by zero");
  std::int64_t r = x % m;
  if (r != 0 && ((r < 0) != (m < 0))) r += m;
  return r;
}
// (u16 word << left) >> 16, arithmetic: an angle built from the frame counter
std::int64_t angle_shift(Raw word, unsigned left) {
  return floor_shift(s32v((static_cast<std::uint64_t>(word & 0xFFFF) << left) & M32), 16);
}
Value I(std::uint64_t v) { return Value::integer(v); }
Value V(const Vec3 &v) { return Value::vec(v); }
std::uint64_t as_int(const Value &v, const char *what) {
  if (!v.is_int()) throw Refusal(std::string("not an integer answer: ") + what);
  return v.i;
}
Raw as_raw(const Value &v, const char *what) { return static_cast<Raw>(as_int(v, what) & M32); }
const Value &item(const Value &v, std::size_t k, const char *what) {
  if (!v.is_sequence() || k >= v.items.size()) throw Refusal(std::string("not a sequence answer: ") + what);
  return v.items[k];
}
// ref_skills_c.desc: a damage descriptor built on the stack, an all-empty item record whose
// effect slots are `slots`
Value desc(const std::array<std::int64_t, 4> &slot) {
  std::vector<Value> items{Value::str("desc"), Value::str("00000000000000000000000000000000ffff")};
  std::vector<Value> s;
  for (auto v : slot) s.push_back(I(static_cast<std::uint64_t>(v) & 0xFFFF));
  items.push_back(Value::tuple(std::move(s)));
  for (int k = 0; k < 3; ++k) items.push_back(Value::tuple({I(0xFFFF)}));
  return Value::tuple(std::move(items));
}
} // namespace

// cvt.s.w of an unsigned value, the compiler's way: halve (keeping the low bit) and double
// when bit 31 is set (ref_fx_skills._from_uint)
Raw from_uint(std::uint32_t v) {
  if (v < 0x80000000u) return fi(static_cast<std::int64_t>(v));
  const Raw half = fi(static_cast<std::int64_t>((v >> 1) | (v & 1)));
  return add(half, half);
}

// =======================================================================================
// Values
// =======================================================================================
Value Value::vec(const Vec3 &v) { return tuple({integer(v[0]), integer(v[1]), integer(v[2])}); }

bool Value::truthy() const {
  switch (kind) {
  case Kind::None: return false;
  case Kind::Int: return i != 0;
  case Kind::Str:
  case Kind::Bytes: return !s.empty();
  default: return !items.empty();
  }
}

bool Value::operator==(const Value &o) const {
  return kind == o.kind && i == o.i && s == o.s && items == o.items;
}

std::string Value::serialize() const {
  static const char *hex = "0123456789abcdef";
  std::string out;
  switch (kind) {
  case Kind::None: return "N";
  case Kind::Int: {
    std::ostringstream os;
    os << std::hex << i;
    return os.str();
  }
  case Kind::Str: return "'" + s;
  case Kind::Bytes:
    out = "y";
    for (unsigned char c : s) {
      out.push_back(hex[c >> 4]);
      out.push_back(hex[c & 15]);
    }
    return out;
  case Kind::Tuple:
  case Kind::List: {
    out.push_back(kind == Kind::Tuple ? '(' : '[');
    for (std::size_t k = 0; k < items.size(); ++k) {
      if (k) out.push_back(',');
      out += items[k].serialize();
    }
    out.push_back(kind == Kind::Tuple ? ')' : ']');
    return out;
  }
  }
  return out;
}

namespace {
struct Parser {
  std::string_view t;
  std::size_t p = 0;
  [[noreturn]] void fail() const { throw Refusal("bad value text at " + std::to_string(p) + ": " + std::string(t)); }
  int hexdigit(char c) const {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
  }
  Value value() {
    if (p >= t.size()) fail();
    const char c = t[p];
    if (c == 'N') { ++p; return Value::none(); }
    if (c == '\'') {
      const auto e = t.find_first_of(",)]", p);
      Value v = Value::str(std::string(t.substr(p + 1, (e == std::string_view::npos ? t.size() : e) - p - 1)));
      p = e == std::string_view::npos ? t.size() : e;
      return v;
    }
    if (c == 'y') {
      ++p;
      std::string data;
      while (p + 1 < t.size() + 1 && p < t.size() && hexdigit(t[p]) >= 0) {
        if (p + 1 >= t.size() || hexdigit(t[p + 1]) < 0) fail();
        data.push_back(static_cast<char>(hexdigit(t[p]) * 16 + hexdigit(t[p + 1])));
        p += 2;
      }
      return Value::bytes(std::move(data));
    }
    if (c == '(' || c == '[') {
      const char close = c == '(' ? ')' : ']';
      ++p;
      std::vector<Value> items;
      if (p < t.size() && t[p] == close) { ++p; }
      else {
        for (;;) {
          items.push_back(value());
          if (p >= t.size()) fail();
          if (t[p] == ',') { ++p; continue; }
          if (t[p] == close) { ++p; break; }
          fail();
        }
      }
      return c == '(' ? Value::tuple(std::move(items)) : Value::list(std::move(items));
    }
    std::uint64_t v = 0;
    const std::size_t start = p;
    while (p < t.size() && hexdigit(t[p]) >= 0) {
      if (v >> 60) fail();
      v = v * 16 + static_cast<unsigned>(hexdigit(t[p++]));
    }
    if (p == start) fail();
    return Value::integer(v);
  }
};
} // namespace

Value Value::parse(std::string_view text) {
  Parser parser{text};
  Value v = parser.value();
  if (parser.p != text.size()) parser.fail();
  return v;
}

std::string Callout::serialize() const { return name + Value::tuple(args).serialize(); }

// =======================================================================================
// 1. Call-outs -> primitives
// =======================================================================================
const std::vector<CalloutInfo> &callouts() {
  static const std::vector<CalloutInfo> rows = {
    {"particle_spawn", 0x18F3F8, "particle", false, {"definition", "pos", "vel", "backdate"}, "ref_particles.ParticleSystem.spawn(def, pos, vel, backdate); definition by address"},
    {"glow_add", 0x18FC78, "glow", false, {"pos", "r", "g", "b", "a", "texture", "angle", "flags", "bias", "size"}, "docs/effects.md 6; texture is the resource word the code loaded"},
    {"LightEffect_ctor", 0x201BD8, "light_effect", false, {"block", "pos", "follow", "flag", "c0", "c1", "c2", "c3", "d0", "d1", "d2", "d3", "radius", "radius_end", "floor", "seconds"}, "ref_lighting.LightEffect; a GameObject: ONE SHARED DRAW in its constructor"},
    {"trail_create", 0x190FF8, "trail", false, {"row", "width", "flags", "r", "g", "b", "a", "lifetime"}, "docs/effects.md 7: (lifetime s; row, width, flags, rgba) -> handle"},
    {"trail_addPoint", 0x192550, "trail_point", false, {"handle", "pos"}, "stamp = trail clock + 1"},
    {"trail_release", 0x192800, "trail_release", false, {"handle"}, ""},
    {"beam_create", 0x193088, "beam", false, {"from", "to", "width", "depth", "rgba", "jitter", "scale"}, ""},
    {"beam_update", 0x1933C8, "beam_update", false, {"handle", "from", "to", "alpha", "width"}, ""},
    {"decal_add", 0x19F110, "decal", false, {"pos", "angle", "type", "grow", "size"}, "docs/effects.md 8"},
    {"fx_spawn", 0x1EF270, "fx_spawn", false, {"id", "pos"}, "docs/effects.md 10, tools/ref_fx_recipes.py"},
    {"fx_ringBurst", 0x1F3D00, "ring_burst", true, {}, "arguments as the model records them"},
    {"fx_impactPuffNearCamera", 0x1FC150, "puff", false, {"pos", "step", "mode", "radius", "size"}, ""},
    {"fx_emitFromModelVerts", 0x1EB1E8, "model_verts", false, {"object", "count", "definition", "a", "b", "speed"}, ""},
    {"fx_shockModel", 0x1F61C0, "model_particles_light", true, {}, "docs/effects.md calls it fx_modelParticlesNearCamera_1f61c0"},
    {"fx_burnModel", 0x1F5210, "model_particles", true, {}, "docs/effects.md: fx_modelParticlesNearCamera_1f5210"},
    {"fx_disintegrateModel", 0x1F4340, "disintegrate", true, {}, "tools/ref_fx_recipes.py"},
    {"fx_shatterToLooseIce", 0x1FB240, "ice_shatter", true, {}, "draws the shared seed (docs/gameplay.md)"},
    {"trail_setColor", 0x191100, "trail_color", false, {"handle", "r", "g", "b", "a"}, ""},
    {"trail_setLength", 0x1911A8, "trail_width", false, {"handle", "value"}, "stores value >> 2 at +0x40f (round 1: trail_setWidth)"},
    {"trail_setPoints", 0x1918A0, "trail_points", true, {}, "a point array handed to the trail"},
    {"trail_setPoints_1918a0", 0x1918A0, "trail_points", true, {}, "the same function, another model's name"},
    {"trail_clear_191e58", 0x191E58, "trail_clear", true, {}, ""},
    {"GlowingShaft_ctor", 0x202FF0, "object", true, {}, "a visual GameObject (shared draw in its constructor)"},
    {"LightningBolt_ctor", 0x295360, "object", true, {}, "a GameObject with beams"},
    {"LightningBolt_ctor2", 0x295584, "object", true, {}, "a GameObject with beams"},
    {"GutChunk_ctor", 0x201958, "object", true, {}, "docs/effects.md 9: a gib"},
    {"camera_shake", 0x1EBAE0, "camera_shake", true, {}, "per machine"},
    {"gfx_setColorOffset", 0x19B6BC, "screen_tint", true, {}, "per machine"},
    {"particleGroup_create", 0x18E880, "group_create", false, {}, "-> handle"},
    {"particleGroup_begin", 0x18E958, "group_begin", false, {"handle"}, ""},
    {"particleGroup_end", 0x18E9A0, "group_end", false, {"handle"}, ""},
    {"particleGroup_setMatrix", 0x18E9E8, "group_matrix", false, {"handle", "matrix"}, ""},
    {"particleGroup_free", 0x18EA90, "group_free", false, {"handle"}, ""},
    {"light_link", 0x148AF8, "light_link", false, {"light"}, "the class's own light node (ref_lighting.LightNode)"},
    {"light_relink", 0x14A528, "light_relink", false, {"light"}, "the light follows its new position"},
    {"light_unlink", 0x14A4F0, "light_unlink", false, {"light"}, ""},
  };
  return rows;
}

const CalloutInfo *find_callout(std::string_view name) {
  for (const auto &row : callouts())
    if (row.name == name) return &row;
  return nullptr;
}

const Field *FxCall::field(std::string_view name) const {
  for (const auto &f : fields)
    if (f.first == name) return &f.second;
  return nullptr;
}

namespace {
Field int_field(const Value &v) {
  if (!v.is_int()) throw DecodeError("integer field expected");
  Field f;
  f.kind = Field::Kind::Int;
  f.raw = static_cast<Raw>(v.i);
  return f;
}
Field float_field(const Value &v) {
  if (!v.is_int()) throw DecodeError("float pattern expected");
  Field f;
  f.kind = Field::Kind::Float;
  f.raw = static_cast<Raw>(v.i & M32);
  return f;
}
Field vector_field(const Value &v) {
  if (!v.is_sequence()) throw DecodeError("vector expected");
  Field f;
  f.kind = Field::Kind::Vector;
  for (const auto &lane : v.items) {
    if (!lane.is_int()) throw DecodeError("vector lane expected");
    f.lanes.push_back(static_cast<Raw>(lane.i & M32));
  }
  return f;
}
// the else branch of to_primitive: tuples become float tuples, anything else stays
Field generic_field(const Value &v) {
  if (v.kind == Value::Kind::Tuple) return vector_field(v);
  if (v.is_int()) return int_field(v);
  Field f;
  f.kind = Field::Kind::Other;
  f.other = v;
  return f;
}
const Value &at(const std::vector<Value> &a, std::size_t k) {
  if (k >= a.size()) throw DecodeError("too few arguments");
  return a[k];
}
} // namespace

std::optional<FxCall> to_primitive(const Callout &c) {
  const CalloutInfo *out = find_callout(c.name);
  if (!out) return std::nullopt;
  FxCall fx;
  fx.primitive = std::string(out->primitive);
  if (out->opaque) {
    Field f;
    f.kind = Field::Kind::Other;
    f.other = Value::tuple(c.args);
    fx.fields.emplace_back("args", std::move(f));
    return fx;
  }
  const auto &a = c.args;
  std::vector<Field> values;
  if (c.name == "particle_spawn") {
    values = {int_field(at(a, 0)), vector_field(at(a, 1)), vector_field(at(a, 2)), float_field(at(a, 3))};
  } else if (c.name == "glow_add") {
    values.push_back(vector_field(at(a, 0)));
    for (std::size_t k = 1; k < 8 && k < a.size(); ++k) values.push_back(generic_field(a[k]));
    values.push_back(float_field(at(a, 8)));
    values.push_back(float_field(at(a, 9)));
  } else if (c.name == "LightEffect_ctor") {
    values = {generic_field(at(a, 0)), vector_field(at(a, 1)), generic_field(at(a, 2)), generic_field(at(a, 3))};
    for (std::size_t k = 4; k < 16 && k < a.size(); ++k) values.push_back(float_field(a[k]));
  } else if (c.name == "trail_create") {
    for (std::size_t k = 0; k < 7 && k < a.size(); ++k) values.push_back(generic_field(a[k]));
    values.push_back(float_field(at(a, 7)));
  } else if (c.name == "trail_addPoint") {
    values = {generic_field(at(a, 0)), at(a, 1).kind == Value::Kind::Tuple ? vector_field(a[1]) : generic_field(a[1])};
  } else if (c.name == "decal_add") {
    values = {vector_field(at(a, 0)), generic_field(at(a, 1)), generic_field(at(a, 2)), generic_field(at(a, 3)),
              float_field(at(a, 4))};
  } else if (c.name == "fx_spawn") {
    values = {generic_field(at(a, 0)), vector_field(at(a, 1))};
  } else {
    for (const auto &v : a) values.push_back(generic_field(v));
  }
  for (std::size_t k = 0; k < values.size() && k < out->fields.size(); ++k)
    fx.fields.emplace_back(std::string(out->fields[k]), std::move(values[k]));
  return fx;
}

std::vector<FxCall> effects_of(const std::vector<Callout> &calls) {
  std::vector<FxCall> out;
  for (const auto &c : calls)
    if (auto fx = to_primitive(c)) out.push_back(std::move(*fx));
  return out;
}

// ---- adapters ----------------------------------------------------------------------------
namespace {
struct SigRow {
  std::string_view name;
  std::array<std::optional<Signature>, 5> by_port; // SkillsA, SkillsB, SkillsC, ArenaMonsters, FxSkills
};
// The EXTERNALS rows of every call-out per reference module (ref_skills_a, _b, _c,
// ref_arena_monsters, ref_fx_skills at 19f80a6).
constexpr Signature S(std::string_view ints, unsigned floats, unsigned stack = 0) { return {ints, floats, stack}; }
const std::vector<SigRow> &signature_rows() {
  const std::optional<Signature> no;
  static const std::vector<SigRow> rows = {
      {"particle_spawn", {S("pvv", 1), S("pvv", 1), S("pvv", 1), S("pvv", 1), S("pvv", 1)}},
      {"glow_add", {S("viiiiiii", 2), no, S("viiiiiii", 2), no, S("viiiiiii", 2)}},
      {"LightEffect_ctor", {S("pvii", 8, 4), S("pvii", 8, 4), S("pvii", 8, 4), S("pvii", 8, 4), S("pvii", 8, 4)}},
      {"trail_create", {S("iiiiiii", 1), S("iiiiiii", 1), S("iiiiiii", 1), no, S("iiiiiii", 1)}},
      {"trail_addPoint", {S("iv", 0), S("iv", 0), no, no, S("iv", 0)}},
      {"trail_release", {no, no, no, no, S("i", 0)}},
      {"beam_create", {no, S("vviii", 2), no, no, no}},
      {"beam_update", {no, S("ivvii", 0), no, no, no}},
      {"decal_add", {S("viii", 1), S("viii", 1), S("viii", 1), S("viii", 1), S("viii", 1)}},
      {"fx_spawn", {S("iv", 0), S("iv", 0), S("iv", 0), S("iv", 0), S("iv", 0)}},
      {"fx_ringBurst", {S("v", 1), S("v", 1), S("v", 1), S("v", 1), S("v", 1)}},
      {"fx_impactPuffNearCamera", {no, S("vvi", 2), no, no, no}},
      {"fx_emitFromModelVerts", {no, S("piivV", 1), S("pipii", 1), no, S("pipii", 1)}},
      {"fx_shockModel", {S("ppvipi", 3), S("ppvipi", 3), S("ppvipi", 3), S("ppvipi", 3), S("ppvipi", 3)}},
      {"fx_burnModel", {S("ppvip", 4), S("ppvip", 4), S("ppvip", 4), S("ppvip", 4), S("ppvip", 4)}},
      {"fx_disintegrateModel", {S("pppvii", 2), S("pppvii", 2), S("pppvii", 2), S("pppvii", 2), S("pppvii", 2)}},
      {"fx_shatterToLooseIce", {S("ppvip", 3), S("ppvip", 3), S("ppvip", 3), S("ppvip", 3), S("ppvip", 3)}},
      {"trail_setColor", {no, no, S("iiiii", 0), no, S("iiiii", 0)}},
      {"trail_setLength", {no, no, S("ii", 0), no, S("ii", 0)}},
      {"trail_setPoints", {no, no, S("iTii", 2), no, S("iTii", 2)}},
      {"trail_setPoints_1918a0", {S("ipii", 2), no, no, no, no}},
      {"trail_clear_191e58", {S("i", 0), no, no, no, no}},
      {"GlowingShaft_ctor", {S("pviiii", 0), S("pviiii", 0), S("pviiii", 0), no, S("pviiii", 0)}},
      {"LightningBolt_ctor", {no, no, S("pvii", 0), no, S("pvii", 0)}},
      {"LightningBolt_ctor2", {no, no, S("pvvii", 0), no, S("pvvii", 0)}},
      {"GutChunk_ctor", {S("pvii", 1), S("pvii", 1), S("pvii", 1), S("pvii", 1), S("pvii", 1)}},
      {"camera_shake", {no, S("i", 1), S("i", 1), no, S("i", 1)}},
      {"gfx_setColorOffset", {S("iiii", 0), no, no, no, no}},
      {"particleGroup_create", {S("", 0), S("", 0), S("", 0), no, S("", 0)}},
      {"particleGroup_begin", {S("i", 0), S("i", 0), S("i", 0), no, S("i", 0)}},
      {"particleGroup_end", {S("i", 0), S("i", 0), S("i", 0), no, S("i", 0)}},
      {"particleGroup_setMatrix", {S("im", 0), S("iM", 0), S("ip", 0), no, S("ip", 0)}},
      {"particleGroup_free", {no, no, S("i", 0), no, S("i", 0)}},
      {"light_link", {S("p", 0), S("p", 0), S("p", 0), no, S("p", 0)}},
      {"light_relink", {S("p", 0), S("p", 0), S("p", 0), no, S("p", 0)}},
      {"light_unlink", {S("p", 0), S("p", 0), S("p", 0), no, S("p", 0)}},
  };
  return rows;
}
} // namespace

std::optional<Signature> signature(Port port, std::string_view name) {
  for (const auto &row : signature_rows())
    if (row.name == name) return row.by_port[static_cast<unsigned>(port)];
  return std::nullopt;
}

Callout from_lanes(Port port, std::string name, const std::vector<std::uint32_t> &lanes) {
  const auto sig = signature(port, name);
  if (!sig) throw DecodeError("no signature for " + name + " in this port");
  Callout out{std::move(name), {}};
  std::size_t k = 0;
  auto take = [&]() {
    if (k >= lanes.size()) throw DecodeError("too few lanes for " + out.name);
    return lanes[k++];
  };
  auto vec_of = [&](unsigned n) {
    std::vector<Value> v;
    for (unsigned j = 0; j < n; ++j) v.push_back(Value::integer(take()));
    return Value::tuple(std::move(v));
  };
  for (char kind : sig->ints) {
    switch (kind) {
    case 'v':
    case 'V': out.args.push_back(vec_of(3)); break;
    case 'm':
    case 'M': out.args.push_back(vec_of(12)); break;
    case 'T': {
      std::vector<Value> points;
      for (int j = 0; j < 10; ++j) points.push_back(vec_of(3));
      out.args.push_back(Value::tuple(std::move(points)));
      break;
    }
    case 'o': case 'w': case 'b': case 'n': case 'L': case '-': break; // not arguments of the model
    default: out.args.push_back(Value::integer(take())); break;
    }
  }
  for (unsigned j = 0; j < sig->floats + sig->stack; ++j) out.args.push_back(Value::integer(take()));
  if (k != lanes.size()) throw DecodeError("too many lanes for " + out.name);
  if (port == Port::SkillsC && out.name == "particleGroup_setMatrix" && out.args.size() == 2)
    out.args[1] = Value::str("stack");   // skills_c's 0 stands for the reference's 'stack'
  return out;
}

std::optional<std::string> skills_c_service_name(unsigned service) {
  namespace sc = champions::simulation::rta::skills_c;
  static const std::map<unsigned, std::string> names = {
      {sc::LightLink, "light_link"}, {sc::LightRelink, "light_relink"}, {sc::LightUnlink, "light_unlink"},
      {sc::ParticleGroupCreate, "particleGroup_create"}, {sc::ParticleGroupBegin, "particleGroup_begin"},
      {sc::ParticleGroupEnd, "particleGroup_end"}, {sc::ParticleGroupSetMatrix, "particleGroup_setMatrix"},
      {sc::ParticleGroupFree, "particleGroup_free"}, {sc::EmitFromModelVertices, "fx_emitFromModelVerts"},
      {sc::LightEffectConstructor, "LightEffect_ctor"}, {sc::ParticleSpawn, "particle_spawn"},
      {sc::EffectSpawn, "fx_spawn"}, {sc::GlowAdd, "glow_add"}, {sc::GlowingShaftConstructor, "GlowingShaft_ctor"},
      {sc::LightningBoltConstructor, "LightningBolt_ctor"}, {sc::LightningBoltConstructor2, "LightningBolt_ctor2"},
      {sc::CameraShake, "camera_shake"}, {sc::TrailCreate, "trail_create"}, {sc::TrailSetLength, "trail_setLength"},
      {sc::TrailSetColor, "trail_setColor"}, {sc::TrailSetPoints, "trail_setPoints"},
  };
  const auto it = names.find(service);
  if (it == names.end()) return std::nullopt;
  return it->second;
}

// =======================================================================================
// 2. Recipes per class
// =======================================================================================
// One row per function of the skill files (0x2c3cd4-0x320000) that calls an effect primitive
// (fx_callsites.py map 2c3cd4 320000), 161 of them: class, phase, function, address, model,
// primitive calls in the code, generators (see ref_fx_skills.py part 2 for the columns).
const std::vector<Recipe> &recipes() {
  static const std::vector<Recipe> rows = {
    {"ShockProjectile", Phase::Cast, "ShockProjectile_ctor", 0x2C3FE4, "ref_skills_b.shock_ctor", "lightLink:1", ""},
    {"ShockProjectile", Phase::Run, "ShockProjectile_run", 0x2C42E4, "ref_skills_b.shock_run", "beam:1 beamUpdate:1 particle:2", "SHARED own"},
    {"ShockProjectile", Phase::Draw, "ShockProjectile_draw", 0x2C4E64, "ref_fx_skills.shock_draw", "glow:1", ""},
    {"StaticFire", Phase::Cast, "StaticFire_ctor", 0x2C5240, "ref_fx_skills.staticfire_ctor", "lightLink:1", "own"},
    {"StaticFire", Phase::Cast, "StaticFire_ctor2", 0x2C54F8, "ref_fx_skills.staticfire_ctor2", "lightLink:1", "own"},
    {"StaticFire", Phase::Run, "StaticFire_run", 0x2C595C, "ref_fx_skills.staticfire_run", "particle:2", "SHARED own"},
    {"StaticFire", Phase::Draw, "StaticFire_draw", 0x2C65E8, "ref_fx_skills.staticfire_draw", "glow:2", ""},
    {"GroundPoundEffect", Phase::Cast, "GroundPoundEffect_ctor", 0x2C74AC, "ref_skills_b.groundpound_effect_ctor", "lightLink:1", ""},
    {"GroundPoundEffect", Phase::Run, "GroundPoundEffect_run", 0x2C7808, "ref_skills_b.groundpound_effect_run", "LightEffect:1 decal:1 particle:7", "SHARED(LightEffect) own seed3"},
    {"SpellCharge", Phase::Cast, "SpellCharge_ctor", 0x2C8CB4, "ref_skills_b.charge_spell_ctor", "lightLink:1 trail:1 trailPoint:1", "SHARED"},
    {"SpellCharge", Phase::Run, "SpellCharge_run", 0x2C8F2C, "ref_skills_b.charge_run", "particle:2 trailPoint:1", "spell"},
    {"SpellCharge", Phase::Event, "SpellCharge_onContact", 0x2C95F4, "ref_skills_b.charge_on_contact", "fx_spawn:1 particle:2", "spell"},
    {"DiseaseBolt", Phase::Run, "DiseaseBolt_run", 0x2CA1A8, "ref_skills_a.diseasebolt_run", "fx_spawn:1 particle:6", "own"},
    {"DiseaseTrail", Phase::Run, "DiseaseTrail_run", 0x2CB65C, "ref_skills_a.diseasetrail_run", "particle:3", "own"},
    {"SpellDiseaseTrail", Phase::Run, "SpellDiseaseTrail_run", 0x2CC248, "ref_skills_a.diseasetrail_spell_run", "particle:3", "SHARED spell"},
    {"SpellDiseaseShield", Phase::Run, "SpellDiseaseShield_run", 0x2CD318, "ref_skills_a.diseaseshield_run", "glow:1 particle:2", "spell"},
    {"DiseaseCloud", Phase::Run, "DiseaseCloud_run", 0x2CE6E0, "ref_fx_skills.diseasecloud_run", "particle:3", "own"},
    {"DarknessProjectile", Phase::Cast, "DarknessProjectile_ctor", 0x2CF460, "ref_skills_b.darkness_ctor", "lightLink:1", ""},
    {"DarknessProjectile", Phase::Run, "DarknessProjectile_run", 0x2CF690, "ref_skills_b.darkness_run", "particle:1", "own"},
    {"SpellConvertEnemy", Phase::Cast, "SpellConvertEnemy_ctor", 0x2D0154, "ref_skills_b.convert_spell_ctor", "particleGroup:1", "SHARED"},
    {"SpellConvertEnemy", Phase::Run, "SpellConvertEnemy_run", 0x2D0354, "ref_skills_b.convert_run", "particle:1", "SHARED"},
    {"SpellConvertUndead", Phase::Cast, "SpellConvertUndead_ctor", 0x2D0CC0, "ref_skills_b.convert_spell_ctor", "particleGroup:1", "SHARED"},
    {"SpellConvertUndead", Phase::Run, "SpellConvertUndead_run", 0x2D0EDC, "ref_skills_b.convert_run", "particle:1", "SHARED"},
    {"SpellCyclone", Phase::Cast, "SpellCyclone_ctor", 0x2D17C8, "ref_skills_b.cyclone_spell_ctor", "lightLink:1", "SHARED spell"},
    {"SpellCyclone", Phase::Run, "SpellCyclone_updateFx", 0x2D21E0, "ref_skills_b.cyclone_update_fx", "particle:2", "spell"},
    {"SpellHarmTouch", Phase::Event, "SpellHarmTouch_applyToHit", 0x2D2CBC, "ref_skills_b.harmtouch_apply_to_hit", "LightEffect:1 particle:2", "SHARED(LightEffect) spell"},
    {"FireStormFlare", Phase::Run, "FireStormFlare_run", 0x2D33C0, "ref_fx_skills.firestormflare_run", "decal:1 particle:5", "own"},
    {"FireStorm", Phase::Cast, "FireStorm_ctor2", 0x2D4048, "ref_skills_a.firestorm_ctor2", "lightLink:1", "SHARED SHARED(LightEffect)"},
    {"FireStorm", Phase::Cast, "FireStorm_ctor", 0x2D41F8, "ref_skills_a.firestorm_ctor", "lightLink:1", "SHARED SHARED(LightEffect)"},
    {"FireStorm", Phase::Cast, "FireStorm_initCommon", 0x2D4480, "ref_skills_a.firestorm_init_common", "LightEffect:1", "SHARED(LightEffect)"},
    {"SpellDiseaseEffect", Phase::Run, "SpellDiseaseEffect_run", 0x2D5854, "ref_skills_a.disease_spell_run", "glow:1", "SHARED spell"},
    {"FireArrowProjectile", Phase::Cast, "FireArrowProjectile_ctor", 0x2D6238, "ref_skills_b.firearrow_ctor", "lightLink:1", "SHARED"},
    {"FireArrowProjectile", Phase::Cast, "FireArrowProjectile_ctor2", 0x2D6440, "ref_skills_b.firearrow_ctor2", "lightLink:1", "SHARED"},
    {"FireArrowProjectile", Phase::Run, "FireArrowProjectile_run", 0x2D66AC, "ref_skills_b.firearrow_run", "puff:1", "own"},
    {"FireArrowProjectile", Phase::Draw, "FireArrowProjectile_draw", 0x2D67E8, "ref_fx_skills.firearrow_draw", "glow:1", ""},
    {"ColdArrowProjectile", Phase::Cast, "ColdArrowProjectile_ctor", 0x2D6FB4, "ref_skills_b.coldarrow_ctor", "lightLink:1", "SHARED"},
    {"ColdArrowProjectile", Phase::Cast, "ColdArrowProjectile_ctor2", 0x2D718C, "ref_skills_b.coldarrow_ctor2", "lightLink:1", "SHARED"},
    {"ColdArrowProjectile", Phase::Run, "ColdArrowProjectile_run", 0x2D73C8, "ref_skills_b.coldarrow_run", "particle:2", "own"},
    {"ColdArrowProjectile", Phase::Event, "ColdArrowProjectile_onHit", 0x2D7AAC, "ref_skills_b.coldarrow_on_hit", "fx_spawn:2", "own"},
    {"ColdArrowProjectile", Phase::Event, "ColdArrowProjectile_end", 0x2D7ECC, "ref_skills_b.coldarrow_end", "particle:3", "own"},
    {"PoisonArrowProjectile", Phase::Cast, "PoisonArrowProjectile_ctor", 0x2D8A64, "ref_skills_b.poisonarrow_ctor", "lightLink:1", "SHARED"},
    {"PoisonArrowProjectile", Phase::Cast, "PoisonArrowProjectile_ctor2", 0x2D8C40, "ref_skills_b.poisonarrow_ctor2", "lightLink:1", "SHARED"},
    {"PoisonArrowProjectile", Phase::Run, "PoisonArrowProjectile_run", 0x2D8E80, "ref_skills_b.poisonarrow_run", "particle:2", "own"},
    {"PoisonArrowProjectile", Phase::Event, "PoisonArrowProjectile_end", 0x2D95C8, "ref_skills_b.poisonarrow_end", "particle:1", ""},
    {"SkillMultiFire", Phase::Cast, "SkillMultiFire_cast", 0x2D9A90, "ref_skills_b.multifire_cast", "LightEffect:1", "SHARED SHARED(LightEffect) cosmetic"},
    {"MultiFireProjectile", Phase::Cast, "MultiFireProjectile_ctor", 0x2DA164, "ref_fx_skills.multifire_ctor", "trail:1 trailPoint:1", ""},
    {"MultiFireProjectile", Phase::Run, "MultiFireProjectile_run", 0x2DA2BC, "ref_fx_skills.multifire_run", "particle:1 trailPoint:1", "own"},
    {"MultiFireProjectile", Phase::Event, "MultiFireProjectile_end", 0x2DA928, "ref_fx_skills.multifire_end", "fx_spawn:2", "own"},
    {"ExplodeArrowProjectile", Phase::Run, "ExplodeArrowProjectile_run", 0x2DB17C, "ref_skills_b.explodearrow_run", "particle:1", "own"},
    {"ExplodeArrowProjectile", Phase::Event, "ExplodeArrowProjectile_explode", 0x2DB50C, "ref_skills_b.explodearrow_explode", "LightEffect:1 fx_spawn:2 ringBurst:1", "SHARED(LightEffect) own"},
    {"FireFlare", Phase::Run, "FireFlare_run", 0x2DBF10, "ref_skills_a.fireflare_run", "particle:5", "own"},
    {"SpellFireFlares", Phase::Cast, "SpellFireFlares_ctor", 0x2DCD40, "ref_skills_a.fireflares_ctor", "lightLink:1", "SHARED"},
    {"SkillRiposte", Phase::Event, "SkillRiposte_counterAttack", 0x2DDCCC, "ref_skills_a.riposte_counter_attack", "particle:2", "SHARED cosmetic"},
    {"SpellBash", Phase::Run, "SpellBash_run", 0x2DE7DC, "ref_skills_b.bash_run", "LightEffect:1", "SHARED SHARED(LightEffect) spell"},
    {"SpellBash", Phase::Run, "SpellBash_windupFx", 0x2DEDEC, "ref_skills_b.bash_windup_fx", "particle:1", "spell"},
    {"SpellBlessed", Phase::Cast, "SpellBlessed_ctor", 0x2DF078, "ref_skills_a.blessed_ctor", "lightLink:1", "SHARED spell"},
    {"SpellBlessed", Phase::Run, "SpellBlessed_run", 0x2DF4BC, "ref_skills_a.blessed_run", "glow:1 particle:2", "spell"},
    {"SpellAncestralCall", Phase::Cast, "SpellAncestralCall_ctor", 0x2E09B4, "ref_skills_b.ancestralcall_spell_ctor", "LightEffect:1 particleGroup:1", "SHARED SHARED(LightEffect)"},
    {"SpellAncestralCall", Phase::Run, "SpellAncestralCall_run", 0x2E0D10, "ref_skills_b.ancestralcall_run", "beam:1 beamUpdate:1 particle:1", "spell"},
    {"SpellAncestralCall", Phase::Run, "SpellAncestralCall_tintAndGlow", 0x2E15A0, "ref_skills_b.ancestralcall_tint_and_glow", "modelParticlesLight:1", ""},
    {"SkillSummonSkeleton", Phase::Cast, "SkillSummonSkeleton_cast", 0x2E18A4, "ref_arena_monsters.summon_cast", "particle:2", "SHARED cosmetic"},
    {"SpellLightningWeapons", Phase::Cast, "SpellLightningWeapons_ctor", 0x2E24D8, "ref_skills_a.weaponbuff_ctor", "LightEffect:1", "SHARED SHARED(LightEffect)"},
    {"SpellLightningWeapons", Phase::Run, "SpellLightningWeapons_run", 0x2E2AC4, "ref_skills_a.weaponbuff_run", "modelParticlesLight:1", "spell"},
    {"SpellFireWeapons", Phase::Cast, "SpellFireWeapons_ctor", 0x2E34E0, "ref_skills_a.fireweapons_ctor", "lightLink:1", "SHARED"},
    {"SpellFireWeapons", Phase::Run, "SpellFireWeapons_run", 0x2E3B28, "ref_skills_a.fireweapons_run", "particle:3", "spell"},
    {"SpellColdWeapons", Phase::Cast, "SpellColdWeapons_ctor", 0x2E4F94, "ref_skills_a.weaponbuff_ctor", "LightEffect:1", "SHARED SHARED(LightEffect)"},
    {"SpellColdWeapons", Phase::Run, "SpellColdWeapons_run", 0x2E5578, "ref_skills_a.weaponbuff_run", "particle:2", "spell"},
    {"SpellRoot", Phase::Cast, "SpellRoot_ctor", 0x2E68C8, "ref_skills_a.root_spell_ctor", "particleGroup:1", "SHARED"},
    {"SpellRoot", Phase::Run, "SpellRoot_run", 0x2E699C, "ref_skills_a.root_spell_run", "particle:3", "SHARED spell"},
    {"SpellCriticalHit", Phase::Cast, "SpellCriticalHit_ctor", 0x2E7988, "ref_skills_a.crit_spell_ctor", "lightLink:1 particleGroup:1", "SHARED"},
    {"SpellCriticalHit", Phase::Run, "SpellCriticalHit_run", 0x2E7ACC, "ref_skills_a.crit_spell_run", "particle:1", "spell"},
    {"SpellHolyShield", Phase::Cast, "SpellHolyShield_ctor", 0x2E82FC, "ref_skills_b.holyshield_spell_ctor", "particleGroup:1", "SHARED"},
    {"SpellHolyShield", Phase::Event, "SpellHolyShield_absorb", 0x2E8534, "ref_skills_b.holyshield_absorb", "LightEffect:1 particle:1", "SHARED(LightEffect) spell"},
    {"SpellHealing", Phase::Cast, "SpellHealing_ctor", 0x2E8EC8, "ref_skills_b.healing_spell_ctor", "particleGroup:1", "SHARED"},
    {"SpellHealing", Phase::Run, "SpellHealing_run", 0x2E90B8, "ref_skills_b.healing_run", "particle:1", "spell"},
    {"SpellConeOfFrost", Phase::Cast, "SpellConeOfFrost_ctor", 0x2E9B08, "ref_skills_b.coneoffrost_spell_ctor", "lightLink:1", "SHARED"},
    {"SpellConeOfFrost", Phase::Run, "SpellConeOfFrost_run", 0x2E9D10, "ref_skills_b.coneoffrost_run", "particle:3", "SHARED spell"},
    {"SpellUndeadShield", Phase::Cast, "SpellUndeadShield_ctor", 0x2EA4D8, "ref_skills_a.undeadshield_ctor", "lightLink:1 particleGroup:1", "SHARED"},
    {"SpellUndeadShield", Phase::Run, "SpellUndeadShield_run", 0x2EA74C, "ref_skills_a.undeadshield_run", "particle:1", "SHARED spell"},
    {"SpellSecondaryUndeadShield", Phase::Cast, "SpellSecondaryUndeadShield_ctor", 0x2EB050, "ref_skills_a.secondary_shield_ctor", "particleGroup:1", "SHARED"},
    {"SpellSecondaryUndeadShield", Phase::Run, "SpellSecondaryUndeadShield_run", 0x2EB170, "ref_skills_a.secondary_shield_run", "particle:1", "spell"},
    {"HolyStrike", Phase::Cast, "HolyStrike_ctor", 0x2EBA48, "ref_skills_a.holystrike_ctor", "trail:1 trailPoint:1", "SHARED"},
    {"HolyStrike", Phase::Run, "HolyStrike_run", 0x2EBC50, "ref_skills_a.holystrike_run", "LightEffect:1 particle:6 trailPoint:1", "SHARED(LightEffect) own"},
    {"HolyStrike", Phase::Draw, "HolyStrike_draw", 0x2ECD90, "ref_fx_skills.holystrike_draw", "glow:1", ""},
    {"HammerOfWrath", Phase::Run, "HammerOfWrath_run", 0x2EE068, "ref_skills_a.hammer_run", "particle:1", "own seed3"},
    {"SkillHammerOfWrath", Phase::Cast, "SkillHammerOfWrath_cast", 0x2EFB20, "ref_skills_a.hammer_cast", "LightEffect:1", "SHARED SHARED(LightEffect)"},
    {"SpellConeOfFire", Phase::Cast, "SpellConeOfFire_ctor", 0x2F00AC, "ref_skills_a.coneoffire_ctor", "lightLink:1", "SHARED"},
    {"SpellConeOfFire", Phase::Run, "SpellConeOfFire_run", 0x2F02FC, "ref_skills_a.coneoffire_run", "particle:3", "spell"},
    {"SpellWizardBeam", Phase::Cast, "SpellWizardBeam_ctor", 0x2F1528, "ref_skills_a.wizardbeam_ctor", "trail:7", "SHARED spell"},
    {"BlindingLightEffect", Phase::Cast, "BlindingLightEffect_ctor", 0x2F3F38, "ref_skills_a.blindinglight_effect_ctor", "LightEffect:1", "SHARED SHARED(LightEffect) own"},
    {"BlindingLightEffect", Phase::Run, "BlindingLightEffect_run", 0x2F42E4, "ref_skills_a.blindinglight_effect_run", "particle:1", "own"},
    {"SkillRepulseUndead", Phase::Cast, "SkillRepulseUndead_cast", 0x2F4D34, "ref_skills_a.repulse_cast", "LightEffect:1", "SHARED(LightEffect)"},
    {"HolyBolt", Phase::Cast, "HolyBolt_ctor", 0x2F5248, "ref_skills_a.holybolt_ctor", "lightLink:1 trail:1 trailPoint:1", "SHARED"},
    {"HolyBolt", Phase::Run, "HolyBolt_run", 0x2F57B0, "ref_skills_a.holybolt_run", "particle:1 trailPoint:1", "own"},
    {"HolyBolt", Phase::Event, "HolyBolt_onHit", 0x2F5FB4, "ref_skills_a.holybolt_hit", "LightEffect:1 particle:1", "SHARED(LightEffect) own"},
    {"FrostShard", Phase::Run, "FrostShard_run", 0x2F6B3C, "ref_skills_a.frostshard_run", "fx_spawn:1 particle:2", "own"},
    {"ShockOfFrost", Phase::Cast, "ShockOfFrost_ctor", 0x2F7404, "ref_skills_a.shockoffrost_ctor", "lightLink:1", "SHARED"},
    {"ShockOfFrost", Phase::Run, "ShockOfFrost_run", 0x2F77C0, "ref_skills_a.shockoffrost_run", "fx_spawn:1 particle:4", "SHARED own"},
    {"PoisonRain", Phase::Cast, "PoisonRain_ctor", 0x2F8F48, "ref_skills_a.poisonrain_ctor", "LightEffect:1", "SHARED SHARED(LightEffect)"},
    {"PoisonRain", Phase::Run, "PoisonRain_run", 0x2F9254, "ref_skills_a.poisonrain_run", "LightEffect:1 particle:2", "SHARED(LightEffect) own"},
    {"SpellShieldBash", Phase::Run, "SpellShieldBash_run", 0x2FAE80, "ref_skills_a.shieldbash_run", "LightEffect:1", "SHARED SHARED(LightEffect) spell"},
    {"SpellShieldBash", Phase::Run, "SpellShieldBash_windupFx", 0x2FB49C, "ref_skills_a.shieldbash_windup_fx", "particle:1", "spell"},
    {"SpellPlanarSteelEffect", Phase::Cast, "SpellPlanarSteelEffect_ctor", 0x2FB728, "ref_fx_skills.planarsteel_ctor", "lightLink:1", "SHARED"},
    {"SpellPlanarSteelEffect", Phase::Run, "SpellPlanarSteelEffect_run", 0x2FB9B4, "ref_fx_skills.planarsteel_run", "glow:1 particle:5", "spell"},
    {"SpellHornOmenEffect", Phase::Cast, "SpellHornOmenEffect_ctor", 0x2FC574, "ref_fx_skills.hornomen_ctor", "particleGroup:1", "SHARED"},
    {"SpellHornOmenEffect", Phase::Run, "SpellHornOmenEffect_run", 0x2FC684, "ref_fx_skills.hornomen_run", "particle:2", "spell"},
    {"SkillUnspokenWord", Phase::Cast, "SkillUnspokenWord_cast", 0x2FD030, "ref_skills_c.unspokenword_cast", "LightEffect:1", "SHARED SHARED(LightEffect)"},
    {"SpellUnspokenWord", Phase::Cast, "SpellUnspokenWord_ctor", 0x2FD368, "ref_skills_c.unspokenword_spell_ctor", "trail:2", "SHARED"},
    {"SpellUnspokenWord", Phase::Run, "SpellUnspokenWord_run", 0x2FD5D4, "ref_skills_c.unspokenword_spell_run", "particle:2", "spell"},
    {"SkillSummonAxe", Phase::Cast, "SkillSummonAxe_cast", 0x2FE644, "ref_skills_c.summonaxe_cast", "LightEffect:1", "SHARED SHARED(LightEffect)"},
    {"SpellSummonAxe", Phase::Cast, "SpellSummonAxe_ctor", 0x2FEB88, "ref_skills_c._summon_ctor", "particleGroup:1", "SHARED"},
    {"SpellSummonAxe", Phase::Run, "SpellSummonAxe_run", 0x2FEDDC, "ref_skills_c.summonaxe_spell_run", "particle:1", "spell"},
    {"SkillSummonArrows", Phase::Cast, "SkillSummonArrows_cast", 0x2FF5F4, "ref_skills_c.summonarrows_cast", "LightEffect:1", "SHARED SHARED(LightEffect)"},
    {"SpellSummonArrows", Phase::Cast, "SpellSummonArrows_ctor", 0x2FF87C, "ref_skills_c._summon_ctor", "particleGroup:1", "SHARED"},
    {"SpellSummonArrows", Phase::Run, "SpellSummonArrows_run", 0x2FFAD4, "ref_skills_c.summonarrows_spell_run", "particle:1", "spell"},
    {"SpellRoar", Phase::Run, "SpellRoar_run", 0x3003D8, "ref_skills_c.roar_spell_run", "particle:5", "spell"},
    {"SpellBattleCry", Phase::Run, "SpellBattleCry_run", 0x301A34, "ref_skills_c.battlecry_spell_run", "LightEffect:1 particle:5 particleGroup:1", "SHARED(LightEffect) spell"},
    {"SpellBloodLust", Phase::Run, "SpellBloodLust_run", 0x3034DC, "ref_skills_c.bloodlust_spell_run", "glow:4 particle:6", "spell"},
    {"SpellArena", Phase::Run, "SpellArena_run", 0x305BBC, "ref_fx_skills.arena_run", "particle:2", "spell"},
    {"SkillStrongStance", Phase::Event, "SkillStrongStance_trigger", 0x308578, "ref_skills_b.strongstance_trigger", "LightEffect:1 particle:1", "SHARED(LightEffect) seed3"},
    {"SkillStunningStrike", Phase::Cast, "SkillStunningStrike_spawnEffect", 0x308CB4, "ref_skills_b.stunningstrike_effect", "LightEffect:1", "SHARED SHARED(LightEffect)"},
    {"SpellGiantGrowth", Phase::Cast, "SpellGiantGrowth_ctor", 0x3090D0, "ref_skills_b.giantgrowth_spell_ctor", "lightLink:1 particleGroup:1", "SHARED spell"},
    {"SpellGiantGrowth", Phase::Run, "SpellGiantGrowth_emitParticles", 0x309678, "ref_skills_b.giantgrowth_emit", "particle:1", "spell"},
    {"SpellPlanarWind", Phase::Cast, "SpellPlanarWind_ctor", 0x309D0C, "ref_skills_b.planarwind_spell_ctor", "lightLink:1", "SHARED"},
    {"SpellPlanarWind", Phase::Run, "SpellPlanarWind_run", 0x30A0B0, "ref_skills_b.planarwind_run", "particle:3", "SHARED spell"},
    {"PusStorm", Phase::Cast, "PusStorm_ctor", 0x30AF6C, "ref_skills_c._storm_ctor", "LightEffect:1 lightLink:1", "SHARED(LightEffect)"},
    {"Pustule", Phase::Run, "Pustule_run", 0x30BDE0, "ref_fx_skills.pustule_run", "particle:2", "own"},
    {"SpellTagarsInsects", Phase::Run, "SpellTagarsInsects_run", 0x30CBC8, "ref_arena_monsters.tagar_spell_run", "particle:3", "SHARED spell"},
    {"SpellTagarsInsects", Phase::Run, "SpellTagarsInsects_tickVictims", 0x30D558, "ref_arena_monsters.tagar_tick_victims", "particle:1", "spell"},
    {"SkillDrowsy", Phase::Cast, "SkillDrowsy_cast", 0x30E1F0, "ref_skills_c.drowsy_cast", "LightEffect:1", "SHARED SHARED(LightEffect)"},
    {"SpellDrowsy", Phase::Run, "SpellDrowsy_run", 0x30E6F0, "ref_skills_c.drowsy_spell_run", "particle:2", "spell"},
    {"PoisonBolt", Phase::Cast, "PoisonBolt_ctor", 0x30F398, "ref_skills_c.poisonbolt_ctor", "lightLink:1", ""},
    {"PoisonBolt", Phase::Run, "PoisonBolt_run", 0x30F800, "ref_skills_c.poisonbolt_run", "particle:2", "own"},
    {"PoisonBolt", Phase::Event, "PoisonBolt_burst", 0x3101CC, "ref_skills_c.poisonbolt_burst", "particle:1", "own"},
    {"SkillSummonBadger", Phase::Cast, "SkillSummonBadger_cast", 0x3106F4, "ref_skills_c.summonbadger_cast", "LightEffect:1", "SHARED SHARED(LightEffect)"},
    {"SpellSummonBadger", Phase::Run, "SpellSummonBadger_run", 0x310CD0, "ref_skills_c.summonbadger_spell_run", "particle:1", "spell"},
    {"SpellSummonBadger", Phase::Draw, "SpellSummonBadger_draw", 0x311034, "ref_fx_skills.badger_draw", "glow:1", ""},
    {"SpellShadowStep", Phase::Cast, "SpellShadowStep_ctor", 0x311638, "ref_skills_b.shadowstep_spell_ctor", "LightEffect:1 particle:1", "SHARED SHARED(LightEffect) spell"},
    {"SpellResurrect", Phase::Cast, "SpellResurrect_ctor", 0x3127B4, "ref_skills_b.resurrect_spell_ctor", "lightLink:1", "SHARED"},
    {"SpellResurrect", Phase::Run, "SpellResurrect_run", 0x312B30, "ref_skills_b.resurrect_run", "modelVerts:2", ""},
    {"SpellResurrect", Phase::Draw, "SpellResurrect_draw", 0x312F48, "ref_fx_skills.resurrect_draw", "glow:1", ""},
    {"SpellResurrect", Phase::Event, "SpellResurrect_onOwnerDeath", 0x3130F4, "ref_skills_b.resurrect_on_owner_death", "LightEffect:1", "SHARED SHARED(LightEffect)"},
    {"SkillRaiseArmy", Phase::Cast, "SkillRaiseArmy_cast", 0x3134E4, "ref_skills_c.raisearmy_cast", "LightEffect:1", "SHARED SHARED(LightEffect)"},
    {"SpellRaiseArmy", Phase::Run, "SpellRaiseArmy_run", 0x31399C, "ref_skills_c.raisearmy_spell_run", "particle:1", "spell"},
    {"SpellRetribution", Phase::Cast, "SpellRetribution_ctor", 0x314488, "ref_skills_c.retribution_spell_ctor", "lightLink:1 particleGroup:1", "SHARED"},
    {"SpellRetribution", Phase::Run, "SpellRetribution_run", 0x314618, "ref_skills_c.retribution_spell_run", "modelVerts:1", ""},
    {"SpellSpiritShield", Phase::Cast, "SpellSpiritShield_ctor", 0x31500C, "ref_skills_c.spiritshield_spell_ctor", "LightEffect:1", "SHARED SHARED(LightEffect)"},
    {"SpellSpiritShield", Phase::Run, "SpellSpiritShield_dissolveParticles", 0x31539C, "ref_skills_c._spiritshield_particles", "particle:1", "spell"},
    {"SpellSpiritShield", Phase::Run, "SpellSpiritShield_reformParticles", 0x315610, "ref_skills_c._spiritshield_particles", "particle:1", "spell"},
    {"SpellPoisonWeapons", Phase::Cast, "SpellPoisonWeapons_ctor", 0x315F54, "ref_skills_a.weaponbuff_ctor", "LightEffect:1", "SHARED SHARED(LightEffect)"},
    {"SpellWaveOfFlame", Phase::Cast, "SpellWaveOfFlame_ctor", 0x316D10, "ref_skills_c.waveofflame_spell_ctor", "LightEffect:1", "SHARED SHARED(LightEffect)"},
    {"SpellWaveOfFlame", Phase::Run, "SpellWaveOfFlame_run", 0x317000, "ref_skills_c.waveofflame_spell_run", "particle:3", "spell"},
    {"SpellFrostArmor", Phase::Cast, "SpellFrostArmor_ctor", 0x317D1C, "ref_skills_c.frostarmor_spell_ctor", "LightEffect:1", "SHARED SHARED(LightEffect)"},
    {"SpellFrostArmor", Phase::Run, "SpellFrostArmor_run", 0x317EC0, "ref_skills_c.frostarmor_spell_run", "particle:2", "spell"},
    {"SpellFrostArmor", Phase::Event, "SpellFrostArmor_onHit", 0x318264, "ref_skills_c.frostarmor_on_hit", "fx_spawn:1", "SHARED"},
    {"SpellDiseaseBlade", Phase::Cast, "SpellDiseaseBlade_ctor", 0x318AD4, "ref_skills_c.diseaseblade_spell_ctor", "LightEffect:1", "SHARED SHARED(LightEffect)"},
    {"SkillFireAxe", Phase::Cast, "SkillFireAxe_cast", 0x31979C, "ref_skills_c.fireaxe_cast", "LightEffect:1", "SHARED SHARED(LightEffect)"},
    {"SkillColdAxe", Phase::Cast, "SkillColdAxe_cast", 0x31A0D8, "ref_skills_c.coldaxe_cast", "LightEffect:1", "SHARED SHARED(LightEffect)"},
    {"LightningStrikeSpell", Phase::Cast, "LightningStrikeSpell_ctor", 0x31AE48, "ref_skills_c._storm_ctor", "LightEffect:1 lightLink:1", "SHARED(LightEffect)"},
    {"SpellConeOfShock", Phase::Cast, "SpellConeOfShock_ctor", 0x31BAC8, "ref_skills_c.coneofshock_spell_ctor", "lightLink:1", "SHARED"},
    {"SmokeGen", Phase::Draw, "SmokeGen_draw", 0x31CBF8, "ref_fx_skills.smokegen_draw", "glow:1", ""},
  };
  return rows;
}

std::string_view phase_name(Phase p) {
  switch (p) {
  case Phase::Cast: return "cast";
  case Phase::Run: return "run";
  case Phase::Draw: return "draw";
  default: return "event";
  }
}

std::vector<Recipe> recipe(std::string_view cls) {
  std::vector<Recipe> out;
  for (const auto &row : recipes())
    if (row.cls == cls) out.push_back(row);
  return out;
}

std::vector<std::string> classes() {
  std::set<std::string> names;
  for (const auto &row : recipes()) names.emplace(row.cls);
  return {names.begin(), names.end()};
}

const Recipe *recipe_at(std::uint32_t addr) {
  for (const auto &row : recipes())
    if (row.addr == addr) return &row;
  return nullptr;
}

std::pair<std::string_view, std::string_view> model_method(const Recipe &row) {
  const auto dot = row.model.find('.');
  if (dot == std::string_view::npos) return {row.model, {}};
  return {row.model.substr(0, dot), row.model.substr(dot + 1)};
}

std::map<std::string, unsigned> static_calls(const Recipe &row) {
  std::map<std::string, unsigned> out;
  std::istringstream in{std::string(row.calls)};
  std::string part;
  while (in >> part) {
    const auto colon = part.find(':');
    out[part.substr(0, colon)] += static_cast<unsigned>(std::stoul(part.substr(colon + 1)));
  }
  return out;
}

unsigned generators(const Recipe &row) {
  unsigned out = 0;
  std::istringstream in{std::string(row.generators)};
  std::string g;
  while (in >> g) {
    if (g == "own") out |= Own;
    else if (g == "spell") out |= SpellGen;
    else if (g == "SHARED") out |= Shared;
    else if (g == "SHARED(LightEffect)") out |= SharedLightEffect;
    else if (g == "cosmetic") out |= Cosmetic;
    else if (g == "seed3") out |= Seed3;
    else throw Refusal("unknown generator " + g);
  }
  return out;
}

bool must_run_headless(const Recipe &row) { return generators(row) & (Own | SpellGen | Shared | SharedLightEffect); }

bool natively_modelled(const Recipe &row) { return model_method(row).first == "ref_fx_skills"; }

// =======================================================================================
// 3. The model
// =======================================================================================
std::uint64_t Memory::load(Addr addr, unsigned size) const {
  std::uint64_t v = 0;
  for (unsigned k = 0; k < size; ++k) {
    const auto it = bytes.find(addr + k);
    if (it != bytes.end()) v |= static_cast<std::uint64_t>(it->second) << (8 * k);
  }
  return v;
}

void Memory::poke(Addr addr, unsigned size, std::uint64_t value) {
  for (unsigned k = 0; k < size; ++k) bytes[addr + k] = static_cast<std::uint8_t>(value >> (8 * k));
}

namespace {
// The call sites of the model, in RecipeConstants order (see fx_skills.hpp, SITE MAPPING).
enum SiteId : unsigned {
  ShockGlow, FireArrowGlow, HolyStrikeGlow, BadgerGlow, ResurrectGlow, SmokeGlow,
  MultiFireTrail, MultiFireParticle, MultiFireBurstFire, MultiFireBurstDust,
  FlareTrail0, FlareTrail1, FlareTrail2, FlareRing0, FlareRing1, FlareDecal,
  PustuleFall, PustuleDrop, Cloud0, Cloud1, Cloud2, Arena0, Arena1, HornOmen0, HornOmen1,
  PlanarBurst, PlanarSpark, PlanarSpark2, PlanarFall, PlanarGround, PlanarGlow,
  FireFlame, FireEmber, FireBigGlow, FireFlameGlow, SITE_COUNT
};
struct SiteRef {
  SiteId id;
  std::uint32_t function;
  const char *primitive;
  unsigned ordinal;
};
const SiteRef SITE_REFS[] = {
    {ShockGlow, 0x2C4E64, "glow", 0}, {FireArrowGlow, 0x2D67E8, "glow", 0}, {HolyStrikeGlow, 0x2ECD90, "glow", 0},
    {BadgerGlow, 0x311034, "glow", 0}, {ResurrectGlow, 0x312F48, "glow", 0}, {SmokeGlow, 0x31CBF8, "glow", 0},
    {MultiFireTrail, 0x2DA164, "trail", 0}, {MultiFireParticle, 0x2DA2BC, "particle", 0},
    {MultiFireBurstFire, 0x2DA928, "fx_spawn", 0}, {MultiFireBurstDust, 0x2DA928, "fx_spawn", 1},
    {FlareTrail0, 0x2D33C0, "particle", 0}, {FlareTrail1, 0x2D33C0, "particle", 1},
    {FlareTrail2, 0x2D33C0, "particle", 2}, {FlareRing0, 0x2D33C0, "particle", 3},
    {FlareRing1, 0x2D33C0, "particle", 4}, {FlareDecal, 0x2D33C0, "decal", 0},
    {PustuleFall, 0x30BDE0, "particle", 0}, {PustuleDrop, 0x30BDE0, "particle", 1},
    {Cloud0, 0x2CE6E0, "particle", 0}, {Cloud1, 0x2CE6E0, "particle", 1}, {Cloud2, 0x2CE6E0, "particle", 2},
    {Arena0, 0x305BBC, "particle", 0}, {Arena1, 0x305BBC, "particle", 1},
    {HornOmen0, 0x2FC684, "particle", 0}, {HornOmen1, 0x2FC684, "particle", 1},
    {PlanarBurst, 0x2FB9B4, "particle", 0}, {PlanarSpark, 0x2FB9B4, "particle", 1},
    {PlanarSpark2, 0x2FB9B4, "particle", 2}, {PlanarFall, 0x2FB9B4, "particle", 3},
    {PlanarGround, 0x2FB9B4, "particle", 4}, {PlanarGlow, 0x2FB9B4, "glow", 0},
    {FireFlame, 0x2C595C, "particle", 0}, {FireEmber, 0x2C595C, "particle", 1},
    {FireBigGlow, 0x2C65E8, "glow", 0}, {FireFlameGlow, 0x2C65E8, "glow", 1},
};
enum Key : unsigned { A0, A1, A2, A3, T0, T1, T2, T3, F12, F13, KEY_COUNT };
const char *const KEY_NAMES[KEY_COUNT] = {"a0", "a1", "a2", "a3", "t0", "t1", "t2", "t3", "f12", "f13"};
} // namespace

struct Model::Constants {
  std::array<std::array<std::optional<Raw>, KEY_COUNT>, SITE_COUNT> site{};
  std::map<std::string, LightRecord, std::less<>> lights;
  Raw k(SiteId id, Key key) const {
    const auto &v = site[id][key];
    if (!v) throw Refusal(std::string("recipe constant not loaded: ") + KEY_NAMES[key]);
    return *v;
  }
};

Model::Model(const RecipeConstants &constants) {
  auto k = std::make_shared<Constants>();
  static_assert(sizeof(SITE_REFS) / sizeof(SITE_REFS[0]) == SITE_COUNT, "one reference per site");
  for (const auto &ref : SITE_REFS)
    for (unsigned key = 0; key < KEY_COUNT; ++key) {
      const auto it = constants.needed_.find({ref.function, ref.primitive, ref.ordinal, KEY_NAMES[key]});
      if (it != constants.needed_.end()) k->site[ref.id][key] = it->second;
    }
  for (const auto &[name, rec] : constants.lights_) k->lights.emplace(name, rec);
  k_ = std::move(k);
}

std::int32_t Model::s8(Addr a) const { return static_cast<std::int8_t>(static_cast<std::uint8_t>(mem.load(a, 1))); }
std::int32_t Model::s16(Addr a) const { return s16v(mem.load(a, 2)); }
std::int32_t Model::s32(Addr a) const { return s32v(mem.load(a, 4)); }

void Model::w(Addr a, unsigned size, std::uint64_t v) {
  if (size < 8) v &= (std::uint64_t{1} << (8 * size)) - 1;
  writes.push_back({a, size, v});
  mem.poke(a, size, v);
}

void Model::wvec(Addr a, const Vec3 &v) {
  // the twelve-byte copy the compiler emits for a vector assignment
  w(a, 8, static_cast<std::uint64_t>(v[0]) | (static_cast<std::uint64_t>(v[1]) << 32));
  w(a + 8, 4, v[2]);
}

std::string Model::cstr(Addr a) const {
  std::string out;
  while (out.size() < 256) {
    const auto b = u8(a + static_cast<Addr>(out.size()));
    if (!b) break;
    out.push_back(static_cast<char>(b));
  }
  return out;
}

// ---- callbacks --------------------------------------------------------------------------
namespace {
struct Ext {
  const char *ints;
  unsigned floats;
  char ret;
};
// The pinned ref_fx_skills.EXTERNALS rows of every name part 3 calls (for the defaults).
const std::map<std::string, Ext, std::less<>> &externals() {
  static const std::map<std::string, Ext, std::less<>> rows = {
      {"glow_add", {"viiiiiii", 2, 'n'}}, {"isNearView", {"v", 1, 'i'}},
      {"model_getSamplePointCount", {"p", 0, 'i'}}, {"model_getSamplePoint", {"ppviioOi", 1, 'n'}},
      {"Projectile_ctor", {"pvpi", 0, 'n'}}, {"trail_create", {"iiiiiii", 1, 'i'}},
      {"trail_addPoint", {"iv", 0, 'n'}}, {"obj_runEveryFrame", {"p", 0, 'n'}},
      {"obj_moveAndCollide", {"pv", 0, 'i'}}, {"particle_spawn", {"pvv", 1, 'n'}},
      {"trail_release", {"i", 0, 'n'}}, {"libc_sprintf", {"-pi", 0, 's'}}, {"fx_spawn", {"iv", 0, 'n'}},
      {"sound_playOnObject", {"pp", 1, 'i'}}, {"res_find", {"pS", 0, 'i'}}, {"obj_delete", {"p", 0, 'n'}},
      {"terrain_height", {"pv", 1, 'f'}}, {"sound_playAtPos", {"pv", 1, 'i'}}, {"decal_add", {"viii", 1, 'n'}},
      {"Creature.v1", {"pi", 0, 'n'}}, {"world_collectObjectsInBox", {"vvni", 0, 'n'}},
      {"damage_apply", {"ppiiiidi", 2, 'i'}}, {"Spell.v1", {"pi", 0, 'n'}},
      {"particleGroup_create", {"", 0, 'i'}}, {"particleGroup_setMatrix", {"ip", 0, 'n'}},
      {"particleGroup_begin", {"i", 0, 'n'}}, {"particleGroup_end", {"i", 0, 'n'}},
      {"light_link", {"p", 0, 'n'}}, {"light_relink", {"p", 0, 'n'}}, {"light_unlink", {"p", 0, 'n'}},
      {"terrain_heightAtPoint", {"pv", 0, 'f'}}, {"GameObject_ctor", {"pvpp", 0, 'n'}},
      {"Creature.v0", {"", 0, 'i'}}, {"typeinfo", {"i", 0, 'i'}}, {"rtti_typeInfoEquals", {"ii", 0, 'i'}},
      {"obj_addToAlwaysDraw", {"p", 0, 'n'}}, {"path_blockNodes", {"pv", 1, 'n'}},
      {"mem_allocTagged", {"ip", 0, 'i'}}, {"obj_getPropStr", {"ppi", 0, 'i'}},
      {"obj_getPropInt", {"ppi", 0, 'i'}}, {"Creature.v16", {"p", 0, 'i'}}, {"Creature.v20", {"pp", 0, 'i'}},
      {"Creature.v37", {"p", 0, 'i'}}, {"Skill.v11", {"piii", 0, 'i'}}, {"Skill.v12", {"piipi", 0, 'i'}},
  };
  return rows;
}
} // namespace

Value Model::default_answer(const std::string &name, const std::vector<Value> &args) const {
  // ref_skills_c.shape_result with no answers given: zeros, (0, 0, 0) for point outputs,
  // empty lists for list outputs; a may-be-null output only when its flag argument is set.
  const auto it = externals().find(name);
  if (it == externals().end()) throw Refusal("no handler and no reference default for " + name);
  const Ext &ext = it->second;
  std::vector<Value> values;
  std::size_t position = 0;
  for (const char *p = ext.ints; *p; ++p) {
    const char kind = *p;
    if (std::strchr("oOwWbnL", kind)) {
      Value v = (kind == 'o' || kind == 'O') ? V({0, 0, 0}) : (kind == 'n' || kind == 'L') ? Value::list({}) : I(0);
      if ((kind != 'O' && kind != 'W') || (position < args.size() && args[position].truthy())) values.push_back(v);
    }
    if (std::strchr("ipvelsSOWdTI", kind)) ++position;
  }
  if (ext.ret == 's') return Value::tuple({Value::str("buf"), Value::bytes("")});
  std::vector<Value> result;
  if (ext.ret != 'n') result.push_back(I(0));
  for (auto &v : values) result.push_back(std::move(v));
  if (result.empty()) return Value::none();
  if (result.size() == 1) return result[0];
  return Value::tuple(std::move(result));
}

Value Model::call(std::string name, std::vector<Value> args) {
  for (auto &a : args)
    if (a.is_int()) a.i &= M32;
  calls.push_back({name, args});
  const Callout &recorded = calls.back();
  if (++depth_ > MAX_DEPTH) {
    --depth_;
    throw Refusal("call depth");
  }
  struct Guard {
    unsigned &d;
    ~Guard() { --d; }
  } guard{depth_};
  const Callout copy = recorded; // handlers may append to `calls`
  const auto h = handlers.find(copy.name);
  if (h != handlers.end() && h->second) return h->second(*this, copy);
  if (fallback)
    if (auto v = fallback(*this, copy)) return *v;
  return default_answer(copy.name, copy.args);
}

// A virtual call through object +0x48 (ref_arena_monsters.Model.vcall over
// ref_creature.Model.vcall): a class body the reference runs inline runs here (slot 16's
// 0x3c2738 model getter, 0x3c2708 set-home-range; base bodies slot 20 Creature_isEnemy
// 0x119328 and slot 37 0x3c2730 team), any other body the reference has refuses (not
// modelled natively), anything else is the call-out 'Creature.v<slot>'.
Value Model::vcall(Addr obj, unsigned slot, std::vector<Value> args) {
  static const std::set<std::uint32_t> class_bodies = {0x10BEB8, 0x10E658, 0x27236C, 0x272F4C, 0x2A8B84, 0x2A91B8,
                                                       0x2C2440, 0x2C34E4, 0x2C382C, 0x2C3C54, 0x3C2708, 0x3C2738};
  if (++depth_ > MAX_DEPTH) {
    --depth_;
    throw Refusal("virtual call depth");
  }
  struct Guard {
    unsigned &d;
    ~Guard() { --d; }
  } guard{depth_};
  const Addr table = u32(obj + 0x48);
  const std::uint32_t function = u32(table + 8 * slot + 4);
  const Raw self = u(obj + s16(table + 8 * slot));
  if (class_bodies.count(function)) {
    if (function == 0x3C2738 && args.empty()) // slot16_model: the model of the class definition
      return I(u32(u32(Addr(self) + 0x24) + 0xC));
    if (function == 0x3C2708 && args.size() == 1) { // slot34_set_home_range
      w32(Addr(self) + 0x3C0, fi(static_cast<std::int64_t>(as_int(args[0], "home range") & 0xFFFF)));
      return Value::none();
    }
    throw Refusal("virtual body not modelled natively: " + std::to_string(function));
  }
  if (slot == 20 && function == 0x119328) {
    if (args.size() != 1) throw Refusal("Creature_isEnemy arity");
    return I(static_cast<std::uint64_t>(static_cast<std::int64_t>(is_enemy(self, args[0]))));
  }
  if (slot == 37 && function == 0x3C2730) {
    if (!args.empty()) throw Refusal("slot 37 arity");
    return I(static_cast<std::uint64_t>(static_cast<std::int64_t>(s8(Addr(self) + 0x310))));
  }
  std::vector<Value> lanes{I(self)};
  for (auto &a : args) lanes.push_back(std::move(a));
  return call("Creature.v" + std::to_string(slot), std::move(lanes));
}

// Creature_isEnemy 0x119328, virtual slot 20 (creature, other) -> 0, 1 or 4 (ref_creature.is_enemy)
std::int32_t Model::is_enemy(Addr c, const Value &other_v) {
  const std::uint64_t other = as_int(other_v, "is_enemy other");
  if (!other || static_cast<Addr>(other) == c) return 0;
  const auto flags = u32(Addr(other) + 0xC);
  if (!(flags & 0x60) || (flags & 0x20008)) return 0;
  if (u64(Addr(other) + 0xA8) & 0x4000800) return 0;
  const auto mine = s32v(as_int(vcall(c, 37, {}), "team"));
  const auto theirs = s32v(as_int(vcall(Addr(other), 37, {}), "team"));
  if (mine == theirs) return 0;
  return (u64(Addr(other) + 0xA8) & 0x100000000000ull) ? 4 : 1;
}

// A virtual call on a Skill object (table pointer at +0x60; ref_skills_c.Base.skill_call):
// the base statistic bodies run here, another body the reference has refuses, anything else
// is the call-out 'Skill.v<slot>'.
Value Model::skill_call(Addr skill, unsigned slot, std::vector<std::int64_t> args) {
  static const std::set<std::uint32_t> other_bodies = {
      0x2DDB90, 0x2E6534, 0x2E6598, 0x2E6894, 0x2E7224, 0x2E74B0, 0x2E7690, 0x2E8A88, 0x2E8CF8, 0x2E8E94, 0x2FD030,
      0x2FD334, 0x2FE644, 0x2FE938, 0x2FF5EC, 0x2FF5F4, 0x2FF848, 0x2FFF64, 0x2FFF98, 0x300138, 0x301464, 0x301498,
      0x3016E0, 0x30300C, 0x303040, 0x303220, 0x30AAA8, 0x30AF38, 0x30E1F0, 0x30E5EC, 0x30F168, 0x30F364, 0x310690,
      0x3106F4, 0x310C08, 0x3134B0, 0x3134E4, 0x3137A4, 0x3142BC, 0x3142F0, 0x314454, 0x314E1C, 0x314E50, 0x314FD8,
      0x316B1C, 0x316B24, 0x316CDC, 0x317AFC, 0x317B30, 0x317CE8, 0x318730, 0x318764, 0x318AA0, 0x319710, 0x31979C,
      0x319EA0, 0x31A04C, 0x31A0D8, 0x31A7D4, 0x31A920, 0x31AE14, 0x31B8F0, 0x31BA94, 0x3C8218, 0x3C8220, 0x3C82A8,
      0x3C82E4, 0x3C88E4, 0x2E18A4, 0x2E2368, 0x30C708};
  if (++depth_ > MAX_DEPTH) {
    --depth_;
    throw Refusal("skill call depth");
  }
  struct Guard {
    unsigned &d;
    ~Guard() { --d; }
  } guard{depth_};
  const Addr table = u32(skill + 0x60);
  const Addr self = u(skill + s16(table + 8 * slot));
  const std::uint32_t function = u32(table + 8 * slot + 4);
  auto round = [](Raw value) { // trunc(x + 0.5) for x >= 0, trunc(x - 0.5) below
    return le(ZERO, value) ? ti(add(value, HALF)) : ti(sub(value, HALF));
  };
  auto tier = [&](Addr first_at, Addr base_at, Addr step_at, std::int64_t stat, std::int64_t level) -> std::uint64_t {
    const std::int64_t first = s16(self + first_at + 2 * stat);
    if (level < first) return 0;
    const Raw step = mul(u32(self + step_at + 4 * stat), fi(level - first));
    return static_cast<std::uint32_t>(round(add(fi(s16(self + base_at + 2 * stat)), step)));
  };
  switch (function) {
  case 0x3C85FC: { // Skill_getStat, slot 10 (skill, stat, level, *_): tier 1
    if (args.size() < 2) throw Refusal("Skill_getStat arity");
    const std::int64_t stat = s32v(static_cast<std::uint64_t>(args[0])), level = s32v(static_cast<std::uint64_t>(args[1]));
    if (level == 0) return I(0);
    return I(tier(0x0E, 0x18, 0x24, stat, level));
  }
  case 0x3C8690: { // Skill_getStatTier2, slot 11 (skill, stat, level, cap)
    if (args.size() != 3) throw Refusal("Skill_getStatTier2 arity");
    const std::int64_t stat = s32v(static_cast<std::uint64_t>(args[0]));
    std::int64_t level = s32v(static_cast<std::uint64_t>(args[1]));
    const std::int64_t cap = s32v(static_cast<std::uint64_t>(args[2]));
    if (level == 0) return I(0);
    if (cap > 0 && cap < level) level = cap;
    if (s16(self + 0x38 + 2 * stat) < 0) return I(tier(0x0E, 0x18, 0x24, stat, level));
    return I(tier(0x38, 0x42, 0x4C, stat, level));
  }
  case 0x3C87F0: { // Skill_getStatFor, slot 12 (skill, stat, level, caster, cap)
    if (args.size() != 4) throw Refusal("Skill_getStatFor arity");
    const Addr caster = args[2];
    if (caster && (u32(caster + 0xC) & 0x40)) return skill_call(self, 10, {args[0], args[1]});
    return skill_call(self, 11, {args[0], args[1], args[3]});
  }
  case 0x3C8864: { // Skill_getStatForHandle, slot 13 (skill, stat, level, handle, cap)
    if (args.size() != 4) throw Refusal("Skill_getStatForHandle arity");
    const Addr node = u32(args[2]);
    if (node && (u32(u32(node + 0xC) + 0xC) & 0x40)) return skill_call(self, 10, {args[0], args[1]});
    return skill_call(self, 11, {args[0], args[1], args[3]});
  }
  case 0x3C88DC: // Skill_adjustLevel, slot 14 (skill, level, n=0): the level
    if (args.empty() || args.size() > 2) throw Refusal("Skill_adjustLevel arity");
    return I(static_cast<std::uint64_t>(args[0]) & M32);
  default:
    break;
  }
  if (other_bodies.count(function)) throw Refusal("skill body not modelled natively: " + std::to_string(function));
  std::vector<Value> lanes{I(static_cast<std::uint64_t>(self))};
  for (auto a : args) lanes.push_back(I(static_cast<std::uint64_t>(a)));
  return call("Skill.v" + std::to_string(slot), std::move(lanes));
}

// What a spell does to end itself: its own slot 1 with argument 3 (table at +0x14).  A spell
// run body the reference has (ref_skills_c / ref_arena_monsters SPELL_BODIES) refuses.
void Model::spell_destroy(Addr spell) {
  static const std::set<std::uint32_t> spell_bodies = {
      0x2E699C, 0x2E7ACC, 0x2E90B8, 0x2FD5D4, 0x2FEDDC, 0x2FFAD4, 0x3003D8, 0x301A34, 0x3034DC, 0x30E6F0,
      0x310CD0, 0x31399C, 0x314618, 0x3151B4, 0x317000, 0x317EC0, 0x319068, 0x31BCD0, 0x2E177C, 0x30CBC8};
  const Addr table = u32(spell + 0x14);
  const std::uint32_t function = u32(table + 8 + 4);
  if (spell_bodies.count(function)) throw Refusal("spell body not modelled natively: " + std::to_string(function));
  call("Spell.v1", {I(u(spell + s16(table + 8))), I(3)});
}

// ---- generators -------------------------------------------------------------------------
std::uint32_t Model::draw() { // one draw from the shared seed 0x404464
  const auto seed = static_cast<std::uint32_t>((std::uint64_t{u32(G_RAND_SEED)} * 0x19660D + 0x3C6EF35F) & M32);
  w32(G_RAND_SEED, seed);
  ++draws;
  return seed >> 16;
}
std::uint32_t Model::obj_rand(Addr obj) { // GameObject_rand 0x1509d0: the object's generator +0x44
  const auto seed = static_cast<std::uint32_t>((std::uint64_t{u32(obj + 0x44)} * 0x19660D + 0x3C6EF35F) & M32);
  w32(obj + 0x44, seed);
  return seed >> 16;
}
std::uint32_t Model::spell_rand(Addr spell) { // Spell_rand 0x16a964: the spell's generator +0x10
  const auto seed = static_cast<std::uint32_t>((std::uint64_t{u32(spell + 0x10)} * 0x19660D + 0x3C6EF35F) & M32);
  w32(spell + 0x10, seed);
  return seed >> 16;
}
// A point of the unit ball: three draws per try, repeated while 1 < x*x + y*y + z*z
Vec3 Model::obj_ball(Addr obj) {
  for (;;) {
    Vec3 p;
    for (auto &x : p) x = sub(mul(mul(fi(obj_rand(obj)), F_2M16), core::TWO), ONE);
    if (!lt(ONE, add(add(mul(p[0], p[0]), mul(p[1], p[1])), mul(p[2], p[2])))) return p;
  }
}
Vec3 Model::spell_ball(Addr spell) {
  for (;;) {
    Vec3 p;
    for (auto &x : p) x = sub(mul(mul(fi(spell_rand(spell)), F_2M16), core::TWO), ONE);
    if (!lt(ONE, add(add(mul(p[0], p[0]), mul(p[1], p[1])), mul(p[2], p[2])))) return p;
  }
}
// Creature_getVisualPos 0x118738: the position moved along the facing by +0xe0, height +0x3c
Vec3 Model::visual_pos(Addr c) const {
  Raw x = u32(c + 0x10), y = u32(c + 0x14);
  if (!eq(u32(c + 0xE0), ZERO)) {
    x = add(x, mul(cos16(static_cast<std::uint32_t>(s16(c + 0x38))), u32(c + 0xE0)));
    y = add(y, mul(sin16(static_cast<std::uint32_t>(s16(c + 0x38))), u32(c + 0xE0)));
  }
  return {x, y, u32(c + 0x3C)};
}

// =======================================================================================
// Draw slots (GameObject slot 5, Spell slot 3): glow sprites, once per rendered frame
// =======================================================================================
#define K(site, key) k_->k(site, key)

// ShockProjectile_draw 0x2c4e64.  t = min(1, (expiry +0x58 - frame + 300) / 30) (no lower
// bound); one glow at the position: alpha int(128 t), size int(15152 t + sin16(frame mod
// 0x8000 x 0x1dd7) x int(15152 t x 0.195006)), texture *0x4f8498, angle (frame << 26) >> 16.
void Model::shock_draw(Raw obj) {
  const std::int64_t frame = s32(G_SIM_FRAME);
  Raw t = mul(fi(std::int64_t{s32(Addr(obj) + 0x58)} - frame + 300), 0x3D088888);
  if (lt(ONE, t)) t = ONE;
  const std::int64_t rem = frame % 0x8000;
  const Raw s = sin16(static_cast<std::uint32_t>((rem * 0x1DD7) & static_cast<std::int64_t>(M32)));
  const Raw big = mul(t, 0x466CC000);
  const auto alpha = ti(mul(t, 0x43000000));
  const Raw k = fi(ti(mul(big, 0x3E47AFA7)));
  const Raw size = fi(ti(add(big, mul(s, k))));
  const auto angle = angle_shift(u32(G_SIM_FRAME), 26);
  call("glow_add", {V(vec(Addr(obj) + 0x10)), I(K(ShockGlow, A1)), I(K(ShockGlow, A2)), I(K(ShockGlow, A3)),
                    I(u(alpha)), I(u32(G_GLOW_TEX_C)), I(u(angle)), I(K(ShockGlow, T3)), I(K(ShockGlow, F12)), I(size)});
}

// FireArrowProjectile_draw 0x2d67e8: one glow at the arrow, size radius +0x1c x 1408,
// texture *0x4f8494.
void Model::firearrow_draw(Raw obj) {
  const Raw size = mul(u32(Addr(obj) + 0x1C), 0x44B00000);
  call("glow_add", {V(vec(Addr(obj) + 0x10)), I(K(FireArrowGlow, A1)), I(K(FireArrowGlow, A2)),
                    I(K(FireArrowGlow, A3)), I(K(FireArrowGlow, T0)), I(u32(G_GLOW_TEX_B)), I(K(FireArrowGlow, T2)),
                    I(K(FireArrowGlow, T3)), I(K(FireArrowGlow, F12)), I(size)});
}

// HolyStrike_draw 0x2ecd90: one glow one unit off the object sideways (the unit vector
// (dir.y, -dir.x, 0) of the direction +0x6c, +0x70) and one unit up; size 5000 + 250
// sin16(frame << 10), texture *0x4f8498, angle (frame << 25) >> 16.
void Model::holystrike_draw(Raw obj) {
  const Raw d0 = u32(Addr(obj) + 0x70), d1 = neg(u32(Addr(obj) + 0x6C)), d2 = ZERO;
  const Raw length2 = add(add(mul(d0, d0), mul(d1, d1)), mul(d2, d2));
  const Raw r = rsqrt(ONE, length2);
  const Vec3 n{mul(d0, r), mul(d1, r), mul(d2, r)};
  const Vec3 p = vec(Addr(obj) + 0x10);
  const Vec3 point{add(add(n[0], ZERO), p[0]), add(add(n[1], ZERO), p[1]), add(add(n[2], ONE), p[2])};
  const Raw s = sin16(static_cast<std::uint32_t>((std::uint64_t{u32(G_SIM_FRAME)} << 10) & M32));
  const Raw size = add(mul(s, 0x437A0000), 0x459C4000);
  const auto angle = angle_shift(u32(G_SIM_FRAME), 25);
  call("glow_add", {V(point), I(K(HolyStrikeGlow, A1)), I(K(HolyStrikeGlow, A2)), I(K(HolyStrikeGlow, A3)),
                    I(K(HolyStrikeGlow, T0)), I(u32(G_GLOW_TEX_C)), I(u(angle)), I(K(HolyStrikeGlow, T3)),
                    I(K(HolyStrikeGlow, F12)), I(size)});
}

// SpellSummonBadger_draw 0x311034, Spell slot 3.  Needs the badger (handle +0x18).  f = +0x1c,
// folded above 1 to 1 - (f - 1) and clamped at 0; nothing when f <= 0.  One glow at every
// sample point of the badger's model (slot 16 gives the model): size = the second output's x
// + 250 sin16(frame << 10), alpha int(65 f), texture *0x4f848c, angle (frame << 22) >> 16.
void Model::badger_draw(Raw spell) {
  const Addr node = u32(Addr(spell) + 0x18);
  if (!node) return;
  const Addr badger = u32(node + 0xC);
  if (!badger) return;
  Raw f = u32(Addr(spell) + 0x1C);
  if (lt(ONE, f)) {
    f = sub(ONE, sub(f, ONE));
    if (lt(f, ZERO)) f = ZERO;
  }
  if (le(f, ZERO)) return;
  const std::int64_t count = s32v(as_int(call("model_getSamplePointCount", {vcall(badger, 16, {})}), "sample count"));
  const Vec3 at = visual_pos(badger);
  std::int64_t index = 0;
  std::size_t guard = 0;
  while (index < count) {
    if (++guard > MAX_LOOP) throw Refusal("badger sample loop");
    Value model = vcall(badger, 16, {});
    const Value result = call("model_getSamplePoint", {model, I(u(badger + 0x114)), V(at), I(u(s16(badger + 0x38))),
                                                       I(u(index)), I(1), I(0), I(u32(badger + 0xDC))});
    if (!result.is_sequence() || result.items.size() != 2) throw Refusal("sample point answer: (point, second)");
    const Value point = result.items[0];
    const Value &extra = result.items[1];
    ++index;
    const Raw s = sin16(static_cast<std::uint32_t>((std::uint64_t{u32(G_SIM_FRAME)} << 10) & M32));
    const Raw size = add(as_raw(item(extra, 0, "second point"), "second point x"), mul(s, 0x437A0000));
    const auto alpha = ti(mul(f, 0x42820000));
    const auto angle = angle_shift(u32(G_SIM_FRAME), 22);
    call("glow_add", {point, I(K(BadgerGlow, A1)), I(K(BadgerGlow, A2)), I(K(BadgerGlow, A3)), I(u(alpha)),
                      I(u32(G_GLOW_TEX_A)), I(u(angle)), I(K(BadgerGlow, T3)), I(K(BadgerGlow, F12)), I(size)});
  }
}

// SpellResurrect_draw 0x312f48, Spell slot 3.  Only with one player in the session
// (*0x4d8ae0 == 1).  f = 0 while +0x20 is 0, else min(1, (frame - +0x6c, unsigned) / 60).
// Thirteen glows on a ring of radius int(100 f) + 15 around the owner (+0x18), 116 above its
// position; angles start at frame x 0x75 and step 0x13b1; alpha int((1 - f) x 128), texture
// *0x44fe04.
void Model::resurrect_draw(Raw spell) {
  if (s32(G_NUM_PLAYERS) != 1) return;
  const std::uint64_t frame = u32(G_SIM_FRAME);
  std::uint64_t angle = (frame * 0x75) & 0xFFFF;
  Raw f = ZERO;
  if (u32(Addr(spell) + 0x20)) f = fmin(mul(from_uint(static_cast<std::uint32_t>(frame - u32(Addr(spell) + 0x6C))), 0x3C888888), ONE);
  const std::int64_t radius_i = ti(mul(f, 0x42C80000));
  const auto alpha = ti(mul(sub(ONE, f), 0x43000000));
  for (int k = 0; k < 13; ++k) {
    const Raw s = sin16(static_cast<std::uint32_t>(angle));
    const Raw radius = fi(radius_i + 15);
    const Addr owner = u32(Addr(spell) + 0x18);
    const Raw x = add(u32(owner + 0x10), mul(s, radius));
    const Raw c = cos16(static_cast<std::uint32_t>(angle));
    const Raw y = add(u32(owner + 0x14), mul(c, radius));
    const Raw z = add(u32(owner + 0x18), 0x42E80000);
    call("glow_add", {V({x, y, z}), I(K(ResurrectGlow, A1)), I(K(ResurrectGlow, A2)), I(K(ResurrectGlow, A3)),
                      I(u(alpha)), I(u32(G_GLOW_TEX_RES)), I(K(ResurrectGlow, T2)), I(K(ResurrectGlow, T3)),
                      I(K(ResurrectGlow, F12)), I(K(ResurrectGlow, F13))});
    angle = (angle + 0x13B1) & 0xFFFF;
  }
}

// SmokeGen_draw 0x31cbf8: when isNearView(position; 1000), one glow per live puff of the ring
// at +0x50 (100 records of five s16; first +0x438, end +0x43a).  t = age / (lifetime +0x464 x
// 60 x speed +0x474); point = position + (x, y) / 32, z = position.z + age / 32 + rise +0x47c;
// size, colour and alpha run linearly from +0x45c / +0x43c..+0x448 to +0x460 / +0x44c..+0x458;
// texture +0x4c, angle s16(+0x488 x +0x472), negated on odd records.
void Model::smokegen_draw(Raw obj) {
  const Addr o = obj;
  if (!call("isNearView", {V(vec(o + 0x10)), I(0x447A0000)}).truthy()) return;
  const Raw span = mul(fi(std::int64_t{s32(o + 0x464)} * 60), u32(o + 0x474));
  std::int64_t i = s16(o + 0x438);
  const std::int64_t end = s16(o + 0x43A);
  std::size_t guard = 0;
  while (i != end) {
    if (++guard > MAX_LOOP) throw Refusal("smoke ring loop");
    const Addr rec = o + 0x50 + i * 10;
    const Raw age = fi(s16(rec + 4));
    const Raw t = div(age, span);
    const Raw z = add(add(u32(o + 0x18), mul(age, 0x3D000000)), u32(o + 0x47C));
    const Raw x = add(u32(o + 0x10), mul(fi(s16(rec)), 0x3D000000));
    const Raw y = add(u32(o + 0x14), mul(fi(s16(rec + 2)), 0x3D000000));
    auto lerp = [&](Addr a_off, Addr b_off) {
      const std::int64_t a = s32(o + a_off);
      return add(fi(a), mul(fi(std::int64_t{s32(o + b_off)} - a), t));
    };
    std::array<std::int32_t, 4> colour{};
    for (unsigned k = 0; k < 4; ++k) colour[k] = ti(lerp(0x43C + 4 * k, 0x44C + 4 * k));
    const Raw size = lerp(0x45C, 0x460);
    std::int64_t angle = s16v(static_cast<std::uint64_t>(std::int64_t{s32(o + 0x488)} * s16(o + 0x472)) & 0xFFFF);
    if (i & 1) angle = s16v(static_cast<std::uint64_t>(-angle) & 0xFFFF);
    call("glow_add", {V({x, y, z}), I(u(colour[0])), I(u(colour[1])), I(u(colour[2])), I(u(colour[3])),
                      I(u32(o + 0x4C)), I(u(angle)), I(K(SmokeGlow, T3)), I(K(SmokeGlow, F12)), I(size)});
    ++i;
    if (i == 100) i = 0;
  }
}

// =======================================================================================
// MultiFireProjectile (the arrows of Multi Fire): everything placed with the OWN generator
// =======================================================================================
namespace {
constexpr Raw MF_VT = 0x519128;
constexpr Raw MF_LUMP = 0x54F560, MF_SOUND_FIRE = 0x54F598, MF_SOUND_DUST = 0x54F5B0;
constexpr Addr LEVEL_FLAGS = 0x44FE08; // level flag word; 0x40000 selects the fire burst
} // namespace

// MultiFireProjectile_ctor 0x2da164 (object, position, angle, owner, level, skill).
// Projectile_ctor(object, position, owner, 0); angle s16 +0x64, table, radius +0x1c = 8, skill
// +0x70, level +0x68; speed = skill slot 13 (1, level, owner handle +0x4c, 10); velocity +0x54 =
// speed x (cos16, sin16)(angle), 0; age +0x6c and ended +0x60 = 0; the trail -> +0x74, its first
// point at the position; the run list.
void Model::multifire_ctor(Raw obj, const Vec3 &pos, Raw angle_in, Raw owner, Raw level, Raw skill) {
  const Addr o = obj;
  const std::int64_t angle = s16v(angle_in);
  call("Projectile_ctor", {I(obj), V(pos), I(owner), I(0)});
  w16(o + 0x64, u(angle));
  w32(o + 0x48, MF_VT);
  w32(o + 0x1C, 0x41000000);
  w32(o + 0x70, skill);
  w32(o + 0x68, level);
  const Raw speed = fi(s32v(as_int(skill_call(skill, 13, {1, level, o + 0x4C, 10}), "speed statistic")));
  w32(o + 0x54, mul(cos16(static_cast<std::uint32_t>(s16(o + 0x64))), speed));
  const Raw sine = sin16(static_cast<std::uint32_t>(s16(o + 0x64)));
  w32(o + 0x5C, 0);
  w32(o + 0x6C, 0);
  w32(o + 0x60, 0);
  w32(o + 0x58, mul(sine, speed));
  const Value handle = call("trail_create", {I(K(MultiFireTrail, A0)), I(K(MultiFireTrail, A1)),
                                             I(K(MultiFireTrail, A2)), I(K(MultiFireTrail, A3)),
                                             I(K(MultiFireTrail, T0)), I(K(MultiFireTrail, T1)),
                                             I(K(MultiFireTrail, T2)), I(K(MultiFireTrail, F12))});
  w32(o + 0x74, as_int(handle, "trail handle"));
  call("trail_addPoint", {handle, V(vec(o + 0x10))});
  call("obj_runEveryFrame", {I(obj)});
}

// MultiFireProjectile_run 0x2da2bc, GameObject slot 4.  Nothing once ended.  A trail point at
// the position, age + 1, obj_moveAndCollide with the velocity; terrain (bit 1): end(1); age 601
// or more: end(0); else ten particles: v = 8 x a unit-ball point, position + v x r (r = one
// more draw / 65536), velocity v.
void Model::multifire_run(Raw obj) {
  const Addr o = obj;
  if (u32(o + 0x60)) return;
  call("trail_addPoint", {I(u32(o + 0x74)), V(vec(o + 0x10))});
  w32(o + 0x6C, std::uint64_t{u32(o + 0x6C)} + 1);
  const auto result = as_int(call("obj_moveAndCollide", {I(obj), V(vec(o + 0x54))}), "move result");
  if (result & 1) return multifire_end(obj, 1);
  if (!(s32(o + 0x6C) < 0x259)) return multifire_end(obj, 0);
  for (int k = 0; k < 10; ++k) {
    const Vec3 ball = obj_ball(o);
    const Vec3 v{mul(ball[0], 0x41000000), mul(ball[1], 0x41000000), mul(ball[2], 0x41000000)};
    const Raw share = mul(fi(obj_rand(o)), F_2M16);
    const Vec3 step{mul(v[0], share), mul(v[1], share), mul(v[2], share)};
    const Vec3 p = vec(o + 0x10);
    call("particle_spawn", {I(K(MultiFireParticle, A0)), V({add(p[0], step[0]), add(p[1], step[1]), add(p[2], step[2])}),
                            V(v), I(K(MultiFireParticle, F12))});
  }
}

// MultiFireProjectile_end 0x2da928 (object, reason), once (ended +0x60).  The trail is
// released; with a reason, one own draw picks one of four impact sounds and the burst at the
// position: site 0 when level flag 0x40000 is set (the reference's fx_spawn(8), fire), else site
// 1 (fx_spawn(2), dust); then the sound on the object; obj_delete; ended = 1.
void Model::multifire_end(Raw obj, Raw reason) {
  const Addr o = obj;
  if (u32(o + 0x60)) return;
  call("trail_release", {I(u32(o + 0x74))});
  if (reason) {
    const bool fire = u32(LEVEL_FLAGS) & 0x40000;
    const auto d = obj_rand(o);
    const Value text = call("libc_sprintf", {I(fire ? MF_SOUND_FIRE : MF_SOUND_DUST), I((d & 3) + 1)});
    call("fx_spawn", {I(fire ? K(MultiFireBurstFire, A0) : K(MultiFireBurstDust, A0)), V(vec(o + 0x10))});
    const Value entry = call("res_find", {I(MF_LUMP), text});
    call("sound_playOnObject", {entry, I(obj), I(ONE)});
  }
  call("obj_delete", {I(obj)});
  w32(o + 0x60, 1);
}

// =======================================================================================
// FireStormFlare: the falling fire balls of Fire Storm (skill 7); OWN generator only
// =======================================================================================
namespace {
constexpr Raw FSF_LUMP = 0x54F110, FSF_SOUND = 0x54F138;
}

// One of the three trail streams of a falling flare: while i < scale x per_scale, p =
// unit-ball x (scale x 8.7999935) + position, moved back along the velocity by r (one draw /
// 65536); velocity = a second ball point, scaled by `speed` (z once more by z_extra), or raw.
void Model::fsf_trail(Raw obj, const Vec3 &nv, Raw per_scale, unsigned site, std::optional<std::pair<Raw, Raw>> speed) {
  const Addr o = obj;
  const auto id = static_cast<SiteId>(site);
  std::int64_t i = 0;
  while (lt(fi(i), mul(u32(o + 0x4C), per_scale))) {
    if (static_cast<std::size_t>(i) > MAX_LOOP) throw Refusal("flare trail loop");
    const Vec3 ball = obj_ball(o);
    const Raw k = mul(u32(o + 0x4C), 0x410CCCC6);
    const Vec3 pos = vec(o + 0x10);
    Vec3 p{add(mul(ball[0], k), pos[0]), add(mul(ball[1], k), pos[1]), add(mul(ball[2], k), pos[2])};
    const Raw share = mul(fi(obj_rand(o)), F_2M16);
    p = {add(p[0], mul(nv[0], share)), add(p[1], mul(nv[1], share)), add(p[2], mul(nv[2], share))};
    Vec3 v = obj_ball(o);
    if (speed) v = {mul(v[0], speed->first), mul(v[1], speed->first), mul(mul(v[2], speed->first), speed->second)};
    call("particle_spawn", {I(K(id, A0)), V(p), V(v), I(K(id, F12))});
    ++i;
  }
}

// FireStormFlare_run 0x2d33c0, GameObject slot 4.  Falling (landed +0x50 = 0 and frame <
// expiry +0x68): vz += -0.39999998, position += velocity +0x5c..; three particle streams with
// the reversed velocity (scale +0x4c): x30 (speed 10.419806, z x 10.261909), x18 (5.0619698, z x
// 7.171931), x4 (raw ball); then terrain_height(world, position; radius +0x1c): less than 16
// above the ground lands it (+0x50 = 1) with a sound.  Landed, while the ring +0x58 (s16) is
// below radius x 3 x scale: ring += 7 and 85 pairs of particles (the same point and velocity)
// at angle = one draw, ring x (cos16, sin16) + 6.76 x a ball point + position, velocity a second
// ball point.  Then: one draw % 100 < 33 lowers z by 6 and lays a scorch decal (size = scale,
// angle = s16 of one more draw); slot 1 (3) destroys it.
void Model::firestormflare_run(Raw obj) {
  const Addr o = obj;
  if (!u32(o + 0x50) && s32(G_SIM_FRAME) < s32(o + 0x68)) {
    w32(o + 0x64, add(u32(o + 0x64), 0xBECCCCCC));
    for (int j = 0; j < 3; ++j) w32(o + 0x10 + 4 * j, add(u32(o + 0x10 + 4 * j), u32(o + 0x5C + 4 * j)));
    const Vec3 nv{neg(u32(o + 0x5C)), neg(u32(o + 0x60)), neg(u32(o + 0x64))};
    fsf_trail(obj, nv, 0x41F00000, FlareTrail0, std::pair<Raw, Raw>{0x4126B786, 0x412430C7});
    fsf_trail(obj, nv, 0x41900000, FlareTrail1, std::pair<Raw, Raw>{0x40A1FBA8, 0x40E58075});
    fsf_trail(obj, nv, 0x40800000, FlareTrail2, std::nullopt);
    const Raw ground = as_raw(call("terrain_height", {I(u32(G_WORLD)), V(vec(o + 0x10)), I(u32(o + 0x1C))}), "ground");
    if (lt(sub(u32(o + 0x18), ground), 0x41800000)) {
      w32(o + 0x50, 1);
      const Value entry = call("res_find", {I(FSF_LUMP), I(FSF_SOUND)});
      call("sound_playOnObject", {entry, I(obj), I(ONE)});
    }
    return;
  }
  if (lt(fi(s16(o + 0x58)), mul(mul(u32(o + 0x1C), 0x40400000), u32(o + 0x4C)))) {
    w16(o + 0x58, std::uint64_t{u16(o + 0x58)} + 7);
    for (int k = 0; k < 0x55; ++k) {
      const auto d = obj_rand(o);
      const Raw cx = mul(cos16(d), fi(s16(o + 0x58)));
      const Raw sy = mul(sin16(d), fi(s16(o + 0x58)));
      const Vec3 ring{cx, sy, ZERO};
      const Vec3 ball = obj_ball(o);
      const Vec3 pos = vec(o + 0x10);
      Vec3 p;
      for (int j = 0; j < 3; ++j) p[j] = add(add(ring[j], mul(ball[j], 0x40D851EB)), pos[j]);
      const Vec3 v = obj_ball(o);
      call("particle_spawn", {I(K(FlareRing0, A0)), V(p), V(v), I(K(FlareRing0, F12))});
      call("particle_spawn", {I(K(FlareRing1, A0)), V(p), V(v), I(K(FlareRing1, F12))});
    }
    return;
  }
  if (obj_rand(o) % 100 < 0x21) {
    w32(o + 0x18, sub(u32(o + 0x18), 0x40C00000));
    const auto d = obj_rand(o);
    call("decal_add", {V(vec(o + 0x10)), I(u(s16v(d))), I(K(FlareDecal, A2)), I(K(FlareDecal, A3)), I(u32(o + 0x4C))});
  }
  if (obj) vcall(o, 1, {I(3)});
}

// =======================================================================================
// Pustule: the falling blobs of Pus Storm (skills 80 / 100); OWN generator throughout
// =======================================================================================
namespace {
constexpr Raw PUS_LUMP = 0x552328, PUS_SOUND = 0x552388;
}

// Pustule_run 0x30bde0, GameObject slot 4.  Timer +0x64, burst flag +0x6c, velocity +0x58,
// floor +0x54, drops: positions +0x70 and velocities +0xe8 (10 x 12).  It falls with a
// dribble of three still particles a tick, bursts on its floor into ten drops that arc out
// under gravity, each leaving one particle a tick; destroyed 300 ticks after launch or 36
// ticks after the burst.
void Model::pustule_run(Raw obj) {
  const Addr o = obj;
  const std::int64_t timer = s32(o + 0x64);
  if (u32(o + 0x6C) && lt(0x420ECCFB, fi(timer))) {
    if (obj) vcall(o, 1, {I(3)});
    return;
  }
  if (!(timer < 0x12D)) {
    if (obj) vcall(o, 1, {I(3)});
    return;
  }
  if (!u32(o + 0x6C)) {
    const Vec3 old = vec(o + 0x10);
    w32(o + 0x60, add(u32(o + 0x60), 0xBE4CCAB3));
    Vec3 d;
    for (int j = 0; j < 3; ++j) {
      const Raw now = add(u32(o + 0x10 + 4 * j), u32(o + 0x58 + 4 * j));
      w32(o + 0x10 + 4 * j, now);
      d[j] = sub(old[j], now);
    }
    for (int k = 0; k < 3; ++k) {
      const Raw r = mul(fi(obj_rand(o)), F_2M16);
      const Vec3 pos = vec(o + 0x10);
      call("particle_spawn", {I(K(PustuleFall, A0)), V({add(pos[0], mul(d[0], r)), add(pos[1], mul(d[1], r)),
                                                       add(pos[2], mul(d[2], r))}),
                              V({ZERO, ZERO, ZERO}), I(K(PustuleFall, F12))});
    }
    if (le(u32(o + 0x18), u32(o + 0x54))) {
      w32(o + 0x64, 0);
      w32(o + 0x6C, 1);
      const Value entry = call("res_find", {I(PUS_LUMP), I(PUS_SOUND)});
      call("sound_playAtPos", {entry, V(vec(o + 0x10)), I(ONE)});
      Vec3 base = vec(o + 0x10);
      const Raw ground = as_raw(call("terrain_height", {I(u32(G_WORLD)), V(vec(o + 0x10)), I(0x41000000)}), "ground");
      base[2] = add(ground, 0x41400000);
      for (int i = 0; i < 10; ++i) {
        const Raw r1 = mul(fi(obj_rand(o)), F_2M16);
        const auto a = obj_rand(o);
        const Raw x = add(base[0], mul(mul(r1, cos16(a)), 0x40C3693C));
        const Raw y = add(base[1], mul(mul(r1, sin16(a)), 0x40C3693C));
        const auto a2 = obj_rand(o);
        const Raw r2 = mul(mul(fi(obj_rand(o)), F_2M16), 0x3F49FB1E);
        const Raw vx = mul(mul(cos16(a2), r2), 0x408DAF39);
        const Raw vy = mul(mul(sin16(a2), r2), 0x408DAF39);
        wvec(o + 0x70 + 12 * i, {x, y, base[2]});
        wvec(o + 0xE8 + 12 * i, {vx, vy, mul(ONE, 0x408DAF39)});
      }
    }
    if (!u32(o + 0x6C)) {
      w32(o + 0x64, std::uint64_t{u32(o + 0x64)} + 1);
      return;
    }
  }
  for (int i = 0; i < 10; ++i) {
    const Addr p_at = o + 0x70 + 12 * i, v_at = o + 0xE8 + 12 * i;
    for (int j = 0; j < 3; ++j) w32(p_at + 4 * j, add(u32(p_at + 4 * j), u32(v_at + 4 * j)));
    w32(v_at + 8, add(u32(v_at + 8), 0xBEE1C9F7));
    const Raw k = mul(mul(fi(obj_rand(o)), F_2M16), 0x40A729F7);
    const Vec3 v{mul(u32(v_at), k), mul(u32(v_at + 4), k), mul(u32(v_at + 8), k)};
    const Vec3 ball = obj_ball(o);
    Vec3 p;
    for (int j = 0; j < 3; ++j) p[j] = add(mul(ball[j], 0x403FEE34), u32(p_at + 4 * j));
    call("particle_spawn", {I(K(PustuleDrop, A0)), V(p), V(v), I(K(PustuleDrop, F12))});
  }
  w32(o + 0x64, std::uint64_t{u32(o + 0x64)} + 1);
}

// =======================================================================================
// DiseaseCloud_run 0x2ce6e0, GameObject slot 4.  Until frame reaches +0x54: the radius +0x5c
// grows by 0.5 while below the integer +0x58; three streams of +0x58 / 10 particles each at
// radius x a unit-ball point around the position, velocity a second ball point x 6, x 6 and x
// 12; on frames divisible by 30, every object in the box position -+ int(radius) other than
// the owner (+0x4c) - two players are skipped - takes damage_apply(victim, 0 (NO attacker),
// 100000, 0, 0, 0x1000040, entry (0x26, +0x60, +0x60, 0), 0; 0, 0).  At the end slot 1 (3).
// OWN generator (+0x44).
// =======================================================================================
void Model::diseasecloud_run(Raw obj) {
  const Addr o = obj;
  const std::int64_t frame = s32(G_SIM_FRAME);
  if (!(frame < s32(o + 0x54))) {
    if (obj) vcall(o, 1, {I(3)});
    return;
  }
  if (lt(u32(o + 0x5C), fi(s32(o + 0x58)))) w32(o + 0x5C, add(u32(o + 0x5C), HALF));
  const SiteId sites[3] = {Cloud0, Cloud1, Cloud2};
  const Raw speeds[3] = {0x40C00000, 0x40C00000, 0x41400000};
  for (int s = 0; s < 3; ++s) {
    std::int64_t i = 0;
    while (i < s32(o + 0x58) / 10) {
      if (static_cast<std::size_t>(i) > MAX_LOOP) throw Refusal("cloud particle loop");
      const Vec3 ball = obj_ball(o);
      const Raw radius = u32(o + 0x5C);
      const Vec3 pos = vec(o + 0x10);
      const Vec3 p{add(mul(ball[0], radius), pos[0]), add(mul(ball[1], radius), pos[1]), add(mul(ball[2], radius), pos[2])};
      const Vec3 b = obj_ball(o);
      const Vec3 v{mul(b[0], speeds[s]), mul(b[1], speeds[s]), mul(b[2], speeds[s])};
      call("particle_spawn", {I(K(sites[s], A0)), V(p), V(v), I(K(sites[s], F12))});
      ++i;
    }
  }
  if (frame % 30) return;
  const Raw h = fi(ti(u32(o + 0x5C)));
  const Vec3 pos = vec(o + 0x10);
  const Vec3 low{sub(pos[0], h), sub(pos[1], h), sub(pos[2], h)};
  const Vec3 high{add(pos[0], h), add(pos[1], h), add(pos[2], h)};
  const Value found = call("world_collectObjectsInBox", {V(low), V(high), I(0x40)});
  if (!found.truthy()) return;
  if (!found.is_sequence()) throw Refusal("object list answer");
  for (const Value &victim_v : found.items) {
    const Addr victim = static_cast<Addr>(as_int(victim_v, "object"));
    const Addr node = u32(o + 0x4C);
    if (!node) continue;
    const Addr owner = u32(node + 0xC);
    if (owner == victim) continue;
    if ((u32(owner + 0xC) & 0x40) && (u32(victim + 0xC) & 0x40)) continue;
    const std::int64_t v = u32(o + 0x60);
    call("damage_apply", {victim_v, I(0), I(100000), I(0), I(0), I(0x1000040), desc({0x26, v, v, 0}), I(0), I(ZERO),
                          I(ZERO)});
  }
}

// =======================================================================================
// SpellArena_run 0x305bbc, Spell slot 2 (spell, position): the spell an arena puts on a
// fighter while the s16 counter 0x5038c8 runs.  On the creature +0xc: object flag 2 is set
// when the counter is 61 or more, cleared below.  From counter 30 on: f = 1 - (counter - 30) x
// 0.0066666664; int(23 f) particles, each at a random sample point of the creature's model
// (index = draw % count) moved back by v x 7.5742016, velocity v = (cos16, sin16, 0)(one draw)
// x (1 - f) x 10; every fourth (i % 4 == 0) from site 0, the rest site 1.  Then both tint
// colours (+0x90.. and +0x98..) are set to (0x56, 0x35, 0x21, 0x3d).  SPELL generator.
// =======================================================================================
void Model::arena_run(Raw spell, const Vec3 &) {
  constexpr Addr G_ARENA_COUNTER = 0x5038C8;
  const Addr sp = spell;
  const std::int64_t counter = s16(G_ARENA_COUNTER);
  Addr c = u32(sp + 0xC);
  const auto flags = u32(c + 0xC);
  w32(c + 0xC, counter < 0x3D ? (flags & ~2u) : (flags | 2u));
  if (counter < 0x1E) return;
  const Raw f = sub(ONE, mul(sub(fi(counter), 0x41F00000), 0x3BDA740D));
  const std::int64_t count = s32v(as_int(call("model_getSamplePointCount", {vcall(u32(sp + 0xC), 16, {})}), "count"));
  const std::int64_t n = ti(mul(f, 0x41B80000));
  for (std::int64_t i = 0; i < n; ++i) {
    const Value model = vcall(u32(sp + 0xC), 16, {});
    const auto d = spell_rand(sp);
    c = u32(sp + 0xC);
    const std::int64_t index = count ? pymod(s32v(d), count) : 0;
    const Value result = call("model_getSamplePoint", {model, I(u(c + 0x114)), V(vec(c + 0x10)), I(u(s16(c + 0x38))),
                                                       I(u(index)), I(1), I(0), I(ONE)});
    if (!result.is_sequence() || result.items.size() != 2) throw Refusal("sample point answer: (point, second)");
    const Value &point = result.items[0];
    const Raw k = mul(sub(ONE, f), 0x41200000);
    const auto a = spell_rand(sp);
    const Vec3 v{mul(cos16(a), k), mul(sin16(a), k), mul(ZERO, k)};
    Vec3 p;
    for (int j = 0; j < 3; ++j) p[j] = sub(as_raw(item(point, j, "point"), "point"), mul(v[j], 0x40F25FDC));
    const SiteId site = (i & 3) == 0 ? Arena0 : Arena1;
    call("particle_spawn", {I(K(site, A0)), V(p), V(v), I(K(site, F12))});
  }
  c = u32(sp + 0xC);
  const std::uint32_t tint[4] = {0x56, 0x35, 0x21, 0x3D};
  for (unsigned k = 0; k < 8; ++k) w16(c + 0x90 + 2 * k, tint[k % 4]);
}

// =======================================================================================
// SpellHornOmenEffect: a ten-second glow on a creature made by a script command (caller
// 0x146628); its particles go into the spell's own particle GROUP in model space.
// =======================================================================================
namespace {
constexpr Raw HO_VT = 0x51AE58;
}

// SpellHornOmenEffect_ctor 0x2fc574 (spell, kind byte, creature, word): the Spell base (ONE
// SHARED DRAW seeds the spell generator +0x10), group +0x1c = particleGroup_create(), end +0x18
// = frame + 600.
void Model::hornomen_ctor(Raw spell, Raw kind, Raw owner, Raw extra) {
  const Addr sp = spell;
  w8(sp, kind);
  w32(sp + 0x14, VT_SPELL);
  w32(sp + 0xC, owner);
  w32(sp + 8, extra);
  w32(sp + 4, 0);
  const auto seed = draw();
  w32(sp + 0x14, HO_VT);
  w32(sp + 0x10, seed);
  w32(sp + 0x1C, as_int(call("particleGroup_create", {}), "group handle"));
  w32(sp + 0x18, std::uint64_t{u32(G_SIM_FRAME)} + 600);
}

// SpellHornOmenEffect_run 0x2fc684, Spell slot 2 (spell, position).  Until the end +0x18: a =
// frame - end + 600.  Below 140: tint of the creature (+0x90.. and +0x98..) = int(t x (225, 20,
// 0, 100)) with t = a x 0.0071428567, and int(150 t) particles; from 140 on the tint is (225,
// 20, 0, 100) and there are 150.  Each tick the group matrix is mat34_setPosRotZ of the
// owner's position (+0x10) and facing (+0x38, the low 16 bits) and is passed to
// particleGroup_setMatrix of the group (+0x1c).  The particles go into the group: each at a
// random sample point of the creature's
// model taken at the origin with facing 0 (index = draw % count; none when the count is 0) +
// 3.0007868 x a ball point, velocity 30.000292 x a second ball point.  At the end slot 1 (3).
void Model::hornomen_run(Raw spell, const Vec3 &) {
  const Addr sp = spell;
  const std::int64_t frame = s32(G_SIM_FRAME);
  if (!(frame < s32(sp + 0x18))) {
    if (spell) spell_destroy(sp);
    return;
  }
  const Raw a = fi(frame - s32(sp + 0x18) + 600);
  const Addr c = u32(sp + 0xC);
  const bool early = lt(a, 0x430C0000);
  std::int64_t r = 0xE1, g = 0x14, b = 100;
  if (early) {
    const Raw t = mul(a, 0x3BEA0EA0);
    r = ti(mul(t, 0x43610000));
    g = ti(mul(t, 0x41A00000));
    b = ti(mul(t, 0x42C80000));
  }
  for (Addr base : {Addr{0x90}, Addr{0x98}}) {
    w16(c + base, u(r));
    w16(c + base + 2, u(g));
    w16(c + base + 4, 0);
    w16(c + base + 6, u(b));
  }
  const Addr owner = u32(sp + 0xC);
  const core::Mat34 placed = core::mat34_set_pos_rot_z(vec(owner + 0x10), u16(owner + 0x38));
  std::vector<Value> lanes;
  lanes.reserve(placed.size());
  for (const Raw lane : placed) lanes.push_back(I(lane));
  call("particleGroup_setMatrix", {I(u32(sp + 0x1C)), Value::tuple(std::move(lanes))});
  call("particleGroup_begin", {I(u32(sp + 0x1C))});
  const std::int64_t count = s32v(as_int(call("model_getSamplePointCount", {vcall(u32(sp + 0xC), 16, {})}), "count"));
  if (count) {
    for (std::int64_t i = 0;; ++i) {
      if (early) {
        if (!lt(fi(i), mul(mul(a, 0x3BEA0EA0), 0x43160000))) break;
      } else if (i > 0x95) {
        break;
      }
      const Value model = vcall(u32(sp + 0xC), 16, {});
      const std::int64_t index = pymod(spell_rand(sp), count);
      const Value point = call("model_getSamplePoint", {model, I(u(Addr(u32(sp + 0xC)) + 0x114)), V({ZERO, ZERO, ZERO}),
                                                        I(0), I(u(index)), I(0), I(0), I(ONE)});
      const Vec3 ball = spell_ball(sp);
      Vec3 p;
      for (int j = 0; j < 3; ++j) p[j] = add(as_raw(item(point, j, "point"), "point"), mul(ball[j], 0x40400CE4));
      const Vec3 b2 = spell_ball(sp);
      const Vec3 v{mul(b2[0], 0x41F00099), mul(b2[1], 0x41F00099), mul(b2[2], 0x41F00099)};
      call("particle_spawn", {I(K(HornOmen0, A0)), V(p), V(v), I(K(HornOmen0, F12))});
    }
  }
  call("particleGroup_end", {I(u32(sp + 0x1C))});
}

// =======================================================================================
// SpellPlanarSteelEffect: four seconds of light, glows and sparks on a creature, made by a
// script command (caller 0x1466d4).  SPELL generator; the constructor draws the shared seed
// once (the Spell base).
// =======================================================================================
namespace {
constexpr Raw PS_VT = 0x51AE28;
constexpr Raw PS_LUMP = 0x551320, PS_SOUND_START = 0x551328, PS_SOUND_BURST = 0x551340;
} // namespace

// SpellPlanarSteelEffect_ctor 0x2fb728 (spell, kind byte, creature, word): the Spell base (ONE
// SHARED DRAW), a light record at +0x30 (colours, radius and the +0x2c word from the
// "light_records" constants; falloff 1 / 0) at the creature's position, linked; burst flag
// +0x20 = 0, end +0x24 = frame + 240, burst countdown +0x1c = int(height +0x20 x 3), orbit
// angles +0x18 = 0 and +0x1a = 0x7fff; the start sound.
void Model::planarsteel_ctor(Raw spell, Raw kind, Raw owner, Raw extra) {
  const auto it = k_->lights.find(std::string_view("SpellPlanarSteelEffect_ctor"));
  if (it == k_->lights.end())
    throw Refusal("SpellPlanarSteelEffect_ctor needs light_records (omitted from the runtime JSON)");
  const LightRecord &rec = it->second;
  const Addr sp = spell, ow = owner;
  w8(sp, kind);
  w32(sp + 0x14, VT_SPELL);
  w32(sp + 4, 0);
  const auto seed = draw();
  w32(sp + 0xC, owner);
  w32(sp + 8, extra);
  w32(sp + 0x10, seed);
  w32(sp + 0x14, PS_VT);
  const Addr light = sp + 0x30;
  const Raw inverse = div(ONE, ZERO);
  w8(light + 9, 0);
  wvec(sp + 0x60, vec(ow + 0x10));
  w32(light + 0x28, inverse);
  w32(light + 0x2C, rec.extra);
  for (unsigned k = 0; k < 8; ++k) w16(light + 0xA + 2 * k, rec.colour[k]);
  w32(light + 0x20, inverse);
  w32(light + 0x1C, rec.radius);
  w32(light + 0x24, rec.radius);
  w8(light + 8, 0);
  call("light_link", {I(u(light))});
  w8(sp, kind);
  w32(sp + 8, extra);
  w32(sp + 0xC, owner);
  w32(sp + 0x20, 0);
  w32(sp + 0x24, std::uint64_t{u32(G_SIM_FRAME)} + 0xF0);
  w16(sp + 0x1C, u(ti(mul(u32(ow + 0x20), 0x40400000))));
  w16(sp + 0x18, 0);
  w16(sp + 0x1A, 0x7FFF);
  const Value entry = call("res_find", {I(PS_LUMP), I(PS_SOUND_START)});
  call("sound_playAtPos", {entry, V(vec(ow + 0x10)), I(ONE)});
}

// SpellPlanarSteelEffect_run 0x2fb9b4, Spell slot 2 (spell, position).  Until the end +0x24,
// with s = sin16(frame x 1000): the light follows the creature, radius = (kind byte + 0x42) +
// 10 s, falloff its inverse square, creature tint int(49 s + 75) x 2, int(49 s + 200), int(5 s
// + 15) at +0x90 and +0x98; the countdown +0x1c falls by one, the first time it is negative 60
// burst particles 18 above the creature, velocity (100 cos a, 100 sin a, 20 r), a stepping by
// 0x444, and a sound; two orbit points (angles +0x18, +0x1a, each -0x5dc a tick) 40 from the
// creature at the countdown's height: a glow each (colour (k, k, k + 50, alpha k), k = int(40 s
// + 50), texture *0x4f8498), 6 sparks (velocity 0.4 x ball) and 4 sparks (6 x ball) at the
// point + 2 x ball; 10 falling sparks within 40 of the position, 100..300 above it, velocity
// (0, 0, -80); 10 ground sparks within 40, 5 above terrain_heightAtPoint, velocity (+-1 or 0,
// +-1 or 0, 0).  At the end slot 1 (3).
void Model::planarsteel_run(Raw spell, const Vec3 &pos) {
  const Addr sp = spell;
  const std::int64_t frame = s32(G_SIM_FRAME);
  if (!(frame < s32(sp + 0x24))) {
    if (spell) spell_destroy(sp);
    return;
  }
  const Raw s = sin16(static_cast<std::uint32_t>((frame * 1000) & static_cast<std::int64_t>(M32)));
  Addr c = u32(sp + 0xC);
  wvec(sp + 0x60, vec(c + 0x10));
  call("light_relink", {I(u(sp + 0x30))});
  const Raw radius = add(fi(s8(sp) + 0x42), mul(s, 0x41200000));
  const Raw m49 = mul(s, 0x42440000);
  const auto hi = ti(add(m49, 0x43480000));
  const auto low = ti(add(mul(s, 0x40A00000), 0x41700000));
  const auto mid = ti(add(m49, 0x42960000));
  w32(sp + 0x4C, radius);
  w32(sp + 0x54, radius);
  const Raw inverse = div(ONE, mul(radius, radius));
  c = u32(sp + 0xC);
  for (Addr base : {Addr{0x90}, Addr{0x98}}) {
    w16(c + base, u(mid));
    w16(c + base + 2, u(mid));
    w16(c + base + 4, u(hi));
    w16(c + base + 6, u(low));
  }
  w32(sp + 0x58, inverse);
  w32(sp + 0x50, inverse);
  const std::uint32_t count = (u16(sp + 0x1C) - 1u) & 0xFFFF;
  w16(sp + 0x1C, count);
  if ((count & 0x8000) && !u32(sp + 0x20)) {
    w32(sp + 0x20, 1);
    std::int64_t angle = 0;
    for (int k = 0; k < 60; ++k) {
      const Raw vx = mul(cos16(u(angle)), 0x42C80000);
      const Raw vy = mul(sin16(u(angle)), 0x42C80000);
      const Raw vz = mul(mul(fi(spell_rand(sp)), F_2M16), 0x41A00000);
      c = u32(sp + 0xC);
      const Vec3 p{add(u32(c + 0x10), ZERO), u32(c + 0x14), add(u32(c + 0x18), 0x41900000)};
      call("particle_spawn", {I(K(PlanarBurst, A0)), V(p), V({vx, vy, vz}), I(K(PlanarBurst, F12))});
      angle = s16v(static_cast<std::uint64_t>(angle + 0x444));
    }
    const Value entry = call("res_find", {I(PS_LUMP), I(PS_SOUND_BURST)});
    call("sound_playAtPos", {entry, V(vec(Addr(u32(sp + 0xC)) + 0x10)), I(ONE)});
  }
  for (int i = 0; i < 2; ++i) {
    const std::uint32_t a = (u16(sp + 0x18 + 2 * i) - 0x5DCu) & 0xFFFF;
    w16(sp + 0x18 + 2 * i, a);
    const Raw x = mul(cos16(u(s16v(a))), 0x42200000);
    const Raw y = mul(sin16(u(s16(sp + 0x18 + 2 * i))), 0x42200000);
    const Raw k40 = mul(s, 0x42200000);
    c = u32(sp + 0xC);
    const Raw z = fi(s16(sp + 0x1C));
    const auto k = ti(add(k40, 0x42480000));
    const auto blue = ti(add(k40, 0x42C80000));
    const Vec3 point{add(x, u32(c + 0x10)), add(y, u32(c + 0x14)), add(z, u32(c + 0x18))};
    call("glow_add", {V(point), I(u(k)), I(u(k)), I(u(blue)), I(u(k)), I(u32(G_GLOW_TEX_C)), I(K(PlanarGlow, T2)),
                      I(K(PlanarGlow, T3)), I(K(PlanarGlow, F12)), I(K(PlanarGlow, F13))});
    const struct {
      SiteId site;
      int count;
      Raw speed;
    } sparks[2] = {{PlanarSpark, 6, 0x3ECCCCCC}, {PlanarSpark2, 4, 0x40C00000}};
    for (const auto &sk : sparks)
      for (int n = 0; n < sk.count; ++n) {
        const Vec3 b = spell_ball(sp);
        const Vec3 v{mul(b[0], sk.speed), mul(b[1], sk.speed), mul(b[2], sk.speed)};
        const Vec3 b2 = spell_ball(sp);
        Vec3 p;
        for (int j = 0; j < 3; ++j) p[j] = add(add(b2[j], b2[j]), point[j]);
        call("particle_spawn", {I(K(sk.site, A0)), V(p), V(v), I(K(sk.site, F12))});
      }
  }
  for (int n = 0; n < 10; ++n) {
    const auto a = s16v(spell_rand(sp));
    const Raw r = mul(mul(fi(spell_rand(sp)), F_2M16), 0x42200000);
    const Raw x = mul(cos16(u(a)), r);
    const Raw y = mul(sin16(u(a)), r);
    const Raw z = add(add(0x43480000, fi(spell_rand(sp) % 200)), 0xC2C80000);
    const Vec3 p{add(x, pos[0]), add(y, pos[1]), add(z, pos[2])};
    call("particle_spawn", {I(K(PlanarFall, A0)), V(p), V({ZERO, ZERO, 0xC2A00000}), I(K(PlanarFall, F12))});
  }
  for (int n = 0; n < 10; ++n) {
    const auto a = s16v(spell_rand(sp));
    const Raw r = mul(mul(fi(spell_rand(sp)), F_2M16), 0x42200000);
    const Raw x = mul(cos16(u(a)), r);
    const Raw y = mul(sin16(u(a)), r);
    const Raw vx = add(fi(spell_rand(sp) & 1), MINUS_ONE);
    const Raw vy = add(fi(spell_rand(sp) & 1), MINUS_ONE);
    Vec3 p{add(x, pos[0]), add(y, pos[1]), add(ZERO, pos[2])};
    const Raw ground = as_raw(call("terrain_heightAtPoint", {I(u32(G_WORLD)), V(p)}), "ground");
    p[2] = add(ground, 0x40A00000);
    call("particle_spawn", {I(K(PlanarGround, A0)), V(p), V({vx, vy, ZERO}), I(K(PlanarGround, F12))});
  }
}

// =======================================================================================
// StaticFire: a fire burning on the ground (ctor 0x2c5240; level fires ctor2 0x2c54f8).  It
// blocks path nodes, has its own light (+0x80), keeps a ring of rising flame points at +0x4a4
// (10-byte records: x, y, z in 1/16 units, drift x, y), spawns flame particles and embers,
// damages what stands in it every 12th frame - all of that only while isNearView(941) - and
// draws a glow per flame point.  OWN generator (+0x44) for everything visual; ONE SHARED DRAW
// per player it burns (the damage roll).
// =======================================================================================
namespace {
constexpr Raw SF_VT = 0x50DA38, SF_TYPEINFO = 0x3C7318;
constexpr Addr LEVEL_NAME = 0x41B721;                      // the current level's name
constexpr Addr SF_LEVEL_CUTSCENE = 0x54E778, SF_LEVEL_DARK = 0x54E788;
constexpr Raw SF_PROPS[6] = {0x54E728, 0x54E730, 0x54E740, 0x54E750, 0x54E758, 0x54E760};
constexpr Addr SF_SKILL = 0x41B1A4; // g_skillTable[13]: Fire Weapons, whose tier-2 statistic sets player burns
} // namespace

// The light record +0x80 both constructors fill: radius, falloff and the +0x2c word and the
// colour shorts from "light_records"; position = pos + (0, 0, 36); linked.
void Model::sf_light(Raw obj, const Vec3 &pos, std::string_view which) {
  const auto it = k_->lights.find(which);
  if (it == k_->lights.end() || !it->second.falloff)
    throw Refusal(std::string(which) + " needs light_records (omitted from the runtime JSON)");
  const LightRecord &rec = it->second;
  const Addr o = obj, light = o + 0x80;
  w8(light + 9, 0);
  wvec(o + 0xB0, {add(pos[0], ZERO), pos[1], add(pos[2], 0x42100000)});
  w32(light + 0x24, rec.radius);
  w32(light + 0x28, *rec.falloff);
  w32(light + 0x2C, rec.extra);
  w32(light + 0x1C, rec.radius);
  w32(light + 0x20, *rec.falloff);
  for (unsigned k = 0; k < 8; ++k) w16(light + 0xA + 2 * k, rec.colour[k]);
  w8(light + 8, 0);
  call("light_link", {I(u(light))});
}

// 0x2c6aa8 (object, angle): the drift +0x72, +0x74 = int(3.733333 x the unit vector of the
// angle) (s16), then one draw picks every fourth flame point of the ring (index % 4 == draw %
// 4), whose drift takes the new drift added.
void Model::staticfire_wind(Raw obj, Raw angle) {
  const Addr o = obj;
  const Raw c = cos16(angle), s = sin16(angle);
  const Raw r = rsqrt(ONE, add(mul(c, c), mul(s, s)));
  w16(o + 0x74, u(ti(mul(mul(s, r), 0x406EEEEE))));
  w16(o + 0x72, u(ti(mul(mul(c, r), 0x406EEEEE))));
  const std::int64_t pick = obj_rand(o) & 3;
  std::int64_t i = s16(o + 0x7A);
  std::size_t guard = 0;
  while (i != s16(o + 0x78)) {
    if (++guard > MAX_LOOP) throw Refusal("flame ring loop");
    const Addr rec = Addr(u32(o + 0x4A4)) + 10 * i;
    if ((i & 3) == pick) {
      w16(rec + 6, std::uint64_t{u16(rec + 6)} + u16(o + 0x72));
      w16(rec + 8, std::uint64_t{u16(rec + 8)} + u16(o + 0x74));
    }
    ++i;
    if (i == s16(o + 0x76)) i = 0;
  }
}

// 0x2c6bc0 (object): a new flame point at the ring's head when the ring is not full: a
// unit-ball point x radius +0x1c x fade +0x64, z made positive, in 1/16 units, drift ((draw /
// 65536 - 0.5) / 60 x 224) twice.
void Model::staticfire_add_flame(Raw obj) {
  const Addr o = obj;
  const Vec3 ball = obj_ball(o);
  const Raw h = mul(u32(o + 0x1C), u32(o + 0x64));
  Vec3 p{mul(ball[0], h), mul(ball[1], h), mul(ball[2], h)};
  if (lt(p[2], ZERO)) p[2] = neg(p[2]);
  const std::int64_t head = s16(o + 0x78);
  std::int64_t next = head + 1;
  if (next == s16(o + 0x76)) next = 0;
  if (next == s16(o + 0x7A)) return;
  const Addr rec = Addr(u32(o + 0x4A4)) + 10 * head;
  for (int k = 0; k < 3; ++k) w16(rec + 2 * k, u(ti(mul(p[k], 0x41800000))));
  for (int k = 3; k < 5; ++k) {
    const Raw d = mul(mul(sub(mul(fi(obj_rand(o)), F_2M16), HALF), 0x3C888888), 0x43600000);
    w16(rec + 2 * k, u(ti(d)));
  }
  w16(o + 0x78, u(next));
}

// 0x2c6df0 (object): every flame point drifts and rises 7 (1/16 units); one that reaches
// int(+0x58) (compared as 16-bit values) pushes the ring's tail +0x7a on.
void Model::staticfire_move_flames(Raw obj) {
  const Addr o = obj;
  std::int64_t i = s16(o + 0x7A);
  std::size_t guard = 0;
  while (i != s16(o + 0x78)) {
    if (++guard > MAX_LOOP) throw Refusal("flame ring loop");
    const Addr rec = Addr(u32(o + 0x4A4)) + 10 * i;
    const auto top = ti(u32(o + 0x58));
    const std::uint32_t z = (u16(rec + 4) + 7u) & 0xFFFF;
    w16(rec, std::uint64_t{u16(rec)} + u16(rec + 6));
    w16(rec + 2, std::uint64_t{u16(rec + 2)} + u16(rec + 8));
    w16(rec + 4, z);
    if (!(s16v(z) < s16v(static_cast<std::uint32_t>(top) & 0xFFFF))) {
      const std::uint32_t tail = (u16(o + 0x7A) + 1u) & 0xFFFF;
      w16(o + 0x7A, tail);
      if (s16v(tail) == s16(o + 0x76)) w16(o + 0x7A, 0);
    }
    ++i;
    if (i == s16(o + 0x76)) i = 0;
  }
}

// StaticFire_initCommon 0x2c5814: radius 16, heading 0, byte +0x28 = 60, ring empty; one own
// draw sets the wind clock +0x4c = draw % 300; +0x68 = 25000, flags |= 0x40001, life +0x7c =
// -1, +0x54 = +0x64 = 1, +0x6c = 0; the wind (angle 0), the run and always-draw lists,
// path_blockNodes(world, position; radius).  Ring: 250 points in a 0x9c4-byte block
// (mem_allocTagged) and height 600 when byte +0x6f is set, else 100 points at +0xbc and height
// 480 (240 when byte +0x70 is set).
void Model::staticfire_init_common(Raw obj) {
  const Addr o = obj;
  w32(o + 0x1C, 0x41800000);
  w16(o + 0x38, 0);
  w8(o + 0x28, 0x3C);
  w16(o + 0x78, 0);
  w16(o + 0x7A, 0);
  const auto d = obj_rand(o);
  w32(o + 0x68, 0x46C35000);
  w32(o + 0xC, u32(o + 0xC) | 0x40001u);
  w32(o + 0x7C, M32);
  w32(o + 0x54, ONE);
  w32(o + 0x64, ONE);
  w8(o + 0x6C, 0);
  w32(o + 0x4C, fi(d % 300));
  staticfire_wind(obj, u(s16(o + 0x38)));
  call("obj_runEveryFrame", {I(obj)});
  call("obj_addToAlwaysDraw", {I(obj)});
  call("path_blockNodes", {I(u32(G_WORLD)), V(vec(o + 0x10)), I(u32(o + 0x1C))});
  if (s8(o + 0x6F)) {
    w16(o + 0x76, 0xFA);
    w32(o + 0x4A4, as_int(call("mem_allocTagged", {I(0x9C4), I(0x54E768)}), "flame block"));
    w32(o + 0x50, 0x44160000);
    return;
  }
  w32(o + 0x50, 0x43F00000);
  w16(o + 0x76, 100);
  w32(o + 0x4A4, u(o + 0xBC));
  if (s8(o + 0x70)) w32(o + 0x50, 0x43700000);
}

// StaticFire_ctor 0x2c5240 (object, position, life, owner, quiet; height f12, scale f13).
// GameObject_ctor (a shared draw inside); the table; the light 36 above the given position;
// initCommon; life +0x7c, height +0x50 = f12, +0x68 x= f13, owner +0x4a8, burning +0x6e = 1;
// quiet: the light is unlinked again.  Then every other object in the +-30 box: when not quiet
// and it is a StaticFire (typeid), its light is unlinked; with more than three of them around
// the life is halved.
void Model::staticfire_ctor(Raw obj, const Vec3 &pos, Raw life, Raw owner, Raw quiet, Raw fuel, Raw scale) {
  const Addr o = obj;
  call("GameObject_ctor", {I(obj), V(pos), I(0), I(0)});
  w32(o + 0x48, SF_VT);
  sf_light(obj, pos, "StaticFire_ctor");
  w8(o + 0x6F, 0);
  w8(o + 0x70, 0);
  staticfire_init_common(obj);
  w32(o + 0x7C, life);
  w32(o + 0x50, fuel);
  w32(o + 0x68, mul(u32(o + 0x68), scale));
  w32(o + 0x4A8, owner);
  w8(o + 0x6E, 1);
  if (quiet) call("light_unlink", {I(u(o + 0x80))});
  const Vec3 p = vec(o + 0x10);
  const Vec3 high{add(p[0], 0x41F00000), add(p[1], 0x41F00000), add(p[2], 0x41F00000)};
  const Vec3 low{sub(p[0], 0x41F00000), sub(p[1], 0x41F00000), sub(p[2], 0x41F00000)};
  const Value found = call("world_collectObjectsInBox", {V(low), V(high), I(0x80)});
  std::int64_t count = 0;
  if (found.truthy()) {
    if (!found.is_sequence()) throw Refusal("object list answer");
    for (const Value &other_v : found.items) {
      const auto other = as_int(other_v, "object");
      if (other == obj) continue;
      if (!quiet) {
        const Value kind = call("Creature.v0", {});
        const Value mine = call("typeinfo", {I(SF_TYPEINFO)});
        if (call("rtti_typeInfoEquals", {kind, mine}).truthy()) call("light_unlink", {I(other + 0x80)});
      }
      ++count;
    }
  }
  if (count > 3) w32(o + 0x7C, u(s32(o + 0x7C) / 2));
}

// StaticFire_ctor2 0x2c54f8 (object, position): a level fire.  GameObject_ctor (object,
// position, 0, the caller's a3); the light 36 above the object; owner 0; properties: +0x6f and
// +0x70 (read before initCommon), then +0x6c, radius +0x1c, particle rate +0x54, burning +0x6e
// (default 1).
void Model::staticfire_ctor2(Raw obj, const Vec3 &pos, Raw, Raw a3) {
  const Addr o = obj;
  call("GameObject_ctor", {I(obj), V(pos), I(0), I(a3)});
  w32(o + 0x48, SF_VT);
  sf_light(obj, vec(o + 0x10), "StaticFire_ctor2");
  w32(o + 0x4A8, 0);
  auto prop_str = [&](Raw prop) { return call("obj_getPropStr", {I(obj), I(prop), I(0)}).truthy(); };
  auto prop_int = [&](Raw prop) { return as_int(call("obj_getPropInt", {I(obj), I(prop), I(0)}), "property"); };
  for (auto [off, prop] : {std::pair<Addr, Raw>{0x6F, SF_PROPS[0]}, std::pair<Addr, Raw>{0x70, SF_PROPS[1]}}) {
    if (prop_str(prop)) w8(o + off, prop_int(prop));
    else w8(o + off, 0);
  }
  staticfire_init_common(obj);
  if (prop_str(SF_PROPS[2])) w8(o + 0x6C, prop_int(SF_PROPS[2]));
  for (auto [off, prop] : {std::pair<Addr, Raw>{0x1C, SF_PROPS[3]}, std::pair<Addr, Raw>{0x54, SF_PROPS[4]}})
    if (prop_str(prop)) w32(o + off, fi(static_cast<std::int64_t>(prop_int(prop))));
  if (prop_str(SF_PROPS[5])) w8(o + 0x6E, prop_int(SF_PROPS[5]));
  else w8(o + 0x6E, 1);
}

Vec3 Model::sf_offset(Raw obj, const Vec3 &ball) const {
  const Raw r = u32(Addr(obj) + 0x1C), fade = u32(Addr(obj) + 0x64);
  Vec3 out;
  for (int k = 0; k < 3; ++k) out[k] = mul(mul(mul(ball[k], r), fade), 0x3F3168B5);
  return out;
}

// StaticFire_run 0x2c595c, GameObject slot 4: cutscene flag +0x6d (only in the one level
// 0x54e778, while a cutscene runs); life +0x7c (-1 = forever): counts down, fades in by 1/60 a
// tick and out by 1/120 over its last 120 ticks, unlinks the light at 0; the wind every 61
// ticks unless +0x6c; a new flame point on one draw in 14 (top +0x58 = height x 0.45 x 16); the
// flames move.  Burning (+0x6e) and alive and isNearView(941): the burn (sf_burn).  A fire whose
// life is 0 and whose ring is empty deletes itself.
void Model::staticfire_run(Raw obj) {
  const Addr o = obj;
  const bool cutscene = cstr(LEVEL_NAME) == cstr(SF_LEVEL_CUTSCENE) && s8(G_CUTSCENE_ACTIVE);
  w8(o + 0x6D, cutscene ? 1 : 0);
  std::int64_t life = s32(o + 0x7C);
  if (life != -1) {
    bool unlink = life == 0;
    if (life) {
      life -= 1;
      w32(o + 0x7C, static_cast<std::uint64_t>(life));
      unlink = life == 0;
      if (life) {
        Raw fade = u32(o + 0x64);
        if (life < 0x78) {
          fade = sub(fade, 0x3C088888);
          w32(o + 0x64, fade);
          if (lt(fade, ZERO)) w32(o + 0x64, ZERO);
        } else {
          fade = add(fade, 0x3C888888);
          w32(o + 0x64, fade);
          if (lt(ONE, fade) || s8(o + 0x6D)) w32(o + 0x64, ONE);
        }
      }
    }
    if (unlink) call("light_unlink", {I(u(o + 0x80))});
  }
  if (!s8(o + 0x6C)) {
    const Raw clock = u32(o + 0x4C);
    w32(o + 0x4C, add(clock, ONE));
    if (lt(0x42700000, clock)) {
      staticfire_wind(obj, obj_rand(o));
      w32(o + 0x4C, ZERO);
    }
  }
  if (s32(o + 0x7C) && obj_rand(o) % 14 == 0) {
    const Raw top = mul(mul(u32(o + 0x50), 0x3EE66665), 0x41800000);
    w32(o + 0x58, top);
    w32(o + 0x5C, div(ONE, top));
    staticfire_add_flame(obj);
  }
  staticfire_move_flames(obj);
  if (s8(o + 0x6E) && s32(o + 0x7C) && call("isNearView", {V(vec(o + 0x10)), I(0x446B4000)}).truthy()) sf_burn(obj);
  if (s32(o + 0x7C) == 0 && s16(o + 0x7A) == s16(o + 0x78)) call("obj_delete", {I(obj)});
}

// The burning part of StaticFire_run: the light flickers (+0x60 between 80 and 108), radius
// (+0x60 x 0.34 + 54) x fade; int(4 x rate +0x54 x fade) flame particles; on one draw in 33,
// int(2 fade) embers; on frames divisible by 12 damage_apply to everything in the +-radius x
// fade box that is not itself and has no flag 1 (fire entry 0x15: (1, 1) for others, for
// players the Fire Weapons tier-2 statistic x 0.15 (at least 1) .. + 20 % of it times ONE
// SHARED DRAW); attacker the owner +0x4a8 when it is a creature whose slot 20 accepts the
// victim, else the fire itself for creatures.
void Model::sf_burn(Raw obj) {
  const Addr o = obj;
  const Raw flicker = u32(o + 0x60);
  if (eq(flicker, 0x42A00000)) {
    if (obj_rand(o) % 10 == 0) w32(o + 0x60, add(u32(o + 0x60), 0x41E00000));
  } else {
    const Raw v = sub(flicker, 0x40000000);
    w32(o + 0x60, v);
    if (lt(v, 0x42A00000)) w32(o + 0x60, 0x42A00000);
  }
  const Raw radius = mul(add(mul(u32(o + 0x60), 0x3EAE147A), 0x42580000), u32(o + 0x64));
  w32(o + 0x9C, radius);
  w32(o + 0xA4, radius);
  const Raw inverse = div(ONE, mul(radius, radius));
  w32(o + 0xA8, inverse);
  w32(o + 0xA0, inverse);
  std::int64_t n = ti(mul(mul(u32(o + 0x54), 0x40800000), u32(o + 0x64)));
  if (n > static_cast<std::int64_t>(MAX_LOOP)) throw Refusal("flame particle count");
  while (n > 0) {
    const Vec3 off = sf_offset(obj, obj_ball(o));
    const Vec3 pos = vec(o + 0x10);
    const Vec3 p{add(pos[0], off[0]), add(pos[1], off[1]), add(pos[2], off[2])};
    const Vec3 b = obj_ball(o);
    Vec3 v{mul(b[0], 0x40A00000), mul(b[1], 0x40A00000), mul(b[2], 0x40A00000)};
    if (lt(v[2], ZERO)) v[2] = neg(v[2]);
    --n;
    call("particle_spawn", {I(K(FireFlame, A0)), V(p), V(v), I(K(FireFlame, F12))});
  }
  if (obj_rand(o) % 33 == 0) {
    std::int64_t i = 0;
    while (i < ti(add(u32(o + 0x64), u32(o + 0x64)))) {
      if (static_cast<std::size_t>(i) > MAX_LOOP) throw Refusal("ember loop");
      const Vec3 off = sf_offset(obj, obj_ball(o));
      const Vec3 pos = vec(o + 0x10);
      const Vec3 base{add(pos[0], off[0]), add(pos[1], off[1]), add(pos[2], off[2])};
      const Vec3 p{add(base[0], ZERO), base[1], add(base[2], 0x41400000)};
      Vec3 b = obj_ball(o);
      if (lt(b[2], 0x3F4CCCCC)) b[2] = 0x3F4CCCCC;
      ++i;
      const Raw x = mul(b[0], 0x3E800000), y = mul(b[1], 0x3E800000), z = b[2];
      const Raw r = rsqrt(ONE, add(add(mul(x, x), mul(y, y)), mul(z, z)));
      const Vec3 n3{mul(x, r), mul(y, r), mul(z, r)};
      const Raw k = mul(add(mul(mul(fi(obj_rand(o)), F_2M16), HALF), HALF), 0x41800000);
      call("particle_spawn", {I(K(FireEmber, A0)), V(p), V({mul(n3[0], k), mul(n3[1], k), mul(n3[2], k)}),
                              I(K(FireEmber, F12))});
    }
  }
  const std::int64_t frame = s32(G_SIM_FRAME);
  if (frame % 12) return;
  const Raw h = mul(u32(o + 0x1C), u32(o + 0x64));
  const Vec3 pos = vec(o + 0x10);
  const Value found = call("world_collectObjectsInBox", {V({sub(pos[0], h), sub(pos[1], h), sub(pos[2], h)}),
                                                         V({add(pos[0], h), add(pos[1], h), add(pos[2], h)}), I(0x40)});
  if (!found.truthy()) return;
  if (!found.is_sequence()) throw Refusal("object list answer");
  for (const Value &victim_v : found.items) {
    const auto victim = as_int(victim_v, "object");
    if (victim == obj || (u32(Addr(victim) + 0xC) & 1)) continue;
    std::array<std::int64_t, 4> entry{0x15, 1, 1, 0};
    if (u32(Addr(victim) + 0xC) & 0x40) {
      const Addr skill = u32(SF_SKILL);
      const std::int64_t level = ti(u32(G_LEVEL_MONSTER_LEVEL));
      Raw low = mul(fi(s32v(as_int(skill_call(skill, 11, {0, level, 0}), "burn statistic"))), 0x3E199999);
      if (lt(low, ONE)) low = ONE;
      const Raw share = mul(fi(draw()), F_2M16);
      const Raw high = add(low, mul(mul(low, 0x3E4CCCCC), share));
      entry = {0x15, ti(low), ti(high), 0};
    }
    const Addr owner = u32(o + 0x4A8);
    if (owner && (u32(owner + 0xC) & 0x20)) {
      if (vcall(owner, 20, {victim_v}).truthy())
        call("damage_apply", {victim_v, I(u32(o + 0x4A8)), I(100000), I(0), I(0), I(0x100C040), desc(entry), I(0),
                              I(ZERO), I(ZERO)});
    } else if (u32(Addr(victim) + 0xC) & 0x20) {
      call("damage_apply", {victim_v, I(obj), I(100000), I(0), I(0), I(0x100C040), desc(entry), I(0), I(ZERO), I(ZERO)});
    }
  }
}

// StaticFire_draw 0x2c65e8, GameObject slot 5: with isNearView(941), burning, alive and no
// cutscene flag, one glow over the fire (size (flicker x 59 + 12000) x fade, texture *0x4f848c);
// then one glow per flame point near the view: size from 4800 toward +0x68 with the point's
// height t = min(1, z x 1/top) (doubled for a level fire with +0x6f), colour from (56 - 46 t,
// 56 - 43 t, 56 - 42 t) toward (188, 83, 69) as the point rises (alive) or half of it (dying),
// dimmed toward (70, 102, 107) in levels whose name has 0x54e788 by the height above -480,
// alpha int(171 - 171 t), texture *0x4f84a0 or the dim one *0x4f84a4, angle s16(+-(object +
// frame) x 0xb3).
void Model::staticfire_draw(Raw obj) {
  const Addr o = obj;
  std::int64_t i = s16(o + 0x7A);
  const std::string level = cstr(LEVEL_NAME);
  const bool dark = level.find(cstr(SF_LEVEL_DARK)) != std::string::npos;
  const bool near = call("isNearView", {V(vec(o + 0x10)), I(0x446B4000)}).truthy();
  if (near && s8(o + 0x6E) && u32(o + 0x7C) && !s8(o + 0x6D)) {
    const Raw size = mul(add(mul(u32(o + 0x60), 0x426C0000), 0x463B8000), u32(o + 0x64));
    const Vec3 pos = vec(o + 0x10);
    const Vec3 p{add(pos[0], ZERO), pos[1], add(pos[2], 0x41400000)};
    call("glow_add", {V(p), I(K(FireBigGlow, A1)), I(K(FireBigGlow, A2)), I(K(FireBigGlow, A3)), I(K(FireBigGlow, T0)),
                      I(u32(G_GLOW_TEX_A)), I(K(FireBigGlow, T2)), I(K(FireBigGlow, T3)), I(K(FireBigGlow, F12)), I(size)});
  }
  std::size_t guard = 0;
  while (i != s16(o + 0x78)) {
    if (++guard > MAX_LOOP) throw Refusal("flame ring loop");
    const Addr rec = Addr(u32(o + 0x4A4)) + 10 * i;
    const Vec3 pos = vec(o + 0x10);
    Vec3 p;
    for (int k = 0; k < 3; ++k) p[k] = add(pos[k], mul(fi(s16(rec + 2 * k)), 0x3D800000));
    if (call("isNearView", {V(p), I(0x446B4000)}).truthy()) {
      Raw t = mul(fi(s16(rec + 4)), u32(o + 0x5C));
      if (lt(ONE, t)) t = ONE;
      const Raw t0 = t;
      if (s8(o + 0x6F)) {
        t = add(t0, t0);
        if (lt(ONE, t)) t = ONE;
      }
      const Raw a46 = mul(t, 0xC2380000), a43 = mul(t, 0xC22C0000), a42 = mul(t, 0xC2280000);
      Raw r, g, b;
      if (u32(o + 0x7C) && s8(o + 0x6E)) {
        const Raw zf = mul(fi(s16(rec + 4)), 0x3D800000);
        const Raw k = sub(ONE, fmin(ONE, div(zf, mul(u32(o + 0x60), u32(o + 0x64)))));
        r = add(add(a46, mul(sub(0x433C0000, add(a46, 0x42600000)), k)), 0x42600000);
        g = add(add(a43, mul(sub(0x42A60000, add(a43, 0x42600000)), k)), 0x42600000);
        b = add(add(a42, mul(sub(0x428A0000, add(a42, 0x42600000)), k)), 0x42600000);
      } else {
        r = mul(add(a46, 0x42600000), HALF);
        g = mul(add(a43, 0x42600000), HALF);
        b = mul(add(a42, 0x42600000), HALF);
      }
      bool dim = false;
      if (dark) {
        Raw f = mul(sub(p[2], 0xC3F00000), 0x3B08D180);
        f = fmax(ZERO, fmin(ONE, f));
        r = add(mul(sub(r, 0x428C0000), f), 0x428C0000);
        g = add(mul(sub(g, 0x42CC0000), f), 0x42CC0000);
        b = add(mul(sub(b, 0x42D60000), f), 0x42D60000);
        dim = lt(f, 0x3F7F3A68);
      }
      const auto alpha = ti(add(mul(t0, 0xC32B0000), 0x432B0000));
      const std::uint64_t spin = static_cast<std::uint64_t>((std::int64_t{obj} + s32(G_SIM_FRAME)) * 0xB3) & M32;
      const std::int64_t angle = (i & 1) ? s16v((0 - spin) & 0xFFFF) : s16v(spin & 0xFFFF);
      const Raw size = add(mul(sub(u32(o + 0x68), 0x45960000), t), 0x45960000);
      const Raw tex = u32(dim ? G_GLOW_TEX_FIRE_DIM : G_GLOW_TEX_FIRE);
      call("glow_add", {V(p), I(u(ti(r))), I(u(ti(g))), I(u(ti(b))), I(u(alpha)), I(tex), I(u(angle)),
                        I(K(FireFlameGlow, T3)), I(K(FireFlameGlow, F12)), I(size)});
    }
    ++i;
    if (i == s16(o + 0x76)) i = 0;
  }
}
#undef K

// ---- dispatch by name (test vectors) ---------------------------------------------------
const std::vector<std::string_view> &Model::methods() {
  static const std::vector<std::string_view> names = {
      "shock_draw", "firearrow_draw", "holystrike_draw", "badger_draw", "resurrect_draw", "smokegen_draw",
      "multifire_ctor", "multifire_run", "multifire_end", "firestormflare_run", "pustule_run",
      "diseasecloud_run", "arena_run", "hornomen_ctor", "hornomen_run", "planarsteel_ctor", "planarsteel_run",
      "staticfire_ctor", "staticfire_ctor2", "staticfire_init_common", "staticfire_wind", "staticfire_add_flame",
      "staticfire_move_flames", "staticfire_run", "staticfire_draw"};
  return names;
}

void Model::invoke(std::string_view method, const std::vector<Value> &args) {
  auto raw = [&](std::size_t k) -> Raw {
    if (k >= args.size() || !args[k].is_int()) throw Refusal("argument " + std::to_string(k) + " of " + std::string(method));
    return static_cast<Raw>(args[k].i);
  };
  auto v3 = [&](std::size_t k) -> Vec3 {
    if (k >= args.size() || !args[k].is_sequence() || args[k].items.size() != 3) throw Refusal("vector argument");
    return {static_cast<Raw>(args[k].items[0].i), static_cast<Raw>(args[k].items[1].i), static_cast<Raw>(args[k].items[2].i)};
  };
  auto arity = [&](std::size_t n) {
    if (args.size() != n) throw Refusal("arity of " + std::string(method));
  };
  if (method == "shock_draw") { arity(1); shock_draw(raw(0)); }
  else if (method == "firearrow_draw") { arity(1); firearrow_draw(raw(0)); }
  else if (method == "holystrike_draw") { arity(1); holystrike_draw(raw(0)); }
  else if (method == "badger_draw") { arity(1); badger_draw(raw(0)); }
  else if (method == "resurrect_draw") { arity(1); resurrect_draw(raw(0)); }
  else if (method == "smokegen_draw") { arity(1); smokegen_draw(raw(0)); }
  else if (method == "multifire_ctor") { arity(6); multifire_ctor(raw(0), v3(1), raw(2), raw(3), raw(4), raw(5)); }
  else if (method == "multifire_run") { arity(1); multifire_run(raw(0)); }
  else if (method == "multifire_end") { arity(2); multifire_end(raw(0), raw(1)); }
  else if (method == "firestormflare_run") { arity(1); firestormflare_run(raw(0)); }
  else if (method == "pustule_run") { arity(1); pustule_run(raw(0)); }
  else if (method == "diseasecloud_run") { arity(1); diseasecloud_run(raw(0)); }
  else if (method == "arena_run") { arity(2); arena_run(raw(0), v3(1)); }
  else if (method == "hornomen_ctor") { arity(4); hornomen_ctor(raw(0), raw(1), raw(2), raw(3)); }
  else if (method == "hornomen_run") { arity(2); hornomen_run(raw(0), v3(1)); }
  else if (method == "planarsteel_ctor") { arity(4); planarsteel_ctor(raw(0), raw(1), raw(2), raw(3)); }
  else if (method == "planarsteel_run") { arity(2); planarsteel_run(raw(0), v3(1)); }
  else if (method == "staticfire_ctor") { arity(7); staticfire_ctor(raw(0), v3(1), raw(2), raw(3), raw(4), raw(5), raw(6)); }
  else if (method == "staticfire_ctor2") { arity(4); staticfire_ctor2(raw(0), v3(1), raw(2), raw(3)); }
  else if (method == "staticfire_init_common") { arity(1); staticfire_init_common(raw(0)); }
  else if (method == "staticfire_wind") { arity(2); staticfire_wind(raw(0), raw(1)); }
  else if (method == "staticfire_add_flame") { arity(1); staticfire_add_flame(raw(0)); }
  else if (method == "staticfire_move_flames") { arity(1); staticfire_move_flames(raw(0)); }
  else if (method == "staticfire_run") { arity(1); staticfire_run(raw(0)); }
  else if (method == "staticfire_draw") { arity(1); staticfire_draw(raw(0)); }
  else throw Refusal("no such method: " + std::string(method));
}

} // namespace champions::rta::fx::skills
