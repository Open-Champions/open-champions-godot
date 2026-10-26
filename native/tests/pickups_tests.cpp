// Authored synthetic state only. Regression cases for the original research
// boundaries, including registry deletion and shared/own draw ownership.
#include "champions/pickups.hpp"
#include "champions/combat_ctors.hpp"
#include "champions/script_commands.hpp"
#include "champions/arena_executable.hpp"
#include "champions/player_services.hpp"
#define private public
#include "champions/campaign_fight.hpp"
#undef private
#include "champions/session_combat_runtime_adapter.hpp"
#include "../src/campaign_fight.cpp"
#include <cstring>
#include <iostream>
#include <tuple>
namespace R = champions::simulation::rta;
namespace P = R::pickups;
namespace C = R::combat_runtime;
namespace I = R::items;
namespace S = R::script_items;
namespace {
unsigned checks = 0;
void check(bool yes, const char *why) { ++checks; if (!yes) throw std::runtime_error(why); }
R::Raw raw(float f) { R::Raw x; std::memcpy(&x, &f, 4); return x; }
struct Inventory {
  R::builtins::Variables variables;
  S::Service service{variables};
  std::shared_ptr<I::Tables> tables = std::make_shared<I::Tables>();
  std::shared_ptr<I::Definition> definition = std::make_shared<I::Definition>();
  I::Item item;
  Inventory() {
    definition->name = "authored fixture"; definition->addr = I::A_DEFS;
    definition->count = 1; definition->weight = raw(10); definition->slot = 0;
    definition->category = 0; definition->level = 1;
    tables->defs.push_back(definition); item.definition = definition; item.count = 1;
    service.bind_tables(tables);
    variables.local_player_slot = [] { return 0; };
    auto &m = variables.memory;
    m.word(S::G_NUM_PLAYERS, 2);
    m.word(definition->addr, 0x10000); m.string(0x10000, definition->name);
    m.byte(definition->addr + 0x1d, 0);
    for (unsigned k = 0; k < 2; ++k) {
      m.word(S::rec(k) + 0x120, raw(90)); m.word(S::rec(k) + 0x124, raw(100));
      m.w16(S::rec(k) + 0x4b08, 0xffff);
    }
  }
  std::vector<unsigned> take(unsigned slot = 0) {
    return P::take_item(service, variables.memory, *tables, item.pack(), slot);
  }
};
void inventory_limits() {
  { Inventory i; i.variables.memory.word(S::bag_count_at(0, 0), 62);
    check(i.take() == std::vector<unsigned>{0}, "62 entries accept at the exact weight limit");
    check(i.variables.memory.word(S::bag_count_at(0, 0)) == 63, "pickup inserts into the actual bag");
    check(i.service.counts.given == 1, "one inventory insertion"); }
  for (unsigned count : {63u, 64u}) {
    Inventory i; i.variables.memory.word(S::bag_count_at(0, 0), count);
    check(i.take().empty() && i.service.counts.given == 0, "63/64 refuse before insertion");
  }
  { Inventory i; i.definition->weight = raw(11);
    check(i.take().empty(), "load plus weight greater than limit refuses"); }
  { Inventory i; i.item.flags = UINT64_C(1) << 39;
    check(i.take(1) == std::vector<unsigned>({0, 1}), "bit 39 tries all slots in order");
    check(i.service.counts.given == 2, "both eligible recipients receive the record"); }
  { Inventory i; i.item.flags = UINT64_C(1) << 39;
    i.variables.memory.word(S::bag_count_at(0, 0), 63);
    check(i.take(1) == std::vector<unsigned>{1}, "a full first recipient does not stop shared pickup"); }
  { Inventory i; check(i.take(1) == std::vector<unsigned>{1}, "ordinary item only tries chosen slot"); }
  { Inventory i;
    auto &m = i.variables.memory;
    m.string(0x537e08, i.definition->name);
    const auto record = i.item.pack(); m.write(S::bag_entry(0, 0, 0), {record.begin(), record.end()});
    m.word(S::bag_count_at(0, 0), 1);
    check(i.take() == std::vector<unsigned>{0}, "helper accepts a unique-item refusal");
    check(i.service.counts.lost == 1 && m.word(S::bag_count_at(0, 0)) == 1,
          "give's -1 is distinct from helper acceptance"); }
}
void equip_callback() {
  Inventory i; auto &m=i.variables.memory;
  m.byte(S::rec(0)+0x58,1);
  for(unsigned k=0;k<4;++k) m.word(S::rec(0)+0x90+4*k,20);
  i.service.set_record(i.item.pack());
  i.service.call("inventory_giveItem",{0u,0u});
  unsigned notifications=0;
  i.service.delegates["script_runByName"]=[&](const std::vector<std::uint64_t>&){++notifications;return std::uint64_t(0);};
  const auto entry=S::bag_entry(0,0,0);
  i.service.call("inventory_equipItem",{0u,entry,1u});
  check(notifications==1 && m.word(S::rec(0)+0x43f0)==entry,"equip executes the bound notification callback and stores the real bag pointer");
}
void bounds() {
  std::vector<std::uint8_t> model(0x60, 0);
  model[0x12] = 2; model[0x26] = 1;
  const std::array<int, 12> bb{-2,-3,-4,4,5,8,-10,-11,-12,13,14,15};
  for (unsigned mesh = 0; mesh < 2; ++mesh)
    for (unsigned k = 0; k < 6; ++k) model[0x34 + 10 * mesh + k] = std::uint8_t(bb[6 * mesh + k]);
  auto b = P::model_bounds({model.data(), model.size()}, 1);
  check(b == std::array<R::Raw, 6>{R::Raw(-2),R::Raw(-3),R::Raw(-4),4,5,8}, "signed integer bounds and LOD offset");
  b = P::model_bounds({model.data(), model.size()}, 3);
  check(b == std::array<R::Raw, 6>{R::Raw(-10),R::Raw(-11),R::Raw(-12),13,14,15}, "included mesh union");
  b = P::model_bounds({model.data(), model.size()}, 0);
  check(b == std::array<R::Raw, 6>{100000,100000,100000,R::Raw(-100000),R::Raw(-100000),R::Raw(-100000)},
        "zero mask retains original sentinel extrema");
}
struct Ground {
  std::shared_ptr<R::WorldAuthority> authority = std::make_shared<R::WorldAuthority>();
  unsigned bounds_calls = 0, prompts = 0, uses = 0;
  C::Address prompted = 0;
  C::InteractionPrompt prompt{1, true};
  std::function<void(C::Address, R::Raw)> pickup;
  C::Services services() {
    auto s = C::flat_fixture_services();
    s.model_bounds = [this](R::Raw, std::uint64_t) { ++bounds_calls; return std::array<R::Raw,6>{R::Raw(-2),R::Raw(-3),R::Raw(-4),4,5,8}; };
    s.resource = [](const C::ResourceQuery &) { return R::Raw(0x00c00000); };
    s.resref = [](R::Raw) { return R::Raw(0); };
    s.terrain_sweep = [](const R::Vec3 &, const R::Vec3 &, R::Raw, R::Raw) { return std::array<R::Raw,3>{R::ONE,0,0}; };
    s.interact_prompt = [this](C::Address object, R::Raw) { ++prompts; prompted = object; return prompt; };
    s.interact_use = [this](C::Address a, R::Raw slot) { ++uses; if (pickup) pickup(a, slot); };
    return s;
  }
  C::World world{authority, services(), 0, C::Composition::Full};
  Ground() {
    const std::string text = "authored";
    for (auto a : {0x3000u, 0x3100u, 0x3200u}) {
      std::vector<std::uint8_t> bytes(text.begin(), text.end()); bytes.push_back(0);
      world.load_runtime_bytes(a, bytes);
    }
    std::vector<std::uint8_t> model(128,0); model[0x25] = 4;
    world.load_runtime_bytes(0x00c00000, model);
    world.write(0x404464, 4, 20973);
  }
  std::array<std::uint8_t, 0x48> record() {
    std::array<std::uint8_t, 0x48> r{};
    r[0] = 1; r[4] = 1; return r; // authored opaque record; no inventory use here
  }
  C::Address flying() { return world.spawn_item({0,0,raw(36)},0x3000,0x3100,0x3200,record()); }
};
void flight_and_settle() {
  Ground g; const auto a = g.flying();
  check(a && g.world.contains(a) && g.authority->find(a), "flying item is a registered world object");
  check(g.world.read(a + 0x48) == 0x511ba8 && g.world.read(a + 0x238,1) == 1, "PhProp constructor and item marker");
  check(g.bounds_calls == 2 && g.world.read(a + 0x10e,2) == 180, "launch timer and two integer-bounds services");
  check(g.world.read(a+0xe8,8)==1 && !(g.world.read(a+0xc)&0x4800) &&
        g.world.read(a+0x23c)==0xbecccccc,"loot mesh mask, item grid flags and physics coefficient");
  check(g.world.read(C::SHARED_SEED) == R::Raw(std::uint64_t(20973)*0x19660d+0x3c6ef35f), "one shared constructor draw");
  R::Raw own = R::Raw(g.world.read(a+0x44));
  R::Raw expected = R::Raw(g.world.read(C::SHARED_SEED)) >> 16;
  for (unsigned n=0;n<8;++n) expected=R::Raw(std::uint64_t(expected)*0x19660d+0x3c6ef35f);
  check(own == expected, "eight own draws remain separate from shared and cosmetic generators");
  g.world.throw_item(a,0,R::TWO);
  // Original cos16(0)=1, sin16(0)=0x34800000 (not mathematical zero).
  check(g.world.read(a+0xfc)==raw(48) && g.world.read(a+0x100)==0x37400000, "equipment horizontal override");
  check(g.world.read(a+0x1bc)==R::mul(raw(48),0x3c888888), "exact momentum factor");
  g.world.write(a+0x10e,2,std::uint16_t(-221));
  g.world.run_actor(a);
  C::Address replacement=0;
  for (const auto &event:g.world.events()) if(event.name=="item_settled") replacement=C::Address(event.args.at(1));
  check(replacement && g.world.read(replacement+0x48)==0x511c38, "signed timer replaces flight with ItemProp ctor3");
  check(g.world.read(replacement+4)==0 && g.bounds_calls==3, "ctor3 requests neither run list nor fresh bounds (third call is the throw reinit)");
  g.world.destroy(replacement);
  check(!g.world.contains(replacement) && !g.authority->find(replacement), "accepted pickup destructor removes real registry entry");
}
void adopted_chest_lifetime() {
  auto authority = std::make_shared<R::WorldAuthority>();
  R::script_commands::Host host(authority);
  std::map<C::Address, std::shared_ptr<R::builtins::Obj>> chests;
  unsigned chest_ticks = 0, replacement_ticks = 0, retirements = 0;
  auto services = C::flat_fixture_services();
  services.external_run = [&](C::Address address) {
    const auto found = chests.find(address);
    if (found != chests.end() && authority->find(address) == found->second.get() &&
        found->second->alive && found->second->type_name == "5Chest") ++chest_ticks;
    else ++replacement_ticks;
    return true;
  };
  C::World world(authority, services, 0, C::Composition::Full);
  auto construct = [&](C::Address address, const std::string &type) {
    auto owner = host.variables().new_object({}, 0x100018, {0,0,0}, 0, type, {}, address, 0x88);
    authority->register_factory(address);
    world.write(address + 0x48, 4, 0x5118d8);
    if (type == "5Chest") {
      check(chests.emplace(address, owner).second, "Chest address index insertion is unique");
      world.adopt_prop(address, [&, address] { chests.erase(address); ++retirements; });
      world.write(address + 0x84, 4, 1);
    }
    host.variables().world->run_every_frame(*owner);
    return owner;
  };
  const auto address = authority->objects->pool.allocate(0x88);
  auto first = construct(address, "5Chest");
  const auto retained = world.retain(address);
  world.run_actor(address);
  world.destroy(address);
  check(!first->alive && !authority->find(address) && !world.contains(address) && chests.empty(),
        "Chest destruction retires exact host owner, combat view and address state");
  check(!world.target(retained) && !first->heads[0].next && !first->heads[1].next && !first->heads[2].next,
        "Chest destructor clears actual host handle, run and grid heads");
  check(authority->objects->pool.allocate(0x88) == address, "Chest allocation immediately reuses freed block");
  auto second = construct(address, "5Chest");
  check(second.get() != first.get() && authority->find(address) == second.get(), "replacement Chest has distinct registered identity");
  auto hits = world.collect_box({raw(-10),raw(-10),raw(-10)}, {raw(10),raw(10),raw(10)}, 256);
  check(hits == std::vector<C::Address>{address}, "query returns replacement Chest exactly once");
  world.run_actor(address);
  world.destroy(address);
  check(authority->objects->pool.allocate(0x88) == address, "different small-pool object reuses Chest block");
  auto other = construct(address, "10GameObject");
  hits = world.collect_box({raw(-10),raw(-10),raw(-10)}, {raw(10),raw(10),raw(10)}, 256);
  check(hits == std::vector<C::Address>{address} && chests.empty(), "replacement non-Chest is queried without stale Chest index");
  world.run_actor(address);
  check(chest_ticks == 2 && replacement_ticks == 1 && retirements == 2, "replacement dispatch uses live identity and class");
  authority->objects->destroy(*other, 3);
  authority->unregister_object(*other);
}
void chest_branch_refusals() {
  for (const auto &props : std::vector<std::vector<std::string>>{{"action=authored"}, {"action="}, {"name=fixture", "action=authored"}}) {
    bool refused = false;
    try {
      P::require_supported_chest(props, false);
    } catch (const C::Unsupported &error) { refused = std::string(error.what()).find("action script") != std::string::npos; }
    check(refused, "helper rejects every action property, including an empty value");
  }
  bool refused = false;
  try { P::require_supported_chest({}, true); }
  catch (const C::Unsupported &error) { refused = std::string(error.what()).find("timed explosion") != std::string::npos; }
  check(refused, "trapped opening refuses before dispensing or opening");
  P::require_supported_chest({}, false);
  P::require_supported_chest({"name=fixture", "item=authored"}, false);
  check(true, "nontrapped Chest with no action properties has a demonstrable no-op action loop");
}
void production_chest_refusals(const std::string &selected = {}) {
  for (const std::string branch : {"action-use", "action-run", "trap-use", "trap-run"}) {
    if (!selected.empty() && selected != branch) continue;
    auto authority = std::make_shared<R::WorldAuthority>();
    R::script_commands::Host host(authority);
    R::world_queries::Level level(R::world_queries::LevelData{});
    R::player_services::Bundle bundle(level);
    auto executable = std::make_shared<R::arena_modes::Executable>();
    R::campaign_fight::Fight fight(authority, executable, bundle, [] { return false; });
    auto &impl = *fight.p_;
    impl.host = &host;
    impl.item_tables = std::make_shared<I::Tables>();
    const bool running = branch.find("run") != std::string::npos;
    const bool trapped = branch.find("trap") != std::string::npos;
    const auto address = authority->objects->pool.allocate(0x88);
    auto owner = host.variables().new_object({}, 0x100018, {}, 0, "5Chest", {}, address, 0x88);
    authority->register_factory(address);
    auto &chest = impl.chests[address];
    chest.owner = owner;
    if (!trapped) chest.props = {"action=authored"};
    chest.animation = impl.anim->add_state(1);
    R::creature_anim::Clip clip;
    clip.length = 1;
    chest.clip = impl.anim->add_clip(clip);
    fight.world().write(address + 0x48, 4, 0x5118d8);
    fight.world().write(address + 0x4c, 2, 9);
    fight.world().write(address + 0x58, 1, trapped);
    fight.world().write(address + 0x84, 4, running ? 1 : 0);
    fight.world().write(C::PLAYERS + 0xe4, 4, 500);
    if (running) {
      impl.anim->anim_push(chest.animation, chest.clip, 0x204);
      impl.anim->anim_snap_top_to_end(chest.animation);
      host.variables().world->run_every_frame(*owner);
    }
    const auto animation = [&] {
      const auto &state = impl.anim->state(chest.animation);
      std::vector<std::uint64_t> observation{state.pose, std::uint64_t(state.count),
          std::uint64_t(state.capacity), state.flags};
      for (const auto track_id : state.tracks) {
        const auto &track = impl.anim->track(track_id);
        observation.insert(observation.end(), {track_id, track.clip, track.slot,
            track.weight, track.rate, track.life, track.life_rate, track.time,
            track.flags, std::uint64_t(track.sound_idx)});
      }
      return observation;
    };
    const auto run_links = [&] {
      std::vector<const R::Link *> links{&authority->objects->run_active};
      for (const auto &head : authority->objects->run15) links.push_back(&head);
      for (const auto &head : authority->objects->run60) links.push_back(&head);
      for (const auto &head : owner->heads) links.push_back(&head);
      std::set<const R::Link *> seen(links.begin(), links.end());
      std::vector<std::uintptr_t> observation;
      for (std::size_t index = 0; index < links.size(); ++index) {
        const auto *link = links[index];
        observation.insert(observation.end(), {reinterpret_cast<std::uintptr_t>(link),
            reinterpret_cast<std::uintptr_t>(link->next), reinterpret_cast<std::uintptr_t>(link->prev),
            reinterpret_cast<std::uintptr_t>(link->partner), reinterpret_cast<std::uintptr_t>(link->obj),
            std::uintptr_t(link->kind), link->tag, link->object_raw});
        for (const auto *neighbor : {link->next, link->prev, link->partner})
          if (neighbor && seen.insert(neighbor).second) links.push_back(neighbor);
      }
      return observation;
    };
    const auto services = [&] {
      const auto &counts = impl.counts;
      return std::make_tuple(counts.chests_opened, counts.loot_calls, counts.loot_items,
          counts.loot_shared_draws, counts.pickups, counts.pickup_recipients,
          impl.ground_drops.size(), impl.drop_handles, impl.item_resources.size(),
          impl.refusals, fight.world().events().size(), host.items().counts.given,
          host.items().counts.lost);
    };
    const auto bytes_before = authority->memory.bytes();
    const auto chest_before = authority->memory.effective_bytes(address, 0x88);
    const auto gold_before = fight.world().read(C::PLAYERS + 0xe4);
    const auto animation_before = animation();
    const auto links_before = run_links();
    const auto services_before = services();
    const auto owner_before = authority->find(address);
    bool refused = false;
    try {
      if (running) impl.chest_run(address);
      else impl.chest_use(address, 0);
    } catch (const C::Unsupported &error) {
      refused = std::string(error.what()).find(trapped ? "timed explosion" : "action script") != std::string::npos;
    }
    check(authority->memory.bytes() == bytes_before &&
          authority->memory.effective_bytes(address, 0x88) == chest_before,
          "production Chest refusal preserves authoritative bytes, gold, state, flags and seeds");
    check(fight.world().read(C::PLAYERS + 0xe4) == gold_before,
          "production Chest refusal preserves player gold");
    check(animation() == animation_before, "production Chest refusal preserves animation cells");
    check(run_links() == links_before, "production Chest refusal preserves actual World run-list links");
    check(services() == services_before, "production Chest refusal makes no loot, inventory or service changes");
    check(authority->find(address) == owner_before && owner->alive && authority->is_factory(address),
          "production Chest refusal preserves live factory ownership");
    check(refused, "production Chest path must refuse the unsupported action or trap, not an unrelated service");
    if (running) {
      chest.props.clear();
      fight.world().write(address + 0x58, 1, 0);
      check(impl.chest_run(address) && fight.world().read(address + 0x84) == 2 &&
            (fight.world().read(address + 0x62, 2) & 0x80),
            "supported control actually completes via production animation status, not a guessed duration");
      check(run_links() != links_before, "supported completing Chest actually leaves the run list");
    }
  }
}
void save_preflight() {
  Inventory inventory;
  auto &memory = inventory.variables.memory;
  inventory.service.set_record(inventory.item.pack());
  inventory.service.call("inventory_giveItem", {0u,0u});
  bool refused = false;
  try { P::require_supported_save(memory); }
  catch (const S::Refused &error) { refused = std::string(error.what()).find("inventory/equipment") != std::string::npos; }
  check(refused, "save after pickup refuses rather than writing a scalar-only fresh-session reload");
  R::builtins::Memory clean;
  P::require_supported_save(clean);
  clean.word(S::rec(3) + 0x443c, 0x1234);
  refused = false;
  try { P::require_supported_save(clean); } catch (const S::Refused &) { refused = true; }
  check(refused, "equipment alone cannot bypass save preflight");
  clean.word(S::rec(3) + 0x443c, 0);
  clean.word(S::rec(3) + 0x4440, 0x1234);
  P::require_supported_save(clean);
  check(true, "ability data following equipment is not an equipped item pointer");
  refused = false;
  try { P::require_supported_save(clean, true); } catch (const S::Refused &) { refused = true; }
  check(refused, "changed Chest/dropped-item world refuses even without remaining inventory");
  Ground ground;
  ground.world.add_gold_floater(0, {0,0,0}, 9);
  R::builtins::Memory pending(ground.authority->memory);
  for (unsigned tick = 0; tick < 44; ++tick) {
    refused = false;
    try { P::require_supported_save(pending); }
    catch (const S::Refused &error) { refused = std::string(error.what()).find("pending Gold") != std::string::npos; }
    check(refused, "save refuses throughout the delayed Gold credit window");
    ground.world.tick();
  }
  check(pending.word(0x86ace8) == 7, "age 43 moves remaining Gold from floater to pending credit");
  for (unsigned tick = 0; tick < 7; ++tick) {
    refused = false;
    try { P::require_supported_save(pending); }
    catch (const S::Refused &error) { refused = std::string(error.what()).find("pending Gold") != std::string::npos; }
    check(refused, "save refuses until all delayed Gold has reached the balance");
    ground.world.tick();
  }
  P::require_supported_save(pending);
  check(pending.word(C::PLAYERS + 0xe4) == 9, "fully credited Gold leaves scalar save preflight supported");
}
void allocation_loss() {
  Ground g; const auto a = g.flying();
  while (g.authority->objects->pool.allocate(0x190)) {}
  g.world.write(a+0x10e,2,std::uint16_t(-221));
  g.world.run_actor(a);
  bool lost=false;
  for (const auto &e:g.world.events()) if(e.name=="item_settled") lost=e.args.at(1)==0;
  check(lost && (g.world.read(a+0xc)&8), "failed replacement allocation still queues flight deletion");
}
void campaign_use_binding() {
  auto authority = std::make_shared<R::WorldAuthority>();
  R::world_queries::Level level(R::world_queries::LevelData{});
  R::player_services::Bundle bundle(level);
  auto executable = std::make_shared<R::arena_modes::Executable>();
  R::campaign_fight::Fight fight(authority, executable, bundle, [] { return false; });
  check(fight.world().read(0x41b9c0) == 0x80,
        "campaign binds the configured default use action instead of the zero executable initializer");
}
void manual_input() {
  Ground g;
  const auto player = g.world.spawn_player(0, {});
  R::Mat34 matrix{}; matrix[0]=matrix[4]=matrix[8]=R::ONE;
  const auto item = g.world.spawn_resting_item({raw(10),0,0},0x3000,0x3100,0x3200,g.record(),matrix);
  C::SessionAdapter adapter(g.world);
  adapter.bind_button(0, C::World::Button::Use, 0x80);
  g.world.interact(player,C::PADS);
  check(g.prompts==1 && g.uses==0, "prompt is called without a press");
  g.world.pad(0,0x80,0); g.world.interact(player,C::PADS);
  check(g.prompts==2 && g.uses==0, "held use does not activate pickup");
  g.world.pad(0,0,0x80); g.world.interact(player,C::PADS,1);
  check(g.prompts==3 && g.uses==0, "display-only prompt never activates");
  g.world.interact(player,C::PADS);
  check(g.uses==1, "matching remappable pressed-use activates");
  g.world.write(item+0x10,4,raw(80)); g.world.update_grid(item);
  g.world.interact(player,C::PADS);
  check(g.uses==1, "manual query is bounded in x/y");
  g.world.write(item+0x10,4,0); g.world.write(item+0x18,4,raw(61)); g.world.update_grid(item);
  g.world.interact(player,C::PADS);
  check(g.uses==1, "query ends at player z+60");
  g.world.write(item+0x18,4,0); g.world.update_grid(item);
  adapter.bind_button(0, C::World::Button::Use, 0x40);
  g.world.pad(0,0,0x80); g.world.interact(player,C::PADS);
  check(g.uses==1, "old use binding does not activate after remapping");
  g.world.pad(0,0,0x40); g.world.interact(player,C::PADS);
  check(g.uses==2, "configured use binding is read at interaction time");
  adapter.bind_button(0, C::World::Button::Use, 0);
  g.world.interact(player,C::PADS);
  check(g.uses==2, "zero use binding cannot activate a pickup");
}
void carried_equipment() {
  Ground g;
  const auto entry=S::bag_entry(0,0,0);
  g.world.write(C::PLAYERS+0x4434,4,entry);
  g.world.write(C::PLAYERS+0x4a86,1,2);
  C::Spawn start; start.session_record=true;
  g.world.spawn_player(0,start);
  check(g.world.read(C::PLAYERS+0x4a86,1)==2 && g.world.read(C::PLAYERS+0x4434)==entry,
        "session spawn preserves inventory's carried weapon class and pointer");
}
void manual_ranking_and_prompt_status() {
  Ground ground;
  const auto player = ground.world.spawn_player(0, {});
  R::Mat34 matrix{}; matrix[0] = matrix[4] = matrix[8] = R::ONE;
  const auto above = ground.world.spawn_resting_item({0,0,raw(30)},0x3000,0x3100,0x3200,ground.record(),matrix);
  const auto beside = ground.world.spawn_resting_item({raw(20),0,0},0x3000,0x3100,0x3200,ground.record(),matrix);
  ground.world.interact(player,C::PADS);
  check(ground.prompted == beside, "manual ranking uses all three origin coordinates, not horizontal distance");
  ground.world.write(above+0x10,4,raw(-10)); ground.world.write(above+0x18,4,0);
  ground.world.write(beside+0x10,4,raw(10));
  ground.world.update_grid(above); ground.world.update_grid(beside);
  const auto candidates = ground.world.collect_box({raw(-48),raw(-48),0},{raw(48),raw(48),raw(60)},256);
  C::Address first = 0;
  for (auto object : candidates)
    if (object == above || object == beside) { first = object; break; }
  ground.world.interact(player,C::PADS);
  check(first && ground.prompted == first, "strict nearest comparison preserves the first query candidate on a tie");
  ground.world.write(0x41b9c0,4,0x80);
  ground.prompt = {2,true};
  ground.world.pad(0,0,0x80); ground.world.interact(player,C::PADS);
  check(ground.uses == 0, "status two ignores the ordinary remapped use press");
  ground.world.pad(0,0,0x10); ground.world.interact(player,C::PADS);
  check(ground.uses == 1, "status two activates on pressed bit 0x10");
  ground.prompt = {1,false};
  ground.world.pad(0,0,0x80); ground.world.interact(player,C::PADS);
  check(ground.uses == 1, "an empty prompt never activates slot fourteen");
}
void world_to_inventory() {
  Ground g;
  Inventory fixture;
  R::builtins::Variables variables(g.authority);
  S::Service service(variables);
  service.bind_tables(fixture.tables);
  variables.local_player_slot=[] { return 0; };
  const auto player=g.world.spawn_player(0,{});
  auto &memory=variables.memory;
  memory.word(fixture.definition->addr,0x10000);
  memory.string(0x10000,fixture.definition->name);
  memory.word(S::rec(0)+0x124,raw(100));
  g.pickup=[&](C::Address object,R::Raw slot) {
    std::array<std::uint8_t,0x48> record{};
    for(unsigned k=0;k<record.size();++k) record[k]=std::uint8_t(g.world.read(object+0x148+k,1));
    if(!P::take_item(service,memory,*fixture.tables,record,slot).empty()) g.world.destroy(object);
  };
  const auto flying=g.world.spawn_item({0,0,raw(36)},0x3000,0x3100,0x3200,fixture.item.pack());
  // Authored signed-timer fixture; live flight fidelity remains unverified.
  g.world.write(flying+0x10e,2,std::uint16_t(-221));
  g.world.run_actor(flying);
  C::Address resting=0;
  for(const auto &e:g.world.events()) if(e.name=="item_settled") resting=C::Address(e.args.at(1));
  check(resting && g.world.contains(resting),"actual flight replacement is available for manual pickup");
  // Finish the frame's queued flying-object deletion, without requesting
  // unrelated player animation in this service-only fixture.
  g.world.write(player+0xc,4,g.world.read(player+0xc)|0x8000);
  g.world.tick();
  g.world.write(0x41b9c0,4,0x80);
  g.world.interact(player,C::PADS);
  check(memory.word(S::bag_count_at(0,0))==0 && g.world.contains(resting),"prompt alone leaves world and inventory unchanged");
  g.world.pad(0,0,0x80);
  g.world.interact(player,C::PADS);
  check(memory.word(S::bag_count_at(0,0))==1 && service.counts.given==1,"pressed pickup writes the shared inventory record");
  check(!g.world.contains(resting) && !g.authority->find(resting),"successful pickup removes the actual resting object");
  g.world.interact(player,C::PADS);
  check(service.counts.given==1,"the removed world item cannot grant twice");
}
void gold_contact_and_credit() {
  for (float distance : {35.99f,36.f}) {
    Ground g;
    const auto p=g.world.spawn_player(0,{});
    g.world.write(p+0x1c,4,raw(12));
    g.world.write(C::PLAYERS+0xe4,4,500);
    const auto gold=g.world.spawn_gold({raw(distance),0,0},9);
    check(g.world.read(gold+0x1c)==raw(24) && g.world.read(gold+0x20)==raw(72), "Gold keeps base radius and height");
    g.world.move_with_collision(p,{});
    check(g.world.read(gold+0x80)==9, "41-tick countdown prevents credit on spawn");
    for(unsigned n=0;n<41;++n) g.world.run_actor(gold);
    check(g.world.contains(gold) && g.world.read(gold+0x4c,1)==0, "countdown never despawns Gold");
    g.world.move_with_collision(p,{});
    if(distance==36.f) { check(g.world.read(gold+0x80)==9, "strict 36-unit contact boundary"); continue; }
    check(g.world.read(gold+0x80)==0 && g.world.read(C::PLAYERS+0xe4)==500, "touch clears amount but requests delayed HUD credit");
    g.world.move_with_collision(p,{});
    // This fixture measures movement/contact and HUD scheduling, without an
    // unrelated player animation service. Retain the live player record.
    g.world.write(p+0xc,4,g.world.read(p+0xc)|0x8000);
    for(unsigned age=0;age<43;++age) {
      g.world.tick(); check(g.world.read(C::PLAYERS+0xe4)==500,"ages 0-42 do not credit");
    }
    g.world.tick();
    check(g.world.read(C::PLAYERS+0xe4)==502 && g.world.read(0x86ace8)==7, "age 43 transfers two of nine");
    for(unsigned n=0;n<7;++n) g.world.tick();
    check(g.world.read(C::PLAYERS+0xe4)==509, "remaining seven settle once, with no duplicate touch award");
  }
  { Ground g;
    for(unsigned k=0;k<8;++k) g.world.write(0x86a91c+0x24*k,4,1);
    g.world.write(C::PLAYERS+0xe4,4,99999995);
    g.world.add_gold_floater(0,{},9);
    check(g.world.read(C::PLAYERS+0xe4)==99999999, "full floater queue credits immediately with cap"); }
}
void container_gold_branch() {
  for(bool fail : {false,true}) {
    Ground g;
    const auto source=g.world.spawn_skelight({});
    g.world.write(source+0x13c,4,1); g.world.write(source+0x124,4,1);
    g.world.write(source+0x44,4,0); // first own draw 15470, below 24000
    const auto shared=g.world.read(C::SHARED_SEED);
    if(fail) while(g.authority->objects->pool.allocate(0xc0)) {}
    const auto gold=g.world.broken_container_gold(source,raw(8.8f));
    if(fail) {
      check(!gold && g.world.read(source+0x44)==0x3c6ef35f,
            "container allocation failure consumes only the threshold own draw");
      check(g.world.read(C::SHARED_SEED)==shared,"failed container Gold leaves shared seed alone");
    } else {
      const auto second=R::Raw(std::uint64_t(0x3c6ef35f)*0x19660d+0x3c6ef35f);
      constexpr std::array<unsigned,4> amounts{1,9,18,27};
      check(gold && g.world.read(gold+0x80)==amounts[(second>>16)&3],"container amount uses its second own draw");
      check(g.world.read(C::SHARED_SEED)==R::Raw(shared*0x19660d+0x3c6ef35f),"successful Gold construction makes one separate shared draw");
    }
  }
}
void grant_sequences() {
  for (unsigned cls=0;cls<7;++cls) {
    const auto grants=P::starting_grants(cls);
    unsigned copies=0,equips=0;
    for(const auto &g:grants) { copies+=g.copies; equips+=g.equip; }
    check(copies==(cls==3?13u:cls==5?11u:10u),"all seven grant counts");
    check(equips==(cls==3||cls==5?5u:4u),"all seven equip request counts");
    check(grants.front().descriptor==(cls==4?0x5373a0u:0x537310u),"armor group selection");
    check(grants[grants.size()-3].copies==2 && grants[grants.size()-3].variant==-1 &&
          grants[grants.size()-3].announce==0,"consumables parse once and give twice with exact flag");
  }
}
void item_constructors() {
  namespace A = R::arena_monsters;
  R::creature_hp::Memory mem;
  A::Model m(mem);
  const auto str=[&](unsigned a,const std::string &s) {
    std::vector<std::uint8_t> bytes(s.begin(),s.end()); bytes.push_back(0); mem.write_bytes(a,bytes);
  };
  str(0x5326b0,"fixture-lump"); str(0x1000,"fixture-model"); str(0x1100,"fixture-texture");
  mem.poke(0x3e9ce0,4,0x1000); mem.poke(0x3e9ce4,4,0x1100);
  mem.poke(0x404464,4,20973); mem.poke(0x2000,4,0x3000);
  unsigned queries=0,runs=0;
  for(const auto n:{"handle_link","obj_getPropInt","obj_updateGridCell"})
    m.callbacks[n]=[](A::Model &,const std::vector<A::Value>&)->A::Value {return std::int64_t(0);};
  m.callbacks["item_getResourceIndex"]=[](A::Model &,const std::vector<A::Value>&)->A::Value {return std::int64_t(0);};
  m.callbacks["res_find"]=[](A::Model &,const std::vector<A::Value>&)->A::Value {return std::int64_t(0x1234);};
  m.callbacks["obj_runEveryFrame"]=[&](A::Model &,const std::vector<A::Value>&)->A::Value {++runs;return std::int64_t(0);};
  m.callbacks["model_getBounds"]=[&](A::Model &,const std::vector<A::Value>&)->A::Value {
    ++queries;return A::Bounds{{R::Raw(-2),R::Raw(-3),R::Raw(-4),4,5,8}};
  };
  R::combat_ctors::itemprop_ctor2(m,0x4000,{raw(10),raw(20),raw(30)},0x2000,0);
  check(queries==1 && runs==1 && mem.load(0x4048,4)==0x511c38,"ctor2 uses named-launch bounds/run services");
  check(mem.load(0x4190,4)==0 && mem.load(0x40e8,8)==UINT64_MAX,"ctor2 is a Prop with no rigid body");
  R::Mat34 matrix{}; matrix[0]=matrix[4]=matrix[8]=R::ONE; matrix[9]=raw(10);
  R::combat_ctors::itemprop_ctor3(m,0x5000,{raw(10),raw(20),raw(30)},0x2000,matrix);
  check(queries==1 && runs==1 && mem.load(0x5048,4)==0x511c38,"ctor3 uses supplied matrix with no fresh bounds/run request");
  for(unsigned k=0;k<12;++k) check(mem.load(0x5110+4*k,4)==matrix[k],"ctor3 preserves supplied transform");
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc == 2) {
      production_chest_refusals(argv[1]);
      check(checks != 0, "selected production Chest regression exists");
      std::cout << "pickup checks " << checks << " passed\n";
      return 0;
    }
    inventory_limits(); equip_callback(); bounds(); flight_and_settle(); adopted_chest_lifetime(); allocation_loss(); manual_input(); manual_ranking_and_prompt_status(); carried_equipment(); world_to_inventory();
    gold_contact_and_credit(); container_gold_branch(); grant_sequences(); item_constructors(); chest_branch_refusals(); production_chest_refusals(); save_preflight(); campaign_use_binding();
    std::cout << "pickup checks " << checks << " passed\n"; return 0;
  } catch(const std::exception &e) { std::cerr << "pickup check " << checks << ": " << e.what() << '\n'; return 1; }
}
