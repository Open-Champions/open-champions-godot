#include "champions/core_math.hpp"
#include "champions/arena_executable.hpp"
#include "champions/objects.hpp"
#include "champions/session_level_flow.hpp"
#include "../tools/level_spawn_lmp.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <sstream>
using namespace champions;
using namespace champions::simulation::rta;
using namespace champions::simulation::rta::level_flow;
namespace {
using Vec3 = champions::simulation::rta::level_flow::Vec3;
unsigned checks = 0;
#define CHECK(...)                                                             \
  do {                                                                         \
    ++checks;                                                                  \
    if (!(__VA_ARGS__))                                                        \
      throw Error(std::string("line ") + std::to_string(__LINE__) + ": " +     \
                  #__VA_ARGS__);                                               \
  } while (false)
template <class F> void refuses(F f) {
  bool caught = false;
  try {
    f();
  } catch (const std::exception &) {
    caught = true;
  }
  CHECK(caught);
}
Raw f(float v) {
  Raw r;
  std::memcpy(&r, &v, 4);
  return r;
}
Vec3 v(float x, float y, float z) { return {f(x), f(y), f(z)}; }
Record record(std::string cls, std::vector<std::string> props = {},
              Vec3 pos = {}, unsigned angle = 0) {
  return {std::move(cls), std::uint16_t(angle), pos, std::move(props)};
}
void put(Blob &b, std::size_t p, std::uint32_t x, unsigned n = 4) {
  if (b.size() < p + n)
    b.resize(p + n);
  for (unsigned i = 0; i < n; ++i)
    b[p + i] = std::uint8_t(x >> (i * 8));
}
Blob objects(const std::vector<Record> &rs) {
  Blob b(8), strings{0};
  put(b, 0, std::uint32_t(rs.size()), 2);
  auto intern = [&](const std::string &s) {
    auto p = std::uint32_t(strings.size());
    strings.insert(strings.end(), s.begin(), s.end());
    strings.push_back(0);
    return p;
  };
  for (auto &r : rs) {
    auto p = b.size();
    b.resize(p + 24 + r.props.size() * 4);
    put(b, p, intern(r.cls));
    put(b, p + 4, std::uint32_t(24 + r.props.size() * 4), 2);
    put(b, p + 6, r.angle, 2);
    for (unsigned i = 0; i < 3; ++i)
      put(b, p + 8 + 4 * i, r.position[i]);
    for (std::size_t i = 0; i < r.props.size(); ++i)
      put(b, p + 20 + 4 * i, intern(r.props[i]));
  }
  put(b, 4, std::uint32_t(b.size()));
  b.insert(b.end(), strings.begin(), strings.end());
  return b;
}
Blob lump(const std::vector<std::pair<std::string, Blob>> &entries) {
  Blob b(4 + 12 * entries.size());
  put(b, 0, std::uint32_t(entries.size()));
  for (std::size_t i = 0; i < entries.size(); ++i) {
    put(b, 4 + 12 * i, std::uint32_t(b.size()));
    b.insert(b.end(), entries[i].first.begin(), entries[i].first.end());
    b.push_back(0);
    put(b, 8 + 12 * i, std::uint32_t(b.size()));
    put(b, 12 + 12 * i, std::uint32_t(entries[i].second.size()));
    b.insert(b.end(), entries[i].second.begin(), entries[i].second.end());
  }
  return b;
}
Blob gob(const std::vector<std::pair<std::string, Blob>> &entries) {
  Blob b(0x800);
  for (std::size_t i = 0; i < entries.size(); ++i) {
    CHECK(entries[i].first.size() < 32);
    std::copy(entries[i].first.begin(), entries[i].first.end(),
              b.begin() + 40 * i);
    put(b, 40 * i + 32, 0xdeadbeef);
    put(b, 40 * i + 36, std::uint32_t(entries[i].second.size()));
    b.insert(b.end(), entries[i].second.begin(), entries[i].second.end());
    b.resize((b.size() + 0x7ff) & ~std::size_t(0x7ff));
  }
  return b;
}
Acts acts{{{"hub", 100}, {"aa", 50}, {"aab", 50}}, {{"hub", 100}, {"bb", 70}}};
struct SpawnCase {
  SpawnState spawn;
  unsigned calls = 0;
  std::vector<unsigned> built;
};
SpawnCase vector_spawn_case(unsigned n, bool recall, bool saved, bool gate,
                            int medal) {
  SpawnCase out;
  unsigned draw = 1000;
  auto spot = [&](const Vec3 &c, Raw, Raw, bool) {
    ++out.calls;
    return Vec3{add(c[0], from_int(out.calls)), c[1], c[2]};
  };
  auto rs =
      std::vector<Record>{record("Player1", {}, v(1, 2, 3), 100),
                          record("Player1", {"default=1"}, v(4, 5, 6), 65530),
                          record("Portal", {}, v(7, 8, 9)),
                          record("Player1", {"name=door"}, v(10, 11, 12))};
  run_markers(
      out.spawn, rs, "old", "door", recall, spot,
      [&] { return std::uint16_t(draw++); }, saved, gate, n);
  Placement p;
  p.players = n;
  p.alive[1] = false;
  if (medal >= 0)
    p.medal_slot = unsigned(medal);
  out.built =
      place_players(out.spawn, p, [](Raw, const Vec3 &) { return f(4); }, spot);
  return out;
}
std::optional<std::string> manifest_world(std::vector<std::string> entries) {
  ManifestInput m;
  m.base = "leva";
  m.gob_lumps = {"leva.lmp", "waterenv.lmp"};
  m.level_entries = std::move(entries);
  m.disc_files = {"LEVA.GOB",  "LEVA.TEX",     "LOADMISC.LMP",
                  "FX.LMP",    "HUD.LMP",      "MAPICONS.LMP",
                  "ICONS.LMP", "GOODDOG.LMP", "SKELE_B.LMP"};
  try {
    for (auto &r : level_manifest(m))
      if (r.step == "8")
        return r.name;
  } catch (const MissingResource &) {
  }
  return {};
}
struct Round4Hit {
  Vec3 pos{};
  int ok = 0;
  Raw snap = 0;
  bool operator==(const Round4Hit &other) const {
    return pos == other.pos && ok == other.ok && snap == other.snap;
  }
};
world_queries::Level flat_field(std::int64_t, std::int64_t, std::int64_t,
                                std::int64_t, std::int64_t, std::int64_t = 0,
                                std::int64_t = 0, std::int64_t = 0);
Round4Hit round4_solid(std::uint32_t seed, bool with_object);
Round4Hit round4_hole(std::uint32_t seed);
int round4_solid_failures();
int round4_hole_kept();
int round4_hole_lost();
std::pair<int, int> round4_lattice();
StoryGround round4_arena_pre_search(const std::vector<Record> &);
StoryGround round4_arena_wave(const std::vector<Record> &, std::uint32_t seed);
std::vector<int> arena_mode_a_cycle(int saved);
void reference_vectors() {
#include "level_flow_vectors.inc"
}
void reference_cases() {
  CHECK(split_request("alpha_beta_gamma") ==
        std::make_pair(std::string("alpha"), std::string("beta_gamma")));
  CHECK(split_request("alpha").second.empty());
  CHECK(qualified_name("alpha", "") == "alpha");
  CHECK(qualified_name("alpha", "b") == "alpha_b");
  CHECK(loading_picture_files("abcdefghij") ==
        std::vector<std::string>({"labcdefg.lmp", "lmiss.lmp"}));
  CHECK(loading_picture_files("ab")[0] == "lab.lmp");
  CHECK(loading_picture_alias("xbloy") == "blood1");
  CHECK(loading_picture_alias("tormentb") == "tormb");
  CHECK(loading_picture_alias("tormentq") == "tormentq");
  CHECK(loading_picture_alias("nitdung2") == "nitdun2");
  CHECK(loading_picture_alias("plain") == "plain");
  CHECK(cd_file_name("lab.lmp") == "LAB.LMP;1");
  std::vector<LevelRow> rows(4);
  rows[0].name = "aa2";
  rows[0].monster_level = 5;
  rows[1].name = "aa";
  rows[2].name = "aa_x";
  rows[2].monster_level = 9;
  rows[3].name = "zz";
  CHECK(find_level_row(rows, "aa", "x").monster_level == 9);
  CHECK(find_level_row(rows, "aa", "").name == "aa2");
  CHECK(find_level_row(rows, "xzzx", "").name == "zz");
  auto def = find_level_row(rows, "qq", "");
  CHECK(def.water_pool == 48);
  CHECK(def.kill_plane == f(-100000));
  LevelRow row;
  row.monster_level = 4;
  CHECK(monster_level(row, 1, "aa") == f(6));
  CHECK(monster_level(row, 1, "aa", true) == f(3));
  CHECK(monster_level(row, 3, "aa", true) == f(65.5));
  CHECK(monster_level(row, 5, "stomedal") == ONE);
  CHECK(monster_level(row, 5, "innpit") == TWO);
  ManifestInput m;
  m.base = "leva";
  m.disc_files = {"FX.LMP",       "HUD.LMP",     "MAPICONS.LMP", "ICONS.LMP",
                  "LOADMISC.LMP", "GOODDOG.LMP", "SKELE_B.LMP",  "LEVA.LMP",
                  "LEVA.GOB",     "LEVA.TEX",    "TREE.LMP"};
  m.gob_lumps = {"leva.lmp", "env.lmp", "waterenv.lmp"};
  m.level_entries = {"leva.world", "objects.ob", "script.scr"};
  m.model_lumps = {"tree"};
  auto manifest = level_manifest(m);
  auto index = [&](const std::string &s) {
    auto i = std::find_if(manifest.begin(), manifest.end(),
                          [&](auto &r) { return r.name == s; });
    CHECK(i != manifest.end());
    return i - manifest.begin();
  };
  CHECK(index("leva.gob") < index("leva.world"));
  CHECK(index("tree.lmp") < index("waterenv.lmp"));
  CHECK(index("waterenv.lmp") < index("objects.ob"));
  CHECK(manifest[index("leva.tex")].required);
  CHECK(!manifest[index("ambient.bin")].required);
  CHECK(!manifest[index("dome.lmp")].required);
  auto bad = m;
  bad.disc_files.erase(
      std::find(bad.disc_files.begin(), bad.disc_files.end(), "LEVA.TEX"));
  refuses([&] { level_manifest(bad); });
  bad = m;
  bad.gob_lumps.pop_back();
  refuses([&] { level_manifest(bad); });
  bad = m;
  bad.model_lumps = {"absent"};
  refuses([&] { level_manifest(bad); });
  bad = m;
  bad.disc_files.erase(
      std::find(bad.disc_files.begin(), bad.disc_files.end(), "LEVA.LMP"));
  bad.recall_leg = true;
  manifest = level_manifest(bad);
  CHECK(index("lmiss.lmp") >= 0);
  CHECK(std::none_of(manifest.begin(), manifest.end(),
                     [](auto &r) { return r.name == "dialog.bin"; }));
  bad = m;
  bad.level_entries.push_back("a.world");
  manifest = level_manifest(bad);
  CHECK(manifest[index("leva.world")].step == "8");
  CHECK(std::none_of(manifest.begin(), manifest.end(),
                     [](auto &r) { return r.name == "a.world"; }));
  auto bytes = gob({{"a.lmp", Blob(0x10)},
                    {"gaydark1.lmp", Blob(0x801)},
                    {"c.lmp", Blob(1)}});
  auto es = gob_entries({bytes.data(), bytes.size()});
  CHECK(es.size() == 3);
  CHECK(es[0].offset == 0x800);
  CHECK(es[1].name == "faydark1.lmp");
  CHECK(es[1].offset == 0x1000);
  CHECK(es[2].offset == 0x2000);
  CHECK(save_filter_admits("torch"));
  CHECK(!save_filter_admits("Player1"));
  SpawnState s;
  unsigned calls = 0, draw = 1000;
  auto spot = [&](const Vec3 &c, Raw spread, Raw radius, bool any) {
    CHECK(spread == f(36));
    CHECK(radius == f(24));
    CHECK(!any);
    ++calls;
    return Vec3{add(c[0], from_int(calls)), c[1], c[2]};
  };
  auto took =
      run_markers(s,
                  {record("Player1", {}, v(1, 0, 0)),
                   record("Player1", {"default=1"}, v(2, 0, 0)),
                   record("Player1", {"default=1"}, v(3, 0, 0)),
                   record("Player1", {"name=e"}, v(4, 0, 0)),
                   record("Player1", {"name=e"}, v(5, 0, 0))},
                  "", "e", false, spot, [&] { return std::uint16_t(draw++); });
  CHECK(took == std::vector<std::size_t>({0, 1, 2, 3}));
  CHECK(s.rank == 3);
  CHECK(calls == 16);
  CHECK(s.pos[3][0] == f(20));
  calls = 0;
  draw = 1000;
  run_markers(s, {record("Player1", {}, v(0, 0, 0), 10)}, "", "", false, spot,
              [&] { return std::uint16_t(draw++); });
  CHECK(s.facing[0] == std::uint16_t(10 + 1000 - 2000));
  s.pos[0] = v(5, 6, 7);
  run_markers(s, {record("Lantern")}, "", "", false, {}, {});
  CHECK(s.pos[0] == v(5, 6, 7));
  CHECK(s.rank == 0);
  s = {};
  s.pos.fill(v(0, 0, 99));
  s.facing.fill(100);
  s.rank = 1;
  Placement p;
  p.players = 3;
  p.alive[1] = false;
  p.restore_xy =
      std::array<Vec3, 4>{v(7, 8, 0), v(7, 8, 0), v(7, 8, 0), v(7, 8, 0)};
  unsigned heights = 0;
  auto height = [&](Raw r, const Vec3 &) {
    CHECK(r == f(12));
    ++heights;
    return f(4);
  };
  auto scatter = [](const Vec3 &c, Raw spread, Raw radius, bool any) {
    CHECK(spread == f(36) && radius == f(14) && !any);
    return Vec3{add(c[0], ONE), c[1], c[2]};
  };
  CHECK(place_players(s, p, height, scatter) == std::vector<unsigned>({0, 2}));
  CHECK(s.pos[0] == v(7, 8, 4));
  CHECK(s.pos[1][0] == f(8));
  CHECK(s.facing == std::array<std::uint16_t, 4>({100, 8100, 8100, 100}));
  CHECK(heights == 5);
  s.rank = 3;
  p.players = 2;
  place_players(s, p, height, scatter);
  CHECK(s.facing[1] == 8100);
  p = {};
  p.medal_slot = 2;
  CHECK(place_players(s, p, height, {}) == std::vector<unsigned>({2}));
  p.pvp = true;
  CHECK(place_players(s, p, height, {}).empty());
  CHECK(orientation_from_props({}) == 0xff);
  CHECK(orientation_from_props({"orientation=3", "orientation=7"}) == 7);
  CHECK(orientation_from_props({"Orientation=3"}) == 0xff);
  Pending pending;
  pending.request(std::string(79, 'a'));
  refuses([&] { pending.request(std::string(80, 'a')); });
  CHECK(new_world_gate(false, 0, 5, 3) == std::make_pair(false, 5));
  CHECK(new_world_gate(true, 0, 0, 3) == std::make_pair(false, 600));
  CHECK(new_world_gate(true, 0, 5, 3) == std::make_pair(true, 5));
  CHECK(new_world_gate(true, 0, 5, 1) == std::make_pair(false, 600));
  CHECK(new_world_gate(true, 0, 5, 3, true) == std::make_pair(false, 5));
  CHECK(cancel_pending(true, false, false, false, false));
  CHECK(!cancel_pending(true, false, true, false, false));
  CHECK(!cancel_pending(true, false, false, false, true));
  CHECK(find_act(acts, "bb", -1) == 1);
  CHECK(find_act(acts, "hub2", 1) == 1);
  CHECK(find_act(acts, "hub2", -1) == 0);
  CHECK(find_act(acts, "qq", 0) == -1);
  LevelStates states;
  CHECK(states.store("aa", acts[0], 0, 0, false, {'1'}));
  CHECK(states.store("hub2", acts[0]));
  CHECK(!states.store("aa_x", acts[0]));
  CHECK(!states.store("bb", acts[0]));
  CHECK(!states.store("aa", acts[0], 4));
  CHECK(states.lookup("hub2") == std::make_pair(true, std::size_t(50)));
  State se;
  se.acts = acts;
  se.quest_values["q"] = 3;
  se.pending.request("aa");
  auto d = begin_change(se, {});
  CHECK(d.act_changed);
  CHECK(se.level == "aa" && se.prev.empty() && se.act == 0);
  CHECK(se.gates.names == std::vector<std::string>({"hub"}));
  CHECK(se.pending.orientation == ORIENT_CONSUMED);
  se.entrance = "door";
  se.pending.request("aab_s");
  ChangeOptions co;
  co.old_blob = {'x'};
  d = begin_change(se, co);
  CHECK(d.stored && !d.act_changed);
  CHECK(se.level == "aab" && se.sub == "s" && se.prev == "aa");
  CHECK(se.entrance == "door");
  se.quest_values["q"] = 5;
  se.pending.request("bb");
  d = begin_change(se, {});
  CHECK(d.act_changed && se.quest_values["q"] == 0 &&
        se.states.entries.empty());
  for (auto &player : se.players)
    CHECK(player.recall_dest == "None");
  Trigger t(f(10), f(20), 5, 4, "go");
  CHECK(t.contains(v(10, 16, 100)));
  CHECK(t.contains(v(15, 20, -100)));
  CHECK(!t.contains(v(15.5, 18, 0)));
  CHECK(!t.contains(v(12, 21, 0)));
  auto event = t.run({{1, v(12, 18, 0)}, {2, v(13, 18, 0)}});
  CHECK(event.entry == std::optional<std::string>("go") && event.active == 1);
  CHECK(!t.run({{0, v(12, 18, 0)}}).entry);
  event = t.run({{0, v(0, 0, 0)}}, true);
  CHECK(event.entry == std::optional<std::string>("notgo") && !event.active);
  CHECK(!t.run({{0, v(0, 0, 0)}}).entry);
  CHECK(Trigger(0, 0, -5, -4, "go").contains(v(-5, 4, 0)));
  Trigger edged(0, 0, 10, 10, "exit");
  TriggerParams params;
  params.on_only = true;
  params.off_only = true; // read second, so off wins
  params.timeout = 2;
  std::vector<std::string> dispatch;
  auto linked = [&](bool on) { dispatch.push_back(on ? "on" : "off"); };
  auto script = [&](const std::string &name, unsigned slot) {
    dispatch.push_back(name + std::to_string(slot));
    return 1;
  };
  edged.run_exact({{1, v(1, -1, 7)}, {0, v(2, -2, 8)}}, false, &params, linked, script);
  CHECK(dispatch == std::vector<std::string>({"off", "notexit0"}));
  CHECK(edged.slot == 0 && !edged.inside && edged.delay == 8);
  for (int n = 7; n >= 0; --n) {
    edged.run_exact({}, false, &params, linked, script);
    CHECK(edged.delay == n);
  }
  CHECK(dispatch.size() == 2);
  edged.slot = 42;
  edged.run_exact({{0, v(2, -2, 0)}}, true, &params, linked, script);
  CHECK(edged.slot == -1 && dispatch.size() == 2 && !edged.inside);
  params = {};
  params.on_only = true;
  params.need_script_hit = true;
  auto miss = [](const std::string &, unsigned) { return 0; };
  edged.run_exact({{0, v(2, -2, 0)}}, false, &params, linked, miss);
  CHECK(!edged.inside && edged.delay == 0);
  params.need_script_hit = false;
  edged.run_exact({{0, v(2, -2, 0)}}, false, &params, linked, script);
  CHECK(!edged.inside && edged.delay == -1);
  unsigned searches = 0;
  auto search = [&](const Vec3 &at, Raw spread, Raw radius, bool any) {
    CHECK(at == v(3, 4, 5) && spread == f(36) && radius == f(24) && any);
    ++searches;
    return v(float(searches), 4, 5);
  };
  std::uint32_t arrival_seed = 7;
  auto arrival = portal_arrival(true, false, 2, v(3, 4, 5), arrival_seed, 1, search);
  CHECK(arrival.rank == 3 && searches == 2 && arrival.positions.size() == 2);
  CHECK(arrival.positions[0] == v(1, 4, 5) && arrival.positions[1] == v(2, 4, 5));
  auto seed1 = 7u * 0x19660du + 0x3c6ef35fu;
  auto seed2 = seed1 * 0x19660du + 0x3c6ef35fu;
  CHECK(arrival.seed == seed2 && arrival.facing ==
        std::vector<std::uint16_t>({std::uint16_t(seed1 >> 16), std::uint16_t(seed2 >> 16)}));
  arrival_seed = 7;
  CHECK(portal_arrival(true, true, 2, v(3, 4, 5), arrival_seed, 1, search).rank == 1);
  CHECK(searches == 2);
  CHECK(push_trigger_allowed({}));
  CHECK(!push_trigger_allowed({1, 0, 1}));
  GateList g;
  g.reset("hub");
  CHECK(g.add_current("aa", "hub"));
  CHECK(!g.add_current("aa", "hub"));
  CHECK(g.choosable("aa") == std::vector<std::string>({"hub"}));
  g.add_current("hub2", "hub2");
  CHECK(g.names == std::vector<std::string>({"aa", "hub2"}));
  for (int i = 0; i < 13; ++i)
    g.add_current("l" + std::to_string(i), "hub2");
  CHECK(g.names.size() == 15);
  refuses([&] { g.add_current("one-more", "hub2"); });
  auto prompt = gate_prompt(false, false, true, 1, {});
  CHECK(!prompt.usable && prompt.text == "JumpGateVirgin" && prompt.found);
  CHECK(gate_prompt(true, false, true, 2, {f(96 * 96)}).usable);
  CHECK(gate_prompt(true, false, true, 2, {f(97 * 97)}).text ==
        "JumpGateNeedPlayers");
  CHECK(!gate_prompt(false, true, true, 2, {}).found);
  std::array<std::string, 4> dest{{"None", "None", "None", "None"}};
  std::array<Vec3, 4> pos{v(1, 2, 3), v(4, 5, 6), Vec3{}, Vec3{}};
  auto rr = recall_finish("aa", "s", 2, pos, dest, "hub");
  CHECK(rr.request == "hub" && rr.arrived_by_recall && !rr.restore_xy);
  CHECK(rr.destinations ==
        std::array<std::string, 4>({"aa_s", "aa_s", "None", "None"}));
  rr = recall_finish("hub", "", 2, *rr.saved, rr.destinations, "hub");
  CHECK(rr.request == "aa_s" && !rr.arrived_by_recall &&
        (*rr.restore_xy)[1] == v(4, 5, 6));
  std::vector<Piece> ps(3);
  ps[2].tx = 800;
  CHECK(find_moving_piece(ps, v(0, 0, 0)) == 1);
  CHECK(ps[1].claimed);
  CHECK(find_moving_piece(ps, v(0, 0, 0)) == -1);
  CHECK(find_moving_piece(ps, v(280, 0, 0)) == -1);
  CHECK(find_moving_piece(ps, v(279.99f, 0, 0)) == 2);
  std::vector<Piece> up(1);
  up[0].tz = 320;
  CHECK(find_moving_piece(up, v(0, 0, 0)) == 0);
  up[0].tz = 0;
  up[0].claimed = false;
  CHECK(find_moving_piece(up, v(0, 0, 81)) == -1);
  ps.resize(2);
  ps[0].claimed = true;
  ps[1].claimed = false;
  std::vector<int> off;
  disable_unclaimed(ps, [&](int i, bool on) {
    CHECK(!on);
    off.push_back(i);
  });
  CHECK(off == std::vector<int>({1}));
  release_piece(ps, -1);
  release_piece(ps, 0);
  CHECK(!ps[0].claimed);
  auto pose = autodoor_pose(true);
  CHECK(!pose.collision && pose.offset == v(0, 0, -256) && pose.angle == 0);
}
void robustness_and_state() {
  auto b = objects({record("Player1", {"default=1"})});
  for (std::size_t n = 0; n < b.size(); ++n)
    refuses([&] { parse_objects({b.data(), n}); });
  auto bad = b;
  put(bad, 12, 0, 2);
  refuses([&] { parse_objects({bad.data(), bad.size()}); });
  LevelStates states;
  Act rows{{"large", std::uint32_t(LevelStates::LIMIT + 1)}};
  CHECK(!states.store("large", rows));
  CHECK(states.entries.size() == 1 && !states.lookup("large").first);
  states = {};
  for (int i = 0; i < 10; ++i)
    CHECK(
        states.store("s" + std::to_string(i), {{"s" + std::to_string(i), 1}}));
  refuses([&] { states.store("eleven", {}, 0, 0, true); });
  CHECK(!states.store("s0", {}, 0, 0x800, true));
  State s;
  s.acts = acts;
  s.act = 0;
  s.level = "aa";
  s.entrance = "retained";
  s.quest_values["q"] = 9;
  s.players[0].persistent = {1, 2};
  s.players[0].hp = 12;
  s.players[0].max_hp = 20;
  s.spawn.pos[0] = v(1, 2, 3);
  s.gates.names = {"hub", "aa"};
  auto cp = checkpoint(s, {'x'}, {7});
  CHECK(cp);
  s.players[0].hp = 0;
  s.quest_values["q"] = 0;
  CHECK(party_wipe(s, cp) == Wipe::Reload);
  CHECK(s.players[0].hp == 12 && s.quest_values["q"] == 9 &&
        s.loaded_from_save && s.spawn.rank == 3 &&
        s.spawn.pos[0] == v(1, 2, 3));
  s.level = "outside";
  s.sub = "drop";
  s.players[0].hp = 0;
  CHECK(!checkpoint(s, {}, {}));
  CHECK(party_wipe(s, {}) == Wipe::Restart);
  CHECK(s.players[0].hp == 20 && s.pending.name == "outside" &&
        s.pending.orientation == 0xfd);
  s.transient.alternate_death = true;
  CHECK(party_wipe(s, {}) == Wipe::Wait);
  s = {};
  s.acts = acts;
  s.act = 0;
  s.level = "aa";
  s.entrance = "door";
  s.post_action = "after";
  s.quest_values["q"] = 8;
  s.players[0].persistent = {9};
  s.players[0].recall_dest = "aa";
  s.players[0].quests = {"q"};
  s.transient.objects = {5};
  s.transient.paused = true;
  s.pending.request("bb", 7);
  ChangeOptions opts;
  opts.card_latch = true;
  auto r = begin_change(s, opts);
  CHECK(r.act_changed && !r.reset_act && s.quest_values["q"] == 8);
  CHECK(s.entrance == "door" && s.post_action == "after" &&
        s.players[0].persistent == Blob({9}));
  CHECK(s.transient.objects.empty() && !s.transient.paused);
  CHECK(s.players[0].recall_dest == "None" && s.orientation_copy == 7);
  s = {};
  s.level = "aa";
  s.players[0].hp = 1;
  s.transient.recall_countdown.start();
  std::array<Vec3, 4> pos{};
  pos[0] = v(1, 2, 3);
  CHECK(!complete_recall(s, 1, true, pos, "hub"));
  CHECK(s.transient.recall_countdown.value == 60);
  CHECK(!complete_recall(s, 1, false, pos, "hub"));
  // 60 native calls; a cutscene before the last one changes nothing.
  CHECK(recall_tick(s, 1, true, 1, pos, "hub") == RecallEvent::Aura);
  for (int i = 1; i < 59; ++i)
    CHECK(recall_tick(s, 1, i < 58, 1, pos, "hub") == RecallEvent::None);
  CHECK(s.pending.name.empty());
  CHECK(recall_tick(s, 1, false, 1, pos, "hub") == RecallEvent::Finish);
  CHECK(s.pending.name == "hub" && s.players[0].recall_dest == "aa" &&
        s.players[0].recall_position == pos[0] &&
        s.pending.orientation == ORIENT_RESTORE && s.arrived_by_recall);
  CHECK(recall_tick(s, 1, false, 1, pos, "hub") == RecallEvent::None);
  s = {};
  s.level = "aa";
  s.players[0].hp = 1;
  s.transient.recall_countdown.start();
  for (int i = 0; i < 59; ++i)
    recall_tick(s, 1, false, 1, pos, "hub");
  CHECK(recall_tick(s, 1, true, 1, pos, "hub") ==
        RecallEvent::DroppedCutscene);
  for (int i = 0; i < 10; ++i)
    CHECK(recall_tick(s, 1, false, 1, pos, "hub") == RecallEvent::None);
  CHECK(s.pending.name.empty() && !s.transient.recall_countdown.running() &&
        s.players[0].recall_dest == "None");
  s.transient.recall_countdown.value = 1;
  CHECK(recall_tick(s, 1, false, 0, pos, "hub") == RecallEvent::DroppedNobody);
  CHECK(s.pending.name.empty());
  s = {};
  s.level = "aa";
  s.players[0].hp = 1;
  CHECK(complete_recall(s, 1, false, pos, "hub") && s.pending.name == "hub");
  Trigger t(0, 0, 10, 10, "go");
  t.inside = true;
  t.delay = -7;
  auto tb = t.save();
  Trigger restored(0, 0, 10, 10, "go");
  restored.restore(tb);
  CHECK(restored.inside && restored.delay == -7);
  PushTrigger push;
  push.net_host_only = true;
  push.keys = {"a", "b", "c"};
  int calls = 0;
  CHECK(!push.prompt(true, 1, 0, [&](auto &) {
    ++calls;
    return 1;
  }));
  CHECK(calls == 0);
  CHECK(!push.prompt(true, 0, 0, [&](auto &) { return ++calls != 2; }));
  CHECK(calls == 2);
  push.shop = true;
  push.once = true;
  push.actions = {"go", "then"};
  CHECK(!push.interact(true, {}));
  std::vector<std::string> actions;
  CHECK(push.interact(false, [&](auto &a, bool raw) {
    CHECK(raw);
    actions.push_back(a);
  }));
  CHECK(push.deleted && actions == push.actions);
  CHECK(!push.interact(false, {}));
  world_queries::LevelData data;
  data.heightfields.push_back({});
  data.elements.push_back(
      world_queries::make_element(0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 32767, 0));
  data.pieces = {0};
  world_queries::Level terrain(data);
  Pieces pieces({Piece{}}, terrain);
  pieces.disable_unclaimed();
  CHECK(terrain.data().elements[0].flags & 2);
  pieces.collision(0, true);
  pieces.move(0, v(5, 6, 7), 16384);
  CHECK(terrain.data().elements[0].x == 0 && terrain.data().elements[0].z == 5);
  CHECK(pieces.poses()[0].chunks ==
        move_piece(Piece{}, v(5, 6, 7), 0x4000));
  pieces.height(0, f(-3.9f));
  CHECK(pieces.height(0) == -3);
  pieces.reset();
  CHECK(pieces.height(0) == -3 && !(terrain.data().elements[0].flags & 2));
  CHECK(pieces.poses()[0].offset == Vec3{} &&
        pieces.poses()[0].chunks.empty());
  pieces.move(-1, {}, 0);
  pieces.height(-1, 0);
  CHECK(pieces.height(-1) == 0);
}
// tools/test_ref_levels.py Round2* classes, ported case by case.
void round2_cases() {
  using Names = std::vector<std::string>;
  CHECK(first_entry_with_extension(
            Names{"zeta.world", "alpha.world", "objects.ob"}, "world") ==
        std::optional<std::string>("zeta.world"));
  CHECK(manifest_world({"zeta.world", "alpha.world", "objects.ob"}) ==
        std::optional<std::string>("zeta.world"));
  CHECK(!first_entry_with_extension(
      Names{"world", "a.WORLD", "a.world.bak", "a.worlds"}, "world"));
  CHECK(*first_entry_with_extension(Names{"x.y.world"}, "world") ==
        "x.y.world");
  CHECK(*first_entry_with_extension(Names{".world"}, "world") == ".world");
  CHECK(*first_entry_with_extension(Names{"a.world", "b.ob", "c.world"},
                                    "world", 1) == "c.world");
  CHECK(!first_entry_with_extension(Names{"a.world"}, "world", 1));
  CHECK(!manifest_world({"objects.ob"}));
  std::vector<Entry> es{{"b.WORLD", 0, 0}, {"c.world", 1, 0}, {"a.world", 2, 0}};
  CHECK(first_entry_with_extension(es, "world") == &es[1]);
  CHECK(base_light_level(LEVEL_FLAG_LIGHT_20 | LEVEL_FLAG_LIGHT_40) == 0x40);
  CHECK(base_light_level(LEVEL_FLAG_LIGHT_40) == 0x40);
  CHECK(base_light_level(0xffffffffu & ~LEVEL_FLAG_LIGHT_40) == 0x20);
  CHECK(base_light_level(0) == 0x80);
  CHECK(base_light_level(LEVEL_FLAG_LIGHT_20) == 0x20);
  // Light component of test_chunk_header_state (round 1).
  CHECK(chunk_light(0).dynamic && chunk_light(0x400).dynamic);
  CHECK(chunk_light(0x10100).level == -230 && !chunk_light(0x10100).dynamic);
  CHECK(chunk_light(0x2200).level == 230 && !chunk_light(0x2200).dynamic);
  CHECK(chunk_light(0x2300).level == -230 && chunk_light(0x2300).flags == 0x2200);
  CHECK(chunk_light(0x200).level == 0 && !chunk_light(0x200).dynamic);
  CHECK(chunk_light(0x10).dynamic);
  auto run = [](RecallCountdown &rc, int n, auto cut, auto alive) {
    std::vector<RecallEvent> out;
    for (int t = 0; t < n; ++t)
      out.push_back(rc.tick(cut(t), alive(t)));
    return out;
  };
  auto never = [](int) { return false; };
  auto one = [](int) { return 1u; };
  RecallCountdown rc;
  rc.start();
  auto ev = run(rc, 61, never, one);
  CHECK(ev[0] == RecallEvent::Aura && ev[59] == RecallEvent::Finish &&
        ev[60] == RecallEvent::None && !rc.running());
  CHECK(std::all_of(ev.begin() + 1, ev.begin() + 59,
                    [](auto e) { return e == RecallEvent::None; }));
  rc.start();
  ev = run(rc, 60, [](int t) { return t < 59; }, one);
  CHECK(ev[59] == RecallEvent::Finish);
  rc.start();
  ev = run(rc, 70, [](int t) { return t == 59; }, one);
  CHECK(ev[59] == RecallEvent::DroppedCutscene && rc.value == 0);
  CHECK(std::all_of(ev.begin() + 60, ev.end(),
                    [](auto e) { return e == RecallEvent::None; }));
  rc = {1};
  CHECK(rc.tick(false, 0) == RecallEvent::DroppedNobody);
  rc = {1};
  CHECK(rc.tick(true, 0) == RecallEvent::DroppedCutscene);
  rc = {1};
  CHECK(rc.tick(false, 1) == RecallEvent::Finish);
  rc = {61};
  CHECK(rc.tick(false, 1) == RecallEvent::None);
  CHECK(rc.tick(false, 1) == RecallEvent::Aura);
  rc = {0x80};
  CHECK(rc.tick(false, 1) == RecallEvent::None && rc.value == 0x7f);
  CHECK(recall_request_tick(100, true) == 159);
  CHECK(recall_request_tick(100, false) == 160);
  Piece p{400, -800, 40, 32767, 0};
  auto first = move_piece(p, v(0, 0, -256), 0);
  CHECK(first == std::vector<ChunkTransform>{{399, -800, -984, 32767, 0}});
  CHECK(move_piece(p, v(0, 0, -256), 0) == first);
  CHECK(move_piece(p, v(0, 0, 0), 0)[0].tz == 40);
  CHECK(move_piece(p, v(0, 0, 0), 0x4000) ==
        std::vector<ChunkTransform>{{-800, -399, 40, 0, 32767}});
  Piece q{400, 0, 0, 32767, 0};
  auto both = move_piece(q, v(0, 0, 0), 0x4000, {false, true});
  CHECK(both.size() == 2 && both[0].tx == 0 && both[0].ty == -400 &&
        both[1].tx == 0 && both[1].ty == 400);
  CHECK(move_piece(q, v(0, 0, 0), 0x4000, {}).empty());
  q.chunk_mirrored = std::vector<bool>{};
  CHECK(move_piece(q, v(0, 0, 0), 0x4000).empty());
  auto clamp = move_piece(Piece{0, 0, 0, -0x8000, -0x8000}, v(0, 0, 0), 0x2000);
  CHECK(clamp[0].cosine == 0 && clamp[0].sine == -0x8000);
  // Real-arithmetic geometry check (algebra, test_turns_about_pivot...).
  const double pi = 3.14159265358979323846;
  for (bool mirrored : {false, true}) {
    Piece r{1200, -400, 0, 23170, 23170, mirrored};
    double ox = 12, oy = -5;
    int angle = 0x1555;
    auto t = move_piece(r, v(12, -5, 0), angle)[0];
    auto draw = [&](double tx, double ty, double c16, double s16v,
                    double lx, double ly) {
      double px = (lx + tx) / 4, py = (ly + ty) / 4, c = c16 / 32767,
             sn = s16v / 32767;
      return mirrored ? std::make_pair(px * c + py * sn, px * sn - py * c)
                      : std::make_pair(px * c - py * sn, px * sn + py * c);
    };
    double k = 23170.0 / 32767;
    double m00 = k, m01 = mirrored ? k : -k, m10 = k, m11 = mirrored ? -k : k;
    auto pivot = draw(r.tx, r.ty, r.cosine, r.sine, 0, 0);
    double a = angle / 65536.0 * 2 * pi;
    for (auto l : {std::make_pair(0.0, 0.0), std::make_pair(160.0, 0.0),
                   std::make_pair(0.0, 160.0), std::make_pair(-80.0, 240.0)}) {
      auto rest = draw(r.tx, r.ty, r.cosine, r.sine, l.first, l.second);
      double rx = rest.first - pivot.first, ry = rest.second - pivot.second;
      double wx = pivot.first + m00 * ox + m01 * oy + std::cos(a) * rx -
                  std::sin(a) * ry,
             wy = pivot.second + m10 * ox + m11 * oy + std::sin(a) * rx +
                  std::cos(a) * ry;
      auto got = draw(t.tx, t.ty, t.cosine, t.sine, l.first, l.second);
      CHECK(std::fabs(got.first - wx) <= 0.5 && std::fabs(got.second - wy) <= 0.5);
    }
  }
}
// The loader takes the first `.world` in stored directory order (no sorting,
// case-sensitive extension) and refuses a level lump without one.
void loader_world_order() {
  std::map<std::string, Blob> files;
  auto empty = lump({});
  for (auto n : {"FX.LMP;1", "HUD.LMP;1", "MAPICONS.LMP;1", "ICONS.LMP;1",
                 "LOADMISC.LMP;1", "GOODDOG.LMP;1", "SKELE_B.LMP;1"})
    files[n] = empty;
  auto level = [&](std::vector<std::pair<std::string, Blob>> entries) {
    files["LV.GOB;1"] = gob({{"lv.lmp", lump(entries)},
                             {"waterenv.lmp", lump({{"water.tex", {0}}})}});
  };
  LoadServices svc;
  svc.disc = [&](const std::string &n) -> std::optional<Blob> {
    auto i = files.find(n);
    return i == files.end() ? std::nullopt : std::optional<Blob>(i->second);
  };
  svc.range = [](const std::string &, std::size_t, std::size_t size) {
    return Blob(size);
  };
  svc.player_lumps = [] { return false; };
  Blob seen;
  svc.model_lumps = [&](Bytes b) {
    seen.assign(b.data, b.data + b.size);
    return std::vector<std::string>{};
  };
  svc.event = [](const std::string &) {};
  level({{"x.WORLD", {7}},
         {"b.world.bak", {6}},
         {"zeta.world", {3, 4}},
         {"alpha.world", {5}},
         {"objects.ob", objects({})}});
  std::vector<LevelRow> rows{LevelRow{}};
  rows[0].name = "lv";
  rows[0].flags = LEVEL_FLAG_LIGHT_20 | LEVEL_FLAG_LIGHT_40;
  Loader loader(svc);
  auto out = loader.load("lv", "", rows, 1);
  CHECK(out.world_entry == "zeta.world" && out.world == Blob({3, 4}) &&
        seen == Blob({3, 4}) && out.base_light_level == 0x40);
  level({{"x.WORLD", {7}}, {"objects.ob", objects({})}});
  Loader none(svc);
  bool missing = false;
  try {
    none.load("lv", "", rows, 1);
  } catch (const MissingResource &) {
    missing = true;
  }
  CHECK(missing);
}
void top_floor() {
  world_queries::LevelData data;
  data.tg_w = data.tg_h = 1;
  data.grid = {{0, 1}};
  world_queries::HeightField h;
  h.w = h.h = 1;
  h.samples = {0};
  data.heightfields = {h};
  data.elements.push_back(world_queries::make_element(0, -100, -100, 100, 100,
                                                      0, 0, 0, 0, 0, 32767, 0));
  data.elements.push_back(world_queries::make_element(
      1, -100, -100, 100, 100, 0, 0, 0, 0, 20, 32767, 0));
  world_queries::Level terrain(data);
  SpawnState spawn;
  spawn.rank = 3;
  spawn.pos[0] = v(0, 0, -500);
  Placement p;
  p.players = 1;
  auto height = [&](Raw radius, const Vec3 &position) {
    return world_queries::terrain_height(terrain, radius, position);
  };
  CHECK(place_players(spawn, p, height, {}).size() == 1);
  CHECK(spawn.pos[0][2] == f(20));
  spawn.pos[0] = v(10000, 10000, 99);
  place_players(spawn, p, height, {});
  CHECK(spawn.pos[0][2] == world_queries::NO_FLOOR);
}
void integration() {
  std::map<std::string, Blob> files;
  auto empty = lump({});
  for (auto n : {"FX.LMP;1", "HUD.LMP;1", "MAPICONS.LMP;1", "ICONS.LMP;1",
                 "LOADMISC.LMP;1", "GOODDOG.LMP;1", "SKELE_B.LMP;1"})
    files[n] = empty;
  auto make_level = [&](const std::string &name, Vec3 gate) {
    auto main =
        lump({{"synthetic.world", {1, 2}},
              {"objects.ob", objects({record("Player1", {}, v(1, 2, 99)),
                                      record("Portal", {}, gate)})},
              {"script.scr", {1}},
              {"ntsc_dialog.bin", {2}},
              {"pal_dialog.bin", {3}}});
    files[cd_file_name(name + ".gob")] =
        gob({{name + ".lmp", main},
             {"waterenv.lmp", lump({{"water.tex", {0}}})},
             {"env.lmp", empty}});
  };
  make_level("aa", v(20, 30, 40));
  make_level("aab", v(70, 80, 90));
  std::vector<std::string> io, events, scripts;
  LoadServices svc;
  svc.disc = [&](const std::string &n) -> std::optional<Blob> {
    io.push_back(n);
    auto i = files.find(n);
    return i == files.end() ? std::nullopt : std::optional<Blob>(i->second);
  };
  svc.range = [&](const std::string &n, std::size_t off, std::size_t size) {
    CHECK(off == 0 && size == 0x5000);
    io.push_back(n);
    return Blob(size);
  };
  svc.player_lumps = [&] {
    events.push_back("players");
    return false;
  };
  svc.model_lumps = [](Bytes b) {
    CHECK(b.size == 2);
    return std::vector<std::string>{};
  };
  svc.event = [&](const std::string &s) { events.push_back(s); };
  Loader loader(svc);
  State state;
  state.acts = acts;
  state.players[0].active = true;
  state.players[0].hp = 10;
  state.players[0].max_hp = 10;
  unsigned built = 0, draw = 0;
  std::vector<bool> any_height;
  SessionHooks hooks;
  hooks.find_free_spot = [&](const Vec3 &p, Raw, Raw, bool any) {
    any_height.push_back(any);
    return p;
  };
  hooks.shared_draw = [&] { return std::uint16_t(++draw); };
  hooks.terrain_height = [](Raw r, const Vec3 &) {
    CHECK(r == f(12));
    return f(100);
  };
  hooks.event = svc.event;
  hooks.script = [&](const std::string &s, const Blob &) {
    scripts.push_back(s);
  };
  hooks.construct = [](const Record &) {};
  hooks.restore_level_state = [](const Blob &) {
    return std::vector<Record>{record("Player1", {}, v(51, 52, 53))};
  };
  hooks.construct_player = [&](unsigned slot, const Vec3 &p, std::uint16_t) {
    CHECK(slot == 0 && p[2] == f(100));
    ++built;
  };
  hooks.remove_flag39_items = [&] { events.push_back("remove39"); };
  hooks.disable_unclaimed_pieces = [&] {
    events.push_back("disable_unclaimed");
  };
  SessionAdapter adapter(state, loader, hooks);
  CHECK(adapter.request_script("aa", {}, false, 0));
  Placement placement;
  placement.players = 1;
  auto loaded = adapter.change({}, {}, placement);
  CHECK(adapter.ready() && !adapter.failed());
  CHECK(built == 1 && state.level == "aa");
  CHECK(loaded.world == Blob({1, 2}) && loaded.texture_header.size() == 0x5000);
  CHECK(loader.entry("aa.lmp", "dialog.bin") == std::optional<Blob>({2}));
  CHECK(!loader.entry("aa.lmp", "pal_dialog.bin"));
  CHECK(scripts == std::vector<std::string>({"preStart", "start"}));
  auto index = [&](const std::string &s) {
    auto i = std::find(events.begin(), events.end(), s);
    CHECK(i != events.end());
    return i - events.begin();
  };
  CHECK(index("clear_level") < index("world_fixup"));
  CHECK(index("world_fixup") < index("allocate_water_pool"));
  CHECK(index("disable_unclaimed") < index("paths_join_segments"));
  CHECK(index("warmup") < index("network_barrier"));
  bool found = false;
  CHECK(adapter.prompt_gate(found, true, {}, "hub").usable && found);
  state.gates.add_current("aab", "hub");
  state.travel.open(false);
  CHECK(state.travel.paused);
  CHECK(!state.travel.confirm("aa", "aa", state.gates));
  CHECK(state.travel.confirm("aab", "aa", state.gates));
  for (int i = 0; i < 179; ++i)
    CHECK(!adapter.advance_ready());
  CHECK(state.pending.name.empty());
  CHECK(adapter.advance_ready());
  CHECK(state.pending.name == "aab" && state.travel.ticks == 181);
  CHECK(!adapter.advance_ready());
  state.post_action = "after";
  scripts.clear();
  loaded = adapter.change({}, {}, placement);
  CHECK(adapter.ready() && built == 2 && state.prev == "aa" &&
        state.level == "aab");
  CHECK(state.spawn.rank == 3 && state.spawn.pos[0] == v(70, 80, 100));
  CHECK(state.travel.ticks == 181);
  CHECK(any_height.back());
  CHECK(scripts == std::vector<std::string>({"preStart", "start", "after"}));
  CHECK(!adapter.advance_ready() && state.travel.ticks == 0);
  CHECK(state.states.lookup("aa").first);
  // Re-entry restores saved marker records; static Player1/Portal are filtered.
  state.pending.request("aa");
  scripts.clear();
  loaded = adapter.change({}, {}, placement);
  CHECK(state.spawn.pos[0] == v(51, 52, 100) && state.spawn.rank == 1);
  CHECK(built == 3 && state.level == "aa");
  CHECK(scripts == std::vector<std::string>({"preStart", "start"}));
  // Missing required byte resource cannot publish a ready scene or be retried
  // blindly.
  files.erase("AA.GOB;1");
  state.pending.request("aa");
  refuses([&] { adapter.change({}, {}, placement); });
  CHECK(adapter.failed() && !adapter.ready());
  refuses([&] { adapter.advance_ready(); });
}
// Grid origin (-2, -2) and an 8 by 8 cell table match the round-3 fixture, so a
// point just below zero is still a cell. The rectangle is the sample box padded
// by 8, and only cells that touch it list the element.
world_queries::Level flat_field(std::int64_t x0, std::int64_t y0, std::int64_t w,
                                std::int64_t h, std::int64_t sample_h,
                                std::int64_t element_z, std::int64_t ex,
                                std::int64_t ey) {
  world_queries::LevelData data;
  data.tg_x0 = data.tg_y0 = -2;
  data.tg_w = data.tg_h = 8;
  data.grid.assign(64, {});
  world_queries::HeightField field;
  field.x0 = x0;
  field.y0 = y0;
  field.w = w;
  field.h = h;
  field.lo = field.hi = sample_h;
  field.samples.assign(static_cast<std::size_t>(w * h),
                       static_cast<std::int16_t>(sample_h * 16));
  data.heightfields.push_back(field);
  const auto min_x = ex + x0 * 4 - 8, min_y = ey + y0 * 4 - 8;
  const auto max_x = ex + (x0 + w - 1) * 4 + 8, max_y = ey + (y0 + h - 1) * 4 + 8;
  data.elements.push_back(world_queries::make_element(
      0, min_x, min_y, max_x, max_y, 0, 0, ex, ey, element_z, 32767, 0));
  for (int row = 0; row < 8; ++row)
    for (int col = 0; col < 8; ++col) {
      const auto cx0 = (col - 2) * 160, cy0 = (row - 2) * 160;
      if (cx0 + 160 > min_x && cy0 + 160 > min_y && cx0 < max_x && cy0 < max_y)
        data.grid[static_cast<std::size_t>(row * 8 + col)].push_back(0);
    }
  return world_queries::Level(data);
}
Round4Hit round4_search(world_queries::Level &level, std::uint32_t seed,
                         bool with_object) {
  const Vec3 centre{};
  objects::World world(objects::CallbackPolicy::ReferenceBase);
  world.shared = SharedRandom(seed);
  if (with_object)
    world.construct(0x1000, centre);
  const auto found = world.find_free_spot(
      centre, ARENA_FIRST_SPREAD, ARENA_SEARCH_RADIUS, false,
      [&](Raw radius, const Vec3 &pos) {
        return world_queries::terrain_test_flag1(level, radius, pos);
      },
      [&](Raw radius, const Vec3 &pos) {
        return world_queries::terrain_height(level, radius, pos);
      });
  return {found.first, found.second,
          player_ctor_z(found.first, PLACEMENT_RADIUS,
                        [&](Raw radius, const Vec3 &pos) {
                          return world_queries::terrain_height(level, radius, pos);
                        })};
}
// The generator's height-3 field: no floor within 12 of the origin, floor
// under the mode-A slot-0 centre.
world_queries::Level round4_arena_field() {
  return flat_field(-20, -20, 60, 60, 3, 0, 180, 0);
}
StoryGround round4_arena_pre_search(const std::vector<Record> &records) {
  auto level = round4_arena_field();
  return arena_mode_a_start_ground(records, level);
}
// The test chooses the search (objects::World at a stated seed, no objects);
// the expectations are the reference's composition at the same seed.
StoryGround round4_arena_wave(const std::vector<Record> &records,
                              std::uint32_t seed) {
  auto level = round4_arena_field();
  objects::World world(objects::CallbackPolicy::ReferenceBase);
  world.shared = SharedRandom(seed);
  auto height = [&](Raw radius, const Vec3 &pos) {
    return world_queries::terrain_height(level, radius, pos);
  };
  return arena_mode_a_wave_ground(
      records, 0, 1,
      [&](const Vec3 &centre, int spread, int radius, bool any) {
        return world.find_free_spot(
            centre, spread, radius, any,
            [&](Raw r, const Vec3 &pos) {
              return world_queries::terrain_test_flag1(level, r, pos);
            },
            height);
      },
      height);
}
Round4Hit round4_solid(std::uint32_t seed, bool with_object) {
  auto level = flat_field(-40, -40, 80, 80, 0);
  return round4_search(level, seed, with_object);
}
Round4Hit round4_hole(std::uint32_t seed) {
  auto level = flat_field(0, 0, 1, 1, 0);
  return round4_search(level, seed, false);
}
int round4_solid_failures() {
  int failed = 0;
  for (std::uint32_t seed = 0; seed < 16; ++seed)
    for (bool with_object : {false, true})
      if (round4_solid(seed, with_object).ok == 0)
        ++failed;
  if (round4_solid(20973, false).ok == 0)
    ++failed;
  if (round4_solid(20973, true).ok == 0)
    ++failed;
  return failed;
}
int round4_hole_kept() {
  int kept = 0;
  for (std::uint32_t seed = 0; seed < 32; ++seed)
    if (round4_hole(seed).snap != world_queries::NO_FLOOR)
      ++kept;
  return kept;
}
int round4_hole_lost() {
  int lost = 0;
  for (std::uint32_t seed = 0; seed < 32; ++seed)
    if (round4_hole(seed).snap == world_queries::NO_FLOOR)
      ++lost;
  return lost;
}
std::pair<int, int> round4_lattice() {
  auto level = flat_field(-40, -40, 80, 80, 0);
  const Raw query = f(11.25f);
  int misses = 0, tested = 0;
  constexpr int limit = 37;
  for (int ix = -limit; ix <= limit; ++ix)
    for (int iy = -limit; iy <= limit; ++iy) {
      if (ix * ix + iy * iy > limit * limit)
        continue;
      ++tested;
      const Vec3 pos{f(static_cast<float>(ix)), f(static_cast<float>(iy)), 0};
      if (world_queries::terrain_height(level, query, pos) ==
          world_queries::NO_FLOOR)
        ++misses;
    }
  return {misses, tested};
}
std::vector<int> arena_mode_a_cycle(int saved) {
  std::vector<int> seen{saved};
  int index = saved;
  for (int step = 0; step < 13; ++step) {
    index = arena_mode_a_next(index, saved);
    if (index == saved)
      break;
    seen.push_back(index);
  }
  return seen;
}
std::pair<Vec3, int> search_floor(world_queries::Level &level, Raw seed) {
  objects::World world(objects::CallbackPolicy::ReferenceBase);
  world.shared = SharedRandom(seed);
  return world.find_free_spot(
      Vec3{0, 0, 0}, 36, 24, false,
      [&](Raw radius, const Vec3 &pos) {
        return world_queries::terrain_test_flag1(level, radius, pos);
      },
      [&](Raw radius, const Vec3 &pos) {
        return world_queries::terrain_height(level, radius, pos);
      });
}
void round3_cases() {
  CHECK(PLACEMENT_RADIUS == f(12) && PLACEMENT_RADIUS == 0x41400000u);
  CHECK(ARENA_RING_MODE_A == 0x43340000u && ARENA_RING_MODE_B == 0x42100000u);
  SpawnState spawn;
  spawn.pos[0] = v(3, -4, 0);
  spawn.pos[1] = v(9, 9, 1);
  spawn.facing = {100, 200, 300, 400};
  spawn.rank = 1;
  std::vector<std::pair<Raw, Vec3>> seen;
  Placement one;
  one.players = 1;
  CHECK(place_players(
            spawn, one,
            [&](Raw radius, const Vec3 &pos) {
              seen.push_back({radius, pos});
              return f(20);
            },
            [](const Vec3 &, Raw, Raw, bool) -> Vec3 {
              throw Error("slot 0 is not searched");
            }) == std::vector<unsigned>({0}));
  CHECK(seen.size() == 1 && seen[0].first == f(12) &&
        seen[0].second == v(3, -4, 0));
  CHECK(spawn.pos[0] == v(3, -4, 20) && spawn.pos[1] == v(9, 9, 1));
  CHECK(spawn.facing == std::array<std::uint16_t, 4>({100, 200, 300, 400}));
  CHECK(spawn.rank == 1);
  auto high = flat_field(-4, -4, 8, 8, 0, 20);
  const Vec3 origin{};
  CHECK(world_queries::terrain_height_below(high, f(4), origin) ==
        world_queries::NO_FLOOR);
  CHECK(world_queries::terrain_height(high, f(4), origin) == f(20));
  CHECK(world_queries::terrain_height(high, PLACEMENT_RADIUS, origin) == f(20));
  auto cold = story_start_ground({}, {}, high);
  CHECK(cold.ground && cold.position == v(0, 0, 20) && !cold.marker_taken);
  auto far = flat_field(0, 0, 4, 4, 0, 8, 400, 0);
  CHECK(world_queries::terrain_height(far, PLACEMENT_RADIUS, origin) ==
        world_queries::NO_FLOOR);
  CHECK(world_queries::terrain_height(far, PLACEMENT_RADIUS, v(400, 0, 0)) ==
        f(8));
  auto marked = story_start_ground(
      {}, {record("Player1", {}, v(400, 0, 0))}, far);
  CHECK(marked.marker_taken && marked.ground && marked.rank == 1 &&
        marked.position == v(400, 0, 8));
  // LS-1: a taken Player1 runs findFreeSpot (spread 36) and a facing draw
  // from g_randSeed (section 17.1-17.2). The seed is not a fixed constant, so
  // the xy is the pre-search marker and the facing is left unset.
  CHECK(marked.pre_search && !marked.facing.has_value());
  // No marker takes: no search and no draw, so the incoming slot and its
  // facing are the original's (section 5.1, the spawn is not cleared).
  SpawnState incoming;
  incoming.facing[0] = 0x1234;
  auto untouched = story_start_ground(incoming, {}, high);
  CHECK(!untouched.pre_search && untouched.facing.has_value() &&
        *untouched.facing == 0x1234);
  CHECK(!cold.pre_search && cold.facing.has_value() && *cold.facing == 0);
  // LS-2: the gate's anyHeight search (JumpGate_ctor, section 17.4) was not
  // re-run on the disc, so an entry that takes a portal is refused.
  StartEntry through_gate;
  through_gate.gate = true;
  bool gate_refused = false;
  try {
    entry_start_ground({}, {record("Portal", {}, v(400, 0, 0))}, through_gate,
                       one, far);
  } catch (const Unsupported &) {
    gate_refused = true;
  }
  CHECK(gate_refused);
  gate_refused = false;
  try {
    entry_start_ground({}, {record("PortalHurt", {}, v(400, 0, 0))},
                       through_gate, one, far);
  } catch (const Unsupported &) {
    gate_refused = true;
  }
  CHECK(gate_refused);
  // The gate flag with no portal takes nothing, so nothing is searched.
  auto no_portal = entry_start_ground(
      {}, {record("Player1", {}, v(400, 0, 0))}, through_gate, one, far);
  CHECK(no_portal.marker_taken && no_portal.ground && no_portal.pre_search &&
        no_portal.position == v(400, 0, 8));
  auto reach = flat_field(2, 0, 4, 2, 30);
  CHECK(world_queries::terrain_height(reach, f(4), origin) ==
        world_queries::NO_FLOOR);
  CHECK(world_queries::terrain_height(reach, PLACEMENT_RADIUS, origin) ==
        f(30));
  auto plateau = flat_field(-40, -40, 80, 80, 0, 20);
  auto kept = search_floor(plateau, 20973);
  CHECK(kept.second == 0 && kept.first == origin);
  SpawnState snapped;
  snapped.pos[0] = kept.first;
  place_players(
      snapped, one,
      [&](Raw radius, const Vec3 &pos) {
        return world_queries::terrain_height(plateau, radius, pos);
      },
      [](const Vec3 &centre, Raw, Raw, bool) { return centre; });
  CHECK(snapped.pos[0][2] == f(20));
  CHECK(player_ctor_z(
            snapped.pos[0], PLACEMENT_RADIUS,
            [&](Raw radius, const Vec3 &pos) {
              return world_queries::terrain_height(plateau, radius, pos);
            }) == f(20));
  auto five = flat_field(-40, -40, 80, 80, 0, 5);
  auto six = flat_field(-40, -40, 80, 80, 0, 6);
  auto low = search_floor(five, 1);
  auto exact = search_floor(six, 1);
  CHECK(low.second == 1 && low.first[2] == f(5));
  CHECK(exact.second == 0 && exact.first[2] == 0);
  world_queries::Level empty;
  auto miss = search_floor(empty, 0);
  CHECK(miss.second == 1 && miss.first[2] == world_queries::NO_FLOOR);
  Raw seen_radius = 0, seen_z = 0;
  CHECK(player_ctor_z(v(1, 2, 99), PLACEMENT_RADIUS,
                      [&](Raw radius, const Vec3 &pos) {
                        seen_radius = radius;
                        seen_z = pos[2];
                        return f(4);
                      }) == f(4));
  CHECK(seen_radius == PLACEMENT_RADIUS && seen_z == f(99));
  auto records = std::vector<Record>{
      record("Player1", {"default=1"}, v(0, 0, 0)),
      record("Player1", {"from=hub"}, v(50, 8, 1))};
  SpawnState stale;
  stale.pos[0] = v(9, 9, 9);
  auto took = run_markers(
      stale, records, "hub", "", false,
      [](const Vec3 &centre, Raw, Raw, bool) { return centre; },
      []() -> std::uint16_t { return 0; });
  CHECK(took.size() == 2 && stale.pos[0][0] == f(50) &&
        stale.pos[0][1] == f(8) && stale.rank == 2);
  Placement recall;
  recall.players = 1;
  recall.restore_xy = std::array<Vec3, 4>{v(1.5f, 2.5f, 0), {}, {}, {}};
  place_players(
      stale, recall, [](Raw, const Vec3 &) { return f(3); },
      [](const Vec3 &centre, Raw, Raw, bool) { return centre; });
  CHECK(stale.pos[0] == v(1.5f, 2.5f, 3));
  SpawnState gate;
  std::vector<std::tuple<Raw, Raw, bool, Vec3>> gate_calls;
  run_markers(
      gate, {record("Portal", {}, v(12, 0, 40))}, "", "", false,
      [&](const Vec3 &centre, Raw spread, Raw radius, bool any) {
        gate_calls.push_back({spread, radius, any, centre});
        return centre;
      },
      []() -> std::uint16_t { return 0x1111; }, false, true, 1);
  CHECK(gate.rank == 3 && gate_calls.size() == 1);
  CHECK(std::get<0>(gate_calls[0]) == f(36) &&
        std::get<1>(gate_calls[0]) == f(24) && std::get<2>(gate_calls[0]) &&
        std::get<3>(gate_calls[0]) == v(12, 0, 40));
  place_players(
      gate, one, [](Raw, const Vec3 &) { return f(7); },
      [](const Vec3 &, Raw, Raw, bool) -> Vec3 {
        throw Error("rank 3");
      });
  CHECK(gate.pos[0] == v(12, 0, 7) && gate.pos[1] == v(0, 0, 0));
  SpawnState none;
  none.pos[0] = v(4, 5, 6);
  none.facing[0] = 99;
  run_markers(
      none, {record("Lantern")}, "hub", "door", false,
      [](const Vec3 &centre, Raw, Raw, bool) { return centre; },
      []() -> std::uint16_t { return 0; });
  place_players(
      none, one, [](Raw, const Vec3 &) { return f(1); },
      [](const Vec3 &centre, Raw, Raw, bool) { return centre; });
  CHECK(none.rank == 0 && none.pos[0] == v(4, 5, 1) && none.facing[0] == 99);
  StartEntry act;
  act.act_changed = true;
  act.prev = "hub";
  Placement act_players;
  act_players.players = 1;
  auto with_act = resolve_start(
      {}, records, act, act_players, [](Raw, const Vec3 &) { return f(3); },
      [](const Vec3 &centre, Raw, Raw, bool) { return centre; },
      []() -> std::uint16_t { return 0; });
  act.act_changed = false;
  auto without_act = resolve_start(
      {}, records, act, act_players, [](Raw, const Vec3 &) { return f(3); },
      [](const Vec3 &centre, Raw, Raw, bool) { return centre; },
      []() -> std::uint16_t { return 0; });
  CHECK(with_act.recall_cleared && !without_act.recall_cleared);
  CHECK(with_act.spawn.pos[0] == without_act.spawn.pos[0] &&
        with_act.spawn.rank == without_act.spawn.rank && with_act.slot0_ground);
  std::array<PlayerRecord, 4> party{};
  party[0].recall_dest = "kept";
  apply_act_entry(party, true, 4, false);
  CHECK(party[0].recall_dest == "kept");
  apply_act_entry(party, true, 0, true);
  CHECK(party[0].recall_dest == "kept");
  apply_act_entry(party, true, 0, false);
  CHECK(party[0].recall_dest == "None");
  CHECK(arena_ring_angle(0, 1) == 0);
  CHECK(arena_ring_angle(1, 1) == -1);
  CHECK(arena_ring_angle(2, 4) == 32766);
  CHECK(arena_ring_angle(2, 3) == -21846);
  refuses([] { arena_ring_angle(0, 0); });
  // arena_respawn forwards to the research-arena port. The three flag-0
  // cases generated from ref_arena_modes (arena_modes_vectors.inc), with the
  // table built through arena_register_spawn.
  {
    ArenaSpawnTable three;
    three = arena_register_spawn(three, -1, Vec3{0x00000000u, 0x00000000u, 0x40a00000u}, 4096);
    three = arena_register_spawn(three, -1, Vec3{0x43000000u, 0x00000000u, 0x3f800000u}, 8192);
    three = arena_register_spawn(three, -1, Vec3{0x43800000u, 0x00000000u, 0x41100000u}, 256);
    const Vec3 camera{0x40400000u, 0x40800000u, 0x40a00000u};
    int searches = 0;
    const ArenaSeededSearch none = [&](const Vec3 &c, int, int, std::uint32_t &) {
      ++searches;
      return std::make_pair(c, 0);
    };
    const auto net = arena_respawn(three, 20973, true, Vec3{}, camera, none);
    CHECK(searches == 0 && net.pos == (Vec3{0x00000000u, 0x00000000u, 0x40a00000u}) &&
          net.facing == 20480 && net.seed == 0x5d3c8a68u);
    const auto near = arena_respawn(three, 7, false, Vec3{}, camera, none);
    CHECK(searches == 0 && near.pos == (Vec3{0x00000000u, 0x00000000u, 0x40a00000u}) &&
          near.facing == 20480 && near.seed == 0x3d20bdbau);
    ArenaSpawnTable far;
    far = arena_register_spawn(far, -1, Vec3{0x43800000u, 0x00000000u, 0x41100000u}, 256);
    std::vector<std::pair<int, int>> spreads;
    const auto fell = arena_respawn(far, 3, false, Vec3{}, camera,
                                    [&](const Vec3 &, int spread, int radius, std::uint32_t &) {
                                      spreads.push_back({spread, radius});
                                      return std::make_pair(camera, 1);
                                    });
    CHECK(spreads.size() == 1 && spreads[0] == std::make_pair(72, 24));
    CHECK(fell.pos == camera && fell.facing == 0 && fell.seed == 3u);
    // A hole below count was never written by a factory; an empty table has
    // no entry to pick.
    ArenaSpawnTable holed;
    holed = arena_register_spawn(holed, 1, Vec3{}, 0);
    holed.count = 2;
    refuses([&] { arena_respawn(holed, 1, true, Vec3{}, camera, none); });
    refuses([&] { arena_respawn(ArenaSpawnTable{}, 1, true, Vec3{}, camera, none); });
  }
  for (auto item : {OpenItem::LiveFindFreeSpotSeed, OpenItem::RealObjectList,
                    OpenItem::ArenaRespawn, OpenItem::ArenaRotationTable,
                    OpenItem::PlayerCtorRemainder,
                    OpenItem::OrientationCopyReaders, OpenItem::SentinelFirstTick,
                    OpenItem::FacingWordPointerStore, OpenItem::GateAnyHeightOnDisc,
                    OpenItem::RecallReturnField, OpenItem::NetPeerIndex})
    refuses([item] { refuse_open(item); });
  refuses([] { arena_net_peer_index(); });
  const Vec3 entry{f(10), f(-4), f(2)};
  std::vector<int> spreads;
  auto retry = [&](const Vec3 &centre, int spread, int radius, bool any) {
    CHECK(radius == 24 && !any);
    spreads.push_back(spread);
    if (spread == ARENA_FIRST_SPREAD)
      return std::make_pair(centre, 0);
    Vec3 pos = centre;
    pos[2] = f(6);
    return std::make_pair(pos, 1);
  };
  auto wide = arena_initial_place(entry, 0, 1, false, retry, true, 0x1000);
  CHECK(spreads == std::vector<int>({36, 240}));
  CHECK(wide.ok == 1 && wide.pos[2] == f(6) && wide.facing == 0x5000);
  spreads.clear();
  auto mode_a = arena_initial_place(
      entry, 0, 1, true,
      [&](const Vec3 &centre, int spread, int, bool) {
        spreads.push_back(spread);
        return std::make_pair(centre, 1);
      },
      true, 0x1000);
  CHECK(spreads == std::vector<int>({36}) && mode_a.ok == 1 &&
        mode_a.facing == 0xd000 && mode_a.pos[2] == entry[2]);
  auto turned = arena_initial_place(
      Vec3{}, 1, 4, true,
      [](const Vec3 &centre, int spread, int, bool) {
        CHECK(spread == 36);
        return std::make_pair(Vec3{f(3), f(4), centre[2]}, 1);
      });
  CHECK(turned.ok == 1 && turned.facing == 0xa5c9);
  struct Plan {
    int slot, count;
    bool mode_a, table, fail_first;
    Vec3 pos;
    std::uint16_t facing;
  };
  const Plan plans[] = {
      {0, 1, false, false, false, {0x42380000u, 0xc07fffdcu, 0x40c00000u}, 0x1},
      {2, 4, true, false, false, {0xc32a0000u, 0xc07dc9a2u, 0x40c00000u}, 0xfffd},
      {1, 4, false, true, false, {0x41200e2du, 0x42000000u, 0x40c00000u}, 0x5234},
      {1, 2, false, false, true, {0xc1d00000u, 0xc07fc74bu, 0x40c00000u}, 0x7ffe},
      {3, 3, true, true, true, {0x433e0000u, 0xc0808dc4u, 0x40c00000u}, 0xd234},
  };
  for (auto &plan : plans) {
    auto placed = arena_initial_place(
        entry, plan.slot, plan.count, plan.mode_a,
        [&](const Vec3 &centre, int spread, int radius, bool any) {
          CHECK(radius == ARENA_SEARCH_RADIUS && !any);
          const bool fail = plan.fail_first && spread == ARENA_FIRST_SPREAD;
          Vec3 pos = centre;
          if (!fail)
            pos[2] = f(6);
          return std::make_pair(pos, fail ? 0 : 1);
        },
        plan.table, 0x1234);
    CHECK(placed.pos == plan.pos && placed.facing == plan.facing &&
          placed.ok == 1);
  }
}
void round4_cases() {
  CHECK(ARENA_MODE_A_COUNT == 13 && ARENA_MODE_B_COUNT == 7 &&
        ARENA_RECORD_BYTES == 6);
  CHECK(mul(cos16(0), ARENA_RING_MODE_A) == ARENA_RING_MODE_A);
  CHECK(mul(sin16(0), ARENA_RING_MODE_A) != 0);
  CHECK(11.25 + std::sqrt(0.5) < 12.0);
  ArenaSpawnTable table;
  table.count = 1;
  table = arena_register_spawn(table, 0, Vec3{}, 0);
  CHECK(table.count == 1 && table.entries.size() == 1 &&
        table.entries[0].occupied && table.entries[0].point == Vec3{});
  const Vec3 start = v(10, -4, 2);
  table = arena_register_spawn(table, 0, start, 0x1234);
  CHECK(table.count == 1 && table.entries[0].point == start &&
        table.entries[0].angle == 0x1234);
  const Vec3 alt = v(3, 4, 5);
  table = arena_register_spawn(table, -1, alt, 0x10);
  CHECK(table.count == 2 && table.entries[0].point == start &&
        table.entries[1].point == alt && table.entries[1].angle == 0x10);
  refuses([] { arena_mode_a_next(-1, 0); });
  refuses([] { arena_mode_a_next(13, 0); });
  refuses([] { arena_register_spawn({}, -2, {}, 0); });
  refuses([] { arena_register_spawn({}, 256, {}, 0); });
  auto covered = flat_field(-40, -40, 80, 80, 0, 4);
  auto stayed = story_start_ground({}, {record("ArenaPlayerStart")}, covered);
  CHECK(stayed.ground && stayed.position == v(0, 0, 4) && !stayed.marker_taken);
  auto bare = flat_field(-20, -20, 60, 60, 0, 0, 180, 0);
  const Vec3 origin{};
  CHECK(world_queries::terrain_height(bare, PLACEMENT_RADIUS, origin) ==
        world_queries::NO_FLOOR);
  const auto centre = arena_ring_centre(origin, 0, 1, true);
  CHECK(centre[0] == ARENA_RING_MODE_A && centre[1] != 0);
  CHECK(world_queries::terrain_height(bare, PLACEMENT_RADIUS, centre) !=
        world_queries::NO_FLOOR);
  auto marked = story_start_ground(
      {}, {record("Player1"), record("ArenaPlayerStart")}, bare);
  CHECK(marked.marker_taken && !marked.ground && marked.position[0] == 0 &&
        marked.position[1] == 0);
  auto alternates =
      story_start_ground({}, {record("ArenaPlayerAlternate")}, bare);
  CHECK(!alternates.ground && !alternates.marker_taken &&
        alternates.position[0] == 0);
  // LS2-2: the original reaches a cold archive with no floor only through
  // the mode-A table (section 18.1), which this module cannot see. The story
  // start stays the story snap (the sentinel); the caller who knows the
  // level was loaded in mode A asks arena_mode_a_start_ground.
  auto wave = story_start_ground({}, {record("ArenaPlayerStart")}, bare);
  CHECK(!wave.ground && !wave.marker_taken && !wave.pre_search &&
        wave.position[0] == 0 && wave.position[1] == 0);
  auto arena = arena_mode_a_start_ground({record("ArenaPlayerStart")}, bare);
  CHECK(arena.ground && arena.marker_taken && arena.pre_search &&
        !arena.facing.has_value());
  // Positions, z and facing of the mode-A start (pre-search and with a
  // seeded search) are reference-generated in level_flow_vectors.inc.
}
void arena_executable_case() {
  namespace arena = champions::simulation::rta::arena_modes;
  // Authored ELF32 fixture: one file-backed segment maps both documented
  // table addresses. The names are synthetic and never come from the disc.
  Blob bytes(0x1300);
  bytes[0] = 0x7f;
  bytes[1] = 'E';
  bytes[2] = 'L';
  bytes[3] = 'F';
  bytes[4] = bytes[5] = 1;
  put(bytes, 0x1c, 0x34);
  put(bytes, 0x2a, 0x20, 2);
  put(bytes, 0x2c, 1, 2);
  put(bytes, 0x34, 1);
  put(bytes, 0x38, 0x100);
  put(bytes, 0x3c, 0x503000);
  put(bytes, 0x44, 0x1200);
  auto place = [&](std::uint32_t table, unsigned count, const std::string &prefix,
                   std::uint32_t start) {
    for (unsigned i = 0; i < count; ++i) {
      const auto name = prefix + std::to_string(i);
      const auto address = start + i * 16;
      put(bytes, 0x100 + table - 0x503000 + 4 * i, address);
      const auto offset = 0x100 + address - 0x503000;
      std::copy(name.begin(), name.end(), bytes.begin() + offset);
      bytes[offset + name.size()] = 0;
    }
  };
  place(arena::NAMES_A_TABLE, 13, "A", 0x503900);
  place(arena::NAMES_B_TABLE, 7, "B", 0x503a00);
  arena::Executable executable;
  CHECK(executable.load(bytes));
  const auto names = arena::read_names(executable).names_a;
  CHECK(names.size() == 13);
  CHECK(arena::mode_a_index(names, "a12") == 12);
  CHECK(arena::mode_a_index(names, "B0") == -1);
  CHECK(arena::mode_a_index(names, "") == -1);
  put(bytes, 0x100 + arena::NAMES_A_TABLE - 0x503000 + 4 * 12, 0);
  CHECK(executable.load(bytes));
  refuses([&] { arena::read_names(executable); });
  bytes[0] = 0;
  CHECK(!executable.load(bytes));
}
} // namespace
// Authored LMP: one record naming "a.world" (4 payload bytes) and a count
// word whose low 16 bits are 1. Research FORMATS.md: the LMP count is u32, so
// the old u16 reading of level_spawn_measure (NET-3) accepted a lump that
// champions::readLmp refuses.
std::vector<std::uint8_t> authored_lmp(std::uint32_t count) {
  std::vector<std::uint8_t> lmp(4 + 12, 0);
  auto put = [&](std::size_t at, std::uint32_t value) {
    for (unsigned k = 0; k < 4; ++k)
      lmp[at + k] = std::uint8_t(value >> (8 * k));
  };
  put(0, count);
  put(4, 16);
  put(8, 24);
  put(12, 4);
  for (char c : std::string("a.world"))
    lmp.push_back(std::uint8_t(c));
  lmp.push_back(0);
  lmp.insert(lmp.end(), {1, 2, 3, 4});
  return lmp;
}
void spawn_tool_lmp_count() {
  std::vector<champions::level_spawn::LmpEntry> entries;
  CHECK(champions::level_spawn::lmp_entries(authored_lmp(1), entries));
  CHECK(entries.size() == 1 && entries[0].name == "a.world" &&
        entries[0].bytes == std::vector<std::uint8_t>{1, 2, 3, 4});
  CHECK(!champions::level_spawn::lmp_entries(authored_lmp(0x00010001u), entries));
  CHECK(entries.empty());
}

int main() {
  try {
    spawn_tool_lmp_count();
    reference_vectors();
    reference_cases();
    round2_cases();
    round3_cases();
    round4_cases();
    loader_world_order();
    robustness_and_state();
    top_floor();
    integration();
    arena_executable_case();
    std::cout
        << "PASS level-flow: " << checks
        << " exact checks; reference fixtures and synthetic gate integration\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL " << e.what() << '\n';
    return 1;
  }
}
