// Tests of champions/fx_skills.hpp: the assertions of the research's tools/test_ref_fx_skills.py
// and tools/test_fx_skill_recipes.py that apply to native code (with synthetic stand-ins for
// every recipe constant), the loader's refusals, the decoding of every native port's call-log
// format, and the replay of native/tests/vectors/fx_skills.tsv (tools/generate_fx_skills_vectors.py).
//   champions-fx-skills-tests <native/tests/vectors>
#include "champions/fx_skills.hpp"

#include "champions/arena_monsters.hpp"
#include "champions/skills_a.hpp"
#include "champions/skills_b.hpp"
#include "champions/skills_c.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace fx = champions::rta::fx::skills;
using fx::Raw;
using fx::Value;

namespace {
int failures = 0;
void check(bool ok, const std::string &what) {
  if (!ok) {
    ++failures;
    std::cerr << "FAIL: " << what << "\n";
  }
}
Raw f32(float f) {
  Raw r;
  std::memcpy(&r, &f, 4);
  return r;
}
float fl(Raw r) {
  float f;
  std::memcpy(&f, &r, 4);
  return f;
}
std::string read_file(const std::string &path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error("cannot read " + path);
  std::ostringstream os;
  os << in.rdbuf();
  return os.str();
}
std::vector<std::string> split(const std::string &s, char sep) {
  std::vector<std::string> out;
  std::string cur;
  for (char c : s) {
    if (c == sep) {
      out.push_back(cur);
      cur.clear();
    } else {
      cur.push_back(c);
    }
  }
  out.push_back(cur);
  return out;
}
template <class F> bool refuses(F &&f) {
  try {
    f();
  } catch (const fx::Refusal &) {
    return true;
  }
  return false;
}
template <class F> std::string refusal_text(F &&f) {
  try {
    f();
  } catch (const fx::Refusal &e) {
    return e.what();
  }
  return {};
}
std::vector<Value> args_named(const fx::Model &m, const std::string &name, std::size_t which = 0) {
  std::size_t k = 0;
  for (const auto &c : m.calls)
    if (c.name == name && k++ == which) return c.args;
  return {};
}
std::size_t count_named(const fx::Model &m, const std::string &name) {
  std::size_t k = 0;
  for (const auto &c : m.calls) k += c.name == name;
  return k;
}
void put_vec(fx::Model &m, fx::Addr a, float x, float y, float z) {
  m.mem.poke(a, 4, f32(x));
  m.mem.poke(a + 4, 4, f32(y));
  m.mem.poke(a + 8, 4, f32(z));
}

// ---------------------------------------------------------------------------------------
// part 1: call-outs (test_ref_fx_skills.Callouts)
// ---------------------------------------------------------------------------------------
void test_callouts() {
  const auto v3 = [](float a, float b, float c) { return Value::vec({f32(a), f32(b), f32(c)}); };
  auto fx = fx::to_primitive({"particle_spawn", {Value::integer(0x00A10150), v3(1, 2, 3),
                                                 Value::vec({f32(0.5f), 0, f32(-8)}), Value::integer(0)}});
  check(fx && fx->primitive == "particle", "particle primitive");
  check(fx->field("definition")->raw == 0x00A10150, "particle definition");
  check(fx->field("pos")->lanes == std::vector<Raw>{f32(1), f32(2), f32(3)}, "particle pos");
  check(fx->field("vel")->lanes == std::vector<Raw>{f32(0.5f), 0, f32(-8)}, "particle vel");
  check(fx->field("backdate")->kind == fx::Field::Kind::Float && fx->field("backdate")->raw == 0, "particle backdate");

  auto glow = fx::to_primitive({"glow_add", {Value::vec({0, 0, f32(5)}), Value::integer(1), Value::integer(2),
                                             Value::integer(3), Value::integer(4), Value::integer(0x00D30000),
                                             Value::integer(0x100), Value::integer(6), Value::integer(f32(10)),
                                             Value::integer(f32(1408))}});
  check(glow->field("r")->raw == 1 && glow->field("flags")->raw == 6 && fl(glow->field("size")->raw) == 1408.0f,
        "glow fields");
  check(fl(glow->field("bias")->raw) == 10.0f, "glow bias");
  std::vector<Value> light_args{Value::integer(0x1000), Value::vec({0, 0, 0}), Value::integer(0), Value::integer(1)};
  for (int k = 0; k < 12; ++k) light_args.push_back(Value::integer(f32(static_cast<float>(k))));
  auto light = fx::to_primitive({"LightEffect_ctor", light_args});
  check(light->primitive == "light_effect", "light primitive");
  check(fl(light->field("c1")->raw) == 1.0f && fl(light->field("seconds")->raw) == 11.0f, "light fields");
  auto trail = fx::to_primitive({"trail_create", {Value::integer(1), Value::integer(0x7A), Value::integer(0),
                                                  Value::integer(0x80), Value::integer(0x80), Value::integer(0x80),
                                                  Value::integer(0x20), Value::integer(f32(0.25f))}});
  check(trail->field("row")->raw == 1 && trail->field("width")->raw == 0x7A && fl(trail->field("lifetime")->raw) == 0.25f,
        "trail fields");
  auto decal = fx::to_primitive({"decal_add", {Value::vec({0, 0, 0}), Value::integer(0x1234), Value::integer(9),
                                               Value::integer(0), Value::integer(f32(2))}});
  check(decal->field("type")->raw == 9 && fl(decal->field("size")->raw) == 2.0f, "decal fields");

  std::vector<fx::Callout> calls = {{"sound_playAtPos", {Value::integer(1), Value::vec({0, 0, 0}), Value::integer(0)}},
                                    {"fx_spawn", {Value::integer(8), Value::vec({0, 0, 0})}},
                                    {"obj_delete", {Value::integer(5)}}};
  const auto effects = fx::effects_of(calls);
  check(effects.size() == 1 && effects[0].primitive == "fx_spawn", "effects_of skips other calls");
  check(!fx::to_primitive({"damage_apply", {}}), "damage_apply is not a primitive");
  // every call-out is a call-out of some model (its EXTERNALS row exists in a port)
  check(fx::callouts().size() == 36, "36 call-outs");
  for (const auto &row : fx::callouts()) {
    bool any = false;
    for (auto port : {fx::Port::SkillsA, fx::Port::SkillsB, fx::Port::SkillsC, fx::Port::ArenaMonsters, fx::Port::FxSkills})
      any = any || fx::signature(port, row.name).has_value();
    check(any, "call-out has a model signature: " + std::string(row.name));
  }
  check(fx::find_callout("trail_setLength")->addr == 0x1911A8, "trail_setLength address");
  check(fx::find_callout("fx_ringBurst")->opaque, "ring burst opaque");
  // a short argument list decodes like the reference's zip; a wrong type refuses
  bool threw = false;
  try {
    fx::to_primitive({"particle_spawn", {Value::integer(1)}});
  } catch (const fx::DecodeError &) {
    threw = true;
  }
  check(threw, "malformed particle refuses");
  // the generic rows keep float arguments as integers, as the reference does
  auto shake = fx::to_primitive({"camera_shake", {Value::integer(0), Value::integer(f32(3))}});
  check(shake->primitive == "camera_shake" && shake->field("args")->other.items.size() == 2, "opaque args kept");
  auto matrix = fx::to_primitive({"particleGroup_setMatrix", {Value::integer(4), Value::str("stack")}});
  check(matrix->field("matrix")->kind == fx::Field::Kind::Other && matrix->field("matrix")->other.s == "stack",
        "the stack matrix token stays opaque");
}

// ---- the call logs of the native skill ports ------------------------------------------
void test_port_formats() {
  namespace sa = champions::simulation::rta::skills_a;
  namespace sb = champions::simulation::rta::skills_b;
  namespace sc = champions::simulation::rta::skills_c;
  namespace am = champions::simulation::rta::arena_monsters;
  const Raw def = 0x00A1F000, bd = f32(0.125f);
  auto check_particle = [&](const std::optional<fx::FxCall> &p, const std::string &port) {
    check(p && p->primitive == "particle" && p->field("definition")->raw == def &&
              p->field("pos")->lanes == std::vector<Raw>{f32(1), f32(2), f32(3)} &&
              p->field("vel")->lanes == std::vector<Raw>{f32(4), f32(5), f32(6)} && p->field("backdate")->raw == bd,
          port + ": particle_spawn decodes");
  };
  // skills_a: Call {slot, flat lanes, name}
  sa::Call a{0, {def, f32(1), f32(2), f32(3), f32(4), f32(5), f32(6), bd}, "particle_spawn"};
  check_particle(fx::to_primitive(fx::from_lanes(fx::Port::SkillsA, a.name, a.args)), "skills_a");
  sa::Call ag{0, {f32(1), f32(2), f32(3), 7, 8, 9, 10, 0x00D30100, 0, 2, f32(30), f32(400)}, "glow_add"};
  auto g = fx::to_primitive(fx::from_lanes(fx::Port::SkillsA, ag.name, ag.args));
  check(g && g->field("a")->raw == 10 && fl(g->field("size")->raw) == 400.0f, "skills_a: glow_add decodes");
  std::vector<std::uint32_t> lanes{0x2000, f32(1), f32(2), f32(3), 0, 1};
  for (int k = 0; k < 12; ++k) lanes.push_back(f32(static_cast<float>(k)));
  auto le = fx::to_primitive(fx::from_lanes(fx::Port::SkillsA, "LightEffect_ctor", lanes));
  check(le && le->fields.size() == 16 && fl(le->field("floor")->raw) == 10.0f, "skills_a: LightEffect_ctor decodes");
  std::vector<std::uint32_t> mat{7};
  for (int k = 0; k < 12; ++k) mat.push_back(f32(static_cast<float>(k)));
  auto ma = fx::to_primitive(fx::from_lanes(fx::Port::SkillsA, "particleGroup_setMatrix", mat));
  check(ma && ma->field("matrix")->lanes.size() == 12, "skills_a: the group matrix decodes");
  // skills_b: TypedCall {name, Argument variant}, scalar calls as creature_hp::Call
  sb::Model::TypedCall b{"particle_spawn", {def, sb::Model::Vec{f32(1), f32(2), f32(3)},
                                            sb::Model::Vec{f32(4), f32(5), f32(6)}, bd}};
  check_particle(fx::to_primitive(fx::from_typed(b)), "skills_b");
  champions::simulation::rta::Mat34 m34{};
  for (int k = 0; k < 12; ++k) m34[k] = f32(static_cast<float>(k));
  sb::Model::TypedCall bm{"particleGroup_setMatrix", {Raw(3), m34}};
  auto bmx = fx::to_primitive(fx::from_typed(bm));
  check(bmx && bmx->field("matrix")->lanes.size() == 12 && fl(bmx->field("matrix")->lanes[11]) == 11.0f,
        "skills_b: the group matrix decodes");
  sb::Call bl{"light_link", {0x1230}};
  auto bll = fx::to_primitive(fx::from_lanes(fx::Port::SkillsB, bl.name, bl.args));
  check(bll && bll->primitive == "light_link" && bll->field("light")->raw == 0x1230, "skills_b: scalar call decodes");
  // skills_c: Call {service number, flat lanes}; NamedCall with structured values
  sc::Call c{sc::ParticleSpawn, {def, f32(1), f32(2), f32(3), f32(4), f32(5), f32(6), bd}};
  const auto cname = fx::skills_c_service_name(c.slot);
  check(cname && *cname == "particle_spawn", "skills_c: service name");
  check_particle(fx::to_primitive(fx::from_lanes(fx::Port::SkillsC, *cname, c.args)), "skills_c");
  sc::Call cm{sc::ParticleGroupSetMatrix, {5, 0}};
  auto cmx = fx::to_primitive(fx::from_lanes(fx::Port::SkillsC, *fx::skills_c_service_name(cm.slot), cm.args));
  check(cmx && cmx->field("matrix")->other.s == "stack", "skills_c: the stack matrix token");
  check(!fx::skills_c_service_name(sc::DamageApply), "skills_c: damage is not a call-out");
  sc::ResultValue rv{sc::ResultValue::Scalar, def, {}};
  sc::ResultValue rvec{sc::ResultValue::Tuple, 0, {{sc::ResultValue::Scalar, f32(1), {}},
                                                   {sc::ResultValue::Scalar, f32(2), {}},
                                                   {sc::ResultValue::Scalar, f32(3), {}}}};
  sc::ResultValue rvel{sc::ResultValue::Tuple, 0, {{sc::ResultValue::Scalar, f32(4), {}},
                                                   {sc::ResultValue::Scalar, f32(5), {}},
                                                   {sc::ResultValue::Scalar, f32(6), {}}}};
  sc::NamedCall nc{0, "particle_spawn", {rv, rvec, rvel, {sc::ResultValue::Scalar, bd, {}}}};
  fx::Callout ncall{nc.name, {}};
  for (const auto &x : nc.args) ncall.args.push_back(fx::value_from_result(x));
  check_particle(fx::to_primitive(ncall), "skills_c named");
  // arena_monsters: Call {name, Value variant}
  am::Call d{"particle_spawn", {std::int64_t{def}, am::Vec3{f32(1), f32(2), f32(3)}, am::Vec3{f32(4), f32(5), f32(6)},
                                std::int64_t{bd}}};
  check_particle(fx::to_primitive(fx::from_typed(d)), "arena_monsters");
  // a flat call of a name the port never records refuses
  bool threw = false;
  try {
    fx::from_lanes(fx::Port::ArenaMonsters, "glow_add", {});
  } catch (const fx::DecodeError &) {
    threw = true;
  }
  check(threw, "arena_monsters has no glow_add");
}

// ---------------------------------------------------------------------------------------
// part 2: recipes (test_ref_fx_skills.Recipes, docs/effects.md 15.2 / 15.4)
// ---------------------------------------------------------------------------------------
void test_recipes() {
  const auto &rows = fx::recipes();
  check(rows.size() == 161, "161 recipe rows");
  std::set<std::uint32_t> addrs;
  std::map<fx::Phase, int> phases;
  std::map<unsigned, int> gens;
  int native = 0;
  for (const auto &r : rows) {
    addrs.insert(r.addr);
    ++phases[r.phase];
    check(r.cls == fx::class_of(r.function), "class is the function prefix: " + std::string(r.function));
    const auto [module, method] = fx::model_method(r);
    if (fx::natively_modelled(r)) {
      ++native;
      const auto &names = fx::Model::methods();
      check(std::find(names.begin(), names.end(), method) != names.end(), "native method exists: " + std::string(method));
    } else {
      check(module == "ref_skills_a" || module == "ref_skills_b" || module == "ref_skills_c" ||
                module == "ref_arena_monsters",
            "model module: " + std::string(r.model));
    }
    const unsigned g = fx::generators(r);
    for (unsigned bit = 1; bit <= 32; bit <<= 1)
      if (g & bit) ++gens[bit];
    check(fx::must_run_headless(r) == bool(g & (fx::Own | fx::SpellGen | fx::Shared | fx::SharedLightEffect)),
          "headless rule");
  }
  check(addrs.size() == 161, "161 distinct addresses");
  check(native == 21, "21 rows modelled natively");
  check(phases[fx::Phase::Cast] == 71 && phases[fx::Phase::Run] == 69 && phases[fx::Phase::Draw] == 7 &&
            phases[fx::Phase::Event] == 14,
        "phase counts 71 / 69 / 7 / 14 (15.4)");
  check(gens[fx::Own] == 32 && gens[fx::SpellGen] == 49 && gens[fx::Shared] == 77 && gens[fx::SharedLightEffect] == 40 &&
            gens[fx::Cosmetic] == 3 && gens[fx::Seed3] == 3,
        "generator counts 32 / 49 / 77 / 40 / 3 / 3 (15.2)");
  const auto mf = fx::recipe("MultiFireProjectile");
  check(mf.size() == 3 && mf[0].phase == fx::Phase::Cast && mf[1].phase == fx::Phase::Run && mf[2].phase == fx::Phase::Event,
        "MultiFireProjectile phases");
  const auto cls = fx::classes();
  check(cls.size() == 91, "91 classes");
  check(std::find(cls.begin(), cls.end(), "StaticFire") != cls.end(), "StaticFire is a class");
  check(fx::recipe("NoSuchClass").empty(), "no such class");
  check(fx::static_calls(*fx::recipe_at(0x2D33C0)) == std::map<std::string, unsigned>{{"decal", 1}, {"particle", 5}},
        "static calls parse");
  check(!fx::must_run_headless(*fx::recipe_at(0x2C4E64)) && fx::must_run_headless(*fx::recipe_at(0x2CE6E0)),
        "a draw slot is cosmetic, DiseaseCloud_run is not");
}

// ---------------------------------------------------------------------------------------
// the recipe constants (test_fx_skill_recipes) and the loader's refusals
// ---------------------------------------------------------------------------------------
std::string replace_arg(std::string text, std::uint32_t site, const std::string &key, const std::string &value) {
  const auto at = text.find("\"site\": " + std::to_string(site) + "\n");
  const auto start = text.rfind("\"address\":", at);
  const auto k = text.find("\"" + key + "\": ", start);
  if (at == std::string::npos || k == std::string::npos || k > at) throw std::runtime_error("fixture layout");
  const auto v = k + key.size() + 4;
  const auto e = text.find_first_of(",\n", v);
  return text.substr(0, v) + value + text.substr(e);
}

void test_constants(const std::string &json, const fx::RecipeConstants &k) {
  check(fx::class_of("SpellHealing_run") == "SpellHealing" && fx::class_of("").empty(), "class_of");
  check(fx::meaning("glow", "t0") == "alpha" && fx::meaning("particle", "a0") == "definition", "meanings");
  check(k.range_lo == 0x2C3CD4 && k.range_hi == 0x320000, "range");
  // particle_definitions: sorted unique constant definitions
  std::set<std::uint32_t> defs;
  for (const auto &c : k.calls())
    if (c.primitive == "particle")
      if (auto v = c.arg("a0")) defs.insert(*v);
  check(std::vector<std::uint32_t>(defs.begin(), defs.end()) == k.particle_definitions, "definitions");
  // the text line: meaning=value, ? for null
  fx::SiteCall sc;
  sc.function = "Foo_draw";
  sc.site = 0x2D0000;
  sc.primitive = "glow";
  sc.args = {{"f12", f32(10)}, {"f13", std::nullopt}, {"a0", std::nullopt}, {"a1", 0x31}, {"a2", 0x32},
             {"a3", 0x33},     {"t0", 0x34},          {"t1", std::nullopt}, {"t2", 0}, {"t3", 6}};
  const auto line = sc.describe();
  check(line.find("alpha=0x34") != std::string::npos && line.find("size=?") != std::string::npos &&
            line.find("bias=10") != std::string::npos && line.find("angle=0x0") != std::string::npos,
        "describe: " + line);
  // sites in ascending address order, needed values as loaded
  const auto glows = k.sites(0x2C65E8, "glow");
  check(glows.size() == 2 && glows[0]->site < glows[1]->site, "StaticFire_draw glow sites in order");
  check(k.needed(0x2C65E8, "glow", 1, "t3") == *glows[1]->arg("t3"), "needed value of site 1");
  check(refuses([&] { k.needed(0x2C65E8, "glow", 1, "a1"); }), "a computed argument is not a needed constant");
  check(k.light_record("StaticFire_ctor") && k.light_record("StaticFire_ctor")->falloff, "light records loaded");

  // refusals
  const auto mf_site = k.sites(0x2DA2BC, "particle")[0]->site;
  auto refused = [&](const std::string &text, const std::string &what, const std::string &expect) {
    const auto why = refusal_text([&] { fx::RecipeConstants::from_json(text); });
    check(!why.empty() && why.find(expect) != std::string::npos, "loader refuses " + what + " (" + why + ")");
  };
  check(!refuses([&] { fx::RecipeConstants::from_json(json); }), "the fixture loads");
  refused(replace_arg(json, mf_site, "a0", "null"), "a null needed constant", "null");
  refused(replace_arg(json, mf_site, "f12", "NaN"), "a NaN constant", "NaN");
  refused(replace_arg(json, mf_site, "f12", "1"), "an integer where a float is", "float expected");
  refused(replace_arg(json, mf_site, "f12", "1.1"), "a double", "single-precision");
  const auto ring = k.sites(0x2D33C0, "particle")[3]->site;
  refused(replace_arg(json, ring, "a0", "12345"), "two sites of one class constant disagreeing", "differ");
  {
    std::string t = json;
    const auto r = t.find("3276800");
    t.replace(r, 7, "3276804");
    refused(t, "another range", "range");
  }
  {
    std::string t = json;
    t.insert(t.find('{') + 1, "\n \"extra\": 1,");
    refused(t, "an unknown key", "unexpected top-level key");
  }
  {
    std::string t = json;
    const auto r = t.find("\"calls_in_code\": \"glow:1\"");
    t.replace(r, std::strlen("\"calls_in_code\": \"glow:1\""), "\"calls_in_code\": \"glow:2\"");
    refused(t, "a recipe row that differs", "recipe table");
  }
  {
    // a call moved to another function of the table: counts no longer match
    std::string t = json;
    const auto at = t.find("\"site\": " + std::to_string(mf_site) + "\n");
    const auto start = t.rfind("\"address\": ", at);
    const auto e = t.find(',', start);
    t.replace(start, e - start, "\"address\": " + std::to_string(0x2DA164));
    refused(t, "a call outside its counted function", "");
  }
  // without light_records the constructors that need them refuse; the rest runs
  std::string t = json;
  const auto lr = t.find("\"light_records\": {");
  std::size_t e = lr + std::strlen("\"light_records\": {");
  for (int depth = 1; depth; ++e) depth += t[e] == '{' ? 1 : t[e] == '}' ? -1 : 0;
  t.erase(lr, e - lr + 2); // the member and its comma
  const auto nolight = fx::RecipeConstants::from_json(t);
  fx::Model m(nolight);
  check(refuses([&] { m.staticfire_ctor(0x8000, {0, 0, 0}, 1, 0, 0, f32(240), f32(1)); }),
        "StaticFire_ctor refuses without light_records");
  check(refuses([&] { m.planarsteel_ctor(0x8000, 1, 0x9000, 0); }), "SpellPlanarSteelEffect_ctor refuses too");
  fx::Model m2(nolight);
  m2.mem.poke(0x9010, 4, f32(2));
  check(!refuses([&] { m2.firearrow_draw(0x9000); }), "the other functions run without light_records");
}

// ---------------------------------------------------------------------------------------
// part 3 (test_ref_fx_skills.Draws / Objects) with the fixture's stand-ins
// ---------------------------------------------------------------------------------------
void test_model(const fx::RecipeConstants &k) {
  using champions::rta::fx::skills::Model;
  {
    Model m(k);
    put_vec(m, 0x1010, 1, 2, 3);
    m.mem.poke(0x101C, 4, f32(2));
    m.mem.poke(0x4F8494, 4, 0xABC);
    m.firearrow_draw(0x1000);
    const auto a = args_named(m, "glow_add");
    const auto K = [&](const char *key) { return k.needed(0x2D67E8, "glow", 0, key); };
    check(a.size() == 10 && a[1].i == K("a1") && a[2].i == K("a2") && a[3].i == K("a3") && a[4].i == K("t0") &&
              a[5].i == 0xABC && a[6].i == K("t2") && a[7].i == K("t3") && a[8].i == K("f12"),
          "firearrow glow arguments");
    check(fl(static_cast<Raw>(a[9].i)) == 2816.0f, "firearrow glow size radius x 1408");
  }
  {
    Model m(k);
    put_vec(m, 0x1010, 10, 20, 30);
    m.mem.poke(0x106C, 4, f32(1)); // direction (1, 0): the glow sits at -y
    m.holystrike_draw(0x1000);
    const auto a = args_named(m, "glow_add");
    check(a.size() == 10 && fl(static_cast<Raw>(a[0].items[0].i)) == 10.0f && fl(static_cast<Raw>(a[0].items[1].i)) == 19.0f &&
              fl(static_cast<Raw>(a[0].items[2].i)) == 31.0f,
          "holy strike glow beside and above");
    check(a[7].i == k.needed(0x2ECD90, "glow", 0, "t3"), "holy strike flags");
  }
  {
    Model m(k);
    m.mem.poke(0x2000 + 0x18, 4, 0x3000);
    put_vec(m, 0x3010, 0, 0, 0);
    m.mem.poke(0x4D8AE0, 4, 2);
    m.resurrect_draw(0x2000);
    check(count_named(m, "glow_add") == 0, "resurrect ring not drawn with two players");
    m.mem.poke(0x4D8AE0, 4, 1);
    m.resurrect_draw(0x2000);
    check(count_named(m, "glow_add") == 13, "resurrect ring of 13");
    for (std::size_t g = 0; g < 13; ++g) {
      const auto a = args_named(m, "glow_add", g);
      const float x = fl(static_cast<Raw>(a[0].items[0].i)), y = fl(static_cast<Raw>(a[0].items[1].i));
      check(std::fabs(x * x + y * y - 225.0f) <= 0.5f && fl(static_cast<Raw>(a[0].items[2].i)) == 116.0f && a[4].i == 128,
            "resurrect ring radius 15, 116 up, full alpha");
    }
  }
  {
    Model m(k);
    m.handlers["isNearView"] = [](Model &, const fx::Callout &) { return Value::integer(1); };
    const fx::Addr obj = 0x4000;
    m.mem.poke(obj + 0x438, 2, 98);
    m.mem.poke(obj + 0x43A, 2, 2);
    m.mem.poke(obj + 0x464, 4, 1);
    m.mem.poke(obj + 0x474, 4, f32(1));
    m.smokegen_draw(obj);
    check(count_named(m, "glow_add") == 4, "smoke ring wraps at 100");
  }
  for (const auto &[flags, site] : std::vector<std::pair<Raw, unsigned>>{{0, 1}, {0x40000, 0}}) {
    Model m(k);
    m.handlers["libc_sprintf"] = [](Model &, const fx::Callout &) {
      return Value::tuple({Value::str("buf"), Value::bytes("x")});
    };
    m.handlers["res_find"] = [](Model &, const fx::Callout &) { return Value::integer(1); };
    m.mem.poke(0x44FE08, 4, flags);
    m.multifire_end(0x5000, 1);
    const auto a = args_named(m, "fx_spawn");
    check(!a.empty() && a[0].i == k.needed(0x2DA928, "fx_spawn", site, "a0"), "multifire burst by the level flag");
    check(m.u32(0x5060) == 1, "multifire ended");
    m.calls.clear();
    m.multifire_end(0x5000, 1);
    check(m.calls.empty(), "multifire ends once");
  }
  {
    Model m(k);
    m.handlers["obj_moveAndCollide"] = [](Model &, const fx::Callout &) { return Value::integer(0); };
    m.mem.poke(0x5044, 4, 12345);
    m.multifire_run(0x5000);
    check(count_named(m, "particle_spawn") == 10, "multifire ten particles");
    check(m.u32(0x5044) != 12345 && m.u32(0x506C) == 1, "multifire own generator and age");
    for (std::size_t p = 0; p < 10; ++p) {
      const auto a = args_named(m, "particle_spawn", p);
      float s = 0;
      for (const auto &lane : a[2].items) s += fl(static_cast<Raw>(lane.i)) * fl(static_cast<Raw>(lane.i));
      check(a[0].i == k.needed(0x2DA2BC, "particle", 0, "a0") && s <= 64.0001f, "multifire particle");
    }
  }
  {
    Model m(k);
    const fx::Addr obj = 0x6000;
    put_vec(m, obj + 0x10, 0, 0, 100);
    m.mem.poke(obj + 0x54, 4, f32(-50));
    m.pustule_run(obj);
    check(count_named(m, "particle_spawn") == 3, "pustule three dribble particles");
    for (std::size_t p = 0; p < 3; ++p) check(args_named(m, "particle_spawn", p)[2] == Value::vec({0, 0, 0}), "still");
    check(m.u32(obj + 0x64) == 1, "pustule timer");
  }
  {
    const Raw victim = 0x7100;
    Model m(k);
    m.handlers["world_collectObjectsInBox"] = [&](Model &, const fx::Callout &) {
      return Value::list({Value::integer(victim)});
    };
    const fx::Addr obj = 0x7000, node = 0x7200;
    m.mem.poke(obj + 0x54, 4, 1000);
    m.mem.poke(obj + 0x58, 4, 0);
    m.mem.poke(obj + 0x60, 4, 7);
    m.mem.poke(obj + 0x4C, 4, node);
    m.mem.poke(node + 0xC, 4, 0x7300);
    m.mem.poke(0x44FDCC, 4, 60);
    m.diseasecloud_run(obj);
    const auto a = args_named(m, "damage_apply");
    check(a.size() == 10 && a[0].i == victim && a[1].i == 0, "disease cloud hurts with no attacker");
    check(a[6].items.size() == 6 && a[6].items[2] == Value::tuple({Value::integer(0x26), Value::integer(7),
                                                                   Value::integer(7), Value::integer(0)}),
          "disease entry");
  }
  {
    Model m(k);
    const fx::Addr obj = 0x8000, ring = 0x9000;
    m.mem.poke(obj + 0x4A4, 4, ring);
    m.mem.poke(obj + 0x76, 2, 3); // a ring of three points holds two
    m.mem.poke(obj + 0x1C, 4, f32(16));
    m.mem.poke(obj + 0x64, 4, f32(1));
    m.mem.poke(obj + 0x58, 4, f32(20));
    for (int n = 0; n < 3; ++n) m.staticfire_add_flame(obj);
    check(m.mem.load(obj + 0x78, 2) == 2 && m.mem.load(obj + 0x7A, 2) == 0, "flame ring holds two");
    for (int n = 0; n < 3; ++n) m.staticfire_move_flames(obj);
    check(static_cast<std::int16_t>(m.mem.load(ring + 4, 2)) >= 21, "flames rose 21");
  }
  check(fl(fx::from_uint(5)) == 5.0f && fl(fx::from_uint(0xFFFFFFFF)) == 4294967040.0f, "from_uint toward zero");
  {
    Model m(k);
    m.staticfire_ctor(0x8000, {f32(1), f32(2), f32(3)}, 1, 0, 0, f32(240), f32(1));
    const auto *fire = k.light_record("StaticFire_ctor");
    bool colours = fire != nullptr;
    for (int i = 0; colours && i < 8; ++i)
      colours = m.mem.load(0x8000 + 0x80 + 0xA + 2 * i, 2) == fire->colour[static_cast<std::size_t>(i)];
    check(colours && fire->falloff && m.mem.load(0x8000 + 0x80 + 0x20, 4) == *fire->falloff,
          "StaticFire_ctor stores its light record");
    Model steel(k);
    steel.mem.poke(0x9000 + 0x10, 4, f32(4));
    steel.mem.poke(0x9000 + 0x20, 4, f32(8));
    steel.planarsteel_ctor(0xA000, 1, 0x9000, 0);
    const auto *planar = k.light_record("SpellPlanarSteelEffect_ctor");
    colours = planar != nullptr && !planar->falloff;
    for (int i = 0; colours && i < 8; ++i)
      colours = steel.mem.load(0xA000 + 0x30 + 0xA + 2 * i, 2) == planar->colour[static_cast<std::size_t>(i)];
    check(colours, "SpellPlanarSteelEffect_ctor stores its light record");
  }
  {
    Model m(k);
    const fx::Addr spell = 0x8000, owner = 0x9000;
    m.mem.poke(0x44FDCC, 4, 10);
    m.mem.poke(spell + 0x18, 4, 1000);
    m.mem.poke(spell + 0xC, 4, owner);
    m.mem.poke(spell + 0x1C, 4, 7);
    m.mem.poke(owner + 0x10, 4, f32(1));
    m.mem.poke(owner + 0x14, 4, f32(2));
    m.mem.poke(owner + 0x18, 4, f32(3));
    m.mem.poke(owner + 0x38, 2, 0x1234);
    m.hornomen_run(spell, {0, 0, 0});
    const auto placed = champions::simulation::rta::mat34_set_pos_rot_z(
        {f32(1), f32(2), f32(3)}, 0x1234);
    bool found = false;
    for (const auto &c : m.calls) {
      if (c.name != "particleGroup_setMatrix") continue;
      found = c.args.size() == 2 && c.args[0].i == 7 && c.args[1].items.size() == 12;
      for (int i = 0; found && i < 12; ++i)
        found = c.args[1].items[static_cast<std::size_t>(i)].i == placed[static_cast<std::size_t>(i)];
    }
    check(found, "HornOmen group matrix is mat34_set_pos_rot_z");
  }
  {
    // unknown virtual bodies refuse; a missing handler for a name with no reference row refuses
    Model m(k);
    m.mem.poke(0xA000 + 0x48, 4, 0xB000);
    m.mem.poke(0xB000 + 8 * 1 + 4, 4, 0x27236C); // a class body the reference runs (horsemen_run)
    m.mem.poke(0xA000 + 0x50, 4, 1);              // landed
    m.mem.poke(0xA000 + 0x58, 2, 0x7FFF);         // ring complete
    check(refuses([&] { m.firestormflare_run(0xA000); }), "virtual body not modelled natively refuses");
  }
}

// ---------------------------------------------------------------------------------------
// the vectors
// ---------------------------------------------------------------------------------------
void replay(const std::string &dir, const fx::RecipeConstants &k) {
  std::ifstream in(dir + "/fx_skills.tsv");
  if (!in) {
    check(false, "cannot read " + dir + "/fx_skills.tsv");
    return;
  }
  std::map<std::string, std::pair<int, int>> per; // method -> (cases, mismatches)
  std::size_t calls_total = 0, effects_total = 0, decode_bad = 0;
  std::string line;
  int shown = 0, needs_lines = 0;
  while (std::getline(in, line)) {
    if (line.rfind("# needs ", 0) == 0) { // the generator's site table must be this module's needs()
      std::set<std::string> want, have;
      for (const auto &item : split(line.substr(8), ';')) want.insert(item);
      for (const auto &n : fx::needs()) {
        std::ostringstream os;
        os << std::hex << n.function << '/' << n.primitive << '/' << std::dec << n.ordinal << ':';
        for (std::size_t j = 0; j < n.keys.size(); ++j) os << (j ? "," : "") << n.keys[j];
        have.insert(os.str());
      }
      check(want == have, "the generator's site table equals fx::needs()");
      ++needs_lines;
      continue;
    }
    if (line.empty() || line[0] == '#') continue;
    const auto col = split(line, '\t');
    if (col.size() != 7) {
      check(false, "vector row with " + std::to_string(col.size()) + " columns");
      continue;
    }
    auto &tally = per[col[0]];
    ++tally.first;
    std::string why;
    try {
      fx::Model m(k);
      if (col[2] != "-")
        for (const auto &run : split(col[2], ';')) {
          const auto colon = run.find(':');
          const auto addr = static_cast<fx::Addr>(std::stoull(run.substr(0, colon), nullptr, 16));
          const auto hex = run.substr(colon + 1);
          for (std::size_t b = 0; b * 2 < hex.size(); ++b)
            m.mem.bytes[addr + static_cast<fx::Addr>(b)] = static_cast<std::uint8_t>(std::stoul(hex.substr(2 * b, 2), nullptr, 16));
        }
      const auto answers = col[3] == "." ? std::vector<std::string>{} : split(col[3], ';');
      std::size_t next = 0;
      m.fallback = [&](fx::Model &, const fx::Callout &c) -> std::optional<Value> {
        if (next >= answers.size()) throw std::runtime_error("more calls than answers at " + c.name);
        const auto &a = answers[next++];
        if (a == "-") return std::nullopt;
        return Value::parse(a);
      };
      const Value args = Value::parse(col[1]);
      m.invoke(col[0], args.items);
      auto want_calls = col[4] == "-" ? std::vector<std::string>{} : split(col[4], ';');
      if (col[0] == "hornomen_run") {
        const fx::Addr spell = static_cast<fx::Addr>(args.items.at(0).i);
        const fx::Addr owner = static_cast<fx::Addr>(m.mem.load(spell + 0xC, 4));
        const auto placed = champions::simulation::rta::mat34_set_pos_rot_z(
            {static_cast<Raw>(m.mem.load(owner + 0x10, 4)),
             static_cast<Raw>(m.mem.load(owner + 0x14, 4)),
             static_cast<Raw>(m.mem.load(owner + 0x18, 4))},
            static_cast<std::uint32_t>(m.mem.load(owner + 0x38, 2)));
        std::vector<Value> lanes;
        for (const Raw lane : placed) lanes.push_back(Value::integer(lane));
        const std::string matrix = Value::tuple(std::move(lanes)).serialize();
        for (auto &want : want_calls) {
          const auto at = want.find("'stack");
          if (at != std::string::npos) want.replace(at, 6, matrix);
        }
      }
      if (want_calls.size() != m.calls.size())
        why = "calls " + std::to_string(m.calls.size()) + " want " + std::to_string(want_calls.size());
      for (std::size_t c = 0; why.empty() && c < want_calls.size(); ++c)
        if (m.calls[c].serialize() != want_calls[c])
          why = "call " + std::to_string(c) + ": " + m.calls[c].serialize() + " want " + want_calls[c];
      if (why.empty() && next != answers.size()) why = "answers left over";
      if (why.empty()) {
        const auto want = col[5] == "-" ? std::vector<std::string>{} : split(col[5], ';');
        if (want.size() != m.writes.size())
          why = "writes " + std::to_string(m.writes.size()) + " want " + std::to_string(want.size());
        std::uint64_t la = 0, ls = 0;
        for (std::size_t w = 0; why.empty() && w < want.size(); ++w) {
          std::uint64_t a = la, s = ls, v;
          if (want[w][0] == '=') v = std::stoull(want[w].substr(1), nullptr, 16);
          else {
            const auto p = split(want[w], ',');
            a = std::stoull(p[0], nullptr, 16);
            s = std::stoull(p[1], nullptr, 16);
            v = std::stoull(p[2], nullptr, 16);
          }
          la = a;
          ls = s;
          const auto &got = m.writes[w];
          if (static_cast<std::uint64_t>(got.addr) != a || got.size != s || got.value != v) {
            std::ostringstream os;
            os << "write " << w << ": " << std::hex << got.addr << "," << got.size << "," << got.value << " want "
               << want[w];
            why = os.str();
          }
        }
      }
      if (why.empty() && m.draws != std::stoull(col[6], nullptr, 16)) why = "shared draws";
      // every recorded call-out decodes, field for field
      for (const auto &c : m.calls) {
        ++calls_total;
        try {
          if (auto fxc = fx::to_primitive(c)) {
            ++effects_total;
            const auto *row = fx::find_callout(c.name);
            if (!row->opaque && fxc->fields.size() != row->fields.size()) ++decode_bad;
          }
        } catch (const fx::DecodeError &) {
          ++decode_bad;
        }
      }
    } catch (const std::exception &e) {
      why = std::string("exception: ") + e.what();
    }
    if (!why.empty()) {
      ++tally.second;
      ++failures;
      if (shown++ < 8) std::cerr << "MISMATCH " << col[0] << " (case " << tally.first << "): " << why << "\n";
    }
  }
  int cases = 0, bad = 0;
  for (const auto &[method, t] : per) {
    std::cout << "  " << method << ": " << t.first << " cases, " << t.second << " mismatches\n";
    cases += t.first;
    bad += t.second;
  }
  std::cout << "vectors: " << cases << " cases over " << per.size() << " methods, " << bad << " mismatches; "
            << calls_total << " call-outs, " << effects_total << " decoded as primitives, " << decode_bad
            << " decode failures\n";
  check(per.size() == fx::Model::methods().size(), "every method has vectors");
  check(needs_lines == 1, "the vectors carry the site table");
  check(decode_bad == 0, "every call-out of the vectors decodes");
}
} // namespace

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "usage: champions-fx-skills-tests <native/tests/vectors>\n";
    return 2;
  }
  const std::string dir = argv[1];
  try {
    const std::string json = read_file(dir + "/../fixtures/fx_skill_recipes_synthetic.json");
    const auto constants = fx::RecipeConstants::from_json(json);
    test_callouts();
    test_port_formats();
    test_recipes();
    test_constants(json, constants);
    test_model(constants);
    replay(dir, constants);
  } catch (const std::exception &e) {
    std::cerr << "FAIL: " << e.what() << "\n";
    return 1;
  }
  std::cout << (failures ? "FAILED: " : "ok: ") << failures << " failures\n";
  return failures ? 1 : 0;
}
