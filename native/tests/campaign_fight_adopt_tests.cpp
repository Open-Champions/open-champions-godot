// Synthetic checks of the shared-World adoption pieces: the combat memory
// budget, a class-constructor part over shared bytes, and a factory creature
// the combat World runs through an external owner. Authored fixtures only.
#include "champions/campaign_actors.hpp"
#include "champions/combat_runtime.hpp"
#include "champions/creature_hp.hpp"
#include <iostream>
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

void adopt_and_run() {
  const std::vector<std::string> boar{
      "warboar.vif", "warboar.tex", "warboar_b.tex", "atak_mid.anm",
      "atak_left_close.anm", "atak_front.anm", "atak_right_close.anm",
      "backup.anm", "charge.anm", "chargetak.anm", "death1.anm",
      "death2.anm", "hit2front.anm", "hit2left.anm", "hit2right.anm",
      "idle1.anm", "roar.anm", "walk.anm", "walk_trn_lft.anm",
      "walk_trn_ryt.anm"};
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
} // namespace
int main() {
  try {
    memory_budget();
    adopt_and_run();
  } catch (const std::exception &error) {
    std::cerr << "campaign fight adoption test failed: " << error.what() << "\n";
    return 1;
  }
  std::cout << "campaign fight adoption tests passed\n";
  return 0;
}
