// Synthetic checks of the shared-World adoption pieces: the combat memory
// budget, a class-constructor part over shared bytes, and a factory creature
// the combat World runs through an external owner. Authored fixtures only.
#include "champions/campaign_actors.hpp"
#include "champions/combat_runtime.hpp"
#include "champions/core_state.hpp"
#include "champions/creature_hp.hpp"
#include "champions/detail/checkpoint.hpp"
#include "combat_runtime_internal.hpp"
#include <algorithm>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace R = champions::simulation::rta;
namespace C = R::campaign_actors;
namespace CR = R::combat_runtime;
namespace O = R::objects;
namespace {
void check(bool ok, const char *why) {
  if (!ok) throw std::runtime_error(why);
}
void put(std::vector<std::uint8_t> &b, std::size_t at, std::uint32_t v) {
  for (unsigned i = 0; i != 4; ++i) b.at(at + i) = std::uint8_t(v >> (i * 8));
}
std::vector<std::uint8_t> lump(const std::vector<std::string> &names) {
  std::size_t chars = 0;
  for (const auto &n : names) chars += n.size() + 1;
  const auto table_end = 4 + names.size() * 12;
  std::vector<std::uint8_t> b(table_end + chars + names.size(), 0);
  put(b, 0, std::uint32_t(names.size()));
  std::size_t name_at = table_end, data_at = table_end + chars;
  for (std::size_t i = 0; i < names.size(); ++i) {
    put(b, 4 + i * 12, std::uint32_t(name_at));
    put(b, 8 + i * 12, std::uint32_t(data_at));
    put(b, 12 + i * 12, 1);
    for (char c : names[i]) b[name_at++] = std::uint8_t(c);
    ++name_at;
    b[data_at++] = std::uint8_t(i + 1); // authored synthetic payload
  }
  return b;
}

void memory_budget() {
  R::creature_hp::Memory memory;
  // A level script's store far past the 1 MiB combat budget (audit WA-F4).
  std::vector<std::uint8_t> big(1536 * 1024);
  for (std::size_t i = 0; i < big.size(); ++i) big[i] = std::uint8_t(1 + i % 250);
  memory.write_host_bytes(0x1000000, big);
  check(memory.bytes().size() == big.size(), "script bytes are stored");
  memory.poke(0x1000000, 4, 0x01020304); // pure overwrite of script bytes
  check(memory.load(0x1000000, 4) == 0x01020304, "overwrite past the budget");
  memory.store(0x2000, 4, 0x11223344); // a combat addition
  check(memory.load(0x2000, 4) == 0x11223344, "combat addition past the script size");
  bool refused = false;
  try {
    std::vector<std::uint8_t> too_much(1024 * 1024 + 1, 7);
    memory.write_bytes(0x3000000, too_much);
  } catch (const R::creature_hp::ResourceLimit &) {
    refused = true;
  }
  check(refused, "combat-owned bytes still obey the 1 MiB budget");
}

// Audit VF-8: the WA-F4 property (a script store never makes a combat addition
// throw) must also hold after a checkpoint round trip.
void memory_budget_after_checkpoint() {
  using Codec = R::detail::Checkpoint;
  using Access = R::detail::CheckpointAccess;
  R::creature_hp::Memory original;
  // 60,000 script bytes (a checkpoint restores at most 65,536 sparse entries)
  // plus one combat word. If every restored byte counted as combat, a 1,000,000
  // byte combat addition (under the 1 MiB budget alone) would exceed it.
  std::vector<std::uint8_t> script(60000);
  for (std::size_t i = 0; i < script.size(); ++i) script[i] = std::uint8_t(1 + i % 250);
  original.write_host_bytes(0x1000000, script);
  original.store(0x2000, 4, 0x11223344);
  Codec out;
  Access::run(original, out);
  R::creature_hp::Memory restored;
  Codec in(out.bytes);
  Access::run(restored, in);
  in.finish();
  check(restored.bytes().size() == original.bytes().size(), "checkpoint restores every byte");
  std::vector<std::uint8_t> addition(1000000, 9);
  restored.write_bytes(0x3000000, addition);
  check(restored.load(0x3000000, 1) == 9, "combat addition after a restore with a large script store");
  bool refused = false;
  try {
    std::vector<std::uint8_t> too_much(100000, 7);
    restored.write_bytes(0x5000000, too_much);
  } catch (const R::creature_hp::ResourceLimit &) {
    refused = true;
  }
  check(refused, "combat additions after a restore still obey the 1 MiB budget");
}

// Audit VF-7: clearing a list that holds a node and its partner back to back
// must neither throw (the World destructor is noexcept) nor leak a node.
void clear_nodes_adjacent_partners() {
  R::HandlePool pool(8);
  pool.init();
  const auto free_before = pool.count_free();
  R::Cell head, other;
  R::Object *none = nullptr;
  pool.link_both(head, none, head, none); // both partners in one list, adjacent
  check(head.next && head.next->next && head.next->partner == head.next->next,
        "fixture: adjacent partners");
  pool.add(head, none);                   // a plain node ahead of the pair
  pool.link_both(head, none, other, none); // a pair split across two lists
  bool threw = false;
  try {
    pool.clear_nodes(head);
  } catch (...) {
    threw = true;
  }
  check(!threw, "clear_nodes does not throw for adjacent partners");
  check(head.next == nullptr, "list cleared");
  check(other.next == nullptr, "a partner in another list is unlinked too");
  check(pool.count_free() == free_before, "every node returned to the pool");
}

const std::vector<std::string> BOAR_ENTRIES{
    "warboar.vif", "warboar.tex", "warboar_b.tex", "atak_mid.anm",
    "atak_left_close.anm", "atak_front.anm", "atak_right_close.anm",
    "backup.anm", "charge.anm", "chargetak.anm", "death1.anm",
    "death2.anm", "hit2front.anm", "hit2left.anm", "hit2right.anm",
    "idle1.anm", "roar.anm", "walk.anm", "walk_trn_lft.anm",
    "walk_trn_ryt.anm"};
C::Services boar_actor_services(std::vector<std::string> &calls) {
  C::Services actor_services;
  actor_services.model_hide_mask = [&](std::uint32_t model, std::uint32_t variant) {
    check(model != 0 && variant == 0, "hide mask receives the resolved model slot, variant 0");
    calls.emplace_back("mask");
    return UINT64_C(0x5a5a); // synthetic service result
  };
  actor_services.animation_init = [&](std::uint32_t, bool small) {
    check(small, "boar animation init");
    calls.emplace_back("anim");
  };
  actor_services.set_def_entry_params = [&](std::uint32_t, const C::EventParams &) {
    calls.emplace_back("event");
  };
  actor_services.creature_init = [&](std::uint32_t) { calls.emplace_back("init"); };
  actor_services.process_damage = [&](std::uint32_t) { calls.emplace_back("damage"); };
  actor_services.creature_run = [&](std::uint32_t) { calls.emplace_back("run"); };
  actor_services.tick_input = [&](std::uint32_t) {
    C::TickInput in;
    in.active = true;
    in.state_entry_frame = 0;
    return in;
  };
  actor_services.animation_step = [](std::uint32_t) {};
  actor_services.knockback = [](std::uint32_t) {};
  actor_services.pick_target = [](std::uint32_t) {};
  return actor_services;
}

void adopt_and_run() {
  const auto &boar = BOAR_ENTRIES;
  champions::resources::Manager resources;
  auto bytes = lump(boar);
  resources.load_lmp("warboar.lmp", {bytes.data(), bytes.size()});
  auto authority = std::make_shared<R::WorldAuthority>();
  std::size_t external_runs = 0;
  auto services = CR::flat_fixture_services();
  services.external_run = [&](CR::Address) {
    ++external_runs;
    return true;
  };
  CR::World world(authority, services, 0, CR::Composition::Full);
  std::vector<std::string> calls;
  auto actor_services = boar_actor_services(calls);
  C::Runtime runtime(*authority->objects, resources, actor_services, &authority->memory);
  runtime.set_contain_refusals(true);
  // The host built this object (GameObject_ctor + Creature_ctor) over the
  // shared bytes; here a registered pool object with hit points stands in.
  auto *object = authority->objects->new_object(0x3d8, {0, 0, 0});
  check(object != nullptr, "pool block");
  authority->register_object(*object);
  authority->register_factory(object->addr);
  authority->memory.store(object->addr + 0x228, 4, 0x42c80000);
  authority->memory.store(object->addr + 0x22c, 4, 0x42c80000);
  auto &actor = runtime.adopt(C::Kind::WarBoarSmall, *object, 0, std::int64_t(0));
  const auto at = object->addr;
  check(authority->memory.load(at + 0x48, 4) == 0x517cf0, "class table pointer is in the shared bytes");
  check(authority->memory.load(at + 0xe0, 4) == 0xc1200000 && authority->memory.load(at + 0x3d0, 1) == 1,
        "WarBoarSmall class fields are in the shared bytes");
  check(authority->memory.load(at + 0x220, 8) == 0x5a5a, "hide mask result stored at +0x220");
  check(authority->memory.load(0x502f80 + 8 * 16 + 4, 4) == runtime.definition_entry(C::Kind::WarBoar, 16),
        "definition slot 16 (idle) resolved word");
  check(calls == std::vector<std::string>({"mask", "anim", "event", "event", "event", "event", "event",
                                           "event", "event", "init"}),
        "class constructor order");
  check(actor.health() == 0x42c80000, "hit points are read from the shared bytes");
  // Combat sees the adopted object through its own record.
  world.define_creature_table(0x517cf0, {{4, 0x2ba3a0}, {27, 0x2bac60}});
  world.adopt_creature(at);
  check(world.contains(at), "adopted creature is a combat actor");
  check(world.slot_function(at, 4) == 0x2ba3a0 && world.slot_function(at, 27) == 0x2bac60 &&
            world.slot_function(at, 2) == 0x116638,
        "vtable image: documented overrides over the executed Creature slots");
  // Run list dispatch reaches the external owner once per 60 frames.
  for (unsigned frame = 0; frame < 120; ++frame) world.tick();
  check(external_runs == 2, "slot 4 reaches the external owner on the 60-frame list");
}

// The combat World retains finished iterators only while something still
// refers to them: a long run walks its lists far more than 65,536 times
// (four walks a tick) and must not end at the host's retention budget.
//
// What the prune may release is checked three ways: the closure test below
// (links held by kept ended iterators), the differential run (the same
// simulation with retention Never, Always and Lazy), and the long run.
void prune_keeps_linked_iterators() {
  CR::World::State st(CR::flat_fixture_services(), 1, CR::Composition::Full);
  const CR::Address root = 0x300000;
  for (unsigned k = 0; k < 4; ++k) st.walk(0x300100 + 4 * k, [](CR::Address) {});
  check(st.iterators.size() == 4 && st.ended_iterators.size() == 4, "four ended iterators are retained");
  std::vector<const R::Link *> all;
  for (const auto &it : st.iterators) all.push_back(it.get());
  auto *a = st.iterators[0].get(), *b = st.iterators[1].get(), *c = st.iterators[2].get(),
       *d = st.iterators[3].get();
  // A list head refers to A. A's stale next refers to B and B's stale partner to
  // C (a finished iterator keeps its links). D is referred to by nothing.
  st.head(root).next = a;
  a->next = b;
  b->partner = c;
  st.prune_iterators();
  std::set<const R::Link *> kept;
  for (const auto &it : st.iterators) kept.insert(it.get());
  bool closed = true;
  for (const auto &it : st.iterators)
    for (const auto *ref : {it->next, it->prev, it->partner})
      if (ref && std::find(all.begin(), all.end(), ref) != all.end() && !kept.count(ref)) closed = false;
  const bool as_expected = kept == std::set<const R::Link *>({a, b, c}) && st.ended_iterators == kept;
  (void)d;
  st.prune_iterators();
  const bool stable = st.iterators.size() == kept.size();
  if (!closed || !as_expected || !stable) {
    // Cut every link before failing: a dangling one would crash the unwinding.
    st.head(root).next = nullptr;
    for (const auto &it : st.iterators) it->next = it->prev = it->partner = nullptr;
  }
  check(closed, "no retained iterator holds a link to a released one");
  check(as_expected, "A, and the iterators reached through its links (B, then C), are kept; D goes");
  check(stable, "pruning again changes nothing while the head still refers to A");
  // Drop the root: the whole chain is unreferenced and goes.
  st.head(root).next = nullptr;
  st.prune_iterators();
  check(st.iterators.empty() && st.ended_iterators.empty(), "an unreferenced chain is released");
}

// The sequence of external runs of three adopted boars, the final shared seed
// and the clock, under a retention policy. Pruning is a host choice: the
// simulated visit order must not depend on it.
struct RunTrace {
  std::vector<CR::Address> visits;
  std::vector<std::uint32_t> frames;
  std::uint32_t seed = 0, frame = 0;
  bool operator==(const RunTrace &o) const {
    return visits == o.visits && frames == o.frames && seed == o.seed && frame == o.frame;
  }
};
RunTrace run_trace(CR::IteratorRetention retention, unsigned ticks) {
  champions::resources::Manager resources;
  auto bytes = lump(BOAR_ENTRIES);
  resources.load_lmp("warboar.lmp", {bytes.data(), bytes.size()});
  auto authority = std::make_shared<R::WorldAuthority>();
  RunTrace trace;
  auto services = CR::flat_fixture_services();
  CR::World *self = nullptr;
  services.external_run = [&](CR::Address actor) {
    trace.visits.push_back(actor);
    trace.frames.push_back(std::uint32_t(self->read(CR::FRAME)));
    return true;
  };
  CR::World world(authority, services, 0, CR::Composition::Full);
  self = &world;
  world.set_iterator_retention(retention);
  std::vector<std::string> calls;
  auto actor_services = boar_actor_services(calls);
  C::Runtime runtime(*authority->objects, resources, actor_services, &authority->memory);
  runtime.set_contain_refusals(true);
  world.define_creature_table(0x517cf0, {{4, 0x2ba3a0}, {27, 0x2bac60}});
  for (unsigned n = 0; n < 3; ++n) {
    auto *object = authority->objects->new_object(0x3d8, {0, 0, 0});
    check(object != nullptr, "pool block");
    authority->register_object(*object);
    authority->register_factory(object->addr);
    authority->memory.store(object->addr + 0x228, 4, 0x42c80000);
    authority->memory.store(object->addr + 0x22c, 4, 0x42c80000);
    runtime.adopt(C::Kind::WarBoarSmall, *object, 0, std::int64_t(n));
    world.adopt_creature(object->addr);
  }
  for (unsigned frame = 0; frame < ticks; ++frame) world.tick();
  trace.seed = std::uint32_t(world.read(CR::SHARED_SEED));
  trace.frame = std::uint32_t(world.read(CR::FRAME));
  return trace;
}
void retention_does_not_change_the_walk() {
  const unsigned ticks = 3000; // ~12,000 walks: under the refusal even when nothing is released
  const auto never = run_trace(CR::IteratorRetention::Never, ticks);
  const auto always = run_trace(CR::IteratorRetention::Always, ticks);
  const auto lazy = run_trace(CR::IteratorRetention::Lazy, ticks);
  check(never.frame == ticks, "the run reached the last tick");
  // Three actors on the 60-frame list run once a minute each: 50 visits per actor.
  check(never.visits.size() >= 3 * (ticks / 60 - 1), "the three actors are visited on their run lists");
  check(std::set<CR::Address>(never.visits.begin(), never.visits.end()).size() == 3, "three distinct actors are visited");
  check(never == always, "pruning before every walk gives the same visit order as keeping every iterator");
  check(never == lazy, "the lazy policy gives the same visit order as keeping every iterator");
}
void long_run_releases_iterators() {
  auto authority = std::make_shared<R::WorldAuthority>();
  CR::World world(authority, CR::flat_fixture_services(), 0, CR::Composition::Full);
  for (unsigned frame = 0; frame < 24000; ++frame) world.tick();
  check(world.read(CR::FRAME) == 24000, "24,000 ticks ran (about 96,000 list walks)");
  // Beyond the tick count: the same run with the iterators released at every
  // walk ends in the same clock and shared seed (the walks were not altered).
  auto other = std::make_shared<R::WorldAuthority>();
  CR::World always(other, CR::flat_fixture_services(), 0, CR::Composition::Full);
  always.set_iterator_retention(CR::IteratorRetention::Always);
  for (unsigned frame = 0; frame < 24000; ++frame) always.tick();
  check(always.read(CR::SHARED_SEED) == world.read(CR::SHARED_SEED) && always.read(CR::FRAME) == world.read(CR::FRAME),
        "releasing at every walk leaves the simulated state of a 24,000-tick run unchanged");
}
} // namespace
int main() {
  try {
    memory_budget();
    memory_budget_after_checkpoint();
    clear_nodes_adjacent_partners();
    adopt_and_run();
    prune_keeps_linked_iterators();
    retention_does_not_change_the_walk();
    long_run_releases_iterators();
  } catch (const std::exception &error) {
    std::cerr << "campaign fight adoption test failed: " << error.what() << "\n";
    return 1;
  }
  std::cout << "campaign fight adoption tests passed\n";
  return 0;
}
