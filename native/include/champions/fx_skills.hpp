#pragma once
// Skill and effect-class visual recipes of Champions: Return to Arms (SLUS_209.73, US v2.00):
// a handwritten C++17 port of the research reference tools/ref_fx_skills.py at commit 19f80a6
// (docs/effects.md sections 11, 12, 15 and 17).  RTA only; nothing here applies to Norrath.
//
// Three parts, as in the reference:
//   1. CALLOUTS / to_primitive: how a recorded call-out of a skill model (canonical arguments:
//      integers as unsigned 32-bit, floats as 32-bit patterns, a vector as three patterns; in
//      the order a0.., f12.., stack words) becomes a typed effect primitive, plus adapters from
//      the call logs of the native skill ports (skills_a, skills_b, skills_c, arena_monsters)
//      and of this module's own model.
//   2. RECIPES: the 161 effect-calling functions of the skill files (91 classes), with phase,
//      model, static primitive counts and the generators they draw from.
//   3. Model: the 21 functions round 2 modelled (25 methods with the StaticFire helpers),
//      call for call over a memory image with callbacks for everything outside.
//
// No retail value is compiled in.  Counts, timings, angle rates, offsets, the arithmetic
// constants of the computations, memory offsets and global / code addresses are code (as in
// the existing native skill ports).  Every constant EFFECT ARGUMENT that
// tools/fx_skill_recipes.py --json reports for a call site (particle definitions, glow colours /
// alphas / flags / bias / size / angle, trail row / width / flags / colours / lifetime, decal type
// and growth, fx_spawn ids, particle back-dates) is read at run time from that JSON
// (RecipeConstants).  The light-record words the StaticFire and SpellPlanarSteelEffect
// constructors store (colours, radius, falloff, the +0x2c word) are the "light_records"
// object fx_skill_recipes.py --json emits (research t3/research-fx b6618dd, docs/effects.md
// section 12.2).  They are read from that object.  The three constructors still refuse
// (Refusal) when the object, or a constructor's required falloff, is omitted.
//
// SITE MAPPING (an INFERENCE; the reference does not state it).  A call-out the model makes
// inside an original function is matched to the JSON call of that function (by its address,
// helpers inlined in the original belong to their caller: _fsf_trail -> FireStormFlare_run,
// _sf_burn -> StaticFire_run) and primitive with the same ORDINAL in ascending site address.
// The ordinal is the position of the call-out in the text of the reference model method; a
// call-out in a loop over a class-constant tuple of definitions counts once per tuple element,
// in tuple order; a conditional argument counts its taken value first.  needs() lists the
// result.  Evidence: fx_callsites.scan_calls walks the text section in ascending address order
// (so the JSON lists a function's calls in site order) and the RECIPES static counts, made by
// the same scan, equal the number of model call-outs so mapped for all 21 functions.  That the
// compiler emitted the calls in source order is not documented.  Weakest, as they also assume
// a branch layout: MultiFireProjectile_end fx_spawn (site 0 = level flag 0x40000 set, the
// reference's fx_spawn(8); site 1 = otherwise, fx_spawn(2)), SpellArena_run particle (site 0 =
// i % 4 == 0) and SpellHornOmenEffect_run (particle:2 in the code, ONE call-out in the model:
// both sites must hold the same values).  Where the model fills two sites from one class
// constant the loader requires the JSON to agree (FireStormFlare_run sites 0 = 3 and 1 = 4,
// SpellPlanarSteelEffect_run 1 = 4: a0; SpellHornOmenEffect_run 0 = 1: a0 and f12) and refuses
// otherwise, which catches a wrong order there.
#include "champions/core_math.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace champions::rta::fx::skills {
namespace core = champions::simulation::rta;
using Raw = core::Raw;
using Vec3 = core::Vec3;

// Undocumented or unsupported input: the model stops instead of guessing.
struct Refusal : std::runtime_error {
  using std::runtime_error::runtime_error;
};
// A call-out whose arguments do not have the shape its CALLOUTS row expects.
struct DecodeError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

// ---------------------------------------------------------------------------------------
// Values: the Python-shaped canonical arguments of a call-out and the answers of callbacks.
// Integers are non-negative (call arguments are masked to 32 bits); tuples and lists stay
// distinct, as do text and byte buffers.
// ---------------------------------------------------------------------------------------
struct Value {
  enum class Kind : std::uint8_t { None, Int, Str, Bytes, Tuple, List };
  Kind kind = Kind::None;
  std::uint64_t i = 0;
  std::string s;
  std::vector<Value> items;

  static Value none() { return {}; }
  static Value integer(std::uint64_t v) { Value x; x.kind = Kind::Int; x.i = v; return x; }
  static Value str(std::string v) { Value x; x.kind = Kind::Str; x.s = std::move(v); return x; }
  static Value bytes(std::string v) { Value x; x.kind = Kind::Bytes; x.s = std::move(v); return x; }
  static Value tuple(std::vector<Value> v) { Value x; x.kind = Kind::Tuple; x.items = std::move(v); return x; }
  static Value list(std::vector<Value> v) { Value x; x.kind = Kind::List; x.items = std::move(v); return x; }
  static Value vec(const Vec3 &v);
  bool is_int() const { return kind == Kind::Int; }
  bool is_sequence() const { return kind == Kind::Tuple || kind == Kind::List; }
  bool truthy() const;                       // Python truth
  // Text form used by the test vectors: hex integers, N, 'text, y<hex bytes>, (..), [..].
  std::string serialize() const;
  static Value parse(std::string_view text);
  bool operator==(const Value &o) const;
  bool operator!=(const Value &o) const { return !(*this == o); }
};

struct Callout {
  std::string name;
  std::vector<Value> args;
  std::string serialize() const;            // name(args...)
};

// ---------------------------------------------------------------------------------------
// 1. Call-outs -> primitives (ref_fx_skills.CALLOUTS, to_primitive, effects_of)
// ---------------------------------------------------------------------------------------
struct CalloutInfo {
  std::string_view name;
  std::uint32_t addr;
  std::string_view primitive;
  bool opaque;                              // fields None: the arguments as recorded
  std::vector<std::string_view> fields;
  std::string_view note;
};
const std::vector<CalloutInfo> &callouts(); // the 36 rows, in the reference's order
const CalloutInfo *find_callout(std::string_view name);

// One field of a decoded primitive.  Float: a pattern the reference turned into a float;
// Vector: a tuple of float patterns; Int: an integer kept as recorded (the reference leaves
// the float arguments of the generic rows as integers too); Other: anything else, as recorded.
struct Field {
  enum class Kind : std::uint8_t { Int, Float, Vector, Other };
  Kind kind = Kind::Int;
  Raw raw = 0;
  std::vector<Raw> lanes;
  Value other;
};
struct FxCall {
  std::string primitive;
  std::vector<std::pair<std::string, Field>> fields; // in the CALLOUTS field order
  const Field *field(std::string_view name) const;
};
// nullopt when the name is not an effect primitive; DecodeError on a malformed argument
// list.  Like the reference (zip), a short argument list yields fewer fields.
std::optional<FxCall> to_primitive(const Callout &);
std::vector<FxCall> effects_of(const std::vector<Callout> &);

// ---- adapters from the native ports' call logs ----------------------------------------
enum class Port : std::uint8_t { SkillsA, SkillsB, SkillsC, ArenaMonsters, FxSkills };
// One EXTERNALS row of a reference module: integer argument kinds, float registers and
// stack floats.
struct Signature {
  std::string_view ints;
  unsigned floats;
  unsigned stack;
};
std::optional<Signature> signature(Port, std::string_view name);
// A call-out recorded as flat 32-bit lanes (skills_a Model::calls, skills_b's scalar calls,
// skills_c's numeric service calls): vectors are regrouped by the port's signature.  skills_c
// passes 0 for the stack matrix of particleGroup_setMatrix; it becomes the reference's 'stack'.
Callout from_lanes(Port, std::string name, const std::vector<std::uint32_t> &lanes);
// skills_c Service number -> call-out name, for the services that are call-outs.
std::optional<std::string> skills_c_service_name(unsigned service);

namespace detail {
template <class T> struct is_std_array : std::false_type {};
template <class T, std::size_t N> struct is_std_array<std::array<T, N>> : std::true_type {};
template <class T> Value value_of(const T &x) {
  if constexpr (std::is_integral_v<T>) {
    return Value::integer(static_cast<std::uint64_t>(x) & 0xffffffffu);
  } else if constexpr (is_std_array<T>::value) {
    std::vector<Value> lanes;
    for (const auto &lane : x) lanes.push_back(value_of(lane));
    return Value::tuple(std::move(lanes));
  } else if constexpr (std::is_same_v<T, std::string>) {
    return Value::str(x);
  } else if constexpr (std::is_same_v<T, std::monostate>) {
    return Value::none();
  } else {
    return Value::str("<opaque>");
  }
}
} // namespace detail
// One argument of a typed call log (skills_b::Model::Argument, arena_monsters::Value).
template <class Variant> Value value_from_variant(const Variant &v) {
  return std::visit([](const auto &x) { return detail::value_of(x); }, v);
}
// A typed call record {name, args} (skills_b::Model::TypedCall, arena_monsters::Call).
template <class TypedCall> Callout from_typed(const TypedCall &c) {
  Callout out{c.name, {}};
  for (const auto &a : c.args) out.args.push_back(value_from_variant(a));
  return out;
}
// skills_c::ResultValue (kind None/Scalar/List/Tuple/Bytes/Buffer, scalar, values).
template <class R> Value value_from_result(const R &r) {
  switch (r.kind) {
  case R::Scalar: return Value::integer(r.scalar);
  case R::List:
  case R::Tuple: {
    std::vector<Value> items;
    for (const auto &x : r.values) items.push_back(value_from_result(x));
    return r.kind == R::List ? Value::list(std::move(items)) : Value::tuple(std::move(items));
  }
  case R::Bytes:
  case R::Buffer: {
    std::string data;
    for (const auto &x : r.values) data.push_back(static_cast<char>(x.scalar & 0xff));
    return Value::bytes(std::move(data));
  }
  default: return Value::none();
  }
}

// ---------------------------------------------------------------------------------------
// 2. Recipes per class (ref_fx_skills.RECIPES)
// ---------------------------------------------------------------------------------------
enum class Phase : std::uint8_t { Cast, Run, Draw, Event };
std::string_view phase_name(Phase);
struct Recipe {
  std::string_view cls;
  Phase phase;
  std::string_view function;
  std::uint32_t addr;
  std::string_view model;          // module.method of the executed model
  std::string_view calls;          // static primitive calls in the code, "particle:2 trailPoint:1"
  std::string_view generators;     // "SHARED own", see Generator
};
const std::vector<Recipe> &recipes(); // the 161 rows in the reference's order
std::vector<Recipe> recipe(std::string_view cls);
std::vector<std::string> classes();   // 91, sorted
const Recipe *recipe_at(std::uint32_t addr);
std::pair<std::string_view, std::string_view> model_method(const Recipe &);
std::map<std::string, unsigned> static_calls(const Recipe &);
// What the model's code of a function draws from (docs/effects.md 15.2).
enum Generator : unsigned {
  Own = 1,               // the object's generator +0x44 (GameObject_rand 0x1509d0)
  SpellGen = 2,          // the spell's generator +0x10 (Spell_rand 0x16a964)
  Shared = 4,            // the shared seed 0x404464 (in the sync hash)
  SharedLightEffect = 8, // the one shared draw inside each LightEffect constructor
  Cosmetic = 16,         // 0x404460, per machine
  Seed3 = 32,            // 0x404468, per machine
};
unsigned generators(const Recipe &);
// True when the function draws from a generator other machines also advance (own, spell,
// SHARED, SHARED(LightEffect)): a headless or low-detail client must still run it, every
// particle draw and rejection loop included; only the drawing may differ (15.2).
bool must_run_headless(const Recipe &);
// True for the 21 rows whose model is part 3 (this module's Model).
bool natively_modelled(const Recipe &);

// ---------------------------------------------------------------------------------------
// The recipe constants: tools/fx_skill_recipes.py <elf> --json, loaded at run time.
// ---------------------------------------------------------------------------------------
// fx_skill_recipes.MEANING: what an argument register / stack slot means per primitive.
std::string_view meaning(std::string_view primitive, std::string_view key);
// The argument keys of a primitive in fx_callsites.SIGNATURES order (f-regs, int regs, stack).
std::vector<std::string> argument_keys(std::string_view primitive);
std::string class_of(std::string_view function_name);  // fx_skill_recipes.class_of

struct SiteCall {
  std::string cls;
  std::string function;
  std::uint32_t address = 0;   // the calling function
  std::uint32_t site = 0;      // the call instruction
  std::string primitive;       // fx_callsites.PRIMITIVES name
  // key -> value (integers as u32, floats as f32 patterns), nullopt for the JSON's null
  std::vector<std::pair<std::string, std::optional<Raw>>> args;
  std::optional<Raw> arg(std::string_view key) const;
  std::string describe() const; // the line fx_skill_recipes.text prints for the call
};
struct LightRecord {
  std::array<std::uint16_t, 8> colour{}; // +0xa..+0x18: two colour quads
  Raw radius = 0;                        // +0x1c and +0x24
  std::optional<Raw> falloff;            // +0x20 and +0x28 (StaticFire only)
  Raw extra = 0;                         // +0x2c
};
class RecipeConstants {
public:
  // Parse and validate; Refusal with the reason when the JSON does not fit (not the
  // pinned research's recipe table, wrong counts, a needed constant null, ...).
  static RecipeConstants from_json(std::string_view text);
  std::string executable_sha256;
  std::uint32_t range_lo = 0, range_hi = 0;
  std::vector<std::uint32_t> particle_definitions; // as listed, checked against the calls
  // All calls of one function and primitive, in ascending site address.
  std::vector<const SiteCall *> sites(std::uint32_t function, std::string_view primitive) const;
  const std::vector<SiteCall> &calls() const { return calls_; }
  const LightRecord *light_record(std::string_view function) const;
  // The value the native model uses at (function, primitive, ordinal, key); Refusal when the
  // site or the key is not one the model needs.
  Raw needed(std::uint32_t function, std::string_view primitive, unsigned ordinal, std::string_view key) const;

private:
  std::vector<SiteCall> calls_;
  std::map<std::string, LightRecord, std::less<>> lights_;
  std::map<std::tuple<std::uint32_t, std::string, unsigned, std::string>, Raw> needed_;
  friend class Model;
};
// The needed constants of the native model, (function, primitive, ordinal, keys): the
// literal arguments of the reference model's call-outs.
struct Need {
  std::uint32_t function;
  std::string_view primitive;
  unsigned ordinal;
  std::vector<std::string_view> keys;
};
const std::vector<Need> &needs();

// ---------------------------------------------------------------------------------------
// 3. The model of part 3
// ---------------------------------------------------------------------------------------
// cvt.s.w of an unsigned value as the compiler does it (ref_fx_skills._from_uint): halve
// keeping the low bit and double when bit 31 is set (so the conversion rounds toward zero).
Raw from_uint(std::uint32_t value);

using Addr = std::int64_t;              // the reference's addresses are unbounded integers
// Sparse little-endian memory; unwritten bytes read as zero.
class Memory {
public:
  std::uint64_t load(Addr addr, unsigned size) const;
  void poke(Addr addr, unsigned size, std::uint64_t value);
  std::unordered_map<Addr, std::uint8_t> bytes;
};
struct Write {
  Addr addr;
  unsigned size;
  std::uint64_t value;
};

// The model sees the game exactly as ref_fx_skills.Model does: a memory image, a call-out for
// everything outside (recorded with canonical arguments in `calls`), every store in `writes`.
// Virtual calls run inline the bodies the reference runs inline (slot 16 0x3c2738, 0x3c2708,
// Creature_isEnemy 0x119328, slot 37 0x3c2730; the Skill statistic bodies 0x3c85fc, 0x3c8690,
// 0x3c87f0, 0x3c8864, 0x3c88dc); a table that names any other body the reference models refuses
// (it is not ported here); other functions are call-outs 'Creature.vN' / 'Skill.vN' / 'Spell.v1'.
// A loop the reference would not finish in practice (over MAX 1e6 steps) refuses.
// SpellHornOmenEffect_run places its group each tick: mat34_setPosRotZ of the owner position
// at +0x10 and the facing word at +0x38, then particleGroup_setMatrix of the group at +0x1c
// (docs/effects.md section 12.2, research b6618dd).  The research reference still records the
// token 'stack' because that matrix is built on the stack; this model passes the 12 lanes.
// skills_c's particleGroup_setMatrix still passes 0, which stays the 'stack' token.
class Model {
public:
  // Answer of the outside for one call-out.  Without a handler for the name and without a
  // fallback answer the model uses the reference's default (ref_skills_c.shape_result over
  // the pinned EXTERNALS row: zeros, empty lists), or refuses for a name with no row.
  using Handler = std::function<Value(Model &, const Callout &)>;
  using Fallback = std::function<std::optional<Value>(Model &, const Callout &)>;

  explicit Model(const RecipeConstants &constants);
  Memory mem;
  std::map<std::string, Handler, std::less<>> handlers;
  Fallback fallback;
  std::vector<Callout> calls;   // every call-out, in order (the reference's Model.calls)
  std::vector<Write> writes;    // every store, generator words included (Model.writes)
  std::uint64_t draws = 0;      // shared-seed draws made by the model itself

  // Draw slots (once per rendered frame; no state but the call-outs)
  void shock_draw(Raw obj);          // ShockProjectile_draw 0x2c4e64
  void firearrow_draw(Raw obj);      // FireArrowProjectile_draw 0x2d67e8
  void holystrike_draw(Raw obj);     // HolyStrike_draw 0x2ecd90
  void badger_draw(Raw spell);       // SpellSummonBadger_draw 0x311034
  void resurrect_draw(Raw spell);    // SpellResurrect_draw 0x312f48
  void smokegen_draw(Raw obj);       // SmokeGen_draw 0x31cbf8
  // MultiFireProjectile
  void multifire_ctor(Raw obj, const Vec3 &pos, Raw angle, Raw owner, Raw level, Raw skill); // 0x2da164
  void multifire_run(Raw obj);                  // 0x2da2bc
  void multifire_end(Raw obj, Raw reason);      // 0x2da928
  void firestormflare_run(Raw obj);             // FireStormFlare_run 0x2d33c0
  void pustule_run(Raw obj);                    // Pustule_run 0x30bde0
  void diseasecloud_run(Raw obj);               // DiseaseCloud_run 0x2ce6e0
  void arena_run(Raw spell, const Vec3 &pos);   // SpellArena_run 0x305bbc
  void hornomen_ctor(Raw spell, Raw kind, Raw owner, Raw extra);   // 0x2fc574
  void hornomen_run(Raw spell, const Vec3 &pos);                   // 0x2fc684
  void planarsteel_ctor(Raw spell, Raw kind, Raw owner, Raw extra); // 0x2fb728
  void planarsteel_run(Raw spell, const Vec3 &pos);                 // 0x2fb9b4
  // StaticFire
  void staticfire_ctor(Raw obj, const Vec3 &pos, Raw life, Raw owner, Raw quiet, Raw fuel, Raw scale); // 0x2c5240
  void staticfire_ctor2(Raw obj, const Vec3 &pos, Raw a2, Raw a3); // 0x2c54f8
  void staticfire_init_common(Raw obj);         // 0x2c5814
  void staticfire_wind(Raw obj, Raw angle);     // 0x2c6aa8
  void staticfire_add_flame(Raw obj);           // 0x2c6bc0
  void staticfire_move_flames(Raw obj);         // 0x2c6df0
  void staticfire_run(Raw obj);                 // 0x2c595c
  void staticfire_draw(Raw obj);                // 0x2c65e8

  // Run one method by its reference name with canonical arguments (the test vectors).
  void invoke(std::string_view method, const std::vector<Value> &args);
  static const std::vector<std::string_view> &methods();

  // memory access as the reference sees it
  std::uint32_t u32(Addr a) const { return static_cast<std::uint32_t>(mem.load(a, 4)); }

private:
  struct Constants;
  std::shared_ptr<const Constants> k_;
  unsigned depth_ = 0;

  std::uint32_t u8(Addr a) const { return static_cast<std::uint32_t>(mem.load(a, 1)); }
  std::int32_t s8(Addr a) const;
  std::uint32_t u16(Addr a) const { return static_cast<std::uint32_t>(mem.load(a, 2)); }
  std::int32_t s16(Addr a) const;
  std::int32_t s32(Addr a) const;
  std::uint64_t u64(Addr a) const { return mem.load(a, 8); }
  Vec3 vec(Addr a) const { return {u32(a), u32(a + 4), u32(a + 8)}; }
  void w(Addr a, unsigned size, std::uint64_t v);
  void w8(Addr a, std::uint64_t v) { w(a, 1, v); }
  void w16(Addr a, std::uint64_t v) { w(a, 2, v); }
  void w32(Addr a, std::uint64_t v) { w(a, 4, v); }
  void wvec(Addr a, const Vec3 &v);

  Value call(std::string name, std::vector<Value> args);
  Value default_answer(const std::string &name, const std::vector<Value> &args) const;
  Value vcall(Addr obj, unsigned slot, std::vector<Value> args);
  Value skill_call(Addr skill, unsigned slot, std::vector<std::int64_t> args);
  void spell_destroy(Addr spell);
  std::int32_t is_enemy(Addr c, const Value &other);

  std::uint32_t draw();
  std::uint32_t obj_rand(Addr obj);
  std::uint32_t spell_rand(Addr spell);
  Vec3 obj_ball(Addr obj);
  Vec3 spell_ball(Addr spell);
  Vec3 visual_pos(Addr c) const;

  void fsf_trail(Raw obj, const Vec3 &nv, Raw per_scale, unsigned site, std::optional<std::pair<Raw, Raw>> speed);
  void sf_light(Raw obj, const Vec3 &pos, std::string_view which);
  void sf_burn(Raw obj);
  Vec3 sf_offset(Raw obj, const Vec3 &ball) const;
  std::string cstr(Addr a) const;
};

} // namespace champions::rta::fx::skills
