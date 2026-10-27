#include "champions/combat_play.hpp"
#include "champions/combat_ctors.hpp"
#include "champions/hud_player.hpp"
#include <iostream>

namespace H = champions::hud_extras;
namespace R = champions::simulation::rta;
namespace {
unsigned checks = 0;
void check(bool condition, const char *label) {
  ++checks;
  if (!condition) throw std::runtime_error(label);
}
template<class F> void refuses(F body) {
  bool caught = false;
  try { body(); } catch (const H::Refusal &) { caught = true; }
  check(caught, "missing/mismatched live owner must refuse");
}
void player() {
  R::combat_play::Session session(R::session_control::Controller().setup().initial_terrain,
                                 R::combat_play::Fixture::Full);
  auto &world = session.world();
  const auto read = [&](std::uint32_t a, unsigned n) { return world.read(a, n); };
  const auto present = [&](std::uint32_t a) { return world.contains(a); };
  constexpr auto record = R::combat_runtime::PLAYERS;
  const auto object = std::uint32_t(world.read(record + 0x43d8));
  auto before_seed = world.read(R::combat_runtime::SHARED_SEED);
  const auto before_writes = world.memory().log().size();
  const auto initial = H::observePlayer(0, read, present);
  check(initial.hp == session.observation().hp && initial.max_hp == session.observation().max_health,
        "HUD HP comes from the owned Player record");
  check(world.memory().log().size() == before_writes && world.read(R::combat_runtime::SHARED_SEED) == before_seed,
        "observation has no writes or RNG draws");
  // Authored values, deliberately distinct from every fixture default.
  world.write(record + 0xc0, 4, R::from_int(9));
  world.write(record + 0xc4, 4, R::from_int(30));
  world.write(record + 0xb4, 4, R::from_int(11));
  world.write(record + 0xc8, 4, R::from_int(7));
  world.write(record + 0x58, 1, 2); world.write(record + 0x59, 1, 3);
  world.write(record + 0x5c, 4, 1234); world.write(record + 0xe4, 4, 987);
  world.write(record + 0x4a84, 1, 0xff); world.write(record + 0x4a85, 1, 7);
  world.write(record + 0x4440 + 0x20, 2, 0xffff);
  world.write(0x41b828 + 4 * 4, 4, 240); world.write(0x41b940, 4, 3);
  world.write(object + 0x4ae, 2, 0xffff);
  auto p = H::observePlayer(0, read, present);
  check(p.mana == R::from_int(9) && p.max_mana == R::from_int(30) && p.pending_hp == R::from_int(11) && p.pending_mana == R::from_int(7),
        "current, maximum and pending values share the live record");
  check(p.acknowledged == 2 && p.level == 3 && p.experience == 1234 && p.gold == 987,
        "XP, levels and gold are read from live storage");
  check(p.quick_slots[0] == -1 && p.quick_slots[1] == 7 && p.ability_ids[0] == -1 && p.cooldown == -1,
        "signed byte and halfword fields retain their widths");
  check(p.notice_timers[4] == 240 && p.potion_counts[0] == 3, "notice/count producers are read without an invented bag");
  world.write(record + 0xac, 4, R::from_int(41));
  check(H::observePlayer(0, read, present).hp == R::from_int(41), "later record changes are live, not cached companion HP");
  refuses([&] { H::observePlayer(-1, read, present); });
  refuses([&] { H::observePlayer(4, read, present); });
  refuses([&] { H::observePlayer(1, read, present); });
  refuses([&] { H::observePlayer(0, read, [](auto) { return false; }); });
  world.write(object + 0xc, 4, 0x20040);
  refuses([&] { H::observePlayer(0, read, present); });
  world.write(object + 0xc, 4, 0x20);
  refuses([&] { H::observePlayer(0, read, present); });
  world.write(object + 0xc, 4, 0x40); world.write(object + 0x3c8, 4, 1);
  refuses([&] { H::observePlayer(0, read, present); });
}
void gold() {
  R::creature_hp::Memory memory;
  memory.poke(0x44fdcc, 4, 123);
  memory.poke(0x4d8af0 + 3 * 0x4c30 + 0xe4, 4, 987);
  memory.poke(0x86ace8 + 12, 4, 0xffffffff);
  constexpr auto last = 0x86a900 + 7 * 0x24;
  memory.poke(last + 0x1c, 4, 42); memory.poke(last + 0x20, 1, 3);
  memory.poke(last + 0x18, 4, 100); memory.poke(last, 4, R::from_int(90));
  memory.poke(last + 0xc, 4, R::from_int(-2));
  const auto read = [&](std::uint32_t a, unsigned n) { return memory.load(a, n); };
  const auto observed = H::observeGold(read);
  check(observed.frame == 123 && observed.gold[3] == 987 && observed.pending[3] == -1,
        "live gold reads exact record stride and signed pending words");
  check(observed.floaters[7].slot == 3 && observed.floaters[7].start == 100 &&
        observed.floaters[7].amount == 42 && observed.floaters[7].pos[0] == R::from_int(90) &&
        observed.floaters[7].vel[0] == R::from_int(-2), "last of eight floaters retains raw motion and producer identity");
  memory.poke(last + 0x20, 1, 0xff);
  refuses([&] { H::observeGold(read); });
  H::GoldState authoritative, view;
  authoritative.gold[0] = 500;
  H::FloaterStart start{{R::from_int(300), R::from_int(200), 0}, {}, 0};
  H::addGoldFloater(authoritative, 0, 100, start);
  unsigned first_credit = 0, visible = 0;
  for (unsigned frame = 0; frame < 180; ++frame) {
    authoritative.frame = frame;
    H::tickGold(authoritative); // sole authoritative tick/payout owner
    if (!first_credit && authoritative.gold[0] != 500) first_credit = frame;
    H::syncLiveGold(view, authoritative);
    const auto pos = view.floaters[0].pos;
    H::syncLiveGold(view, authoritative);
    check(view.floaters[0].pos == pos, "re-observing preserves local motion");
    H::drawGold(view, {true, false, false, false}, [](auto &) { return 0; }, 0);
    if (view.alpha[0] > 0) ++visible;
    check(view.gold == authoritative.gold && view.pending == authoritative.pending,
          "display never pays a second copy of the pickup");
    if (frame == 10) {
      const auto moved = view.floaters[0].pos;
      H::syncLiveGold(view, authoritative);
      check(view.floaters[0].pos == moved, "live refresh does not restart floater motion");
    }
  }
  check(first_credit == 43 && visible == 133 && view.gold[0] == 600,
        "live gold keeps audited tick-43 credit and 133 draw visibility");
  authoritative.gold[0] = 777;
  H::syncLiveGold(view, authoritative);
  auto hidden = H::drawGold(view, {true, false, false, false}, [](auto &) { return 0; }, 0);
  check(hidden.counters.empty() && view.gold[0] == 777, "direct writers alone never start a counter");
  view.hold[0] = 40; view.alpha[0] = 128;
  authoritative.frame = 0;
  H::syncLiveGold(view, authoritative);
  check(view.hold[0] == 0 && view.alpha[0] == 0 && view.gold[0] == 777,
        "rewinding a continuation discards future display history, retaining live record gold");
}
void gold_producer() {
  // Executed native Gold_onTouch -> floater -> hud_tickGold, with authored
  // geometry and external sound/delete sinks. This is not a synthetic HUD
  // pickup event credited by the presentation.
  namespace CT = R::combat_ctors;
  CT::A::Memory memory;
  CT::A::Model model(memory);
  for (const char *name : {"res_find", "sound_playOnObject", "obj_delete"})
    model.callbacks[name] = [](auto &, const auto &) -> CT::A::Value { return std::int64_t(0); };
  constexpr CT::A::Address pile = 0x180000, player = 0x190000;
  memory.poke(pile + 0x80, 4, 120);
  memory.poke(player + 0xc, 4, 0x40);
  for (unsigned k = 0; k < 16; ++k)
    memory.poke(0x44f140 + 4 * k, 4,
                k == 0 || k == 5 || k == 10 ? R::ONE : k == 15 ? R::from_int(100) : 0);
  memory.poke(0x4f6710, 4, 200); memory.poke(0x4f6730, 4, 50);
  CT::gold_on_touch(model, pile, player);
  const auto read = [&](std::uint32_t a, unsigned n) { return memory.load(a, n); };
  const auto source = H::observeGold(read);
  const auto cosmetic_seed = memory.load(0x404460, 4);
  check(source.floaters[0].amount == 120 && memory.load(pile + 0x80, 4) == 0,
        "executed touch producer feeds the live HUD source exactly once");
  H::GoldState view;
  for (unsigned frame = 0; frame < 130; ++frame) {
    memory.poke(0x44fdcc, 4, frame);
    CT::hud_tick_gold(model);
    const auto writes = memory.log().size();
    const auto observed = H::observeGold(read);
    H::syncLiveGold(view, observed);
    H::drawGold(view, {true, false, false, false}, [](auto &) { return 0; }, 0);
    check(view.gold[0] == memory.load(0x4d8af0 + 0xe4, 4), "counter follows native payout rather than issuing its own");
    check(memory.log().size() == writes && memory.load(0x404460, 4) == cosmetic_seed,
          "live gold observation/drawing writes no authoritative memory or cosmetic seed");
    if (frame == 42) check(view.gold[0] == 0, "native producer does not pay before tick 43");
    if (frame == 43) check(view.gold[0] == 15, "native first payout reaches the displayed source");
  }
  check(view.gold[0] == 120 && view.pending[0] == 0, "native pickup is paid once in full");
}
} // namespace
int main() {
  try {
    player(); gold(); gold_producer();
    std::cout << "HUD_PLAYER_LIVE " << checks << " checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n'; return 1;
  }
}
