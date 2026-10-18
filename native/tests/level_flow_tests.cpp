#include "champions/session_level_flow.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>
using namespace champions;
using namespace champions::simulation::rta;
using namespace champions::simulation::rta::level_flow;
namespace {
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
  refuses([&] { level_manifest(bad); });
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
  CHECK(!t.run({{0, v(0, 0, 0)}}, true).entry);
  event = t.run({{0, v(0, 0, 0)}});
  CHECK(event.entry == std::optional<std::string>("notgo") && !event.active);
  CHECK(Trigger(0, 0, -5, -4, "go").contains(v(-5, 4, 0)));
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
  s.transient.recall_countdown = 60;
  std::array<Vec3, 4> pos{};
  pos[0] = v(1, 2, 3);
  CHECK(!complete_recall(s, 1, true, pos, "hub"));
  CHECK(s.transient.recall_countdown == 60);
  CHECK(!complete_recall(s, 1, false, pos, "hub"));
  // The host scheduler signals the end of the documented 60 ticks.
  s.transient.recall_countdown = 0;
  CHECK(!complete_recall(s, 1, true, pos, "hub"));
  CHECK(complete_recall(s, 1, false, pos, "hub"));
  CHECK(s.pending.name == "hub" && s.players[0].recall_dest == "aa" &&
        s.players[0].recall_position == pos[0]);
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
  pieces.height(0, f(-3.9f));
  CHECK(pieces.height(0) == -3);
  pieces.reset();
  CHECK(pieces.height(0) == -3 && !(terrain.data().elements[0].flags & 2));
  CHECK(pieces.poses()[0].offset == Vec3{});
  pieces.move(-1, {}, 0);
  pieces.height(-1, 0);
  CHECK(pieces.height(-1) == 0);
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
} // namespace
int main() {
  try {
    reference_vectors();
    reference_cases();
    robustness_and_state();
    top_floor();
    integration();
    std::cout
        << "PASS level-flow: " << checks
        << " exact checks; reference fixtures and synthetic gate integration\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL " << e.what() << '\n';
    return 1;
  }
}
