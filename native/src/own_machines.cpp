#include "champions/own_machines.hpp"
#include <stdexcept>
namespace champions::simulation::rta::own_machines {
namespace {
std::int32_t s32(std::uint32_t n) { return std::int32_t(n); }
void fill(Probe &p, const View &v) {
  p.clock = v.clock;
  p.timer = v.timer;
  p.flight = v.flight;
  p.queue = v.queue & 0xff;
  p.anim = v.anim;
  p.events = v.events;
  p.rand_n = v.rand;
  p.fid = v.fid_time;
  p.z = v.z;
  p.stuck = v.stuck;
  p.cflags = v.cflags;
  p.kind = v.kind;
  p.class_flags = v.class_flags;
  p.frame = v.frame;
  p.serial = v.serial;
  p.seen = v.seen ? 1 : 0;
  p.see_v = v.see ? 1 : 0;
  p.enemy = v.enemy ? 1 : 0;
  p.ally = v.ally ? 1 : 0;
  p.demon = v.demon ? 1 : 0;
  p.path = v.path ? 1 : 0;
  p.away = v.away;
  p.rooted = v.rooted;
  p.ready = v.ready;
  p.stunned = v.stunned;
  p.arrived = v.arrived;
  p.target = v.target;
  p.owner = v.owner;
  p.idle = v.idle;
  p.move = v.move;
  p.fa = v.fidget_a;
  p.fb = v.fidget_b;
  p.flee = v.flee_clip;
  p.snap = v.snap;
  p.seq = v.seq;
  p.timer_out = p.timer;
  p.flight_out = p.flight;
}
bool sight_or_queue(Probe &c) {
  // Slot 25, and again unless the first returned true while the queue was
  // below 4. Either true result with queue < 4 requests state 3.
  if (c.v25() && c.queue < 4) {
    c.request(3);
    return true;
  }
  if (c.v25() && c.queue < 4) {
    c.request(3);
    return true;
  }
  return false;
}
void gnaw(std::uint32_t state, Probe &c) {
  // Gnaw_run 0x267654.
  if (state == 0) {
    if (c.v25())
      c.request(3);
    else {
      c.side("away");
      if (c.away)
        c.request(1);
    }
  } else if (state == 1) {
    if (c.v25())
      c.request(3);
    else
      c.side("move_dest");
  } else if (state == 3) {
    if (!c.target)
      c.request(0);
    else if (c.see())
      c.request(5);
    else if (c.see())
      c.side("turn");
    else
      c.side("move_target");
  } else if (state == 5) {
    if (c.see())
      c.request(6);
    else if (c.see())
      c.side("turn");
    else {
      c.side("unroot");
      if (!c.rooted)
        c.request(3);
    }
  } else if (state == 6) {
    c.side("turn");
    if (c.events & 1)
      c.side("hit_arc");
    if (!c.flag(8))
      c.request(5);
  } else if (state == 0x10) {
    c.side("ready");
    if (c.ready)
      c.request(0);
    else
      c.side("flee");
  } else if (state == 0x12) {
    if (!c.flag(0x10))
      c.request(5);
  } else if (state == 0x13) {
    c.side("unstun");
    if (!c.stunned)
      c.request(3);
  } else if (state == 0x14) {
    c.side("dead");
    c.side("delete");
  } else
    c.request(0);
}
void skelight(std::uint32_t state, Probe &c) {
  // Skelight_run 0x2a8b84. The combat world keeps its existing body for this
  // address; this switch is the decision the own-machine vectors lock.
  if (state == 0) {
    if (c.v25())
      c.request(3);
    else {
      c.side("away");
      if (c.away)
        c.request(1);
    }
  } else if (state == 1) {
    if (c.v25())
      c.request(3);
    else
      c.side("move_dest");
  } else if (state == 3) {
    if (!c.target)
      c.request(0);
    else if (c.see())
      c.request(6);
    else if (c.see())
      c.side("turn");
    else
      c.side("move_target");
  } else if (state == 5) {
    if (c.see())
      c.request(6);
    else {
      c.side("unroot");
      if (!c.rooted)
        c.request(3);
    }
  } else if (state == 6) {
    bool count = true;
    if (c.target) {
      if (c.events & 1) {
        c.side("h17");
        c.side("hit_arc");
      }
      if (c.flag(8)) {
        c.side("turn");
        count = false;
      }
    } else
      count = !c.flag(8);
    if (count)
      c.timer_out = (c.timer - 1) & 0xffffffffu;
    if (s32(c.timer_out) <= 0)
      c.request(3);
  } else if (state == 0x10) {
    c.side("ready");
    if (c.ready)
      c.request(0);
    else
      c.side("flee");
  } else if (state == 0x12)
    c.request(3);
  else if (state == 0x13) {
    c.side("unstun");
    if (!c.stunned)
      c.request(0);
  } else if (state == 0x14)
    c.side("dead");
  else if (state == 0x17) {
    if (c.flag(0x80))
      c.request(0);
    c.side("fx");
  }
}
void gnome(std::uint32_t state, Probe &c) {
  // GnomeNavigator_run 0x26a1e0. Follow bits are 0x400000 and 0x800000.
  if (state == 0) {
    if (c.cflags & 0xc00000) {
      if (c.expired())
        c.request(1);
      else {
        c.side("away");
        if (c.away)
          c.request(1);
      }
    }
  } else if (state == 1) {
    c.side("move_to");
    if (c.arrived || c.expired())
      c.request(0);
  } else if (state == 3) {
    if (!c.target)
      c.request(0);
  } else if (state == 5) {
    c.side("unroot");
    if (!c.rooted)
      c.request(3);
  } else if (state == 0x13) {
    c.side("unstun");
    if (!c.stunned)
      c.request(0);
  } else if (state == 0x14)
    c.side("dead");
}
void mermaid(std::uint32_t state, Probe &c) {
  // Mermaid_run 0x285c58. Every probed state starts with a zero move_by.
  c.side("move_by");
  if (state == 0) {
    if (c.expired())
      c.request(1);
  } else if (state == 1) {
    if (c.expired())
      c.request(0);
    else
      c.side("move_dest");
  } else if (state == 3) {
    if (!c.target)
      c.request(0);
  } else if (state == 5) {
    c.side("unroot");
    if (!c.rooted)
      c.request(3);
  } else if (state == 0x12) {
    if (!c.flag(0x10))
      c.request(0);
  } else if (state == 0x14) {
    if (c.flag(0x80))
      c.side("stop");
  }
}
void npc(std::uint32_t state, Probe &c) {
  // NPC_run 0x29ea40. The lump-name rejection loop is not this path.
  if (state == 0 && (c.cflags & 0x8000000))
    c.request(1);
  else if (state == 1)
    c.side("move_dest");
}
void natasla(std::uint32_t, Probe &) {
  // Natasla_run 0x29dbf0. No switch and no slot 27.
}
void aq_attack(Probe &c, std::uint32_t leave) {
  c.side("aq_ground");
  if ((c.events & 1) && c.target) {
    c.side("000000");
    c.side("hit_reach");
  }
  if (c.target)
    c.side("turn");
  if (!c.flag(8))
    c.request(leave);
}
void antqueen(std::uint32_t state, Probe &c) {
  // AntQueen_run 0x236f78. Bank helpers stay one call each.
  if (state == 0) {
    c.side("aq_ground");
    if (!sight_or_queue(c) && c.expired())
      c.request(1);
  } else if (state == 1) {
    c.side("aq_air");
    if (c.v25())
      c.request(3);
    else
      c.side("move_dest");
  } else if (state == 3) {
    c.side("aq_air");
    if (!c.target) {
      c.request(0);
      return;
    }
    if ((c.roll() & 3) == 0) {
      c.side("enemy");
      if (c.enemy) {
        c.request(7);
        return;
      }
    } else if (c.see()) {
      c.request(6);
      return;
    }
    c.side("move_target");
    if (s32(c.clock) > s32(c.flight) && c.queue < 4)
      c.request(8);
  } else if (state == 5) {
    auto drawn = c.roll();
    if (c.target) {
      if ((drawn & 3) == 0) {
        c.side("enemy");
        if (c.enemy) {
          c.request(7);
          return;
        }
      } else if (c.see()) {
        c.request(6);
        return;
      }
    }
    c.side("unroot");
    if (!c.rooted)
      c.request(3);
  } else if (state == 6) {
    c.side("aq_ground");
    if ((c.events & 1) && c.target)
      c.side("hit_reach");
    else if (c.target)
      c.side("turn");
    if (!c.flag(8))
      c.request(3);
  } else if (state == 7) {
    c.side("aq_ground");
    if (c.events & 1) {
      c.side("000000");
      c.side("hit_or");
    }
    if (!c.flag(8))
      c.request(3);
  } else if (state == 8) {
    c.side("aq_air");
    if (!c.target) {
      c.request(0);
      return;
    }
    if (s32(c.clock) > s32(c.flight)) {
      c.request(0x17);
      return;
    }
    if (c.flag(8))
      return;
    if (c.see()) {
      c.request(9);
      if (c.roll() % 3 == 0)
        c.flight_out = 0;
      return;
    }
    if (c.see())
      c.side("turn");
    else
      c.side("move_target");
  } else if (state == 9 || state == 10)
    aq_attack(c, state == 9 ? 8 : 0x17);
  else if (state == 0x12) {
    c.side("aq_ground");
    if (!c.flag(0x10) && c.queue < 4)
      c.request(3);
  } else if (state == 0x13) {
    c.side("unstun");
    if (!c.stunned)
      c.request(0);
  } else if (state == 0x14) {
    c.side("aq_ground");
    c.side("dead");
  } else if (state == 0x17) {
    c.side("aq_air");
    if (!c.target) {
      c.request(0);
      return;
    }
    if (c.flag(8))
      return;
    if (s32(c.clock) <= s32(c.flight)) {
      c.side("move_to");
      if (c.arrived || c.stuck >= 31)
        c.flight_out = 0;
      return;
    }
    c.side("turn");
    if (c.see())
      c.request(0x18);
  } else if (state == 0x18) {
    c.side("aq_ground");
    if (!c.flag(8) && c.queue < 4)
      c.request(3);
  } else if (state == 0x19) {
    c.side("aq_ground");
    if (c.target && c.clock == ((c.timer - 120) & 0xffffffffu)) {
      c.side("cos16");
      c.side("sin16");
      c.side("pool");
    }
    if (c.expired() && c.queue < 4)
      c.request(3);
  }
}
bool skeleton_sight(Probe &c) {
  // Melee: one slot 25. Bow (kind +0x3d0 == 4): a second unless the first hit.
  if (c.v25()) {
    c.request(3);
    return true;
  }
  if (c.kind == 4 && c.v25()) {
    c.request(3);
    return true;
  }
  return false;
}
void skeleton(std::uint32_t state, Probe &c) {
  // Skeleton_run 0x2a6864.
  if (state == 0) {
    if (!skeleton_sight(c)) {
      if (c.expired())
        c.request(1);
      else {
        c.side("away");
        if (c.away)
          c.request(1);
      }
    }
  } else if (state == 1) {
    if (!skeleton_sight(c)) {
      if (c.expired())
        c.request(0);
      else
        c.side("move_dest");
    }
  } else if (state == 3) {
    if (!c.target) {
      c.request(0);
      return;
    }
    if (c.kind == 4) {
      if (c.see())
        c.request(8);
      else if (c.see())
        c.side("turn");
      else
        c.side("move_target");
      return;
    }
    if (c.see()) {
      c.request(6);
      return;
    }
    c.side("ally");
    if (c.ally) {
      c.roll();
      c.request(5);
      return;
    }
    if (c.see())
      c.side("turn");
    else
      c.side("move_target");
  } else if (state == 5) {
    if (c.kind == 4) {
      if (c.see()) {
        c.request(8);
        return;
      }
    } else if (c.see()) {
      c.request(6);
      return;
    }
    c.side("unroot");
    if (c.rooted)
      return;
    if (c.expired()) {
      c.request(3);
      return;
    }
    if (!c.target)
      return;
    c.side("ally");
    if (!c.ally)
      c.request(3);
  } else if (state == 6) {
    if (c.target && (c.events & 1)) {
      c.side("h17");
      c.side("hit_reach");
    }
    if (c.target && c.flag(8))
      c.side("turn");
    if (!c.flag(8))
      c.timer_out = (c.timer - 1) & 0xffffffffu;
    if (s32(c.timer_out) <= 0)
      c.request(3);
  } else if (state == 8) {
    if (!c.target)
      c.request(0);
    else {
      c.side("cos16");
      c.side("sin16");
      c.side("lead");
      if (c.flag(8))
        c.side("v26");
    }
    if (!c.flag(8)) {
      c.timer_out = (c.timer - 1) & 0xffffffffu;
      if (s32(c.timer_out) <= 0)
        c.request(3);
    }
  } else if (state == 0x10) {
    c.side("ready");
    if (c.ready)
      c.request(0);
    else
      c.side("flee");
  } else if (state == 0x11) {
    if (c.expired())
      c.request(3);
  } else if (state == 0x12) {
    if (!c.flag(0x10))
      c.request(3);
  } else if (state == 0x13) {
    c.side("unstun");
    if (!c.stunned)
      c.request(0);
  } else if (state == 0x14)
    c.side("dead");
  else if (state == 0x17) {
    c.side("fx");
    if (!c.flag(0x10))
      c.request(0);
  }
}
bool fidget(Probe &c) {
  if (!(c.fa && s32(c.clock) > s32(c.fid)))
    return false;
  if (c.fb)
    c.roll();
  c.side("anim_bio");
  c.roll();
  return true;
}
void critter(std::uint32_t state, Probe &c) {
  // Critter_run 0x255008. Class flag 0x10 skips the locomotion call.
  if (state == 0x16) {
    c.side("path");
    if (c.path && !c.snap)
      c.side("approach");
    return;
  }
  if (state == 0) {
    if (!(c.class_flags & 0x10))
      c.side("move_by");
    if (fidget(c))
      return;
    if (c.flag(8) || !c.move)
      return;
    if (c.expired())
      c.request(1);
    else {
      c.side("away");
      if (c.away)
        c.request(1);
    }
  } else if (state == 1) {
    if (!(c.class_flags & 0x10))
      c.side(c.class_flags & 2 ? "move_direct" : "move_to");
    if (c.arrived || c.expired()) {
      if (c.idle && !((c.class_flags & 8) && !c.expired()))
        c.request(0);
      else {
        c.side("clear_path");
        c.side("wander");
        c.roll();
      }
    }
  } else if (state == 0x10) {
    if (!c.target) {
      c.request(c.idle ? 0 : 1);
      return;
    }
    if (c.flee)
      c.side("flee_from");
    if (c.expired()) {
      c.side("handle_clear");
      c.request(c.idle ? 0 : 1);
    }
  } else if (state == 0x14) {
    c.side("stop");
    c.side("v28");
  } else if (state == 0x17)
    fidget(c);
}
bool soul_phase(const Probe &c) {
  return ((c.frame ^ c.serial) & 15) == 0;
}
void soul(std::uint32_t state, Probe &c) {
  // Soul_run 0x2a9870. Owned souls and the thrown state skip the idle move.
  if (!c.owner && state != 0x1b)
    c.side("move_by");
  if (state == 0) {
    if (soul_phase(c)) {
      c.side("demon");
      if (c.demon) {
        c.request(0x18);
        return;
      }
    }
    if (c.expired())
      c.request(1);
    else {
      c.side("away");
      if (c.away)
        c.request(1);
    }
  } else if (state == 1) {
    if (soul_phase(c)) {
      c.side("demon");
      if (c.demon) {
        c.request(0x18);
        return;
      }
    }
    c.side("move_to");
    if (c.arrived || c.expired())
      c.request(0);
  } else if (state == 0x18) {
    if (soul_phase(c)) {
      c.side("demon");
      if (!c.demon)
        c.request(0);
    }
  } else if (state == 3) {
    if (!c.target)
      c.request(0);
  } else if (state == 5) {
    c.side("unroot");
    if (!c.rooted)
      c.request(0);
  } else if (state == 0x12 || state == 0x19) {
    if (!c.flag(0x10))
      c.request(0x18);
  } else if (state == 0x17) {
    if (c.expired())
      c.request(1);
  } else if (state == 0x14) {
    c.side("samples");
    if (c.expired() || c.flag(0x80))
      c.side("delete");
  } else if (state == 0x22) {
    c.side("samples");
    if (c.expired() || c.flag(0x80))
      c.side("stop");
  } else if (state == 0x1b) {
    c.side("samples");
    c.side("mat_place");
    c.side("mat_rot");
    c.side("cos16");
    c.side("sin16");
    if (c.expired())
      c.side("delete");
    else {
      c.side("terrain");
      // From z 0 the four sub-steps land on a terrain stub of 0. From the
      // raw32 0x43340000 (100.0) they do not. Other heights are not recorded.
      if (c.z == 0)
        c.side("delete");
    }
  }
}
using Machine = void (*)(std::uint32_t, Probe &);
Machine machine(const std::string &name) {
  if (name == "Gnaw")
    return gnaw;
  if (name == "Skelight")
    return skelight;
  if (name == "GnomeNavigator")
    return gnome;
  if (name == "Mermaid")
    return mermaid;
  if (name == "Natasla")
    return natasla;
  if (name == "NPC")
    return npc;
  if (name == "AntQueen")
    return antqueen;
  if (name == "Skeleton")
    return skeleton;
  if (name == "Critter")
    return critter;
  if (name == "Soul")
    return soul;
  return nullptr;
}
} // namespace
bool Probe::expired() const { return s32(clock) > s32(timer); }
bool Probe::flag(std::uint32_t bit) const { return (anim & bit) != 0; }
void Probe::call(const std::string &name) { calls.push_back(name); }
int Probe::take(const std::string &name, int fallback) {
  auto it = seq.find(name);
  if (it != seq.end() && !it->second.empty()) {
    int v = it->second.front();
    it->second.erase(it->second.begin());
    return v;
  }
  return fallback;
}
int Probe::v25() {
  if (v25_hook) {
    call("v25");
    return v25_hook();
  }
  int v = take("v25", seen);
  call("v25");
  return v;
}
int Probe::see() {
  if (see_hook) {
    call("see");
    return see_hook();
  }
  int v = take("see", see_v);
  call("see");
  return v;
}
std::uint32_t Probe::roll() {
  if (roll_hook) {
    call("rand");
    return roll_hook();
  }
  auto v = std::uint32_t(take("rand", int(rand_n)));
  call("rand");
  return v;
}
void Probe::side(const char *name) {
  call(name);
  if (side_hook)
    side_hook(name);
}
void Probe::request(std::uint32_t state) {
  call("v27");
  requests.push_back(state);
  if (request_hook)
    request_hook(state);
}
Tick run(const std::string &name, std::uint32_t state, Probe &probe) {
  auto fn = machine(name);
  if (!fn)
    throw std::invalid_argument("unknown own-state machine: " + name);
  fn(state, probe);
  Tick out;
  out.requests = probe.requests;
  out.calls = probe.calls;
  if ((name == "Skelight" || name == "Skeleton") && (state == 6 || state == 8))
    out.words[0xf4] = probe.timer_out;
  if (name == "AntQueen" && (state == 8 || state == 0x17))
    out.words[0x3d4] = probe.flight_out;
  return out;
}
Tick step(const std::string &name, std::uint32_t state, const View &view) {
  Probe probe;
  fill(probe, view);
  return run(name, state, probe);
}
bool root_clears_speed(std::uint32_t word) {
  return (word & 0x7fffffffu) > 0x3f000000u;
}
bool speed_word_is_integer(std::uint32_t bits) {
  bits &= 0x7fffffffu;
  if (bits == 0)
    return true;
  unsigned exp = (bits >> 23) & 0xff;
  std::uint32_t frac = bits & 0x7fffffu;
  if (exp == 0 || exp == 255)
    return false;
  int unbiased = int(exp) - 127;
  if (unbiased < 0)
    return false;
  if (unbiased >= 23)
    return true;
  std::uint32_t mask = (1u << (23 - unbiased)) - 1u;
  return (frac & mask) == 0;
}
} // namespace champions::simulation::rta::own_machines
