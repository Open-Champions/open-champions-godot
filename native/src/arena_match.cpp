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
  // research-arena-live@9a0c26a5 tools/ref_arena_modes.py:325-327.
  return add(mul(from_int(s16(wave)), 0x3f2bc579), 0x40a00000);
}
std::vector<Group> wave_groups(Raw wave, int active, const std::vector<Recipe> &recipes) {
  // research-arena-live@9a0c26a5 tools/ref_arena_modes.py:330-365,
  // docs/arena-modes.md:109-123. Bounds below refuse unexecuted table domains.
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
  // research-arena-live@9a0c26a5 tools/ref_arena_modes.py:404-410.
  if (count <= 0) throw Unsupported("arena level rotation requires recipe count");
  const int index = ((s16(wave) + 1) / 3) % count;
  if (index < 0) throw Unsupported("arena negative next-level recipe index");
  return index;
}

// All R5 citations below name research-arena-live at commit 9a0c26a5.
// These checks are host refusal bounds, not additional RTA gameplay rules.
namespace {
void check_tables(const RequestTables &tables) {
  // docs/arena-modes.md:22-40; signed recipe count and seven-entry B table.
  if (tables.recipes.empty() || tables.recipes.size() > 32767)
    throw Unsupported("arena requests require a positive signed recipe count");
  if (tables.levels.size() != 7)
    throw Unsupported("arena requests require seven caller-supplied level pointers");
}
Address class_pointer(const RequestTables &tables, int index) {
  // docs/spec/arena-live-scheduler.md:22-29,59-63. No native factory mapping implied.
  if (index < 0 || std::size_t(index) >= tables.classes.size() || !tables.classes[index])
    throw Unsupported("arena class index/pointer is not bound");
  return tables.classes[index];
}
int upcoming_wave(Raw current, Raw increment) {
  // docs/spec/arena-live-scheduler.md:14-21,94-99,143-148.
  // Only initial -1 -> 0 and nonnegative, nonoverflowing requests are supported.
  const int wave = s16(current);
  if (increment > 1 || wave < -1 || (wave == -1 && increment != 1) ||
      wave + int(increment) > 32767)
    throw Unsupported("arena signed-wave overflow/negative continuation is unestablished");
  return wave + int(increment);
}
} // namespace

WaveRequest Model::wave_request(Raw increment, Raw left, Raw saved) const {
  const auto &requests = *services_.requests;
  check_tables(requests.tables);
  if (!get(MODE_B) || get(MODE_A)) throw Unsupported("arena native requests require mode B without mode A");
  // docs/arena-modes.md:104-107; this research does not establish saved actors.
  if (saved) throw Unsupported("arena saved-creature wave construction remains unbound");
  require(requests.active_count, "arena request active-slot count missing");
  const auto wave = upcoming_wave(get(WAVE, 2), increment);
  if (s16(left) < int(increment) || s16(left) > 3)
    throw Unsupported("arena waves-left outside documented three-wave request domain");
  // docs/spec/arena-live-scheduler.md:16-29 and tools/ref_arena_modes.py:325-401.
  // Existing raw32 arithmetic retains the documented rounding, cap and split.
  WaveRequest result{increment, Raw(wave), Raw(s16(left) - int(increment)),
                     monster_level(Raw(wave)), {}};
  for (const auto group : wave_groups(Raw(wave), requests.active_count(), requests.tables.recipes))
    result.groups.push_back({class_pointer(requests.tables, group.class_index), group.count});
  return result;
}
std::vector<Address> Model::preload_requests() const {
  const auto &tables = services_.requests->tables;
  check_tables(tables);
  std::vector<int> seen;
  std::vector<Address> result;
  // docs/spec/arena-live-scheduler.md:30-34; upcoming three waves, stable
  // first-use deduplication by signed class index, not by pointer identity.
  // tools/difftest_arena_live_scheduler.py:72-79 exercises aligned begin calls.
  const auto current = s16(get(WAVE, 2));
  if (current < -1 || current > 32764 || (current + 1) % 3 != 0)
    throw Unsupported("arena preload requires an established three-wave boundary without signed overflow");
  for (int wave = current + 1; wave <= current + 3; ++wave) {
    const auto &recipe = tables.recipes.at(unsigned((wave / 3) % int(tables.recipes.size())));
    const int rem = wave % 3;
    const int groups = rem == 0 ? 1 : signed_byte(recipe[0]) / 16 - (rem == 1 ? 1 : 0);
    if (groups < 1 || groups > 5)
      throw Unsupported("arena preload recipe group count is unestablished");
    for (int group = 0; group < groups; ++group) {
      const int index = signed_byte(recipe[1 + group]);
      const auto pointer = class_pointer(tables, index);
      if (std::find(seen.begin(), seen.end(), index) == seen.end()) {
        seen.push_back(index);
        result.push_back(pointer);
      }
    }
  }
  return result;
}
Address Model::level_request(bool restart) const {
  const auto &tables = services_.requests->tables;
  check_tables(tables);
  if (get(MODE_A)) throw Unsupported("arena native level request requires mode B without mode A");
  // docs/arena-modes.md:22-26; tools/difftest_arena_terminal.py:69-76.
  // Restart always selects recipe zero, independently of the exhausted wave.
  if (!restart && s16(get(WAVE, 2)) < 0)
    throw Unsupported("arena negative level continuation is unestablished");
  const auto record = restart ? 0 : next_level_record(get(WAVE, 2), int(tables.recipes.size()));
  // docs/spec/arena-live-scheduler.md:41-47,105-111; no victory at table wrap.
  const auto index = tables.recipes.at(unsigned(record))[0] & 0xf;
  if (unsigned(index) >= tables.levels.size() || !tables.levels[index])
    throw Unsupported("arena level index/pointer is not bound");
  return tables.levels[index];
}
void Model::require_start_wave() const {
  if (services_.requests) {
    const auto &r = *services_.requests;
    require(r.prepare_players, "arena_startWave 0x305ed4: player/resource construction unbound");
    require(r.spawn, "arena_startWave 0x305ed4: spawn request service unbound");
    require(r.camera_reset, "arena_startWave 0x305ed4: camera reset service unbound");
    require(r.close_menus, "arena_startWave 0x305ed4: close-local-menus service unbound");
  } else
    require(services_.start_wave, "arena_startWave 0x305ed4: live player/creature/resource construction unbound");
}
void Model::start_wave(Raw increment) {
  require_start_wave();
  if (services_.requests) {
    const auto &r = *services_.requests;
    const auto request = wave_request(increment, get(WAVES_LEFT, 2), get(SAVED_COUNT, 2));
    const auto count = players();
    // research-arena-live@9a0c26a5 docs/arena-modes.md:69-75,97-107,140-146;
    // docs/spec/arena-live-scheduler.md:16-29. Only the documented request
    // stages are native: player/resource work is an explicit required service.
    r.close_menus(0);
    // docs/arena-modes.md:136-138 records the earlier zero store. The
    // unimplemented player stage's internals remain behind prepare_players.
    put(0x503890, 0);
    put(WAVE, request.wave, 2);
    put(WAVES_LEFT, request.waves_left, 2);
    put(0x4f714c, request.monster_level);
    put(INTRO, 180, 2);
    put(INTERMISSION, 0, 2);
    for (unsigned i = 0; i < count; ++i)
      if (s16(get(score(i) + 2, 2)) >= 0)
        put(score(i) + 4, get(score(i) + 2, 2), 2);
    r.prepare_players(request);
    for (const auto group : request.groups) {
      // docs/arena-modes.md:125-134; tools/ref_arena_modes.py:368-401.
      // Do not deduplicate spawn groups: each takes one shared ring draw.
      const Raw seed = get(0x404464) * 0x19660du + 0x3c6ef35fu;
      put(0x404464, seed);
      put(0x8e7364, Raw(65535 / group.count));
      put(0x8e7360, seed >> 16, 2);
      for (int i = 0; i < group.count; ++i) r.spawn(group.class_pointer, 0);
    }
    // docs/arena-modes.md:136-138. No renderer/actor acceptance is inferred.
    put(0x503890, 1);
    r.camera_reset(); // store is in the call's delay slot, before the callee
  } else {
    auto start = services_.start_wave;
    start(increment);
  }
}
void Model::begin() {
  // docs/spec/arena-live-scheduler.md:10-15. Mode B clear is a real no-op.
  if (!get(MODE_B)) return;
  if (get(MODE_A)) throw Unsupported("arena begin mode-A composition remains unbound");
  if (!services_.requests) throw Unsupported("arena begin requires explicit runtime request tables/services");
  require_start_wave();
  (void)players(); // host record bound, before preload callbacks or stores
  require(services_.requests->preload_factory, "arena preload factory 0x14f6c8 missing");
  const auto preloads = preload_requests();
  (void)wave_request(1, 3, 0); // validate before issuing requests
  for (auto pointer : preloads) {
    if (services_.requests->preload_factory(pointer))
      // docs/spec/arena-live-scheduler.md:30-34,94-99: argument 3 observed,
      // non-null destruction not executed. Do not guess destructor semantics.
      throw Unsupported("arena preload non-null factory destruction remains unestablished");
  }
  put(WAVES_LEFT, 3, 2);
  put(SAVED_COUNT, 0, 2);
  start_wave(1);
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
  // research-arena-live@9a0c26a5 docs/arena-modes.md:222-229.
  const auto next = add(get(FADE), 0x3c61029b);
  put(FADE, next);
  if (lt(ONE, next)) put(FADE, ONE);
}
void Model::open_prompt() {
  // research-arena-live@9a0c26a5 docs/arena-modes.md:213-220;
  // qsort ties are still unknown, including the new terminal probe's stub.
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
  // research-arena-live@9a0c26a5 docs/arena-modes.md:90-93;
  // tools/difftest_arena_live_scheduler.py:132-136 executes the call chain.
  preflight_next_wave(increment);
  perform_next_wave(increment);
}
void Model::preflight_next_wave(Raw increment) const {
  require_start_wave();
  require(services_.screen_effect, "arena_nextWave: screen effect missing");
  if (!services_.requests) return; // legacy complete body retains its own domain
  (void)players();
  (void)wave_request(increment, get(WAVES_LEFT, 2), get(SAVED_COUNT, 2));
  require(services_.prepare, "arena_prepareWave 0x3063fc: service 0x19f018 missing");
  require(services_.walk, "arena_prepareWave: original grid walk missing");
  require(services_.move_off_grid, "arena_prepareWave: obj_moveOffGrid missing");
  require(services_.destroy, "arena_prepareWave: creature destructor missing");
  // Host-only refusal preflight, not an extra RTA gameplay transition.
  // research-arena-live@9a0c26a5 docs/arena-modes.md:90-107: preparation
  // can produce the unsupported saved-creature branch. Reject its existing
  // grid inputs before effects, intermission increment, move-off-grid or saves.
  // Classification retains research-arena-match@d4f58da
  // tools/ref_arena_match_core.py:45-67 (the audited R4 callback contract).
  services_.walk([&](Address object) {
    const auto flags = get(object + 12);
    if ((flags & 0x20) && save_action(flags, get(object + 0x312, 2)) == SaveAction::Save)
      throw Unsupported("arena saved-creature preparation remains unbound");
  });
}
void Model::perform_next_wave(Raw increment) {
  auto effect = services_.screen_effect;
  effect(15);
  prepare_wave();
  start_wave(increment);
}
bool Model::still_in(unsigned slot, bool intermission) const {
  // research-arena-live@9a0c26a5 docs/arena-modes.md:195-209.
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
  // research-arena-live@9a0c26a5 docs/arena-modes.md:140-146 and
  // docs/spec/arena-live-scheduler.md:35-40: positive decrement, separate unlock.
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
    // research-arena-live@9a0c26a5 tools/difftest_arena_terminal.py:59-79:
    // exactly 1.0, including the immediately preceding float as a rejection.
    if (!eq(get(FADE), ONE)) return;
    if (pressed & confirm) {
      require(services_.saved_record, "arena confirm: save_playerCopy record service missing");
      require(services_.shutdown, "arena confirm: shutdown service missing");
      Address restart_pointer = 0;
      if (services_.requests) {
        require(services_.requests->copy_level, "arena confirm: pending-level copy service missing");
        restart_pointer = level_request(true);
      } else {
        require(services_.restart_level, "arena confirm: arena_levelName/pending-level service missing");
      }
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
      if (services_.requests) {
        // research-arena-live@9a0c26a5 docs/arena-modes.md:231-236,
        // tools/difftest_arena_modes.py:96; a request only, not a loader result.
        services_.requests->copy_level(0x41b6d0, restart_pointer);
      } else {
        auto restart = services_.restart_level; restart();
      }
    } else if (pressed & cancel) {
      // research-arena-live@9a0c26a5 tools/difftest_arena_terminal.py:77-79.
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
    // research-arena-live@9a0c26a5 docs/spec/arena-live-scheduler.md:35-47:
    // compare the old counter; expiry advances a wave or wraps a level request.
    if (s16(inter) == limit) {
      if (get(MODE_A) || s16(get(WAVES_LEFT, 2)) == 0) {
        Address pointer = 0;
        if (services_.requests) {
          require(services_.requests->copy_level, "arena continuation: pending-level copy service missing");
          require(services_.requests->close_menus, "arena continuation: close-local-menus service missing");
          pointer = level_request(false);
        } else {
          require(services_.advance_level, "arena final-wave pending-level handoff unbound");
        }
        put(INTERMISSION, inter + 1, 2);
        if (services_.requests) {
          // docs/arena-modes.md:77-88: orientation -1, pending request and close(0).
          put(0x41b6d0 + 0x50, 0xff, 1);
          services_.requests->copy_level(0x41b6d0, pointer);
          services_.requests->close_menus(0);
        } else {
          auto advance = services_.advance_level; advance();
        }
      } else {
        // Native request stages own counters with an explicit RequestServices;
        // the legacy complete startWave service otherwise owns these stores.
        preflight_next_wave(1);
        put(INTERMISSION, inter + 1, 2);
        perform_next_wave(1);
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
    // research-arena-live@9a0c26a5 docs/spec/arena-live-scheduler.md:35-40.
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
    else {
      // research-arena-live@9a0c26a5 docs/spec/arena-live-scheduler.md:113-125:
      // 0x306f94 terminal defeat; never treat recipe exhaustion as victory.
      open_prompt(); put(END, 1);
    }
  }
}
} // namespace champions::simulation::rta::arena_match
