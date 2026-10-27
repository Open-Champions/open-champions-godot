#include "champions/arena_match.hpp"
#include <algorithm>
#include <utility>

namespace champions::simulation::rta::arena_match {
namespace {
int signed_byte(Raw v) { return (v & 0x80) ? int(v & 0xff) - 256 : int(v & 0xff); }
template <class F> void require(const F &fn, const char *name) {
  if (!fn) throw Unsupported(name);
}
constexpr Address PURCHASE_COUNT = 0x4f8420;
} // namespace

bool blocks_wave(Raw flags, Raw state) {
  return (flags & 0x68) == 0x20 && s16(state) >= 0;
}
SaveAction save_action(Raw flags, Raw state) {
  if (!(flags & 0x20)) return SaveAction::Skip;
  if ((flags & 0x60) == 0x20 && (!(flags & 8) || (flags & 2)) && s16(state) >= 0)
    return SaveAction::Save;
  if ((flags & 0x40) && !(flags & 8) && s16(state) >= 0)
    return SaveAction::Skip;
  return SaveAction::Destroy;
}
int compare_kills(unsigned a, unsigned b, Raw ka, Raw kb) {
  if (s16(kb) < s16(ka)) return -1;
  if (s16(ka) < s16(kb)) return 1;
  return b < a ? 1 : 0;
}
Raw monster_level(Raw wave) {
  return add(mul(from_int(s16(wave)), 0x3f2bc579), 0x40a00000);
}
std::vector<Group> wave_groups(Raw wave, int active, const std::vector<Recipe> &recipes) {
  if (active < 1 || active > 4 || recipes.empty())
    throw Unsupported("arena wave requires 1..4 active slots and caller-supplied recipes");
  const int w = s16(wave), q = w / 3, rem = w % 3;
  const int index = q % int(recipes.size());
  if (index < 0) throw Unsupported("arena negative recipe index");
  const auto &r = recipes.at(unsigned(index));
  auto acc = add(mul(from_int(q), 0x3f814738), mul(from_int(rem), 0x3f80a393));
  auto count = to_int(le(0, add(acc, TWO)) ? add(acc, 0x40200000)
                                         : sub(add(acc, TWO), HALF));
  if (count >= 11) count = 10;
  count += active - 1;
  if (rem < 0) return {};
  if (rem && (r[0] & 0x80))
    throw Unsupported("arena negative recipe group nibble is unexecuted");
  const int groups = rem == 0 ? 1 : int(r[0] >> 4) - (rem == 1 ? 1 : 0);
  if (groups < 1 || groups > 5 || count < groups)
    throw Unsupported("arena recipe has an unexecuted group count or a zero-size ring group");
  const int per = count / groups, extra = count - per * groups;
  std::vector<Group> out;
  for (int g = 0; g < groups; ++g)
    out.push_back({signed_byte(r[1 + g]), per + (g == 0 ? extra : 0)});
  return out;
}
int next_level_record(Raw wave, int count) {
  if (count <= 0) throw Unsupported("arena level rotation requires recipe count");
  const int index = ((s16(wave) + 1) / 3) % count;
  if (index < 0) throw Unsupported("arena negative next-level recipe index");
  return index;
}

Model::Model(Memory &memory, Services services) : mem_(memory), services_(std::move(services)) {}
Raw Model::get(Address a, unsigned n) const { return Raw(mem_.load(a, n)); }
void Model::put(Address a, Raw v, unsigned n) { mem_.store(a, n, v); }
unsigned Model::players() const {
  const auto n = get(NUM_PLAYERS);
  if (n > 4) throw Unsupported("arena score block has four slots");
  return n;
}
Address Model::score(unsigned slot) const {
  if (slot >= players()) throw Unsupported("arena credit outside active player-record domain");
  return SCORE + SCORE_STRIDE * slot;
}
void Model::reset_scores() {
  const auto n = players();
  for (unsigned i = 0; i < 0x30; ++i) put(SCORE + i, 0, 1);
  for (unsigned slot = 0; slot < n; ++slot) {
    put(score(slot) + 2, 2, 2);
    put(score(slot) + 8, TWO);
    put(PLAYERS + RECORD_SIZE * slot + 0x4c20, get(NETWORK, 1) ? 0x3fa00000 : 0x3fe00000);
  }
}
void Model::add_kill(Address killer, Raw amount) {
  if (get(MODE_A) && !amount) return;
  const auto flags = killer ? get(killer + 12) : 0;
  int slot = -1;
  if (flags & 0x40) slot = int(get(killer + 0x3c8, 1));
  else if ((flags & 0x20) && signed_byte(get(killer + 0x5a, 1)) >= 0)
    slot = signed_byte(get(killer + 0x5a, 1));
  else if (players() == 1) slot = 0;
  if (slot >= 0) {
    const auto at = score(unsigned(slot));
    put(at, get(at, 2) + 1, 2);
  }
}
void Model::lose_life(unsigned slot, bool prediction) {
  if (prediction) return;
  const auto at = score(slot) + (get(MODE_A) ? 0 : 2);
  put(at, get(at, 2) - 1, 2);
}
void Model::on_player_killed(Address victim, Address killer) {
  if (get(victim + 12) & 0x20000) return;
  const auto slot = get(victim + 0x3c8, 1);
  const auto flags = killer ? get(killer + 12) : 0;
  if (killer && (flags & 0x40) && get(killer + 0x3c8, 1) == slot)
    lose_life(slot);
  else if (!killer || (flags & 0x20)) {
    if (get(MODE_A)) add_kill(killer, 1);
    else lose_life(slot);
  } else lose_life(slot);
}
void Model::add_life(unsigned slot) {
  if (get(MODE_B) && !get(MODE_A)) {
    const auto at = score(slot) + 2;
    put(at, get(at, 2) + 1, 2);
  }
}
void Model::tick_overlay() {
  for (unsigned slot = 0; slot < players(); ++slot) {
    const auto at = score(slot);
    const auto timer = get(at + 8);
    if (eq(timer, TWO)) continue;
    auto next = add(timer, 0x3c2af07f);
    put(at + 8, next);
    if (lt(TWO, next)) {
      next = TWO;
      put(at + 8, next);
      put(at + 4, get(at + 2, 2), 2);
    }
  }
}
void Model::draw_prompt_step() {
  const auto next = add(get(FADE), 0x3c61029b);
  put(FADE, next);
  if (lt(ONE, next)) put(FADE, ONE);
}
void Model::open_prompt() {
  std::vector<unsigned> slots;
  for (unsigned i = 0; i < players(); ++i) {
    for (auto j : slots)
      if (get(score(i), 2) == get(score(j), 2))
        throw Unsupported("arena_rankSlots: original qsort tie order is unestablished");
    slots.push_back(i);
  }
  std::sort(slots.begin(), slots.end(), [&](unsigned a, unsigned b) {
    return s16(get(score(a), 2)) > s16(get(score(b), 2));
  });
  for (unsigned i = 0; i < slots.size(); ++i) put(RANK + i, slots[i], 1);
  put(FADE, 0);
}
void Model::prepare_wave() {
  require(services_.prepare, "arena_prepareWave 0x3063fc: service 0x19f018 missing");
  require(services_.walk, "arena_prepareWave: original grid walk missing");
  require(services_.move_off_grid, "arena_prepareWave: obj_moveOffGrid missing");
  require(services_.destroy, "arena_prepareWave: creature destructor missing");
  auto prepare = services_.prepare;
  prepare();
  put(LIVE_COUNT, 0, 2);
  auto walk = services_.walk;
  walk([&](Address obj) {
    const auto flags = get(obj + 12);
    // d4f58da ref_arena_match_core.save_wave_visit: non-creatures have no state.
    if (!(flags & 0x20)) return;
    switch (save_action(flags, get(obj + 0x312, 2))) {
    case SaveAction::Skip: return;
    case SaveAction::Destroy: { auto fn = services_.destroy; fn(obj); return; }
    case SaveAction::Save: break;
    }
    const auto count = s16(get(LIVE_COUNT, 2));
    if (count < 0 || unsigned(count) >= services_.saved_capacity)
      throw Unsupported("arena saved-pointer array exceeds the supplied host capacity");
    const auto pointers = get(0x50388c);
    if (!pointers) throw Unsupported("arena saved-pointer array missing");
    if (std::uint64_t(pointers) + 4 * std::uint64_t(services_.saved_capacity) > UINT64_C(0x100000000))
      throw Unsupported("arena saved-pointer array exceeds raw32 address space");
    put(obj + 0x231, 0xff, 1);
    auto fn = services_.move_off_grid;
    fn(obj);
    put(pointers + unsigned(count) * 4, obj);
    put(LIVE_COUNT, Raw(count + 1), 2);
  });
  put(SAVED_COUNT, get(LIVE_COUNT, 2), 2);
}
void Model::next_wave(Raw increment) {
  require(services_.start_wave, "arena_startWave 0x305ed4: live player/creature/resource construction unbound");
  require(services_.screen_effect, "arena_nextWave: screen effect missing");
  auto effect = services_.screen_effect;
  effect(15);
  prepare_wave();
  auto start = services_.start_wave;
  start(increment);
}
bool Model::still_in(unsigned slot, bool intermission) const {
  auto active = services_.active;
  if (!active(slot)) return false;
  if (s16(get(score(slot) + 2, 2)) >= 0 || lt(0, get(PLAYERS + RECORD_SIZE * slot + 0xac)))
    return true;
  return !intermission && s16(get(COUNTDOWN + 2 * slot, 2)) > 0;
}
void Model::visit(Raw pressed, Raw confirm, Raw cancel) {
  if (!get(MODE_B)) throw Unsupported("arena_runTask requires mode B");
  if (get(PAUSED)) return;
  const auto intro = s16(get(INTRO, 2));
  if (intro >= 0) {
    if (intro > 180) throw Unsupported("arena intro outside executed domain");
    if (intro) {
      if (!get(MODE_A) && intro - 1 == 0x84)
        require(services_.intro_cue, "arena intro sound resource unbound");
      put(INTRO, Raw(intro - 1), 2);
      for (auto at : {0x41b500u, 0x503890u, 0x41b4fcu}) put(at, 1);
      if (!get(MODE_A) && intro - 1 == 0x84) { auto cue = services_.intro_cue; cue(); }
    } else {
      put(INTRO, 0xffff, 2);
      for (auto at : {0x41b500u, 0x503890u, 0x41b4fcu}) put(at, 0);
    }
    return;
  }
  // The purchase tail is separately documented but not composed here. Refuse
  // instead of silently omitting gold/life writes in a live task.
  if (!get(MODE_A) && get(PURCHASE_COUNT))
    throw Unsupported("arena_runTask: gold-purchase tail and grantLife presentation unbound");
  require(services_.active, "arena_runTask: active-slot service missing");
  if (get(END)) {
    if (!eq(get(FADE), ONE)) return;
    if (pressed & confirm) {
      require(services_.saved_record, "arena confirm: save_playerCopy record service missing");
      require(services_.shutdown, "arena confirm: shutdown service missing");
      require(services_.restart_level, "arena confirm: arena_levelName/pending-level service missing");
      // Lookup and copy each slot before looking up the next, as in the
      // original task. No fixture default stands in for a saved player.
      for (unsigned i = 0; i < players(); ++i) {
        auto active = services_.active;
        if (!active(i)) continue;
        auto saved = services_.saved_record;
        auto bytes = saved(i);
        if (bytes.size() != RECORD_SIZE) throw Unsupported("arena saved record must be exactly 0x4c30 bytes");
        mem_.write_bytes(PLAYERS + RECORD_SIZE * i, bytes);
      }
      auto shutdown = services_.shutdown; shutdown();
      auto restart = services_.restart_level; restart();
    } else if (pressed & cancel) {
      put(MODE_B, 0);
      put(EXIT, 1);
    }
    return;
  }
  tick_overlay();
  const auto inter = get(INTERMISSION, 2);
  if (inter) {
    bool any = false;
    for (unsigned i = 0; i < players(); ++i) any = still_in(i, true) || any;
    if (!any) {
      open_prompt(); put(END, 1); put(INTERMISSION, 0, 2); return;
    }
    const auto limit = get(NETWORK, 1) ? 960 : 480;
    if (s16(inter) == limit) {
      if (get(MODE_A) || s16(get(WAVES_LEFT, 2)) == 0) {
        require(services_.advance_level, "arena final-wave pending-level handoff unbound");
        put(INTERMISSION, inter + 1, 2);
        auto advance = services_.advance_level; advance();
      } else {
        // The complete startWave service owns its wave/left/snapshot stores.
        require(services_.start_wave, "arena_startWave 0x305ed4: live player/creature/resource construction unbound");
        put(INTERMISSION, inter + 1, 2);
        next_wave(1);
      }
      return;
    }
    put(INTERMISSION, inter + 1, 2);
    return;
  }
  if (get(MODE_A)) {
    for (unsigned i = 0; i < players(); ++i)
      if (s16(get(score(i), 2)) >= signed_byte(get(KILL_LIMIT, 1))) {
        open_prompt(); put(INTERMISSION, 1, 2); return;
      }
    return;
  }
  if ((get(FRAME) & 0x7f) == 0) {
    require(services_.walk, "arena empty-wave scan: original grid walk missing");
    put(LIVE_COUNT, 0, 2);
    auto walk = services_.walk;
    walk([&](Address obj) {
      const auto flags = get(obj + 12);
      // d4f58da ref_arena_match_core.empty_wave_visit: flags guard the state read.
      if ((flags & 0x68) == 0x20 && blocks_wave(flags, get(obj + 0x312, 2)))
        put(LIVE_COUNT, get(LIVE_COUNT, 2) + 1, 2);
    });
    if (!get(LIVE_COUNT, 2)) {
      open_prompt(); put(INTERMISSION, 1, 2);
    }
  }
  bool any = false, retry = false;
  for (unsigned i = 0; i < players(); ++i) {
    any = still_in(i, false) || any;
    auto active = services_.active;
    if (active(i) && s16(get(score(i) + 4, 2)) > 0) retry = true;
  }
  if (!any) {
    if (retry) next_wave(0);
    else { open_prompt(); put(END, 1); }
  }
}
} // namespace champions::simulation::rta::arena_match
