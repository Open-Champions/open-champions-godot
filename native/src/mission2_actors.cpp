#include "champions/mission2_actors.hpp"
#include "champions/actor_lifetime.hpp"
#include "champions/creature_ai_m1_3.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace champions::simulation::rta::mission2_actors {
namespace {
constexpr Profile PROFILES[] = {
    {Kind::Cutter, 0x24e124, 0x4ff260, 0x514300, 0x24e758, 0x24f90c, 0x500, 26},
    {Kind::Hammer, 0x24e124, 0x4ff260, 0x514300, 0x24e758, 0x24f90c, 0x500, 26},
    {Kind::Droid, 0x27b840, 0x500a88, 0x515da8, 0x27ba78, 0x27c0a0, 0x3d0, 10},
    {Kind::IronGuardian, 0x27a00c, 0x500960, 0x515c60, 0x27a390, 0x27b550, 0x3e8, 34}};
bool clockwork(Kind k) { return k == Kind::Cutter || k == Kind::Hammer; }
namespace ai = creature_ai_m1_3;
ai::Class kernel_class(Kind k) {
  return clockwork(k) ? ai::Class::Clockwork : k == Kind::Droid ? ai::Class::Droid : ai::Class::IronGuardian;
}
std::vector<unsigned> event_entries(Kind k) {
  if (clockwork(k)) return {3,4,5,12,13,14,15,22,6,7,8,16,17,18};
  if (k == Kind::Droid) return {3,9,10};
  return {3,4,5,6,13,7,8};
}
} // namespace
const Profile &profile(Kind k) {
  for (const auto &p : PROFILES) if (p.kind == k) return p;
  throw Unsupported("mission2: invalid class kind");
}
std::optional<Kind> classify(std::uint32_t ctor, const std::string &name) {
  std::string lower = name;
  for (auto &c : lower) c = char(std::tolower(static_cast<unsigned char>(c)));
  if (ctor == 0x24e124) {
    if (lower.rfind("clockworkcutter", 0) == 0) return Kind::Cutter;
    if (lower.rfind("clockworkhammer", 0) == 0) return Kind::Hammer;
  }
  if (ctor == 0x27b840 && lower == "droid") return Kind::Droid;
  if (ctor == 0x27a00c && lower == "ironguardian") return Kind::IronGuardian;
  return std::nullopt;
}
Runtime::Runtime(WorldAuthority &a, resources::Manager &r,
                 const arena_modes::Executable &e, Services s)
    : authority_(a), resources_(r), exe_(e), services_(std::move(s)) {}
Raw Runtime::read(std::uint32_t a, unsigned n) const { return Raw(authority_.memory.load(a, n)); }
void Runtime::write(std::uint32_t a, std::uint64_t v, unsigned n) { authority_.memory.store(a, n, v); }
Raw Runtime::call(std::uint32_t fn, std::uint32_t a, std::vector<Raw> args, std::vector<Raw> floats) {
  const Call request{fn, a, std::move(args), std::move(floats)};
  if (services_.observe) services_.observe(request);
  if (!services_.call) {
    std::ostringstream out;
    out << "mission2 lower service 0x" << std::hex << fn << ": Unknown";
    throw Unsupported(out.str());
  }
  return services_.call(request);
}
Raw Runtime::own(std::uint32_t a) {
  if (services_.observe) services_.observe({0x1509d0, a});
  // GameObject_rand, ref_core: unsigned 32-bit own generator, never shared.
  const Raw seed = read(a + 0x44) * 0x19660du + 0x3c6ef35fu;
  write(a + 0x44, seed);
  if (auto *actor = find(a)) actor->object->seed = seed;
  return seed >> 16;
}
Actor &Runtime::adopt(Kind k, objects::GameObject &o, std::uint32_t facing) {
  retire_stale();
  if (!actor_lifetime::owns_live_object(*authority_.objects, o) ||
      authority_.find(o.addr) != &o || actors_.count(o.addr))
    throw Unsupported("mission2 adopt: needs one live, registered, unadopted object");
  const auto &p = profile(k);
  auto def_it = definitions_.find(p.definition);
  std::shared_ptr<objects::ClassDef> def;
  if (def_it != definitions_.end()) def = def_it->second;
  else {
    const auto lump = exe_.string(exe_.word(p.definition));
    std::vector<std::string> names;
    std::vector<resources::Handle> handles;
    if (lump.empty() || exe_.word(p.definition + 8 + 8 * p.entries))
      throw Unsupported("mission2 constructor: definition table unavailable");
    for (unsigned i = 1; i <= p.entries; ++i) {
      const auto name = exe_.string(exe_.word(p.definition + 8 * i));
      auto handle = resources_.find(lump, name);
      if (name.empty() || !handle || !resources_.bytes(*handle))
        throw Unsupported("mission2 constructor: named definition resource unavailable");
      names.push_back(name);
      handles.push_back(*handle);
    }
    def = std::make_shared<objects::ClassDef>(lump, names);
    for (unsigned i = 0; i < handles.size(); ++i) {
      resources_held_.push_back(handles[i]);
      // Native opaque resource IDs in a disjoint range from boar/player IDs.
      // They are identities, never guessed retail pointers.
      def->entries[i].second = 0x40000000u + std::uint32_t(resources_held_.size());
      write(p.definition + 8 * (i + 1) + 4, def->entries[i].second);
    }
    def->generation = authority_.objects->lump_generation;
    definitions_[p.definition] = def;
  }
  std::unique_ptr<Actor> actor;
  switch (k) {
  case Kind::Cutter: actor = std::make_unique<ClockworkCutter>(o); break;
  case Kind::Hammer: actor = std::make_unique<ClockworkHammer>(o); break;
  case Kind::Droid: actor = std::make_unique<Droid>(o); break;
  case Kind::IronGuardian: actor = std::make_unique<IronGuardian>(o); break;
  }
  const auto a = o.addr;
  const auto frame = read(creature_hp::G_SIM_FRAME);
  o.class_def = def;
  o.vtable = p.vtable;
  o.angle = facing;
  o.radius = k == Kind::Cutter ? 0x41b00000 : k == Kind::Hammer ? 0x41e00000 :
             k == Kind::Droid ? 0x41700000 : 0x420c0000;
  write(a + 0x48, p.vtable);
  write(a + 0x1c, o.radius);
  write(a + 0xb0, clockwork(k) ? 0x3f199999 : k == Kind::Droid ? 0x3fb33333 : 0x3e999999);
  write(a + 0xb4, 4, 1);
  write(a + 0xb8, 0);
  const auto flags = authority_.memory.load(a + 0xa8, 8) |
      (k == Kind::Droid ? UINT64_C(0x22000020400) : UINT64_C(0x22000020000));
  write(a + 0xa8, flags, 8);
  write(a + 0xec, frame);
  write(a + 0xf4, frame + 300u);
  write(a + 0x232, 0, 2);
  if (clockwork(k) || k == Kind::IronGuardian) {
    for (auto off : {0x5c, 0x64, 0x68}) write(a + off, ONE);
    write(a + 0x6c, 0x3f400000);
  }
  if (clockwork(k)) {
    write(a + 0x3c8, k == Kind::Hammer ? 1 : 0, 1);
    write(a + 0x50, a + 0x3d0);
    if (k == Kind::Cutter) { write(a + 0xdc, 0x3f666666); write(a + 0xe8, 8); }
  } else if (k == Kind::Droid) {
    write(a + 0x300, frame);
    write(a + 0x3cc, frame + 3600u);
  } else {
    o.draw_half_width = 0x48;
    write(a + 0x28, 0x48, 1);
    write(a + 0x3c0, 0x453b8000); // 3000.0
    write(a + 0x3c4, 3000);
    write(a + 0x3cc, 0);
    write(a + 0x3d4, 0);
  }
  // The remaining constructor call-outs have no loaded-service results in
  // ffff9a7. Preserve the bounded numeric actor and record that limitation.
  try {
    if (!services_.initialize) throw Unsupported("mission2 constructor: loaded holder/animation/event services Unknown");
    services_.initialize(a, k, event_entries(k));
  } catch (const Unsupported &e) {
    actor->constructor_refusal = e.what();
    ++refusals_[e.what()];
  }
  authority_.objects->run_every_60(o);
  return actor_lifetime::register_actor<Unsupported>(*authority_.objects, actors_,
      std::move(actor), [this](const objects::GameObject &live) {
        return authority_.find(live.addr) == &live;
      });
}
void Runtime::retire_stale() const {
  actor_lifetime::retire_stale(*authority_.objects, actors_,
      [this](const objects::GameObject &live) { return authority_.find(live.addr) == &live; });
}
Actor *Runtime::find(std::uint32_t a) {
  retire_stale();
  const auto it = actors_.find(a);
  return it == actors_.end() ? nullptr : it->second.get();
}
void Runtime::tail(Actor &a) {
  if (clockwork(a.kind)) {
    // Function-local particle-definition init and hammer steam are lower
    // services. Do not replace them with fake particle results or omit draws.
    if (!particle_initialized_) {
      call(0x18f3d0, a.object->addr);
      particle_initialized_ = true;
    }
    if (a.kind == Kind::Hammer) {
      for (unsigned i = 0; i < 2; ++i) {
        const auto draw = own(a.object->addr);
        if ((draw & 1) && !(read(a.object->addr + 0xc) & 2))
          throw Unsupported("Clockwork steam: attach point and velocity draws Unknown");
      }
    }
  } else if (a.kind == Kind::IronGuardian && read(a.object->addr + 0x232, 2) != 0x14) {
    const auto self = a.object->addr;
    bool free_handle = false;
    for (unsigned i = 0; i < 4; ++i) free_handle |= read(self + 0x3d8 + 4 * i) == 0;
    if (!read(self + 0x231, 1) && free_handle && read(self + 0x3d4) < 50 &&
        std::int32_t(read(self + 0x3d0)) < std::int32_t(read(creature_hp::G_SIM_FRAME)))
      throw Unsupported("IronGuardian Droid spawn: rays, handles and mixed-clock timing unexecuted");
  }
}
void Runtime::run(Actor &a) {
  const auto self = a.object->addr;
  if (!call(0x10c0f0, self)) return;
  if (clockwork(a.kind)) call(0x118058, self + 0x3d0, {}, {0x3f000000});
  const auto events = call(0x1162b0, self, {}, {ONE});
  call(0x115eb8, self, {}, {clockwork(a.kind) ? 0x3ecccccc : a.kind == Kind::Droid ? ONE : ZERO});
  call(0x10ded0, self, {0});
  const auto state = read(self + 0x232, 2);
  // C:416-417 / MOD:752-753 (Executed, 99/100/101 boundaries): the fuse is
  // decided before the switch and requested through slot 27; the switch then
  // still runs on the state read above (MOD:749-754).
  if (a.kind == Kind::Droid &&
      ai::droid_fuse_due(state, std::int32_t(read(self + 0x3cc)), std::int32_t(read(self + 0x300))))
    call(profile(a.kind).set_state, self, {0x17});
  switch (state) {
  case 0: {
    const bool seen = call(0x10e658, self) != 0;
    const bool away = !seen && call(0x10eed8, self, clockwork(a.kind) ? std::vector<Raw>{1} : std::vector<Raw>{}) != 0;
    if (seen || away) throw Unsupported("mission2 state0: target/home transition unexecuted");
    break;
  }
  case 3:
    if (read(self + 0x234)) throw Unsupported("mission2 state3: live-target AI unexecuted");
    call(profile(a.kind).set_state, self, {0});
    break;
  case 6:
    // Droid has no melee state 6: its unlisted-state fallback always asks
    // for state 0, even while an animation track is active (monsters-2.md).
    if (a.kind == Kind::Droid) { call(profile(a.kind).set_state, self, {0}); break; }
    if (clockwork(a.kind)) call(0x1187e0, self);
    if (events & 1) {
      const auto fn = clockwork(a.kind) ? 0x119938u : 0x119b58u;
      const Raw flags = a.kind == Kind::Cutter ? 0x40008 : a.kind == Kind::Hammer ? 1 : 8;
      call(fn, self, {flags});
    }
    if (!(read(self + 0x11a, 2) & 8)) call(profile(a.kind).set_state, self, {5});
    break;
  case 0x14:
    if (clockwork(a.kind)) {
      call(0x113c48, self, {}, {ZERO, ZERO, ZERO});
      if (!(read(self + 0xc) & 2) && (events & 6))
        throw Unsupported("Clockwork corpse events: explosion and part shedding unexecuted");
      call(0x1108a0, self, {events, 1}, {0x41a00000});
    } else if (a.kind == Kind::IronGuardian) call(0x1108a0, self, {events, 0}, {0x41f00000});
    else if (events & 0x40) throw Unsupported("Droid killed-body props: PhProp_ctor2 inputs Unknown");
    break;
  case 0x17:
    if (a.kind != Kind::Droid) throw Unsupported("mission2 state0x17: unexecuted");
    if (!(read(self + 0x11a, 2) & 0x80)) throw Unsupported("Droid fuse: unfinished clip branch unexecuted");
    // Exactly the executed explosion's dispatch order. The services own the
    // cosmetic/physics bodies; only the measured radius-damage inputs follow.
    for (auto fn : {0x1ef270u,0x213ea0u,0x1f3d00u,0x17365cu,0x214230u}) call(fn, self);
    call(0x161270, self, {Raw(std::int16_t(read(self + 0xd2, 2))), 0x40840, 0, 0},
         {0x42700000, 0x43100000});
    own(self);
    for (auto fn : {0x19f110u,0x14fdb8u,0x201bd8u}) call(fn, self);
    write(self + 0xc, read(self + 0xc) | 0xa);
    a.object->flags = read(self + 0xc);
    call(0x14f200, self);
    for (unsigned i = 0; i < 10; ++i) {
      call(0x14fdb8, self, {0x248});
      call(0x155a0c, self, {1u << i, 0x112});
    }
    break;
  default: throw Unsupported("mission2 state: unexecuted");
  }
  tail(a);
}
void Runtime::tick(std::uint32_t self) {
  auto *a = find(self);
  if (!a) throw Unsupported("mission2 tick: actor not live");
  // These calls remain live after an AI refusal, so damage/death never freezes
  // with the unsupported AI. Slot 16 is the base model getter for the other
  // two classes; both original prefix calls are retained.
  if (!clockwork(a->kind)) call(0x3c2738, self);
  call(0x110d30, self);
  if (!clockwork(a->kind)) call(0x3c2738, self);
  if (call(0x112948, self)) return;
  if (a->ai_refusal) return;
  try { run(*a); }
  catch (const Unsupported &e) {
    if (!contain_) throw;
    a->ai_refusal = e.what();
    ++refusals_[e.what()];
  }
}
void Runtime::death(Actor &a) {
  const auto self = a.object->addr;
  if (a.death_started) return;
  if (!call(0x10a9c0, self, {0x14})) return;
  a.death_started = true;
  try {
    if (clockwork(a.kind)) {
      write(self + 0xc, read(self + 0xc) | 0x4000);
      if (!(read(self + 0xc) & 2)) {
        if (own(self) % 3 == 0) {
          for (unsigned i = 0; i < 8; ++i) call(0x24fd8c, self, {i, 0});
          write(self + 0xc, read(self + 0xc) | 0x4002);
        } else write(self + 0xc, read(self + 0xc) & ~1u);
      }
    } else if (a.kind == Kind::Droid) write(self + 0xc, read(self + 0xc) | 0x4000);
    if (a.kind != Kind::Droid)
      call(0x10f2c8, self, {0}, {0x42100000,0x40000000,0x3fcccccc,0x3f199999,0x3fb33333,0x3e800000});
    Raw entry = 8; // IronGuardian death definition slot
    if (clockwork(a.kind)) entry = 6 + (a.kind == Kind::Hammer ? 10 : 0) + own(self) % 3;
    else if (a.kind == Kind::Droid) entry = 9 + (own(self) & 1);
    call(0x16cfe0, self, {entry});
    call(0x10beb8, self, {0});
    write(self + 0x232, 0x14, 2);
    a.object->flags = read(self + 0xc);
    a.death_reported = true;
  } catch (const Unsupported &e) {
    a.death_refusal = e.what();
    ++refusals_[e.what()];
    if (!contain_) throw;
  }
}
void Runtime::enter(Actor &a, std::uint32_t requested) {
  const auto self = a.object->addr;
  // Only the entries research-campaign-combat fbb2e1d executed with their
  // landing: Clockwork and IronGuardian 9 (MOD:1184, MOD:1190) and the Droid
  // fuse 0x17 (MOD:1189, C:151-153). Checked before any call, so every other
  // request keeps its refusal without touching the filter service.
  const bool executed = (requested == 9 && a.kind != Kind::Droid) ||
                        (requested == 0x17 && a.kind == Kind::Droid);
  if (!executed) throw Unsupported("mission2 setState: unexecuted non-death transition");
  // Every row ran on a live rig in state 3 (DT:410); no request on a dead
  // actor was executed.
  if (a.death_started || read(self + 0x232, 2) == 0x14)
    throw Unsupported("mission2 setState: request on a dead actor unexecuted");
  ai::SetContext sc;
  sc.target = read(self + 0x234) != 0;
  sc.root = std::uint16_t(read(self + 0x314, 2));
  sc.cooldown = std::uint16_t(read(self + 0x318, 2));
  sc.stun = std::uint16_t(read(self + 0x31c, 2));
  sc.x31a = std::uint16_t(read(self + 0x31a, 2));
  sc.x231 = std::int8_t(read(self + 0x231, 1));
  if (clockwork(a.kind)) sc.variant = read(self + 0x3c8, 1);
  ai::SetResult r;
  try { r = ai::set_state(kernel_class(a.kind), sc, requested); }
  catch (const ai::Unsupported &e) { throw Unsupported(std::string("mission2 setState: ") + e.what()); }
  // Creature_filterState is the shared World's service (as for death). A
  // refusal or a redirect of these requests was not executed here.
  if (!call(0x10a9c0, self, {requested}) || r.refused || r.filtered != requested || r.landed != requested)
    throw Unsupported("mission2 setState: filterState refusal or redirect of a non-death request unexecuted");
  const auto clock = std::int64_t(std::int32_t(read(self + 0x300)));
  for (const auto &w : r.writes)  // s32 fields (M2:1371, M2:3673-3674)
    write(self + w.offset, std::uint32_t(w.resolve(clock, [this, self] { return own(self); })));
  write(self + 0x232, r.landed, 2);
  // What the research did not compare stays refused: the clip start and the
  // rest of the class setState, in their original order (M2:1443-1446,
  // M2:1500-1503, M2:3726-3728; Observed only). The Droid's slot-28 call is
  // executed as membership, not as sequence (C:151-153).
  if (a.kind == Kind::Droid)
    throw Unsupported("Droid setState(0x17): fuse stored; fuse sound, object flag 0x4000, explode clip and slot-28 order unexecuted");
  throw Unsupported("mission2 setState(9): entry stored; clip start and remaining setState calls unexecuted");
}
void Runtime::set_state(std::uint32_t self, std::uint32_t state) {
  auto *a = find(self);
  if (!a) throw Unsupported("mission2 setState: actor not live");
  if (state != 0x14) { enter(*a, state); return; }
  death(*a);
}
void Runtime::take_damage(std::uint32_t self, std::uint32_t attacker, Raw flags, Raw amount) {
  if (!find(self)) throw Unsupported("mission2 takeDamage: actor not live");
  if (attacker && !authority_.find(attacker))
    throw Unsupported("mission2 takeDamage: attacker not in the shared World");
  creature_hp::Model model(authority_.memory);
  // The verified null-attacker body needs no handles. Live attackers are
  // handled by the combat World's inherited slot 2, which owns the handles.
  if (attacker) throw Unsupported("mission2 takeDamage: use shared combat World slot 2 for live attackers");
  model.take_damage(self, attacker, flags, amount);
}
} // namespace champions::simulation::rta::mission2_actors
