#include "champions/arena_match.hpp"
#include "champions/combat_runtime.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

using namespace champions::simulation::rta;
namespace A = arena_match;
namespace C = combat_runtime;
namespace {
unsigned checks = 0;
void check(bool ok, const std::string &label) {
  ++checks;
  if (!ok) throw std::runtime_error(label);
}
template <class F> void refused(F fn, const std::string &message) {
  std::string failure;
  try { fn(); } catch (const A::Unsupported &e) { failure = e.what(); }
  check(failure.find(message) != std::string::npos, "refusal: " + message + " got " + failure);
}
struct Host {
  A::Memory mem;
  std::vector<A::Address> objects;
  std::vector<std::string> calls;
  std::vector<Raw> increments;
  bool active = true;
  Host() {
    put(A::NUM_PLAYERS, 1); put(A::MODE_B, 1); put(A::INTRO, 0xffff, 2);
    put(A::PLAYERS + 0xac, from_int(20));
    A::Model(mem).reset_scores();
  }
  void put(A::Address a, Raw v, unsigned n = 4) { mem.poke(a, n, v); }
  Raw get(A::Address a, unsigned n = 4) const { return Raw(mem.load(a, n)); }
  A::Services services() {
    A::Services s;
    s.active = [this](unsigned) { return active; };
    s.walk = [this](const std::function<void(A::Address)> &fn) {
      calls.push_back("walk");
      for (auto obj : objects) fn(obj);
    };
    s.prepare = [this] { calls.push_back("prepare"); };
    s.move_off_grid = [this](A::Address) { calls.push_back("off_grid"); };
    s.destroy = [this](A::Address) { calls.push_back("destroy"); };
    s.screen_effect = [this](Raw v) { check(v == 15, "screen effect 15"); calls.push_back("effect"); };
    s.start_wave = [this](Raw v) { calls.push_back("start"); increments.push_back(v); };
    s.advance_level = [this] { calls.push_back("level"); };
    s.intro_cue = [this] { calls.push_back("cue"); };
    s.saved_record = [this](unsigned slot) {
      calls.push_back("saved");
      std::vector<std::uint8_t> bytes(A::RECORD_SIZE);
      for (unsigned i = 0; i < bytes.size(); ++i) bytes[i] = std::uint8_t(i * 7 + slot);
      return bytes;
    };
    s.shutdown = [this] { calls.push_back("shutdown"); };
    s.restart_level = [this] { calls.push_back("restart"); };
    return s;
  }
};
void host_tests() {
  {
    Host h; h.put(A::INTRO, 180, 2);
    A::Model m(h.mem, h.services());
    for (unsigned i = 0; i < 180; ++i) {
      m.visit();
      check(h.get(A::INTRO, 2) == 179 - i, "intro countdown");
      check(h.get(0x41b500) == 1 && h.get(0x503890) == 1 && h.get(0x41b4fc) == 1, "intro locks");
    }
    m.visit();
    check(h.get(A::INTRO, 2) == 0xffff && !h.get(0x41b500) && !h.get(0x503890) && !h.get(0x41b4fc), "visit 181 unlocks");
    check(h.calls == std::vector<std::string>{"cue"}, "one cue and no post-intro scan on unlock visit");
  }
  {
    Host h;
    const A::Address live = 0x10000, corpse = 0x11000;
    h.objects = {live, corpse};
    h.put(live + 12, 0x20); h.put(corpse + 12, 0x28);
    A::Model m(h.mem, h.services());
    h.put(A::FRAME, 127); m.visit();
    check(h.calls.empty(), "no empty scan off 128-frame boundary");
    h.put(A::FRAME, 128); m.visit();
    check(h.get(A::LIVE_COUNT, 2) == 1 && h.get(A::INTERMISSION, 2) == 0, "living enemy blocks wave");
    h.put(live + 12, 0x28); h.put(A::FRAME, 256); m.visit();
    check(h.get(A::LIVE_COUNT, 2) == 0 && h.get(A::INTERMISSION, 2) == 1 && !h.get(A::END), "dead creatures open intermission, not a victory flag");
    h.objects.clear(); // This fixture's walker now supplies an empty grid.
    h.put(A::WAVES_LEFT, 2, 2); h.calls.clear();
    for (unsigned i = 1; i < 480; ++i) m.visit();
    check(h.increments.empty() && h.get(A::INTERMISSION, 2) == 480, "offline pre-increment limit");
    m.visit();
    check(h.increments == std::vector<Raw>{1}, "480 task visits request the next wave");
    check(h.get(A::INTERMISSION, 2) == 481, "expiry still increments before the recording startWave service");
    check(h.calls == std::vector<std::string>({"effect", "prepare", "walk", "start"}), "nextWave ordered calls");
    check(h.get(A::WAVE, 2) == 0, "recording constructor service does not claim a constructed wave");
  }
  {
    Host h; h.put(A::NETWORK, 1, 1); h.put(A::INTERMISSION, 480, 2);
    h.put(A::WAVES_LEFT, 0, 2);
    A::Model m(h.mem, h.services()); m.visit();
    check(h.calls.empty() && h.get(A::INTERMISSION, 2) == 481, "online does not expire at 480");
    h.put(A::INTERMISSION, 960, 2); m.visit();
    check(h.calls == std::vector<std::string>{"level"}, "online 960 requests final-wave level");
  }
  for (bool retry : {false, true}) {
    Host h; h.put(A::FRAME, 1); h.put(A::SCORE + 2, 0xffff, 2);
    h.put(A::SCORE + 4, retry ? 2 : 0, 2); h.put(A::PLAYERS + 0xac, 0);
    A::Model m(h.mem, h.services()); m.visit();
    if (retry) {
      check(h.increments == std::vector<Raw>{0} && !h.get(A::END), "saved positive life retries wave without increment");
    } else {
      check(h.get(A::END) == 1 && h.increments.empty(), "out with no positive snapshot ends");
    }
  }
  for (unsigned lives : {0u, 1u, 2u, 0xffffu}) {
    Host h; h.put(A::FRAME, 1); h.put(A::PLAYERS + 0xac, 0);
    h.put(A::SCORE + 2, lives, 2); h.put(A::COUNTDOWN, lives == 0xffff ? 1 : 0, 2);
    A::Model(h.mem, h.services()).visit();
    check(!h.get(A::END), "nonnegative lives or positive dead countdown stays in");
  }
  {
    Host h; h.put(A::INTERMISSION, 10, 2); h.put(A::PLAYERS + 0xac, 0);
    h.put(A::SCORE + 2, 0xffff, 2); h.put(A::COUNTDOWN, 1, 2);
    A::Model(h.mem, h.services()).visit();
    check(h.get(A::END) == 1 && h.get(A::INTERMISSION, 2) == 0, "intermission wipe ignores dead countdown");
  }
  {
    Host h; h.put(A::MODE_A, 1); h.put(A::KILL_LIMIT, 5, 1); h.put(A::SCORE, 4, 2);
    A::Model m(h.mem, h.services()); m.visit();
    check(h.get(A::INTERMISSION, 2) == 0, "4/5 kill limit stays open");
    h.put(A::SCORE, 5, 2); m.visit();
    check(h.get(A::INTERMISSION, 2) == 1 && h.get(A::FADE) == 0 && h.get(A::RANK, 1) == 0, "5/5 opens ranked intermission");
  }
  {
    Host h; h.put(A::END, 1); h.put(A::FADE, HALF);
    A::Model m(h.mem, h.services()); m.visit(0x100, 0x100, 0);
    check(h.calls.empty(), "buttons ignored before full fade");
    for (unsigned i = 0; i < 100; ++i) m.draw_prompt_step();
    check(h.get(A::FADE) == ONE, "draw fade clamps at one");
    m.visit(0x100, 0x100, 0);
    check(h.calls == std::vector<std::string>({"saved", "shutdown", "restart"}) && h.get(A::MODE_B) == 1, "confirm copies, shuts down, requests restart and keeps mode B");
    for (unsigned i = 0; i < A::RECORD_SIZE; ++i)
      check(h.get(A::PLAYERS + i, 1) == std::uint8_t(i * 7), "complete saved player record including tail");
  }
  {
    Host h; h.put(A::END, 1); h.put(A::FADE, ONE);
    A::Model(h.mem, h.services()).visit(0x100, 0, 0x100);
    check(h.get(A::MODE_B) == 0 && h.get(A::EXIT) == 1 && h.calls.empty(), "cancel exits without level copy");
  }
  {
    Host h; h.put(A::NUM_PLAYERS, 2); h.put(A::END, 1); h.put(A::FADE, ONE);
    auto services = h.services();
    services.saved_record = [&](unsigned slot) {
      if (slot == 1)
        check(h.get(A::PLAYERS, 1) == 0x51, "slot 0 copy precedes slot 1 lookup");
      return std::vector<std::uint8_t>(A::RECORD_SIZE, std::uint8_t(0x51 + slot));
    };
    A::Model(h.mem, services).visit(1, 1, 0);
    check(h.get(A::PLAYERS + A::RECORD_SIZE, 1) == 0x52, "second active saved record copied");
  }
  {
    Host h; const A::Address corpse = 0x10000, player = 0x11000, gone = 0x12000, ptrs = 0x20000;
    h.objects = {corpse, player, gone};
    h.put(corpse + 12, 0x612b); h.put(player + 12, 0x60); h.put(gone + 12, 0x28);
    h.put(0x50388c, ptrs);
    auto services = h.services(); services.saved_capacity = 1;
    A::Model(h.mem, services).prepare_wave();
    check(h.get(corpse + 0x231, 1) == 0xff && h.get(ptrs) == corpse && h.get(A::SAVED_COUNT, 2) == 1, "0x612b corpse is saved in pointer array");
    check(h.calls == std::vector<std::string>({"prepare", "walk", "off_grid", "destroy"}), "alive player skipped, ordinary dead creature destroyed");
    check(h.get(player + 0x231, 1) == 0, "player not marked saved");
  }
  {
    Host h; A::Model m(h.mem);
    refused([&] { m.next_wave(1); }, "arena_startWave 0x305ed4");
    check(h.calls.empty(), "missing wave body does not simulate a spawn");
    h.put(A::NUM_PLAYERS, 2); m.reset_scores();
    refused([&] { m.open_prompt(); }, "qsort tie order");
    h.put(A::SCORE + 12, 5, 2); m.open_prompt();
    check(h.get(A::RANK, 1) == 1, "distinct highest score ranks first");
  }
  {
    Host h; h.put(A::END, 1); h.put(A::FADE, ONE);
    auto services = h.services();
    services.saved_record = [](unsigned) { return std::vector<std::uint8_t>(A::RECORD_SIZE - 1); };
    refused([&] { A::Model(h.mem, services).visit(1, 1, 0); }, "exactly 0x4c30");
    check(h.get(A::PLAYERS + 0xac) == from_int(20) && h.calls.empty(), "short saved record does not overwrite state or shut down");
  }
  {
    C::Services services;
    services.dialog_is_open = [] { return false; };
    services.jumpgate_travel_state = [] { return Raw(0); };
    services.animation_start = [](Raw, Raw, Raw, Raw, Raw) {};
    C::World w(services, 1, C::Composition::Full);
    auto p = w.spawn_player(0, {});
    w.write(A::MODE_B, 4, 1); w.arena_reset_scores();
    w.take_damage(p, 0, 0, from_int(1000));
    w.process_damage(p);
    check(w.read(A::SCORE + 2, 2) == 1 && (w.read(p + 12) & 8) &&
          w.read(A::PLAYERS + 0xac) == 0, "World lethal pending damage reaches arena life loss");
  }
  {
    // The existing native death body calls arena_addKill through its actual
    // shared-memory callback. This is a synthetic base-creature death, not
    // Goblin AI, a real arena, or an automatic loot award.
    C::World w({}, 1, C::Composition::Full);
    auto p = w.spawn_player(0, {});
    auto c = w.spawn_skelight({});
    w.write(A::MODE_B, 4, 1); w.arena_reset_scores();
    w.write(c + 12, 4, w.read(c + 12) | 8);
    w.relink(c + C::LAST_ATTACKER, p);
    w.creature_on_death(c);
    check(w.read(A::SCORE, 2) == 1, "World Creature_onDeath credits the killer");
    w.write(A::INTRO, 2, 0xffff); w.write(A::FRAME, 4, 128);
    // No purchase candidates in this authored task fixture.
    w.write(0x4f8420, 4, 0);
    A::Services services; services.active = [](unsigned) { return true; };
    w.arena_task_visit(services);
    check(w.read(A::LIVE_COUNT, 2) == 0 && w.read(A::INTERMISSION, 2) == 1,
          "native grid walker skips the player and the dead base actor");
    w.write(A::INTERMISSION, 2, 0); w.write(c + 12, 4, w.read(c + 12) & ~8u);
    w.arena_task_visit(services);
    check(w.read(A::LIVE_COUNT, 2) == 1 && w.read(A::INTERMISSION, 2) == 0,
          "native grid walker counts a living base actor");
    w.write(A::INTERMISSION, 2, 480); w.write(A::WAVES_LEFT, 2, 1);
    services.prepare = [] {};
    services.screen_effect = [](Raw) {};
    unsigned starts = 0;
    services.start_wave = [&](Raw) { ++starts; };
    refused([&] { w.arena_task_visit(services); }, "creature destructor missing");
    check(starts == 0, "native adapter does not guess saved-object destructor flags");
  }
}

void vectors(const char *path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("missing arena vectors");
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') continue;
    std::istringstream row(line); std::string kind; row >> kind >> std::hex;
    if (kind == "VISIT") {
      Raw flags, state; int block, action;
      row >> flags >> state >> block >> action;
      check(A::blocks_wave(flags, state) == bool(block), "reference empty callback");
      check(int(A::save_action(flags, state)) == action, "reference saved callback");
    } else if (kind == "KILL") {
      Raw mode, count, flags, slot, owner, amount, before, want0, want1;
      row >> mode >> count >> flags >> slot >> owner >> amount >> before >> want0 >> want1;
      Host h; h.put(A::NUM_PLAYERS, count); h.put(A::MODE_A, mode);
      h.put(A::SCORE, before, 2); h.put(A::SCORE + 12, before, 2);
      const auto killer = flags == 0xffffffff ? 0u : 0x10000u;
      if (killer) { h.put(killer + 12, flags); h.put(killer + 0x3c8, slot, 1); h.put(killer + 0x5a, owner, 1); }
      A::Model(h.mem).add_kill(killer, amount);
      check(h.get(A::SCORE, 2) == want0 && h.get(A::SCORE + 12, 2) == want1, "reference kill credit/wrap");
    } else if (kind == "KILLED") {
      Raw mode, prediction, flags, slot, owner, before, kills, lives, other;
      row >> mode >> prediction >> flags >> slot >> owner >> before >> kills >> lives >> other;
      Host h; h.put(A::NUM_PLAYERS, 2); h.put(A::MODE_A, mode);
      h.put(A::SCORE, before, 2); h.put(A::SCORE + 2, before, 2); h.put(A::SCORE + 12, before, 2);
      const auto victim = 0x10000u, killer = flags == 0xffffffff ? 0u : 0x11000u;
      h.put(victim + 12, 0x60 | (prediction ? 0x20000 : 0)); h.put(victim + 0x3c8, 0, 1);
      if (killer) { h.put(killer + 12, flags); h.put(killer + 0x3c8, slot, 1); h.put(killer + 0x5a, owner, 1); }
      A::Model(h.mem).on_player_killed(victim, killer);
      check(h.get(A::SCORE, 2) == kills && h.get(A::SCORE + 2, 2) == lives && h.get(A::SCORE + 12, 2) == other,
            "reference player death, suicide, owner and prediction rules");
    } else if (kind == "OVERLAY") {
      Raw timer, lives, snapshot, want, want_snapshot;
      row >> timer >> lives >> snapshot >> want >> want_snapshot;
      Host h; h.put(A::SCORE + 8, timer); h.put(A::SCORE + 2, lives, 2); h.put(A::SCORE + 4, snapshot, 2);
      A::Model(h.mem).tick_overlay();
      check(h.get(A::SCORE + 8) == want && h.get(A::SCORE + 4, 2) == want_snapshot, "reference overlay");
    } else if (kind == "WAVE") {
      Raw wave, active, want_level, n; row >> wave >> active >> want_level >> n;
      A::Recipe recipe{}; for (auto &b : recipe) { Raw v; row >> v; b = std::uint8_t(v); }
      const auto groups = A::wave_groups(wave, int(active), {recipe});
      std::vector<int> classes;
      for (auto g : groups) for (int i = 0; i < g.count; ++i) classes.push_back(g.class_index);
      check(classes.size() == n && A::monster_level(wave) == want_level, "reference wave budget and level");
      for (unsigned i = 0; i < n; ++i) { Raw cls; row >> cls; check(classes.at(i) == std::int32_t(cls), "reference recipe split"); }
    } else throw std::runtime_error("unknown arena vector kind");
    if (!row) throw std::runtime_error("truncated arena vector");
  }
}
} // namespace
int main(int argc, char **argv) {
  try {
    set_fpu(Fpu::Model::Ps2);
    if (argc == 2) vectors(argv[1]); else host_tests();
    std::cout << checks << " arena match checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL after " << checks << " checks: " << e.what() << '\n'; return 1;
  }
}
