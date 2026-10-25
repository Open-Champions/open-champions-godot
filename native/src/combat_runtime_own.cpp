#include "champions/combat_ctors.hpp"
#include "champions/core_arithmetic.hpp"
#include "champions/own_machines.hpp"
#include "combat_runtime_internal.hpp"
// The own-state-machine classes in the combat world (creature-ai, audit fixes
// CAI-1, CAI-2, CAI-4, CAI-5, CAI-6). Gnaw has an executed constructor, run
// and setState (research-anim 5694ae3 tools/ref_monsters_1.py gnaw_ctor,
// gnaw_run, gnaw_set_state; docs/spec/monsters-1.md section 4). AntQueen,
// Skeleton and Soul have executed constructors and set-states
// (research-combat-ctors e9777eb), and full run switches
// (research-combat-ctors-3 9ff6ae0). Skelight keeps its own body
// (skelight_run). Every other class's constructor, run prologue and setState
// side effects were not executed, so spawning one refuses.
namespace champions::simulation::rta::combat_runtime {
namespace {
struct OwnClass {
  const char *name;
  Raw run, set_state;
};
// Run and setState addresses from the own-state-machine contract.
const OwnClass kOwn[] = {
    {"Critter", 0x255008, 0x255c5c},
    {"Soul", 0x2a9870, 0x2aac38},
    {"NPC", 0x29ea40, 0x29efcc},
    {"Natasla", 0x29dbf0, 0x29dc3c},
    {"Mermaid", 0x285c58, 0x285ddc},
    {"GnomeNavigator", 0x26a1e0, 0x26a3f4},
    {"Skeleton", 0x2a6864, 0x2a7768},
    {"Skelight", 0x2a8b84, 0x2a91b8},
    {"Gnaw", 0x267654, 0x267a0c},
    {"AntQueen", 0x236f78, 0x237918},
};
// Gnaw (monsters-1.md section 4, monsters-2.md "Gnaw": table 0x515038, draw
// 0x267604, everything else base).
constexpr Address VT_GNAW = 0x515038;
constexpr Raw GNAW_RUN = 0x267654, GNAW_SET_STATE = 0x267a0c,
              GNAW_DRAW = 0x267604;
constexpr Raw F_0_1 = 0x3dcccccc, F_0_2 = 0x3e4ccccc, F_0_3 = 0x3e999999,
              F_1_2 = 0x3f999999, F_6 = 0x40c00000, F_20 = 0x41a00000,
              F_24 = 0x41c00000, F_2M16 = 0x37800000;
// The two doubles Gnaw_ctor reads at 0x545ea0 and 0x545ea8: 0.3, and the
// single 0.45 widened (0.449999988...), monsters-1.md section 4.2.
constexpr DoubleRaw D_GNAW_SCALE_MUL = 0x3fd3333333333333ull,
                                     D_GNAW_SCALE_ADD = 0x3fdcccccc0000000ull;
namespace CT = combat_ctors;
bool ctor_class(const std::string &name) {
  return name == "AntQueen" || name == "Skeleton" || name == "Soul";
}
// Authored fixture texts and words the three constructors read, entering the
// image with the first creature that reads them: the Skeleton prefix texts
// (ref_combat_ctors.SKEL_PREFIXES), bit 0 of the C library class table on
// 'A'..'Z' (ref_monsters_1.G_CTYPE: "bit 0 = upper case"), and "airhub" at
// 0x54c2b8. The skelcom block 0x5023e0 gets authored clip tokens in its
// resource words (as the placement's class definitions do), names 0 and the
// current generation, so no res_find is made.
constexpr Raw SKELCOM_TOKEN = 0xc1200000, CLASS_TOKEN = 0xc1100000;
constexpr Address SKELCOM_ENTRIES = 0x48; // five weapon rows of 0x70 bytes
const char *const SKELETON_TYPE_NAME = "Skeleton";
// Class bodies from the executed constructor rounds. Slot 3's empty overrides
// are docs/monsters-2.md's.
void install_ctor_refusals(A::Model &m) {
  CT::install_round2(m);
  CT::install_round3(m);
  auto empty = [](A::Model &, Address, const std::vector<Value> &) -> Value {
    return std::int64_t(0);
  };
  m.class_bodies[0x2378f4] = empty; // AntQueen slot 3 (empty)
  m.class_bodies[0x2aab60] = empty; // Soul slot 3 (empty)
}
const OwnClass *find_name(const std::string &name) {
  for (const auto &row : kOwn)
    if (name == row.name)
      return &row;
  return nullptr;
}
const OwnClass *find_run(Raw fn) {
  for (const auto &row : kOwn)
    if (row.run == fn && std::string(row.name) != "Skelight")
      return &row;
  return nullptr;
}
const OwnClass *find_set(Raw fn) {
  for (const auto &row : kOwn)
    if (row.set_state == fn && std::string(row.name) != "Skelight")
      return &row;
  return nullptr;
}
} // namespace
// Gnaw_setState 0x267a0c (ref_monsters_1.gnaw_set_state): Creature_filterState
// first, then the clip and its side effects. Clips are class-definition words
// (`clip(c, 0x1c + 8k)`), the convention every World clip start uses.
void World::State::apply_own_state(Address c, const char *cls, Raw requested) {
  const std::string name = cls;
  if (ctor_class(name)) {
    // AntQueen_setState 0x237918, Skeleton_setState 0x2a7768 and
    // Soul_setState 0x2aac38 (ref_combat_ctors), every state number.
    actor(c);
    if (name == "AntQueen")
      CT::antqueen_set_state(model, c, requested);
    else if (name == "Skeleton")
      CT::skeleton_set_state(model, c, requested);
    else
      CT::soul_set_state(model, c, requested);
    return;
  }
  if (name != "Gnaw")
    throw Unsupported(std::string(cls) +
                      "_setState is not ported to the combat world (its "
                      "non-clip side effects were not executed)");
  actor(c);
  auto filtered = model.filter_state(c, requested);
  if (filtered.refused)
    return;
  auto state = Raw(filtered.state);
  const Raw anim = Raw(c + 0x114);
  auto clip = [&](Raw off) { return model.clip(c, off); };
  auto start = [&](Raw off, Raw flags, Raw blend = F_0_1) {
    service("anim_start", {iv(anim), iv(clip(off)), iv(flags), iv(blend)});
  };
  auto sync = [&] { service("anim_syncToSimFrame", {iv(anim)}); };
  switch (state) {
  case 0:
    start(0x1c, 3);
    break;
  case 1:
    model.clear_path(c);
    model.pick_wander_dest(c);
    start(0x3c, 0x803);
    sync();
    break;
  case 3:
    model.clear_path(c);
    start(0x3c, 0x803);
    break;
  case 5:
    start(0x1c + 8 * (model.own_draw(c) % 3), 3);
    sync();
    break;
  case 6:
    service("anim_startBlendInOut",
            {iv(anim), iv(clip(0x1c + 8 * (model.own_draw(c) % 3))), iv(0x808),
             iv(F_0_1), iv(F_0_1)});
    break;
  case 0x10:
    w(c + 0x318, 2, 60);
    model.clear_path(c);
    start(0x34, 0x803);
    sync();
    break;
  case 0x12:
    state = Raw(s16m(c + 0x232));
    break;
  case 0x13:
    start(0x1c, 3, F_0_3);
    break;
  case 0x14:
    for (unsigned k = 0; k < 6; ++k) {
      Vec3 spot{u(c + 0x10), u(c + 0x14), add(u(c + 0x18), F_24)};
      if (k < 2) {
        auto block = pool_new(GUTCHUNK_SIZE);
        if (block) {
          gutchunk_ctor(block, spot, model.own_draw(c) % 3, 2);
          throw Unsupported("PhProp_initPhysics (the Gnaw death GutChunk's "
                            "physics) is not in the combat world");
        }
      }
      service("fx_spawn", {iv(9), spot});
    }
    start(0x1c, 0x803);
    vcall(c, 28, {iv(0)});
    break;
  default:
    state = 0;
    break;
  }
  w(c + 0x232, 2, state);
}
// Full run bodies: AntQueen in round 2, Skeleton and Soul in round 3.
void World::State::ctor_class_run(Address a, const std::string &cls) {
  actor(a);
  if (cls == "AntQueen") CT::antqueen_run(model,a);
  else if (cls == "Skeleton") CT::skeleton_run(model,a);
  else CT::soul_run(model,a);
}
void World::State::own_class_run(Address a, const char *cls) {
  if (ctor_class(cls)) {
    ctor_class_run(a, cls);
    return;
  }
  if (std::string(cls) != "Gnaw")
    throw Unsupported(std::string(cls) +
                      " run prologue is not executed in the combat world "
                      "(own-state-machines.md section 1 stubs it)");
  actor(a);
  auto model16 = number(vcall(a, 16));
  model.process_damage(a, model16);
  if (model.creature_run(a, number(vcall(a, 16))))
    return;
  if (!model.update_active(a))
    return;
  auto ev = model.step_anim(a, ONE);
  knockback(a, ONE);
  model.vcall(a, 23, {iv(0)});
  // The switch reads the state as an unsigned halfword.
  auto state = std::uint32_t(h(a + 0x232));
  own_machines::Probe probe;
  probe.anim = h(a + 0x11a);
  probe.events = ev;
  probe.target = u(a + 0x234) != 0;
  probe.v25_hook = [&] {
    return model.slot25_can_see_target(a, 0) != 0 ? 1 : 0;
  };
  // canSee: the arc test (8000), then the any-direction test (0x7fff), both
  // 0..6 around the creature's own facing.
  int sights = 0;
  probe.see_hook = [&] {
    Raw arc = sights++ == 0 ? 8000 : 0x7fff;
    return model.can_see_target_in(a, s16m(a + 0x38), arc, 0, 0, F_6, 0) != 0
               ? 1
               : 0;
  };
  probe.roll_hook = []() -> std::uint32_t {
    throw Unsupported("Gnaw_run draws no random number");
  };
  probe.side_hook = [&](const std::string &name) {
    if (name == "away") {
      probe.away = model.is_away_from_home(a);
      // Skelight_run refuses the same step outside the Full composition.
      if (probe.away && !full())
        throw Unsupported("walking home");
    } else if (name == "move_dest") {
      if (!full())
        throw Unsupported("walking/wander");
      model.move_to_dest(a);
    } else if (name == "turn")
      model.turn_to_target(a);
    else if (name == "move_target") {
      if (!full())
        throw Unsupported("walking chase");
      model.move_to_target(a);
    } else if (name == "unroot")
      probe.rooted = model.is_not_rooted(a) == 0;
    else if (name == "hit_arc") {
      // Creature_hitTargetInArc(c, facing, 8000, +0xd0, +0xd2, +0xd4, flags
      // 8, no descriptor; 0, 6).
      Arc x;
      x.facing = h(a + 0x38);
      x.angle = 8000;
      x.minimum = Raw(s16m(a + 0xd0));
      x.maximum = Raw(s16m(a + 0xd2));
      x.attack = Raw(s16m(a + 0xd4));
      x.flags = 8;
      x.near = 0;
      x.far = F_6;
      hit_arc(a, x, false);
    } else if (name == "ready")
      probe.ready = model.is_attack_ready(a) != 0;
    else if (name == "flee")
      throw Unsupported("flee walking");
    else if (name == "unstun")
      probe.stunned = model.is_not_stunned(a) == 0;
    else if (name == "dead")
      dead_update(a, ev, 1, F_20, {});
    else if (name == "delete")
      erase_later(a);
    else
      throw Unsupported("Gnaw decision call is not in Gnaw_run: " + name);
  };
  probe.request_hook = [&](std::uint32_t requested) {
    vcall(a, 27, {iv(requested)});
  };
  own_machines::run(cls, state, probe);
}
bool World::State::own_run(Address a, Raw fn) {
  auto row = find_run(fn);
  if (!row)
    return false;
  own_class_run(a, row->name);
  return true;
}
// Gnaw_ctor 0x26741c (ref_monsters_1.gnaw_ctor, monsters-1.md section 4.2).
// Creature_ctor is the authored creature placement the World uses for
// Skelight (research ref_combat_runtime spawn_skelight: authored set-up and
// clip tokens, not the original constructor); Creature_init stays the
// call-out it is in the executed constructor. The class writes are Gnaw's.
static void gnaw_ctor(World::State &s, Address c) {
  auto frame = s.u(FRAME);
  s.w(c + 0xb4, 1, 1);
  s.w(c + 0xb0, F_1_2);
  s.w(c + 0xec, frame);
  s.w(c + 0xa8, 8, s.q(c + 0xa8) | UINT64_C(0x200001000));
  s.w(c + 0x48, VT_GNAW);
  s.w(c + 0x1c, F_6);
  s.w(c + 0xf4, frame + 300);
  s.w(c + 0xb8, 0);
  s.w(c + 0x232, 2, 0);
  auto unit = f2d(mul(from_int(s.model.own_draw(c)), F_2M16));
  s.w(c + 0xdc, d2f(dadd(dmul(unit, D_GNAW_SCALE_MUL), D_GNAW_SCALE_ADD)));
  s.note("anim_clear", {c + 0x114});
  s.apply_own_state(c, "Gnaw", 0);
  // obj_runEveryFrame: the placement already put it on the every-frame list.
  // The three bite clips' event (entries 3, 4, 5: 0.2 s -> bit 0) is the
  // animation service's timeline in this world.
  for (Raw entry : {3u, 4u, 5u})
    s.note("creature_setDefEntryParams", {c, entry, 1, F_0_2});
  s.note("Creature_init", {c});
}
void World::State::plant_ctor_fixtures(const std::string &name) {
  if (!full())
    throw Unsupported(name + " needs the Full composition (its constructor's "
                             "call-outs are the Full World's services)");
  if (!ctor_bodies) {
    install_ctor_services();
    CT::install(model);
    install_ctor_refusals(model);
    ctor_bodies = true;
  }
  if (name == "Skeleton" && !skeleton_fixture) {
    for (const auto &p : CT::SKELETON_PREFIXES) {
      std::string text = p.name;
      for (unsigned k = 0; k <= text.size(); ++k)
        w(p.text + k, 1, k < text.size() ? Raw(std::uint8_t(text[k])) : 0);
    }
    for (Raw ch = 'A'; ch <= 'Z'; ++ch)
      w(CT::G_CTYPE + ch, 1, 1);
    for (Address k = 1; k < SKELCOM_ENTRIES; ++k)
      w(CT::SKEL_COMMON + 8 * k + 4, SKELCOM_TOKEN + 8 * k);
    w(CT::SKEL_COMMON + 4, u(CT::G_LUMP_GENERATION));
    skeleton_type_name = alloc_fixture(16);
    std::string text = SKELETON_TYPE_NAME;
    for (unsigned k = 0; k <= text.size(); ++k)
      w(skeleton_type_name + k, 1, k < text.size() ? Raw(std::uint8_t(text[k])) : 0);
    skeleton_fixture = true;
  }
  // The type-info descriptors' name words (mangled form, as the Full set-up
  // writes the others) of AntQueen 0x908680 and Soul 0x908e30.
  auto descriptor = [&](Address desc, const std::string &text) {
    if (u(desc))
      return;
    auto at = alloc_fixture(unsigned(text.size() + 1));
    for (unsigned k = 0; k <= text.size(); ++k)
      w(at + k, 1, k < text.size() ? Raw(std::uint8_t(text[k])) : 0);
    w(desc, at);
  };
  if (name == "AntQueen")
    descriptor(0x908680, "8AntQueen");
  if (name == "Soul")
    descriptor(0x908e30, "4Soul");
  if (name == "Soul" && !soul_fixture) {
    std::string text = "airhub";
    for (unsigned k = 0; k <= text.size(); ++k)
      w(CT::S_AIRHUB + k, 1, k < text.size() ? Raw(std::uint8_t(text[k])) : 0);
    soul_fixture = true;
  }
}
// AntQueen_ctor 0x236c70, Skeleton_ctor 0x2a5af4 and Soul_ctor 0x2a9478 after
// Creature_ctor, on the authored placement that stands in for Creature_ctor
// (as for Skelight and Gnaw). The placement's class definition grows to the
// last clip word each class reads (AntQueen +0xc4, Soul +0x94) with the same
// authored tokens. The table is the base plus every documented override.
Address World::State::spawn_ctor_class(const std::string &name, const Spawn &v) {
  plant_ctor_fixtures(name);
  auto a = spawn(v, false, 0, 0, 0);
  // Creature_ctor leaves creature flag bit 63 ("active") clear
  // (ref_arena_monsters.creature_ctor); the placement sets it. Cleared here so
  // that the first Creature_updateActive moves an AntQueen or Skeleton off the
  // every-60 list its constructor chose, as the original's would. Soul_ctor
  // sets the bit itself.
  w(a + 0xa8, 8, q(a + 0xa8) & ~(UINT64_C(1) << 63));
  if (name != "Skeleton") {
    auto def = alloc_fixture(0xd0);
    for (Address k = 1; k < 0xd0 / 8; ++k)
      w(def + 8 * k + 4, CLASS_TOKEN + 8 * k);
    w(def + 12, 0);
    w(a + 0x24, def);
  }
  std::map<unsigned, Raw> extra;
  const CT::Slot *slots = name == "AntQueen"   ? CT::ANTQUEEN_SLOTS
                          : name == "Skeleton" ? CT::SKELETON_SLOTS
                                               : CT::SOUL_SLOTS;
  std::size_t count = name == "AntQueen"   ? std::size(CT::ANTQUEEN_SLOTS)
                      : name == "Skeleton" ? std::size(CT::SKELETON_SLOTS)
                                           : std::size(CT::SOUL_SLOTS);
  for (std::size_t k = 0; k < count; ++k)
    extra[slots[k].slot] = slots[k].function;
  write_table(name == "AntQueen"   ? CT::VT_ANTQUEEN
              : name == "Skeleton" ? CT::VT_SKELETON
                                   : CT::VT_SOUL,
              &extra);
  CT::Options placed;
  placed.placed = true;
  if (name == "AntQueen")
    CT::antqueen_ctor(model, a, 0, v.position, v.facing, 0, placed);
  else if (name == "Skeleton")
    CT::skeleton_ctor(model, a, v.position, v.facing, 0, skeleton_type_name, 0,
                      placed);
  else
    CT::soul_ctor(model, a, 0, v.position, v.facing, 0, 0, placed);
  return a;
}
// Slot 1 of the six classes (combat-ctors-2.md section 6): CT::class_dtor on
// the model, whose call-outs are the World's handle_clear, iter_*, Spell.v1
// and pool_free services. sub_202500 (+0x320, 2; it frees only with bit 0) and
// AnimState_dtor are recorded. objNames_remove refuses: no World object has a
// property list at +0x2c, so the name scan never reaches it.
void World::State::class_dtor(Address a, Raw fn, Raw flags) {
  actor(a);
  if (!dtor_services) {
    for (const char *name : {"sub_202500", "AnimState_dtor", "objNames_remove"}) {
      std::string n = name;
      model.callbacks[n] = [this, n](A::Model &, const std::vector<Value> &args) {
        if (n == "objNames_remove")
          throw Unsupported("objNames_remove: the World keeps no object names");
        std::vector<std::uint64_t> words;
        for (const auto &x : args)
          words.push_back(std::uint64_t(number(x)));
        note(n, std::move(words));
        return iv(0);
      };
    }
    dtor_services = true;
  }
  CT::class_dtor(model, fn, a, flags);
  animations.erase(a);
}
Address World::State::spawn_class(const std::string &name, const Spawn &v) {
  if (name == "Skelight")
    return spawn(v, false, 0, 0, 0);
  auto info = find_name(name);
  if (!info)
    throw Unsupported("unimplemented actor run table");
  if (ctor_class(name))
    return spawn_ctor_class(name, v);
  if (name != "Gnaw")
    throw Unsupported(name +
                      " constructor is not executed (own-state-machines.md "
                      "runs only the switch; Gnaw_ctor and the AntQueen, "
                      "Skeleton and Soul constructors are the executed ones)");
  auto a = spawn(v, false, 0, 0, 0);
  std::map<unsigned, Raw> extra{
      {4, GNAW_RUN}, {5, GNAW_DRAW}, {27, GNAW_SET_STATE}};
  write_table(VT_GNAW, &extra);
  gnaw_ctor(*this, a);
  return a;
}
const char *World::State::own_set_class(Raw fn) const {
  auto row = find_set(fn);
  return row ? row->name : nullptr;
}
} // namespace champions::simulation::rta::combat_runtime
