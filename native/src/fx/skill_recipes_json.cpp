// The recipe constants of the skill effect classes: the JSON tools/fx_skill_recipes.py <elf>
// --json writes from the user's executable (research commit 19f80a6), loaded and validated at
// run time.  No value of a real executable is in this file; see champions/fx_skills.hpp.
#include "champions/fx_skills.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <locale>
#include <set>
#include <sstream>

namespace champions::rta::fx::skills {
namespace {

// ---------------------------------------------------------------------------------------
// A small JSON reader (objects keep their key order; numbers keep their text)
// ---------------------------------------------------------------------------------------
struct Json {
  enum class Kind : std::uint8_t { Null, Bool, Number, String, Array, Object };
  Kind kind = Kind::Null;
  bool flag = false;
  std::string text;                  // number text or string value
  bool is_float = false;             // the number has a fraction, an exponent or is NaN / Infinity
  std::vector<Json> items;           // array
  std::vector<std::pair<std::string, Json>> members; // object
  const Json *get(std::string_view key) const {
    for (const auto &m : members)
      if (m.first == key) return &m.second;
    return nullptr;
  }
};

[[noreturn]] void refuse(const std::string &why) { throw Refusal("fx_skill_recipes JSON: " + why); }

struct Reader {
  std::string_view t;
  std::size_t p = 0;
  unsigned depth = 0;
  void space() {
    while (p < t.size() && (t[p] == ' ' || t[p] == '\n' || t[p] == '\r' || t[p] == '\t')) ++p;
  }
  bool word(std::string_view w) {
    if (t.substr(p, w.size()) == w) {
      p += w.size();
      return true;
    }
    return false;
  }
  [[noreturn]] void fail(const char *what) const { refuse(std::string(what) + " at offset " + std::to_string(p)); }
  static void put_utf8(std::string &out, unsigned cp) {
    if (cp < 0x80) out.push_back(static_cast<char>(cp));
    else if (cp < 0x800) {
      out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
      out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
      out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
      out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
      out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
  }
  unsigned hex4() {
    if (p + 4 > t.size()) fail("short \\u escape");
    unsigned v = 0;
    for (int k = 0; k < 4; ++k) {
      const char c = t[p++];
      v <<= 4;
      if (c >= '0' && c <= '9') v |= static_cast<unsigned>(c - '0');
      else if (c >= 'a' && c <= 'f') v |= static_cast<unsigned>(c - 'a' + 10);
      else if (c >= 'A' && c <= 'F') v |= static_cast<unsigned>(c - 'A' + 10);
      else fail("bad \\u escape");
    }
    return v;
  }
  std::string string() {
    if (p >= t.size() || t[p] != '"') fail("string expected");
    ++p;
    std::string out;
    for (;;) {
      if (p >= t.size()) fail("unterminated string");
      const char c = t[p++];
      if (c == '"') return out;
      if (static_cast<unsigned char>(c) < 0x20) fail("control character in string");
      if (c != '\\') {
        out.push_back(c);
        continue;
      }
      if (p >= t.size()) fail("bad escape");
      const char e = t[p++];
      switch (e) {
      case '"': out.push_back('"'); break;
      case '\\': out.push_back('\\'); break;
      case '/': out.push_back('/'); break;
      case 'b': out.push_back('\b'); break;
      case 'f': out.push_back('\f'); break;
      case 'n': out.push_back('\n'); break;
      case 'r': out.push_back('\r'); break;
      case 't': out.push_back('\t'); break;
      case 'u': {
        unsigned cp = hex4();
        if (cp >= 0xD800 && cp < 0xDC00) {
          if (!word("\\u")) fail("lone surrogate");
          const unsigned lo = hex4();
          if (lo < 0xDC00 || lo >= 0xE000) fail("bad surrogate pair");
          cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
        }
        put_utf8(out, cp);
        break;
      }
      default: fail("bad escape");
      }
    }
  }
  Json value() {
    if (++depth > 64) fail("nesting too deep");
    space();
    if (p >= t.size()) fail("value expected");
    Json v;
    const char c = t[p];
    if (c == '{') {
      v.kind = Json::Kind::Object;
      ++p;
      space();
      if (p < t.size() && t[p] == '}') ++p;
      else
        for (;;) {
          space();
          std::string key = string();
          space();
          if (p >= t.size() || t[p] != ':') fail("':' expected");
          ++p;
          Json member = value();
          for (const auto &m : v.members)
            if (m.first == key) fail("duplicate key");
          v.members.emplace_back(std::move(key), std::move(member));
          space();
          if (p < t.size() && t[p] == ',') { ++p; continue; }
          if (p < t.size() && t[p] == '}') { ++p; break; }
          fail("',' or '}' expected");
        }
    } else if (c == '[') {
      v.kind = Json::Kind::Array;
      ++p;
      space();
      if (p < t.size() && t[p] == ']') ++p;
      else
        for (;;) {
          v.items.push_back(value());
          space();
          if (p < t.size() && t[p] == ',') { ++p; continue; }
          if (p < t.size() && t[p] == ']') { ++p; break; }
          fail("',' or ']' expected");
        }
    } else if (c == '"') {
      v.kind = Json::Kind::String;
      v.text = string();
    } else if (word("null")) {
      v.kind = Json::Kind::Null;
    } else if (word("true")) {
      v.kind = Json::Kind::Bool;
      v.flag = true;
    } else if (word("false")) {
      v.kind = Json::Kind::Bool;
    } else if (t.substr(p, 3) == "NaN" || t.substr(p, 8) == "Infinity" || t.substr(p, 9) == "-Infinity") {
      const std::size_t start = p;
      if (!word("NaN") && !word("Infinity")) word("-Infinity");
      v.kind = Json::Kind::Number;
      v.is_float = true;
      v.text = std::string(t.substr(start, p - start));
    } else {
      const std::size_t start = p;
      if (p < t.size() && t[p] == '-') ++p;
      while (p < t.size() && std::strchr("0123456789.eE+-", t[p])) {
        if (std::strchr(".eE", t[p])) v.is_float = true;
        ++p;
      }
      if (p == start) fail("value expected");
      v.kind = Json::Kind::Number;
      v.text = std::string(t.substr(start, p - start));
    }
    --depth;
    return v;
  }
};

std::uint32_t json_u32(const Json &v, const std::string &what) {
  if (v.kind != Json::Kind::Number || v.is_float || v.text.empty() || v.text[0] == '-')
    refuse(what + ": a non-negative integer expected");
  std::uint64_t out = 0;
  for (char c : v.text) {
    if (c < '0' || c > '9') refuse(what + ": a non-negative integer expected");
    out = out * 10 + static_cast<unsigned>(c - '0');
    if (out > 0xffffffffu) refuse(what + ": larger than 32 bits");
  }
  return static_cast<std::uint32_t>(out);
}

// A JSON float that fx_particle_defs.f32bits produced: exactly a single-precision value.
Raw json_f32(const Json &v, const std::string &what) {
  if (v.kind != Json::Kind::Number || !v.is_float) refuse(what + ": a float expected");
  if (v.text == "Infinity") return 0x7F800000;
  if (v.text == "-Infinity") return 0xFF800000;
  if (v.text == "NaN") refuse(what + ": NaN does not keep its pattern; cannot use it");
  std::istringstream in(v.text);
  in.imbue(std::locale::classic());
  double d = 0;
  in >> d;
  if (!in || in.peek() != std::char_traits<char>::eof()) refuse(what + ": bad number " + v.text);
  const float f = static_cast<float>(d);
  if (static_cast<double>(f) != d) refuse(what + ": not a single-precision value: " + v.text);
  Raw bits = 0;
  std::memcpy(&bits, &f, sizeof bits);
  return bits;
}

const std::string &json_string(const Json &v, const std::string &what) {
  if (v.kind != Json::Kind::String) refuse(what + ": a string expected");
  return v.text;
}

const Json &member(const Json &obj, std::string_view key, const std::string &what) {
  if (obj.kind != Json::Kind::Object) refuse(what + ": an object expected");
  const Json *v = obj.get(key);
  if (!v) refuse(what + ": no '" + std::string(key) + "'");
  return *v;
}

// fx_callsites.PRIMITIVES names and SIGNATURES (float registers, integer registers, stack)
const std::vector<std::string> &primitive_names() {
  static const std::vector<std::string> names = {
      "particle", "LightEffect", "glow", "trail", "trailPoint", "beam", "beamUpdate", "decal", "fx_spawn",
      "ringBurst", "modelVerts", "statusFx", "modelParticlesLight", "modelParticles", "lightLink", "lightLink2",
      "particleGroup", "puff", "disintegrate", "iceShatter", "Fire"};
  return names;
}
bool float_key(std::string_view primitive, std::string_view key) {
  return key.substr(0, 1) == "f" || (primitive == "LightEffect" && key.substr(0, 3) == "sp+");
}

constexpr std::uint32_t SKILL_LO = 0x2C3CD4, SKILL_HI = 0x320000;
} // namespace

std::vector<std::string> argument_keys(std::string_view primitive) {
  if (primitive == "particle") return {"f12", "a0", "a1", "a2"};
  if (primitive == "LightEffect")
    return {"f12", "f13", "f14", "f15", "f16", "f17", "f18", "f19", "a0", "a1", "a2", "a3",
            "sp+0x0", "sp+0x8", "sp+0x10", "sp+0x18"};
  if (primitive == "glow") return {"f12", "f13", "a0", "a1", "a2", "a3", "t0", "t1", "t2", "t3"};
  if (primitive == "trail") return {"f12", "a0", "a1", "a2", "a3", "t0", "t1", "t2"};
  if (primitive == "beam") return {"f12", "f13", "a0", "a1", "a2", "a3", "t0"};
  if (primitive == "decal") return {"f12", "a0", "a1", "a2", "a3"};
  if (primitive == "fx_spawn") return {"a0", "a1"};
  return {};
}

std::string_view meaning(std::string_view primitive, std::string_view key) {
  struct Row {
    std::string_view primitive, key, meaning;
  };
  static const Row rows[] = {
      {"particle", "f12", "backdate"}, {"particle", "a0", "definition"}, {"particle", "a1", "position*"},
      {"particle", "a2", "velocity*"}, {"LightEffect", "a0", "block"}, {"LightEffect", "a1", "position*"},
      {"LightEffect", "a2", "follow"}, {"LightEffect", "a3", "flag"}, {"LightEffect", "f12", "r0"},
      {"LightEffect", "f13", "g0"}, {"LightEffect", "f14", "b0"}, {"LightEffect", "f15", "a0"},
      {"LightEffect", "f16", "r1"}, {"LightEffect", "f17", "g1"}, {"LightEffect", "f18", "b1"},
      {"LightEffect", "f19", "a1"}, {"LightEffect", "sp+0x0", "radius0"}, {"LightEffect", "sp+0x8", "radius1"},
      {"LightEffect", "sp+0x10", "floor"}, {"LightEffect", "sp+0x18", "seconds"}, {"glow", "f12", "bias"},
      {"glow", "f13", "size"}, {"glow", "a0", "position*"}, {"glow", "a1", "r"}, {"glow", "a2", "g"},
      {"glow", "a3", "b"}, {"glow", "t0", "alpha"}, {"glow", "t1", "texture"}, {"glow", "t2", "angle"},
      {"glow", "t3", "flags"}, {"trail", "f12", "lifetime"}, {"trail", "a0", "row"}, {"trail", "a1", "width"},
      {"trail", "a2", "flags"}, {"trail", "a3", "r"}, {"trail", "t0", "g"}, {"trail", "t1", "b"},
      {"trail", "t2", "a"}, {"beam", "f12", "jitter"}, {"beam", "f13", "scale"}, {"beam", "a0", "from*"},
      {"beam", "a1", "to*"}, {"beam", "a2", "width"}, {"beam", "a3", "depth"}, {"beam", "t0", "rgba"},
      {"decal", "f12", "size"}, {"decal", "a0", "position*"}, {"decal", "a1", "angle"}, {"decal", "a2", "type"},
      {"decal", "a3", "grow"}, {"fx_spawn", "a0", "id"}, {"fx_spawn", "a1", "position*"},
  };
  for (const auto &r : rows)
    if (r.primitive == primitive && r.key == key) return r.meaning;
  return {};
}

std::string class_of(std::string_view name) {
  const auto underscore = name.find('_');
  return std::string(underscore == std::string_view::npos ? name : name.substr(0, underscore));
}

std::optional<Raw> SiteCall::arg(std::string_view key) const {
  for (const auto &a : args)
    if (a.first == key) return a.second;
  return std::nullopt;
}

std::string SiteCall::describe() const {
  // '    %08x %-34s %-11s %s' with 'meaning=value' per argument (fx_skill_recipes.text)
  std::string args_text;
  for (const auto &key : argument_keys(primitive)) {
    std::optional<Raw> v;
    for (const auto &a : args)
      if (a.first == key) v = a.second;
    if (!args_text.empty()) args_text += ' ';
    const auto m = meaning(primitive, key);
    args_text += std::string(m.empty() ? std::string_view(key) : m) + '=';
    char buf[64];
    if (!v) args_text += '?';
    else if (float_key(primitive, key)) {
      float f;
      const Raw bits = *v;
      std::memcpy(&f, &bits, sizeof f);
      std::snprintf(buf, sizeof buf, "%g", static_cast<double>(f));
      args_text += buf;
    } else {
      std::snprintf(buf, sizeof buf, "0x%x", static_cast<unsigned>(*v));
      args_text += buf;
    }
  }
  char head[160];
  std::snprintf(head, sizeof head, "    %08x %-34s %-11s ", static_cast<unsigned>(site), function.c_str(), primitive.c_str());
  return head + args_text;
}

// ---------------------------------------------------------------------------------------
// The constants the native model needs: the literal arguments of the reference model's
// call-outs at each call site (function, primitive, ordinal in ascending site address).
// tools/generate_fx_skills_vectors.py checks this table against the reference: over all its
// cases exactly these positions are constant, every other position varies.
// ---------------------------------------------------------------------------------------
const std::vector<Need> &needs() {
  static const std::vector<std::string_view> P2 = {"a0", "f12"};
  static const std::vector<Need> rows = {
      {0x2C4E64, "glow", 0, {"a1", "a2", "a3", "t3", "f12"}},              // ShockProjectile_draw
      {0x2D67E8, "glow", 0, {"a1", "a2", "a3", "t0", "t2", "t3", "f12"}},  // FireArrowProjectile_draw
      {0x2ECD90, "glow", 0, {"a1", "a2", "a3", "t0", "t3", "f12"}},        // HolyStrike_draw
      {0x311034, "glow", 0, {"a1", "a2", "a3", "t3", "f12"}},              // SpellSummonBadger_draw
      {0x312F48, "glow", 0, {"a1", "a2", "a3", "t2", "t3", "f12", "f13"}}, // SpellResurrect_draw
      {0x31CBF8, "glow", 0, {"t3", "f12"}},                                // SmokeGen_draw
      {0x2DA164, "trail", 0, {"a0", "a1", "a2", "a3", "t0", "t1", "t2", "f12"}}, // MultiFireProjectile_ctor
      {0x2DA2BC, "particle", 0, P2},                                        // MultiFireProjectile_run
      {0x2DA928, "fx_spawn", 0, {"a0"}},                                    // _end, level flag 0x40000 (fire)
      {0x2DA928, "fx_spawn", 1, {"a0"}},                                    // _end, otherwise (dust)
      {0x2D33C0, "particle", 0, P2}, {0x2D33C0, "particle", 1, P2},         // FireStormFlare_run: falling x30, x18
      {0x2D33C0, "particle", 2, P2},                                        // x4
      {0x2D33C0, "particle", 3, P2}, {0x2D33C0, "particle", 4, P2},         // landed ring pair
      {0x2D33C0, "decal", 0, {"a2", "a3"}},                                 // scorch: type, growth
      {0x30BDE0, "particle", 0, P2}, {0x30BDE0, "particle", 1, P2},         // Pustule_run: fall, drop
      {0x2CE6E0, "particle", 0, P2}, {0x2CE6E0, "particle", 1, P2},         // DiseaseCloud_run streams
      {0x2CE6E0, "particle", 2, P2},
      {0x305BBC, "particle", 0, P2}, {0x305BBC, "particle", 1, P2},         // SpellArena_run: every fourth, rest
      {0x2FC684, "particle", 0, P2}, {0x2FC684, "particle", 1, P2},         // SpellHornOmenEffect_run (one call)
      {0x2FB9B4, "particle", 0, P2}, {0x2FB9B4, "particle", 1, P2},         // SpellPlanarSteelEffect_run: burst,
      {0x2FB9B4, "particle", 2, P2}, {0x2FB9B4, "particle", 3, P2},         // orbit x6, orbit x4, falling,
      {0x2FB9B4, "particle", 4, P2},                                        // ground
      {0x2FB9B4, "glow", 0, {"t2", "t3", "f12", "f13"}},                    // orbit glows
      {0x2C595C, "particle", 0, P2}, {0x2C595C, "particle", 1, P2},         // StaticFire_run: flames, embers
      {0x2C65E8, "glow", 0, {"a1", "a2", "a3", "t0", "t2", "t3", "f12"}},  // StaticFire_draw: the fire
      {0x2C65E8, "glow", 1, {"t3", "f12"}},                                 // each flame point
  };
  return rows;
}

std::vector<const SiteCall *> RecipeConstants::sites(std::uint32_t function, std::string_view primitive) const {
  std::vector<const SiteCall *> out;
  for (const auto &c : calls_)
    if (c.address == function && c.primitive == primitive) out.push_back(&c);
  std::sort(out.begin(), out.end(), [](const SiteCall *a, const SiteCall *b) { return a->site < b->site; });
  return out;
}

const LightRecord *RecipeConstants::light_record(std::string_view function) const {
  const auto it = lights_.find(function);
  return it == lights_.end() ? nullptr : &it->second;
}

Raw RecipeConstants::needed(std::uint32_t function, std::string_view primitive, unsigned ordinal,
                            std::string_view key) const {
  const auto it = needed_.find({function, std::string(primitive), ordinal, std::string(key)});
  if (it == needed_.end()) throw Refusal("not a constant the native model needs");
  return it->second;
}

RecipeConstants RecipeConstants::from_json(std::string_view text) {
  Reader reader{text};
  const Json root = reader.value();
  reader.space();
  if (reader.p != text.size()) refuse("trailing text");
  if (root.kind != Json::Kind::Object) refuse("the top level is not an object");
  for (const auto &m : root.members)
    if (m.first != "classes" && m.first != "executable_sha256" && m.first != "range" &&
        m.first != "particle_definitions" && m.first != "light_records")
      refuse("unexpected top-level key '" + m.first + "'");

  RecipeConstants out;
  out.executable_sha256 = json_string(member(root, "executable_sha256", "top level"), "executable_sha256");
  if (out.executable_sha256.size() != 64 ||
      out.executable_sha256.find_first_not_of("0123456789abcdef") != std::string::npos)
    refuse("executable_sha256 is not 64 lowercase hex digits");
  const Json &range = member(root, "range", "top level");
  if (range.kind != Json::Kind::Array || range.items.size() != 2) refuse("range: two integers expected");
  out.range_lo = json_u32(range.items[0], "range");
  out.range_hi = json_u32(range.items[1], "range");
  if (out.range_lo != SKILL_LO || out.range_hi != SKILL_HI)
    refuse("range is not the skill files 0x2c3cd4-0x320000 the recipe table covers");

  // ---- classes: phases (the embedded recipe rows) and calls --------------------------
  const Json &classes_json = member(root, "classes", "top level");
  if (classes_json.kind != Json::Kind::Object) refuse("classes: an object expected");
  std::set<std::string> recipe_classes;
  for (const auto &row : recipes()) recipe_classes.emplace(row.cls);
  for (const auto &[cls, entry] : classes_json.members) {
    const std::string where = "class " + cls;
    for (const auto &m : entry.members)
      if (m.first != "phases" && m.first != "calls") refuse(where + ": unexpected key " + m.first);
    const Json &phases = member(entry, "phases", where);
    const Json &calls = member(entry, "calls", where);
    if (phases.kind != Json::Kind::Array || calls.kind != Json::Kind::Array) refuse(where + ": arrays expected");
    // phases must be exactly the pinned RECIPES rows of the class
    const auto rows = recipe(cls);
    if (phases.items.size() != rows.size()) refuse(where + ": phases do not match the recipe table");
    for (std::size_t k = 0; k < rows.size(); ++k) {
      const Json &p = phases.items[k];
      const std::string pw = where + " phase " + std::to_string(k);
      if (p.members.size() != 6) refuse(pw + ": unexpected keys");
      if (json_string(member(p, "phase", pw), pw) != phase_name(rows[k].phase) ||
          json_string(member(p, "function", pw), pw) != rows[k].function ||
          json_u32(member(p, "address", pw), pw) != rows[k].addr ||
          json_string(member(p, "model", pw), pw) != rows[k].model ||
          json_string(member(p, "calls_in_code", pw), pw) != rows[k].calls)
        refuse(pw + ": does not match the recipe table");
      const Json &gens = member(p, "generators", pw);
      if (gens.kind != Json::Kind::Array) refuse(pw + ": generators");
      std::string joined;
      for (const auto &g : gens.items) joined += (joined.empty() ? "" : " ") + json_string(g, pw);
      if (joined != rows[k].generators) refuse(pw + ": generators do not match the recipe table");
    }
    for (const auto &c : calls.items) {
      const std::string cw = where + " call";
      for (const auto &m : c.members)
        if (m.first != "function" && m.first != "address" && m.first != "site" && m.first != "primitive" &&
            m.first != "args" && m.first != "meaning")
          refuse(cw + ": unexpected key " + m.first);
      SiteCall sc;
      sc.cls = cls;
      sc.function = json_string(member(c, "function", cw), cw);
      sc.address = json_u32(member(c, "address", cw), cw + " address");
      sc.site = json_u32(member(c, "site", cw), cw + " site");
      sc.primitive = json_string(member(c, "primitive", cw), cw + " primitive");
      const std::string sw = cw + " at site " + std::to_string(sc.site);
      if (std::find(primitive_names().begin(), primitive_names().end(), sc.primitive) == primitive_names().end())
        refuse(sw + ": unknown primitive " + sc.primitive);
      if (sc.site < sc.address || sc.address < SKILL_LO || sc.address >= SKILL_HI) refuse(sw + ": outside its function");
      const Recipe *row = recipe_at(sc.address);
      if (!row) refuse(sw + ": a function the recipe table does not list");
      if (sc.function != row->function || cls != row->cls) refuse(sw + ": function or class name differs from the recipe table");
      const Json &args = member(c, "args", sw);
      const Json &mean = member(c, "meaning", sw);
      const auto keys = argument_keys(sc.primitive);
      if (args.kind != Json::Kind::Object || args.members.size() != keys.size() || mean.kind != Json::Kind::Object ||
          mean.members.size() != keys.size())
        refuse(sw + ": argument keys do not match fx_callsites.SIGNATURES");
      for (const auto &key : keys) {
        const Json *v = args.get(key);
        const Json *m = mean.get(key);
        if (!v || !m) refuse(sw + ": no argument " + key);
        if (json_string(*m, sw) != meaning(sc.primitive, key)) refuse(sw + ": meaning of " + key + " differs");
        std::optional<Raw> value;
        if (v->kind != Json::Kind::Null)
          value = float_key(sc.primitive, key) ? json_f32(*v, sw + " " + key) : json_u32(*v, sw + " " + key);
        sc.args.emplace_back(key, value);
      }
      out.calls_.push_back(std::move(sc));
    }
  }
  // every recipe row: the JSON's calls per primitive equal the static counts in the code
  for (const auto &row : recipes()) {
    std::map<std::string, unsigned> found;
    for (const auto &c : out.calls_)
      if (c.address == row.addr) ++found[c.primitive];
    if (found != static_calls(row))
      refuse(std::string(row.function) + ": primitive calls do not match the recipe table's static counts");
  }
  // sites are distinct
  std::set<std::uint32_t> seen_sites;
  for (const auto &c : out.calls_)
    if (!seen_sites.insert(c.site).second) refuse("site " + std::to_string(c.site) + " listed twice");

  // ---- the native model's constants: present, typed, consistent ------------------------
  for (const auto &need : needs()) {
    const auto sites = out.sites(need.function, need.primitive);
    const Recipe *row = recipe_at(need.function);
    if (need.ordinal >= sites.size())
      refuse(std::string(row ? row->function : "?") + ": no " + std::string(need.primitive) + " site " +
             std::to_string(need.ordinal));
    for (const auto &key : need.keys) {
      const auto v = sites[need.ordinal]->arg(key);
      if (!v)
        refuse(std::string(row->function) + " " + std::string(need.primitive) + " site " + std::to_string(need.ordinal) +
               ": the constant '" + std::string(key) + "' the model needs is null");
      out.needed_[{need.function, std::string(need.primitive), need.ordinal, std::string(key)}] = *v;
    }
  }
  // sites the model fills from one class constant (or one call) must agree
  struct Same {
    std::uint32_t function;
    unsigned a, b;
    std::vector<std::string_view> keys;
  };
  const Same same[] = {{0x2D33C0, 0, 3, {"a0"}}, {0x2D33C0, 1, 4, {"a0"}}, {0x2FB9B4, 1, 4, {"a0"}},
                       {0x2FC684, 0, 1, {"a0", "f12"}}};
  for (const auto &s : same)
    for (const auto &key : s.keys)
      if (out.needed(s.function, "particle", s.a, key) != out.needed(s.function, "particle", s.b, key))
        refuse(std::string(recipe_at(s.function)->function) + ": particle sites " + std::to_string(s.a) + " and " +
               std::to_string(s.b) + " differ in '" + std::string(key) +
               "' although the model uses one value for both (the site-order inference does not hold)");

  // ---- particle_definitions: the sorted constant definitions of the particle calls ---------
  const Json &defs = member(root, "particle_definitions", "top level");
  if (defs.kind != Json::Kind::Array) refuse("particle_definitions: an array expected");
  std::set<std::uint32_t> want;
  for (const auto &c : out.calls_)
    if (c.primitive == "particle")
      if (auto v = c.arg("a0")) want.insert(*v);
  for (const auto &d : defs.items) out.particle_definitions.push_back(json_u32(d, "particle_definitions"));
  if (std::vector<std::uint32_t>(want.begin(), want.end()) != out.particle_definitions)
    refuse("particle_definitions do not match the particle calls");

  // ---- light_records (fx_skill_recipes.py --json at research b6618dd) ------------------
  if (const Json *lights = root.get("light_records")) {
    if (lights->kind != Json::Kind::Object) refuse("light_records: an object expected");
    for (const auto &[name, rec] : lights->members) {
      const std::string lw = "light_records " + name;
      const bool fire = name == "StaticFire_ctor" || name == "StaticFire_ctor2";
      if (!fire && name != "SpellPlanarSteelEffect_ctor") refuse(lw + ": unknown constructor");
      LightRecord lr;
      const Json &colour = member(rec, "colour", lw);
      if (colour.kind != Json::Kind::Array || colour.items.size() != 8) refuse(lw + ": eight colour shorts expected");
      for (std::size_t k = 0; k < 8; ++k) {
        const auto c = json_u32(colour.items[k], lw + " colour");
        if (c > 0xFFFF) refuse(lw + ": colour wider than 16 bits");
        lr.colour[k] = static_cast<std::uint16_t>(c);
      }
      lr.radius = json_f32(member(rec, "radius", lw), lw + " radius");
      lr.extra = json_f32(member(rec, "extra", lw), lw + " extra");
      if (const Json *falloff = rec.get("falloff")) lr.falloff = json_f32(*falloff, lw + " falloff");
      if (fire != lr.falloff.has_value()) refuse(lw + ": falloff is for the StaticFire constructors only");
      for (const auto &m : rec.members)
        if (m.first != "colour" && m.first != "radius" && m.first != "extra" && m.first != "falloff")
          refuse(lw + ": unexpected key " + m.first);
      out.lights_.emplace(name, lr);
    }
  }
  return out;
}

} // namespace champions::rta::fx::skills
