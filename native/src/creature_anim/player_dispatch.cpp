#include "champions/creature_anim/dispatch.hpp"
namespace champions::simulation::rta::creature_anim {
namespace {
constexpr Raw F01 = 0x3dcccccc, F02 = 0x3e4ccccc, F03 = 0x3e999999,
              F05 = 0x3f000000;
void player_input(const PlayerInput &p) {
  if (p.weapon_class > 7 || p.model > 11 || p.variant > 7)
    throw Unsupported("unknown player model/weapon/combo variant");
}
std::string prefix(Raw c) {
  static const char *names[] = {"ohw", "dhs", "dha", "l",
                                "b",   "nw",  "ohw", "dw"};
  if (c > 7)
    throw Unsupported("unknown player weapon class");
  return names[c];
}
ClipId named(const NamedClip &find, const std::string &name) {
  if (!find)
    throw Unsupported("player named resource resolver required");
  auto c = find(name);
  if (!c)
    throw Unsupported("missing named player clip: " + name);
  return c;
}
Track &cell(DispatchContext &c, unsigned i) {
  return c.runtime.track(c.runtime.state(c.state).tracks.at(i));
}
void base_cells(DispatchContext &c) {
  if (c.runtime.state(c.state).count < 4)
    throw Unsupported("player requires four persistent locomotion cells");
  for (unsigned i = 0; i < 4; ++i)
    if (cell(c, i).flags != 0x1205)
      throw Unsupported("player locomotion base cells changed");
}
} // namespace
void admit_player_state(Raw s) {
  // State 7 has no recorded writer. 6, 9 and 0xe are the skill constructors.
  if (s == 7 || s > 14)
    throw Unsupported("unknown or skill-driven player state");
}
Raw player_entry(Raw cls, Raw k) {
  if (cls > 7 || k < 0x44 || k > 0xa4 || (k - 0x44) % 8)
    throw ValueError("player row requires class 0..7 and offset 0x44..0xa4");
  return (cls * 0x68 + k - 4) / 8;
}
Selection player_action(const std::string &action, const PlayerInput &p,
                        const NamedClip &find) {
  player_input(p);
  Selection out;
  auto start = [&](Raw st, Raw entry, Raw blend, Raw flags) {
    out = {{{"start", {entry, blend, flags}}}, st};
  };
  auto bio = [&](Raw st, Raw entry, Raw in, Raw end, Raw flags) {
    out = {{{"blend", {entry, in, end, flags}}}, st};
  };
  auto ns = [&](Raw st, const std::string &n, Raw b, Raw f) {
    out = {{{"named_start", {std::int64_t(named(find, n)), b, f}}}, st};
  };
  auto nb = [&](Raw st, const std::string &n, Raw in, Raw end, Raw f) {
    out = {{{"named_blend", {std::int64_t(named(find, n)), in, end, f}}}, st};
  };
  auto np = [&](Raw st, const std::string &n, Raw flags) {
    out = {{{"named_push", {std::int64_t(named(find, n)), flags}}}, st};
  };
  // The seven state-6 skill constructors ran on a weapon-class-0 player whose
  // object flag word was 0x40 (player-animation-dispatch.md section 6,
  // ref_skill_anim). Classes 4 and 6 and other flag words were not varied.
  auto skill6 = [&] {
    if (p.weapon_class != 0 || p.object_flags != 0x40)
      throw Unsupported(
          "skill constructor not executed for this weapon class/flags");
  };
  auto shout = [&](const char *action_name) {
    skill6();
    return std::string("ohw_") + action_name + ".anm";
  };
  auto e = [&](Raw k) { return player_entry(p.weapon_class, k); };
  auto close = [&] {
    if (p.weapon_class != 4 && p.weapon_class != 6)
      throw Unsupported("close ranged attack requires class 4/6");
    return p.weapon_class == 4 ? "b_attackb.anm" : "nw_atakseq3.anm";
  };
  if (action == "shot") {
    if (p.weapon_class != 4 && p.weapon_class != 6)
      throw Unsupported("shot requires class 4/6");
    bio(0, e(0x74), F01, F01, 0x12);
  } else if (action == "first_swing") {
    if (p.weapon_class == 4 || p.weapon_class == 6)
      throw Unsupported("first melee swing excludes ranged classes");
    bio(1, e(0x74), F01, F01, 0x802);
  } else if (action == "heavy")
    bio(1, e(0x64), F01, F01, 0x812);
  else if (action == "close_attack")
    nb(1, close(), F01, F01, 0x802);
  else if (action == "combo_close")
    ns(1, close(), F01, 0x802);
  else if (action == "combo") {
    if (p.weapon_class == 4 || p.weapon_class == 6 || p.variant < 1 ||
        p.variant > 5)
      throw Unsupported("unknown combo resource choice");
    ns(1,
       prefix(p.weapon_class) + "_atakseq" + std::to_string(p.variant) + ".anm",
       F01, 0x802);
  } else if (action == "script_idle")
    start(0, p.no_combat_level ? 4 : e(0x44), F03, 3);
  else if (action == "script_walk") {
    start(1, e(0x4c), F01, 3);
    out.ops.push_back({"sync", {}});
  } else if (action == "stagger" || action == "flinch")
    bio(3, e(0x7c), F01, F01, 0xa);
  else if (action == "knockdown") {
    auto rel = s16(Raw(p.relative_hit));
    auto abs = rel < 0 ? -rel : rel;
    std::string suffix = abs >= 0x6000    ? "a"
                         : rel <= -0x2000 ? "c"
                         : rel <= 0x2000  ? "b"
                                          : "d";
    nb(3,
       std::string(p.weapon_class == 2 || p.weapon_class == 3 ? "knockdown2_"
                                                              : "knockdown1_") +
           suffix + ".anm",
       F01, F01, 0x80a);
  } else if (action == "death")
    start(4,
          p.weapon_class == 2 && p.model != 2 && p.model != 3 && p.model != 6 &&
                  p.model != 7
              ? 2
              : 1,
          F05, 0x206);
  else if (action == "cast_loop")
    start(5, e(0x6c), F01, 0x8003);
  else if (action == "cast_oneshot")
    bio(5, e(0x6c), F01, F01, 0x8022);
  else if (action == "block_pose")
    start(8, e(0x9c), F01, 0x2801);
  else if (action == "block")
    start(8, e(0x84), F01, 0x803);
  else if (action == "block_reaction") {
    if (p.draw <= 0xafc7)
      bio(8, e(0x94), F02, F02, 0x812);
    else
      bio(8, e(0x8c), F01, F01, 0x812);
  } else if (action == "ground_pound")
    nb(10, "dha_quake.anm", F01, F01, 0xa);
  else if (action == "evade")
    bio(11, e(0xa4), F01, F03, 0x808);
  else if (action == "warcry") {
    if (p.weapon_class == 4 || p.weapon_class == 6)
      throw Unsupported("unwritten original warcry name buffer");
    nb(12, prefix(p.weapon_class) + "_warcry.anm", F01, F01, 0x22);
  }   else if (action == "ride") {
    if (!find)
      throw Unsupported("player named resolver required");
    auto clip = find("nw_seated.anm");
    if (!clip)
      clip = named(find, "nw_idlea.anm");
    out = {{{"named_start", {std::int64_t(clip), F01, 3}}}, 13};
  } else if (action == "cyclone") {
    skill6();
    named(find, "spel_bw_cyclone.anm");
    np(6, "cyclone1.anm", 0x23);
  } else if (action == "bash" || action == "shield_bash") {
    skill6();
    nb(6, "bash.anm", F01, F01, 0x812);
  }
  else if (action == "ancestral_call") {
    auto clip = shout("warcry");
    named(find, "spel_bw_ancestral.anm");
    nb(6, clip, F01, F01, 0x22);
  } else if (action == "roar") {
    auto clip = shout("roar");
    named(find, "spel_vb_roar.anm");
    nb(6, clip, F01, F01, 0x22);
  } else if (action == "battle_cry") {
    auto clip = shout("battlecry");
    named(find, "spel_vb_battlecry.anm");
    nb(6, clip, F01, F01, 0x22);
  } else if (action == "blood_lust") {
    auto clip = shout("bloodlust");
    nb(6, clip, F01, F01, 0x822);
  } else if (action == "charge") {
    out.stored = (p.object_flags & 0x40) ? 9 : 0;
  } else if (action == "shadow_step") {
    if (p.object_flags != 0x40)
      throw Unsupported("shadow step flag pair unrecorded");
    np(0xe, "spel_de_shadowstep.anm", 0x4);
    out.object_flags = 0x45;
  } else if (action == "shadow_step_end") {
    if (p.object_flags != 0x45)
      throw Unsupported("shadow step flag pair unrecorded");
    out.stored = 2;
    out.object_flags = 0x40;
  } else
    throw Unsupported("unknown player animation action: " + action);
  return out;
}
void reset_player_idle(DispatchContext &c, const PlayerInput &p,
                       const NamedClip &find) {
  player_input(p);
  if (c.runtime.state(c.state).capacity < 5)
    throw Unsupported("idle reset requires five cells");
  std::array<ClipId, 4> clips{};
  for (unsigned i = 0; i < 4; ++i)
    clips[i] = c.entry(player_entry(p.weapon_class, 0x44 + 8 * i));
  if (p.display_only) {
    auto cls = p.weapon_class;
    clips[0] =
        named(find, (cls == 1 || cls == 2 || cls == 3 || cls == 4 || cls == 7
                         ? prefix(cls)
                         : "ohw") +
                        "_idled.anm");
  }
  // Preserve the rates, times and slot tokens the original direct stores do
  // not write. Release invalidates the old caches after changing the cells.
  for (unsigned i = 0; i < 4; ++i) {
    auto &t = cell(c, i);
    t.clip = clips[i];
    t.weight = t.life = i ? ZERO : ONE;
    t.flags = 0x1205;
  }
  cell(c, 4).weight = cell(c, 4).life = ZERO;
  const auto &s = c.runtime.state(c.state);
  c.runtime.set_state_scalars(c.state, s.pose, 4, s.flags);
  c.runtime.anim_release_caches(c.state);
}
void install_player_events(DispatchContext &c) {
  for (Raw cls = 0; cls < 6; ++cls)
    for (Raw k : {0x4cu, 0x5cu, 0xa4u}) {
      auto id = c.entry(player_entry(cls, k));
      if (!id)
        continue;
      auto &cl = c.runtime.clip(id);
      bool run = k == 0x5c;
      cl.events[0] = {run ? 0x3ee66666u : 0x3e800000u,
                      std::uint8_t(run ? 4 : 1)};
      cl.events[1] = {run ? 0x3f733333u : 0x3f400000u,
                      std::uint8_t(run ? 4 : 1)};
      // The player constructor writes two slots, not the creature's four.
    }
}
void install_ground_pound_events(Runtime &r, ClipId id) {
  const std::array<Raw, 4> t{0x3e800000, 0x3f000000, 0x3f400000, 0x3fbd2e2f};
  for (unsigned i = 0; i < 4; ++i)
    r.clip(id).events[i] = {t[i], std::uint8_t(1u << i)};
}
Raw player_combo_variant(Raw m, Raw c, Raw old, Raw frame) {
  if (m > 11 || c > 7 || old > 7)
    throw Unsupported("unknown combo input");
  if ((m == 9 && c == 1) || (m == 11 && c == 3))
    throw Unsupported("model/weapon combo transition unrecorded");
  Raw h = (frame * 0x19660du + 0x3c6ef35fu) & 65535;
  auto two = [&](Raw a, Raw b, Raw at) { return h < at ? a : b; };
  auto three = [&](Raw a, Raw b, Raw d, Raw at, Raw end) {
    return h < at ? a : h < end ? b : d;
  };
  if (c == 4 || c == 6)
    return old;
  if (c == 7)
    return old == 1 ? 2 : 1;
  if (m == 9 && c == 0) {
    switch (old) {
    case 1:
      return three(2, 3, 5, 0x5555, 0xaaaa);
    case 2:
      return two(1, 3, 0x7fff);
    case 3:
      return 4;
    case 4:
      return two(2, 5, 0x7fff);
    case 5:
      return three(4, 3, 1, 0x5555, 0xaaaa);
    default:
      return old;
    }
  }
  if (m == 9 && c == 5)
    return old == 1 ? 2 : old == 2 ? 3 : old == 3 ? 1 : old;
  if (m == 11 && c == 0) {
    switch (old) {
    case 1:
      return three(2, 4, 5, 0x7000, 0xe000);
    case 2:
      return three(3, 1, 5, 0x6aa8, 0xd334);
    case 3:
      return three(4, 2, 5, 0x7000, 0xe000);
    case 4:
      return three(5, 1, 3, 0x53ed, 0xaaaa);
    default:
      return two(1, 3, 0x7fff);
    }
  }
  if (m == 11 && c == 5) {
    switch (old) {
    case 2:
      return three(1, 3, 4, 0x6d60, 0xdac0);
    case 3:
      return two(2, 4, 0xbb80);
    case 4:
      return two(2, 3, 0x7fff);
    default:
      return two(2, 4, 0x7fff);
    }
  }
  if (c == 0) {
    if (old == 1 || old == 3)
      return two(2, 4, 0x7fff);
    if (old == 2 || old == 4)
      return three(1, 3, 5, 0x5555, 0xaaaa);
    return two(2, 4, 0xbb80);
  }
  if (c == 1 || c == 5)
    return old == 1 || old == 3 ? 2 : old == 2 ? two(1, 3, 0x7fff) : old;
  if (c == 2 || c == 3) {
    if (old == 1 || old == 3)
      return two(2, 4, m == 11 && old == 3 ? 0xbb80 : 0x7fff);
    if (old == 2 || old == 4)
      return two(1, 3, 0x7fff);
  }
  return old;
}
bool player_fidget(DispatchContext &c, Raw stick, bool &active, Raw cls,
                   const NamedClip &find, const std::function<Raw()> &draw) {
  if (cls > 7)
    throw Unsupported("unknown fidget weapon class");
  auto count = c.runtime.state(c.state).count;
  if (lt(0x3c23d70a, stick) && active) {
    active = false;
    for (int i = 4; i < count; ++i) {
      cell(c, i).life = ONE;
      cell(c, i).life_rate = 0xbdaaaaaa;
    }
    return false;
  }
  if (count != 4)
    return false;
  if (!draw)
    throw Unsupported("fidget needs shared draw service");
  auto value = draw();
  if (value >= 0xe2 || active)
    return false;
  c.runtime.anim_start(HALF, c.state, named(find, prefix(cls) + "_idled.anm"),
                       1);
  active = true;
  return true;
}
PlayerMove player_locomotion(DispatchContext &c, Raw s, Raw blend, Raw previous,
                             unsigned mode) {
  if (mode > 6)
    throw Unsupported("unknown player movement mode");
  base_cells(c);
  PlayerMove out{{}, ZERO, blend, previous};
  if (mode == 1 || mode == 4 || mode == 6) {
    out.blend = ZERO;
    for (unsigned i = 1; i < 4; ++i)
      cell(c, i).weight = cell(c, i).life = ZERO;
    return out;
  }
  if (mode != 0)
    return out;
  if ((s & 0x80000000u) || (s & 0x7f800000u) == 0x7f800000u)
    throw ValueError("scaled stick speed must be finite and nonnegative");
  Raw rate;
  if (lt(s, 0x3edc28f5))
    rate = mul(s, 0x4014d653);
  else if (lt(s, 0x3f333333))
    rate = add(mul(mul(sub(s, 0x3edc28f5), 0x406d097a), 0x3eb33330), ONE);
  else
    rate =
        add(mul(mul(sub(s, 0x3f333333), 0x40555554), 0x3e1999a0), 0x3faccccc);
  if (lt(0x3c23d70a, rate)) {
    blend = add(blend, F01);
    if (lt(ONE, blend)) {
      blend = ONE;
      cell(c, 0).time = ZERO;
    }
  } else {
    blend = sub(blend, F01);
    if (lt(blend, ZERO))
      blend = ZERO;
  }
  bool idle = lt(rate, 0x3c23d70a) && lt(blend, 0x3d4ccccc);
  out.motion = c.runtime.advance(idle ? ONE : rate, c.state, false);
  out.motion.events =
      idle ? 0 : out.motion.events & (lt(s, 0x3f199999) ? 1 : 4);
  if (lt(sub(previous, s), 0xbd4ccccc))
    s = add(previous, 0x3d4ccccc);
  cell(c, 1).weight = cell(c, 1).life = blend;
  cell(c, 2).weight = cell(c, 2).life =
      d2f(dmul(dmul(f2d(blend), dsub(f2d(s), UINT64_C(0x3fdccccccccccccc))),
               UINT64_C(0x4008000000000000)));
  cell(c, 3).weight = cell(c, 3).life =
      d2f(dmul(dmul(f2d(blend), dsub(f2d(s), UINT64_C(0x3fe6666666666666))),
               UINT64_C(0x4008000000000000)));
  out.speed = rate;
  out.blend = blend;
  out.previous = s;
  return out;
}
std::vector<DispatchEvent> player_events(Raw state, std::int32_t word,
                                         bool copy, bool scripted) {
  admit_player_state(state);
  std::vector<DispatchEvent> out;
  auto emit = [&](const char *kind, const char *name, int mask) {
    if (Raw(word) & Raw(mask))
      out.push_back({kind, name, state, Raw(word), mask});
  };
  if (scripted) {
    emit("footstep", "Player_footstep", 1);
    return out;
  }
  if (state == 2 || state == 11)
    emit("footstep", "Player_footstep", 5);
  if (state == 10) {
    emit("ground_pound", "player_voice_and_sound", 1);
    if (!copy) {
      for (int mask : {1, 2, 4})
        emit("hit_test", "world_damageInRadius", mask);
      emit("ground_pound", "GroundPoundEffect", 8);
    }
  }
  return out;
}
std::optional<Raw> player_marker_exit(Raw state, Raw flags) {
  admit_player_state(state);
  Raw mask = 0;
  if (state == 3 || state == 10)
    mask = 8;
  else if (state == 12)
    mask = 0x20;
  // States 0, 5 and 11 additionally require timers/phase/held-input.
  // Do not infer their exits from a flag alone.
  if (mask && !(flags & mask))
    return 2;
  return {};
}
} // namespace champions::simulation::rta::creature_anim
