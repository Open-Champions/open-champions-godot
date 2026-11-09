// Authored port of t3/research-campaign-combat fbb2e1d (MIT research notes);
// see creature_ai_m1_3.hpp for the citation key. No game data or text.
#include "champions/creature_ai_m1_3.hpp"
#include "champions/core_state.hpp"
#include <algorithm>
#include <array>
#include <iterator>

namespace champions::simulation::rta::creature_ai_m1_3 {
namespace {
constexpr std::uint32_t X = 0x7fff;        // MOD:6 any-angle arc
constexpr std::int64_t S = ARG_SCALE;
// Exact IEEE-754 single bits of the corpse distances 20, 25 and 30 (C:148-150).
constexpr Raw F20 = 0x41a00000u, F25 = 0x41c80000u, F30 = 0x41f00000u;

// C:77-80 knock-back scales (four decimals, see ARG_SCALE); C:148-150 corpse
// distances; C:89-93 slots; C:103-106 HomeRange (Observed: M2:6202 WarWraith
// 600, M2:1355 IronGuardian 3000).
constexpr std::uint32_t PICK = 0x10ded0, ALLY = 0x10e2c0, SIGHT = 0x10e658;
constexpr std::uint32_t IS_ENEMY = 0x119328, IS_ALLY = 0x119400, TAKE = 0x116638;
const std::array<ClassInfo, CLASS_COUNT> &table() {
  static const std::array<ClassInfo, CLASS_COUNT> t = {{
    {Class::Kobold, "Kobold", 0x27f0c0, 0x27f52c, 0x27ff6c, 0x515ef0, 0x3d0,
     10000, F20, TAKE, IS_ENEMY, 0x280434, PICK, ALLY, SIGHT, 1000},
    {Class::WarWraith, "WarWraith", 0x2c07a8, 0x2c0c2c, 0x2c1648, 0x517f80, 0x468,
     0, std::nullopt, TAKE, 0x2c1bc0, IS_ALLY, PICK, ALLY, SIGHT, 600},
    {Class::Orc, "Orc", 0x2a0954, 0x2a1848, 0x2a323c, 0x50ffd0, 0x488,
     4000, F30, 0x2a2eac, IS_ENEMY, IS_ALLY, PICK, ALLY, SIGHT, 1000},
    {Class::Goblin, "Goblin", 0x26a624, 0x26b27c, 0x26ccfc, 0x510260, 0x498,
     5000, F20, TAKE, IS_ENEMY, IS_ALLY, PICK, ALLY, SIGHT, 1000},
    {Class::Ogre, "Ogre", 0x29fad0, 0x29fe50, 0x2a060c, 0x517220, 0x468,
     1000, F30, TAKE, IS_ENEMY, IS_ALLY, PICK, ALLY, SIGHT, 1000},
    {Class::Warrior, "Warrior", 0x2bafb4, 0x2bb640, 0x2bc320, 0x517e38, 0x480,
     4000, F20, TAKE, IS_ENEMY, IS_ALLY, PICK, ALLY, SIGHT, 1000},
    {Class::Clockwork, "Clockwork", 0x24e124, 0x24e758, 0x24f90c, 0x514300, 0x500,
     4000, F20, TAKE, IS_ENEMY, IS_ALLY, PICK, ALLY, SIGHT, 1000},
    {Class::Droid, "Droid", 0x27b840, 0x27ba78, 0x27c0a0, 0x515da8, 0x3d0,
     10000, std::nullopt, TAKE, IS_ENEMY, IS_ALLY, PICK, ALLY, SIGHT, 1000},
    {Class::IronGuardian, "IronGuardian", 0x27a00c, 0x27a390, 0x27b550, 0x515c60, 0x3e8,
     0, F30, TAKE, IS_ENEMY, IS_ALLY, PICK, ALLY, SIGHT, 3000},
    {Class::Froglock, "Froglock", 0x263e1c, 0x264314, 0x265150, 0x510780, 0x468,
     5000, F25, TAKE, IS_ENEMY, IS_ALLY, PICK, ALLY, SIGHT, 1000},
    {Class::Amphian, "Amphian", 0x234584, 0x23483c, 0x2350d0, 0x512da0, 0x3c8,
     5000, F20, TAKE, IS_ENEMY, IS_ALLY, PICK, ALLY, SIGHT, 1000},
    {Class::SeaMonster, "SeaMonster", 0x2a428c, 0x2a44a8, 0x2a4aa4, 0x517368, 0x3d0,
     2000, F30, TAKE, IS_ENEMY, IS_ALLY, PICK, ALLY, SIGHT, 1000},
  }};
  return t;
}

// The per-tick model context: the same answers as the probe's query stubs
// (REF:77-157). Draws come from the class object's own generator. Arguments
// are given in whole units and stored x ARG_SCALE.
using Args = std::vector<std::int64_t>;
Args scaled(Args a) { for (auto &v : a) v *= S; return a; }
struct M {
  const Context &c;
  const Answer &answer;
  Tick out;
  Lcg rng;
  M(const Context &ctx, const Answer &a) : c(ctx), answer(a), rng(ctx.seed) {}
  std::int64_t ask(Event e) {
    const auto v = answer ? answer(e) : 0;
    out.events.push_back(std::move(e));
    return v;
  }
  void act(const char *name, Args args = {}) { out.events.push_back({name, scaled(std::move(args))}); }
  void set(std::uint32_t s) { act("set", {std::int64_t(s)}); }
  std::int64_t see(std::int64_t near, std::int64_t far, std::uint32_t arc, int ray = 0, int facing = 0) {
    return ask({"see", scaled({near, far, facing, arc, ray})});
  }
  std::int64_t cone(std::int64_t near, std::int64_t far, std::uint32_t arc, int facing = 0) {
    return ask({"cone", scaled({near, far, facing, arc})});
  }
  std::int64_t sight(int force = 0) { return ask({"sight", scaled({force})}); }
  std::int64_t q(const char *name, Args args = {}) { return ask({name, scaled(std::move(args))}); }
  void hit(std::int64_t near, std::int64_t far, std::uint32_t arc, std::uint32_t flags,
           const char *name = "hitInArc", int facing = 0,
           std::optional<std::array<std::int32_t, 2>> dmg = std::nullopt) {
    const auto lo = dmg ? (*dmg)[0] : c.dmg_low, hi = dmg ? (*dmg)[1] : c.dmg_high;
    act(name, {near, far, facing, arc, lo, hi, c.rating, flags});
  }
  void hit_all(std::int64_t near, std::int64_t far, std::uint32_t arc, std::uint32_t flags, int facing = 0,
               std::optional<std::array<std::int32_t, 2>> dmg = std::nullopt) {
    hit(near, far, arc, flags, "hitAll", facing, dmg);
  }
  // REF:119-121: the wraith tests hitAtReach's result, so it is a query.
  std::int64_t hit_reach(std::int64_t reach, std::uint32_t flags) {
    return ask({"hitAtReach", scaled({reach, c.dmg_low, c.dmg_high, c.rating, flags})});
  }
  std::uint32_t rand() { ++out.draws; return rng.draw(); }
  void unit_ball() {
    // Lcg::in_unit_sphere draws three per try (ref_core in_unit_sphere).
    for (;;) {
      const auto x = rng.signed_unit(), y = rng.signed_unit(), z = rng.signed_unit();
      out.draws += 3;
      if (!lt(ONE, add(add(mul(x, x), mul(y, y)), mul(z, z)))) return;
    }
  }
  void begin_body() { out.body = out.events.size(); }
  std::uint32_t s() const { return c.state; }
  bool target() const { return c.target; }
  std::uint32_t ev() const { return c.ev; }
  std::uint32_t anim() const { return c.anim; }
  std::int64_t field(std::uint32_t o) const { return c.field(o); }
};

// C:66-73 / MOD:13-22: processDamage, Creature_run (nonzero ends the tick),
// updateActive (zero ends the tick). Executed in every state of every variant.
bool entry(M &m) {
  m.act("processDamage");
  m.act("baseRun");
  if (m.c.base_run_ends) { m.begin_body(); return false; }
  m.act("updateActive");
  if (!m.c.active) m.begin_body();
  return m.c.active;
}
// C:73-82 / MOD:25-31
bool head(M &m, Class cls, bool ally = false) {
  if (!entry(m)) return false;
  m.act("stepAnim", {1});
  m.out.events.push_back({"knockback", {table()[unsigned(cls)].knockback}});
  m.act("pickTarget", {0});
  if (ally) m.act("pickAlly");
  m.begin_body();
  return true;
}
[[noreturn]] void unwalked(const char *what) {
  throw Unsupported(std::string("creature_ai_m1_3: ") + what + " was not walked by the research (Unknown)");
}
// CAI13-5: the variant values the research walked; any other value refuses.
void variant(bool walked, const char *what) {
  if (!walked) throw Unsupported(std::string("creature_ai_m1_3: ") + what + " outside the walked variants (Unknown)");
}
// Common flee/hit-reaction/stun shapes (C:131-134, C:147-148).
void flee(M &m) { if (m.q("attackReady")) m.set(0); else m.act("flee"); }

// ---- C:253-275 Kobold (MOD:35-150) --------------------------------------
void kobold(M &m) {
  const auto v = m.field(0x3cc) & 0xff;
  variant(v <= 2, "Kobold +0x3cc");  // MOD:152: 0, 1 and 2
  if (m.s() == 9 || m.s() >= 0x18) unwalked("Kobold state 9/0x18 body");  // C:271-272
  if (!head(m, Class::Kobold, true)) return;
  switch (m.s()) {
  case 0:
    if (m.sight()) m.set(3);
    else if (m.q("away") || m.c.clock > m.c.deadline) m.set(1);
    break;
  case 1:
    if (m.sight()) m.set(3);
    else if (m.c.clock > m.c.deadline) m.set(0);  // C:257: without moving that tick
    else m.act("moveToDest");
    break;
  case 3:
    if (!m.target()) m.set(0);
    else if (v == 1) {
      if (m.q("alliesNearTarget", {24}) > 2) m.set(5);  // C:258: 2 stays, 3 goes
      else if (m.q("targetCharging") && m.see(0, 24, 6000)) m.set(7);
      else if (m.see(0, 24, 6000)) m.set(5);
      else if (m.see(0, 24, X)) m.act("turn");
      else m.act("moveToTarget");
    } else if (v == 2) {
      if (m.see(0, 24, 6000) || m.see(0, 164, 5000)) m.set(5);
      else if (m.see(0, 164, X)) m.act("turn");
      else m.act("moveToTarget");
    } else {
      if (m.see(0, 24, 6000)) m.set(5);
      else if (m.see(0, 24, X)) m.act("turn");
      else {
        m.act("angleAllyToTarget");
        if (m.q("inGuardSpot")) m.set(5);
        else if (m.q("allySeesTarget")) { m.act("getGuardPoint"); m.act("moveTo"); }
        else m.act("moveToTarget");
      }
    }
    break;
  case 5:
    if (!m.target()) { m.set(0); return; }
    m.act("turn");
    if (m.see(0, 24, 6000)) m.set(6);
    else if (v == 1) {
      if (m.q("notRooted") && !(m.q("alliesNearTarget", {24}) > 2)) m.set(3);
    } else if (v == 2) {
      if (m.q("corpse")) m.set(8);
      else if (m.q("notRooted") && !m.see(0, 200, 5000)) m.set(3);
    } else if (m.q("notRooted")) {
      if (!m.c.ally) m.set(3);
      else {
        m.act("angleAllyToTarget");
        if (!m.q("inGuardSpot")) m.set(3);
      }
    }
    break;
  case 6: case 7:
    if (m.target() && (m.ev() & 1))
      m.hit(0, 24, 6000, m.s() == 6 ? (v == 1 ? 0x440008u : 8u) : 0x80001u);
    if (m.s() == 7 && m.target() && (m.ev() & 2)) m.hit(0, 24, 6000, 1);
    if (m.target()) m.act("turn");
    if (!(m.anim() & 8)) m.set(5);
    break;
  case 8:  // C:270-271: even without a target
    if (m.ev() & 1) {
      const auto corpse = m.q("corpse");
      const auto wounded = m.q("wounded");
      if (corpse) m.act("revive");
      else if (wounded) { m.act("findSpellByType"); m.act("skill+0x14"); m.act("addSpell"); }
    }
    if (!(m.anim() & 8)) m.set(5);
    break;
  case 0x10: flee(m); break;
  case 0x12: if (!(m.anim() & 0x10)) m.set(3); break;
  case 0x13: if (m.q("notStunned")) m.set(3); break;
  case 0x14: m.act("deadUpdate", {20}); break;
  case 0x17: if (!(m.anim() & 0x10)) m.set(3); break;
  default: m.set(0);
  }
}

// ---- C:277-300 WarWraith (MOD:162-229) ----------------------------------
void warwraith(M &m) {
  // C:279-282: the fade byte +0x3c8 is read signed; 128-255 act as negative.
  const auto byte = std::uint32_t(m.field(0x3c8)) & 0xff;
  const std::int32_t fade = std::int32_t(byte ^ 0x80u) - 0x80;
  const auto s = m.s();
  if (s > 0x18) unwalked("WarWraith state above 0x18");
  if (!entry(m)) return;
  m.act("stepAnim", {1});
  if (s != 0x14) m.act("pickTarget", {0});  // C:80-81: before knock-back, not when dead
  m.act("knockback", {0});
  if (fade >= 99) m.act("turn");          // C:279: 98 does not
  m.begin_body();
  switch (s) {
  case 0:
    if (m.sight()) m.set(3);
    else if (m.c.clock > m.c.deadline) m.set(1);
    break;
  case 1:
    if (m.sight()) m.set(0);  // C:283: to 0, not 3
    else m.act("moveToDest");
    break;
  case 3: case 5:
    if (s == 3 && !m.target()) m.set(0);
    else if (m.see(0, 20, 6000)) m.set(m.c.clock > m.field(0x3cc) ? 7 : 6);  // C:284-285
    else if (s == 3) { m.see(0, 20, X); m.act("turn"); }
    else if (m.q("notRooted")) m.set(3);
    break;
  case 6: case 7:
    if (m.target() && (m.ev() & 1)) {
      if (s == 7) { m.act("skill+0x5c"); m.hit_reach(36, 0x404); }  // C:290-292 drain
      else m.hit_reach(36, 8);
    }
    if (m.target()) m.act("turn");
    if (!(m.anim() & 8)) m.set(3);
    break;
  case 0x10: flee(m); break;
  case 0x11: if (m.c.clock > m.c.deadline) m.set(3); break;
  case 0x12: if (!(m.anim() & 0x10)) m.set(3); break;
  case 0x13: if (m.q("notStunned")) m.set(0); break;
  case 0x14:
    // C:293-298, M2:6277-6279: a tick starting with fade >= 100 (signed:
    // 100-127) deletes; otherwise one particle (an angle draw and a unit-ball point).
    if (fade >= 100) m.act("objDelete");
    else { m.rand(); m.unit_ball(); }
    break;
  default: break;
  }
}

// ---- C:302-336 Orc (MOD:232-377) ----------------------------------------
void orc(M &m) {
  const auto v = m.field(0x478);
  const auto s = m.s();
  variant(v >= 0 && v <= 5, "Orc +0x478");  // MOD:1111: 0-5
  const bool melee = v <= 3;
  if (s > 0x18) unwalked("Orc state above 0x18");
  if (!head(m, Class::Orc)) return;
  switch (s) {
  case 0: case 1: case 2: {
    const bool forced = v == 4 && m.sight(1);
    if (forced || m.sight(0)) m.set(s == 2 && !forced ? 0 : 3);  // C:307-308
    else if (s == 0) { if (m.c.clock > m.c.deadline || m.q("away")) m.set(1); }
    else if (!m.q("moveToDest") && s == 2) m.set(0);
    break;
  }
  case 3:
    if (!m.target()) m.set(0);
    else if (melee) {
      if (m.see(0, 30, 6000)) m.set(6);
      else if (m.see(0, 30, X)) m.act("turn");
      else if ((v == 1 || v == 2 || v == 3) && m.q("alliesNearTarget", {30}) > 0) m.set(0x17);
      else if (v == 0 && m.q("alliesNearTarget", {30}) > 1) m.set(0x17);  // C:310
      else if ((v == 0 || v == 1) && m.rand() % 30 == 0 && m.q("countInBox") < 3) m.set(4);  // C:311-312
      else m.act("moveToTarget");
    } else if (v == 4) {
      if (m.see(0, 80, X)) m.set(0x10);
      else if (m.see(0, 400, 6000, 1)) m.set(6);
      else m.act("moveToTarget");
    } else {
      if (m.see(0, 400, 6000, 1)) m.set(6);
      else if (m.see(0, 30, 6000)) m.set(7);
      else if (m.see(0, 6000, X)) m.act("turn");
      else m.act("moveToTarget");
    }
    break;
  case 4:  // C:316-317: same tick, one own draw and moveToTarget still follow
    if (!m.target()) { m.set(0); return; }
    if (m.see(0, 30, 6000)) m.set(6);
    else if (m.see(0, 30, X)) m.act("turn");
    m.rand();
    m.act("moveToTarget");
    break;
  case 5:  // C:318-319
    if ((melee && m.see(0, 30, 6000)) || (!melee && m.see(0, 400, 6000, 1))) m.set(6);
    if (m.q("notRooted")) m.set(3);
    break;
  case 6: {
    if (melee) {
      if (m.target()) {
        if (m.ev() & 3) {
          m.hit_reach(36, 0x40008);
          if ((m.ev() & 1) && m.rand() % 5 == 0) m.set(3);
        }
        if (m.anim() & 8) m.act("turn");
      }
    } else {
      if (m.target()) m.act("leadTargetPoint");
      if (m.target() && (m.ev() & 1) && v == 4) {
        m.unit_ball(); m.rand(); m.act("poolNew");
      } else if (m.target() && (m.ev() & 1)) {
        std::int64_t corpse = 0, wounded = 0;
        if (!m.see(0, 80, X)) { corpse = m.q("corpse"); wounded = m.q("wounded"); }
        if (corpse) m.act("revive");
        else if (wounded) { m.act("skill+0x14"); m.act("addSpell"); }
        else if (m.rand() % 3 == 0) m.act("poolNew");
        else m.act("skill+0x14");
      }
    }
    // C:326-327: the stand counter +0xf4 counts down only once the clip has
    // ended, but is tested every tick.
    const auto left = (m.anim() & 8) ? m.c.deadline : m.c.deadline - 1;
    if (left <= 0) {
      if (v == 5) { m.rand(); m.set(0x17); }
      else m.set(3);
    }
    break;
  }
  case 7:
    if (m.ev() & 1) m.hit(0, 30, 6000, 1);
    if (!(m.anim() & 8)) m.set(0x17);
    break;
  case 0x10: flee(m); break;
  case 0x11: if (m.c.clock > m.c.deadline) m.set(3); break;
  case 0x12: if (!(m.anim() & 0x10)) m.set(3); break;
  case 0x13: if (m.q("notStunned")) m.set(0); break;
  case 0x14: m.act("deadUpdate", {30}); break;
  case 0x17:
    if (!m.target()) m.set(0);
    else if (v == 5) {
      if (m.c.clock > m.field(0x47c)) m.set(3);  // C:329-330: 99 goes, 100/101 stay
      else if (m.see(0, 30, 6000)) m.set(7);
      else if (m.see(0, 6000, X)) m.act("turn");  // C:331-332, M2:5492-5494
    } else if (v == 4) {
    } else if (v == 1 || v == 2 || v == 3) {
      if (m.see(0, 30, X) || m.q("alliesNearTarget", {30}) == 0) m.set(3);
    } else if (m.see(0, 30, 6000) || m.c.clock > m.c.deadline ||
               m.q("alliesNearTarget", {30}) < 2) m.set(3);  // C:333-334: 1 goes, 2 stays
    if (m.target()) m.act("turn");
    break;
  default: break;
  }
}

// ---- C:338-358 Goblin (MOD:380-485) -------------------------------------
void goblin(M &m) {
  const auto role = m.field(0x3d4);
  const auto s = m.s();
  variant(role >= 0 && role <= 5, "Goblin +0x3d4");  // MOD:1115: 0-5
  if (s == 7 || s == 8) unwalked("Goblin state 7/8 body (bow shot, shaman cast)");  // C:341-342
  if (s > 0x18) unwalked("Goblin state above 0x18");
  if (!head(m, Class::Goblin, role == 5)) return;
  switch (s) {
  case 0: case 1: case 2: {
    const bool archer = role == 4 && m.sight(1);
    if (archer) m.set(s == 0 ? 3 : 0);
    else if (m.sight(0)) m.set(s == 0 || s == 1 ? 3 : 0);
    else if (s == 0) { if (m.q("away")) m.set(1); }
    else if (!m.q("moveToDest") && s == 2) m.set(0);
    break;
  }
  case 3:
    if (!m.target()) { m.set(0); return; }
    if (role == 4) {  // C:345-346: and then also the melee block
      if (m.see(0, 80, X)) m.set(0x10);
      else if (m.see(0, 400, 6000, 1)) m.set(7);
      else m.act("moveToTarget");
    }
    if (role == 5) {
      if (m.see(0, 60, X)) m.set(0x10);
      else if (m.see(0, 400, 6000, 1)) m.set(8);
      else m.act("moveToTarget");
      return;
    }
    if (m.see(0, 30, 6000)) {
      if (m.q("hasShield") && m.q("targetAttacking") && m.rand() % 3 == 0) m.set(0x11);
      else m.set(6);
    } else if (m.see(0, 30, X)) m.act("turn");
    else if (role > 0 && m.q("allyInLineOfFire")) m.set(0x17);
    else if (m.rand() % 30 == 0 && m.q("countInBox") < 3) m.set(4);  // C:350
    else m.act("moveToTarget");
    break;
  case 4:
    if (!m.target()) m.set(0);
    else if (m.see(0, 30, 6000)) m.set(6);
    else { m.rand(); m.act("moveToTarget"); }
    break;
  case 5:
    if (!m.target()) m.set(3);
    else if (role == 4 || role == 5) {
      if (m.see(0, 400, 6000, 1)) m.set(role == 4 ? 7 : 8);
      else if (m.q("notRooted")) m.set(3);
    } else if (m.see(0, 30, 6000)) m.set(6);
    else if (m.q("notRooted")) m.set(3);
    break;
  case 6:
    if (m.target() && (m.ev() & 3)) {
      m.hit_reach(36, 0x40008);
      if ((m.ev() & 1) && m.rand() % 5 == 0) m.set(3);
    }
    if (m.anim() & 8) { if (m.target()) m.act("turn"); }
    else m.set(3);
    break;
  case 0x10: flee(m); break;
  case 0x11: if (m.c.clock > m.c.deadline) m.set(3); break;
  case 0x12: if (!(m.anim() & 0x30)) m.set(3); break;  // C:147: Goblin 0x30
  case 0x13: if (m.q("notStunned")) m.set(0); break;
  case 0x14: m.act("deadUpdate", {20}); break;
  case 0x17:
    if (!m.target()) { m.set(0); return; }
    if (role == 5) {
      if (m.see(0, 60, X)) m.set(0x10);
      else if (m.c.clock > m.field(0x3d8)) m.set(3);  // C:354-355
    } else if (m.c.clock > m.c.deadline || !m.q("allyInLineOfFire")) m.set(3);
    m.act("turn");
    break;
  default: break;
  }
}

// ---- C:360-371 Ogre (MOD:488-557) ---------------------------------------
void ogre(M &m) {
  const auto last = m.field(0x460);
  const auto s = m.s();
  if (s > 0x18) unwalked("Ogre state above 0x18");
  if (!head(m, Class::Ogre)) return;
  switch (s) {
  case 0: if (m.sight() || m.q("away")) m.set(1); break;  // C:363: to 1, not 3
  case 1: if (m.sight()) m.set(3); else m.act("moveToDest"); break;
  case 3:
    if (!m.target()) m.set(0);
    else if (m.see(0, 60, 2000)) m.set(5);
    else if (last != 8 && last != 9 && m.see(0, 60, X)) m.set(9);
    else if (!m.q("targetCharging") && m.see(0, 60, X)) m.act("turn");
    else if (m.see(0, 120, 2000) && m.q("alliesNearTarget", {60}) > 0) m.set(5);
    else m.act("moveToTarget");
    break;
  case 5:
    if (m.see(0, 60, 2000)) {
      if (last != 8 && last != 9 && m.rand() % 3 == 0) m.set(8);
      else if (last != 7 && m.q("blockedByTarget") && m.rand() % 3 == 0) m.set(7);
      else m.set(6);
    } else if (m.q("notRooted")) {  // C:367-368: the two 1-in-60 draws
      if (!m.see(0, 156, 2000) && m.rand() % 60 == 0) m.set(3);
      else if (m.rand() % 60 == 0 && !m.q("alliesNearTarget", {60})) m.set(3);
    }
    break;
  case 6: case 7: case 8: case 9:  // C:369-371
    if (s == 9) m.act("turn");
    if (s == 6 && (m.ev() & 1)) m.hit_all(0, 60, 6000, 0x40001);
    else if (s == 7 && (m.ev() & 1)) m.hit_all(0, 60, 6000, 0x100001, 0, std::array<std::int32_t, 2>{1, 5});
    else if (s == 8) {
      if (m.ev() & 1) m.hit_all(0, 60, 6000, 1);
      else if (m.ev() & 4) m.hit_all(0, 60, 6000, 0x80001);
    } else if (s == 9 && (m.ev() & 5)) m.hit_all(0, 60, 6000, 1);
    if (!(m.anim() & 8)) m.set(3);
    break;
  case 0x10: flee(m); break;
  case 0x12: if (!(m.anim() & 0x10)) m.set(3); break;
  case 0x13: if (m.q("notStunned")) m.set(3); break;
  case 0x14: m.act("deadUpdate", {30}); break;
  default: m.set(0);
  }
}

// ---- C:373-393 Warrior (MOD:560-648) ------------------------------------
void warrior(M &m) {
  const auto s = m.s();
  if (s > 0x18) unwalked("Warrior state above 0x18");
  if (!head(m, Class::Warrior)) return;
  const auto count = m.field(0x474) & 0xff;
  switch (s) {
  case 0: if (m.sight() || m.q("away")) m.set(1); break;
  case 1:
    if (!m.sight()) m.act("moveToDest");
    else { m.act("turn"); if (m.see(0, m.field(0x3ae) & 0xffff, 8000)) m.set(3); }
    break;
  case 3:
    if (!m.target()) m.set(0);
    else if (m.see(0, 12, 8000)) m.set(5);
    else if (!m.q("targetCharging") && m.see(0, 36, 8000)) m.set(5);
    else if (!m.q("targetCharging") && m.see(0, 36, X)) m.act("turn");
    else if (m.see(0, 120, 8000) && m.q("alliesNearTarget", {36}) >= 1) {  // C:379: 0 stays, 1 goes
      m.act("pickTarget", {1});
      m.set(5);
    } else m.act("moveToTarget");
    break;
  case 5:
    if (m.see(0, 36, 8000)) {
      if (m.q("targetAttacking") && m.rand() % 3 == 0 && m.q("canBlock")) m.set(0x17);
      else { m.q("blockedByTarget"); m.set(m.rand() % 2 == 0 ? 8 : 6); }
    } else if (m.see(0, 36, X)) m.act("turn");
    else if (m.q("notRooted")) {  // C:383-385: near asked every tick, allies only on the 1-in-60
      const auto near = m.see(0, 156, 8000);
      if (m.rand() % 60 == 0 && !(near && m.q("alliesNearTarget", {36}))) m.set(3);
    }
    if (m.c.clock > m.c.deadline) m.rand();
    break;
  case 6: case 7: case 8: {
    m.act("turn");
    const auto blocking = m.field(0x478);
    if (m.ev() & 1) {
      std::uint32_t flags = 8;
      if (s != 7 && (count == 3 || count == 5) && !m.q("alliesNearTarget", {36}))
        flags = count == 3 ? 0x440008u : 0x100008u;
      m.hit(0, 36, 8000, flags);
    }
    if (m.ev() & 2) {  // C:387-390
      if (s == 7) { if (m.see(0, 36, 8000)) m.set(m.field(0x470) == 6 ? 8 : 6); }
      else if (count < 5 && m.see(0, 36, 8000)) {
        if (count == 1) { if (blocking || (m.rand() & 1) == 0) m.set(7); }
        else if (count > 1 && blocking) m.set(7);
      }
    }
    if (!(m.anim() & 8)) m.set(3);
    break;
  }
  case 9: case 10:
    m.act("turn");
    if (s == 9 && (m.ev() & 1)) m.hit(0, 36, 8000, 8);
    if (!(m.anim() & 8)) m.set(3);
    break;
  case 0x10: flee(m); break;
  case 0x12: if (!(m.anim() & 0x10)) m.set(3); break;
  case 0x13: if (m.q("notStunned")) m.set(3); break;
  case 0x14: m.act("deadUpdate", {20}); break;
  case 0x17: if (m.c.clock > m.c.deadline) m.set(3); break;
  default: m.set(0);
  }
}

// ---- C:395-412 Clockwork (MOD:651-746) ----------------------------------
void clockwork(M &m) {
  const auto sub = m.field(0x3c8) & 0xff;
  const auto s = m.s();
  variant(sub <= 1, "Clockwork +0x3c8");  // MOD:1125-1127: 0 cutter, 1 hammer
  if (!m.c.object_flags)
    throw Unsupported("creature_ai_m1_3: Clockwork object flags (+0xc flag 2) not supplied");
  // Object flag 2, as this tick starts (the research walk held it clear).
  bool flag2 = (*m.c.object_flags & 2) != 0;
  if (s > 0x18) unwalked("Clockwork state above 0x18");
  if (!head(m, Class::Clockwork)) return;
  switch (s) {
  case 0: if (m.sight() || m.q("away")) m.set(1); break;
  case 1:
    if (!m.sight()) m.act("moveToDest");
    else { m.act("turn"); if (m.see(0, m.field(0x3ae) & 0xffff, 8000)) m.set(3); }
    break;
  case 3:
    if (!m.target()) m.set(0);
    else if (m.see(0, 36, 8000)) m.set(5);
    else if (m.see(0, 280, 16000)) m.set(9);
    else m.act("moveToTarget");
    break;
  case 5:
    if (m.see(0, 36, 8000)) {
      if (m.field(0x3cc) != 6) m.set(6);
      else m.set((m.rand() & 3) == 0 ? 8 : 7);
    } else if (m.see(0, 36, X)) m.act("turn");
    else if (m.q("notRooted")) m.set(3);
    break;
  case 6: case 7:
    m.act("turn");
    if (m.ev() & 1) {
      if (s == 6) m.hit(0, 36, 8000, sub ? 1u : 0x40008u);
      else m.hit(0, 36, 8000, sub ? 0x40001u : 8u);
    }
    if (!(m.anim() & 8)) m.set(5);
    break;
  case 8:
    if (!m.target()) m.set(0);
    else {
      m.act("turn");
      if (m.ev() & 1) { if (sub) m.hit_all(0, 36, X, 1); else m.hit(0, 36, 8000, 8); }
      if (!m.see(0, 36, 8000)) m.set(3);
      else if (m.c.clock > m.c.deadline) m.set(5);
    }
    break;
  case 9:  // C:405-408: no target -> 0 but the exit tests still run
    if (!m.target()) m.set(0);
    else { m.act("moveToTarget"); if (m.ev() & 1) m.hit(0, 36, 8000, sub ? 1u : 8u); }
    if (!m.q("targetCharging") && m.see(0, 12, 8000)) m.set(5);
    else if (!m.see(0, 280, 16000)) m.set(3);
    break;
  case 0x10: flee(m); break;
  case 0x12: if (!(m.anim() & 0x10)) m.set(3); break;
  case 0x13: if (m.q("notStunned")) m.set(3); break;  // C:409
  case 0x14:
    // M2:3711-3716 (Observed) / MOD:729-735: only while object flag 2 is
    // clear, event 0x2 or 0x4 bursts the wreck (0x4 adds the radius damage,
    // a draw and the light) and sets object flags 0x4003.
    if (!flag2 && (m.ev() & 6)) {
      if (m.ev() & 4) { m.act("damageInRadius"); m.rand(); m.act("poolNew"); }
      flag2 = true;
    }
    m.act("deadUpdate", {20});
    break;
  default: m.set(0);
  }
  // C:410-411, M2:3721-3723, MOD:738-742: hammer steam puffs, each one own
  // draw & 1 and, while object flag 2 is clear, a unit-ball velocity.
  if (sub)
    for (int i = 0; i < 2; ++i)
      if ((m.rand() & 1) && !flag2) m.unit_ball();
}

// ---- C:414-422 Droid (MOD:749-802) --------------------------------------
void droid(M &m) {
  const auto s = m.s();
  if (s > 0x18) unwalked("Droid state above 0x18");
  if (!head(m, Class::Droid)) return;
  if (droid_fuse_due(s, m.field(0x3cc), m.c.clock)) m.set(0x17);
  switch (s) {
  case 0: if (m.sight()) m.set(3); else if (m.q("away")) m.set(1); break;
  case 1: if (m.sight()) m.set(3); else m.act("moveToDest"); break;
  case 3:
    if (!m.target()) m.set(0);
    else if (m.see(0, 24, X)) m.set(0x17);
    else m.act("moveToTarget");
    break;
  case 5: if (m.see(0, 24, X)) m.set(0x17); else if (m.q("notRooted")) m.set(3); break;
  case 0x10: flee(m); break;
  case 0x12: if (!(m.anim() & 0x10)) m.set(5); break;
  case 0x13: if (m.q("notStunned")) m.set(5); break;
  case 0x14:
    if (m.ev() & 0x40) { for (int i = 0; i < 10; ++i) m.act("poolNew"); m.act("objDelete"); }
    break;
  case 0x17:  // C:420-422
    if (m.anim() & 0x80) {
      m.act("damageInRadius"); m.rand(); m.act("poolNew"); m.act("objDelete");
      for (int i = 0; i < 10; ++i) m.act("poolNew");
    }
    break;
  default: m.set(0);
  }
}

// ---- C:424-440 IronGuardian (MOD:805-886) -------------------------------
void ironguardian(M &m) {
  const auto s = m.s();
  if (s > 0x18) unwalked("IronGuardian state above 0x18");
  if (!head(m, Class::IronGuardian)) return;
  const auto timer = m.field(0x3cc);
  switch (s) {
  case 0:  // C:426-427: counter + 1 > 120 (119 stays; 120 and 121 go)
    if (!m.sight()) { if (m.q("away")) m.set(1); }
    else if (timer + 1 > 120) m.set(3);
    break;
  case 1: if (m.sight()) m.set(0); else m.act("moveToDest"); break;
  case 3:
    if (!m.target()) m.set(0);
    else if (m.cone(84, 400, 2000)) m.set(9);
    else if (timer < m.c.clock && m.cone(240, 400, 10000)) m.set(10);  // C:428-429
    else if (m.cone(0, 84, X)) m.set(5);
    else m.act("moveToTarget");
    break;
  case 5:
    if (m.cone(0, 84, 10000)) m.set(6);
    else if (m.cone(0, 84, 0x4000, 0x4000)) m.set(7);
    else if (m.cone(0, 84, 0x4000, -0x4000)) m.set(8);
    else if (m.cone(84, 400, 2000)) m.set(9);
    else if (timer < m.c.clock && m.cone(240, 400, 10000)) m.set(10);
    else if (m.q("notRooted")) m.set(3);
    break;
  case 6:
    if (m.ev() & 1) m.hit_all(0, 84, 10000, 8);
    if (!(m.anim() & 8)) m.set(5);
    break;
  case 7: case 8:
    // C:432-435, M2:1392: turns only while exactly one player is alive
    // (executed with 0, 1, 2, 3 and 4).
    if (m.c.alive_players == 1) m.act("turn");
    if (m.ev() & 1) m.hit_all(0, 84, 0x4000, 0x80008, s == 7 ? 0x4000 : -0x4000);
    if (!(m.anim() & 8)) m.set(5);
    break;
  case 0x10: flee(m); break;
  case 0x12: if (!(m.anim() & 0x10)) m.set(5); break;
  case 0x13: if (m.q("notStunned")) m.set(5); break;
  case 9: m.act("turn"); if (!(m.anim() & 8)) m.set(5); break;
  case 10:
    m.act("turn");
    if (m.ev() & 1) {
      m.rand();
      for (int i = 0; i < 10; ++i) { m.act("poolNew"); m.rand(); m.unit_ball(); }
    }
    if (!(m.anim() & 8)) m.set(3);
    break;
  case 0x14: m.act("deadUpdate", {30}); break;
  default: m.set(0);
  }
}

// ---- C:442-452 Froglock (MOD:889-950) -----------------------------------
void froglock(M &m) {
  // MOD:1142, M2:4130-4134: hand-1 part 0x12 trident, 0x10 blowgun.
  variant(m.c.holder_hand1 == 0x12 || m.c.holder_hand1 == 0x10, "Froglock holder hand-1 part");
  const bool blowgun = m.c.holder_hand1 == 0x10;
  if (!m.c.water) throw Unsupported("creature_ai_m1_3: Froglock water state not supplied");
  const auto &w = *m.c.water;
  for (auto b : {w.was_standing, w.ahead, w.ahead_before})
    variant(b <= 1, "Froglock water byte (+0x460/+0x461/+0x462)");
  // Helper 0x265754 (M2:4143-4145, Observed): water was just entered, or the
  // water ahead has just ended. Its callers re-request the CURRENT state.
  const bool water_changed = (!w.was_standing && w.standing) || (w.ahead_before && !w.ahead);
  const auto s = m.s();
  if (s == 7 || s == 8) unwalked("Froglock state 7/8 body (blow darts)");  // C:451
  if (s > 0x18) unwalked("Froglock state above 0x18");
  if (!head(m, Class::Froglock)) return;
  switch (s) {
  case 0:
    if (m.sight()) m.set(3);
    else if (m.q("away") || m.c.clock > m.c.deadline) m.set(1);
    break;
  case 1:  // M2:4152-4153: after moveToDest the helper re-requests 1
    if (m.sight()) m.set(0);
    else { m.act("moveToDest"); if (water_changed) m.set(1); }
    break;
  case 3:
    if (!m.target()) m.set(0);
    else if (!blowgun) {
      if (m.see(0, 40, 6000)) m.set(6);
      else if (!m.see(0, 300, X) && m.see(0, 450, 1000)) m.set(0x17);
      else if (m.see(0, 40, X)) m.act("turn");
      else { m.act("moveToTarget"); if (water_changed) m.set(3); }  // M2:4157
    } else {
      if (m.see(0, 100, X)) m.set(0x10);
      // C:447-448, M2:4158-4159: 7 dry, 8 while the standing-in-water cflag is set.
      else if (m.see(0, 400, 6000)) m.set(w.standing ? 8 : 7);
      else if (m.see(0, 400, X)) m.act("turn");
      else { m.act("moveToTarget"); if (water_changed) m.set(3); }  // M2:4160
    }
    break;
  case 5:
    if ((blowgun && m.see(0, 400, 6000)) || (!blowgun && m.see(0, 40, 6000))) m.set(blowgun ? 7 : 6);
    else if (m.q("notRooted")) m.set(3);
    break;
  case 6:
    if (m.target() && (m.ev() & 1)) m.hit_reach(36, 8);
    if (!(m.anim() & 8)) m.set(5);
    break;
  case 0x10:  // M2:4178-4179: the helper first re-requests 0x10
    if (water_changed) m.set(0x10);
    else flee(m);
    break;
  case 0x12: if (!(m.anim() & 0x10)) m.set(3); break;
  case 0x13: if (m.q("notStunned")) m.set(0); break;
  case 0x14: m.act("deadUpdate", {25}); break;
  case 0x17: if (!(m.anim() & 0x10)) m.set(3); break;
  default: break;
  }
}

// ---- C:454-470 Amphian (MOD:953-1027) -----------------------------------
void amphian(M &m) {
  const auto s = m.s();
  if (s > 0x18) unwalked("Amphian state above 0x18");
  if (!head(m, Class::Amphian)) return;
  switch (s) {
  case 0: if (m.sight()) m.set(3); else if (m.q("away")) m.set(1); break;
  case 1: if (m.sight()) m.set(3); else m.act("moveToDest"); break;
  case 3:
    if (!m.target()) m.set(0);
    else if (m.see(0, 12, 8000)) m.set(5);
    else if (!m.q("targetCharging") && m.see(0, 36, 8000)) m.set(5);
    else if (!m.q("targetCharging") && m.see(0, 36, X)) m.act("turn");
    else if (m.see(120, 360, 2000)) m.set(9);
    else m.act("moveToTarget");
    break;
  case 5:
    if (m.see(0, 36, 8000)) {
      // C:459-465: hit points +0x228 < 0.5 x maximum +0x22c (strict, relative)
      // and an own draw & 3 == 0 -> block; the draw is made only below half.
      if (lt(m.c.hp, mul(m.c.max_hp, HALF)) && (m.rand() & 3) == 0) m.set(0x17);
      else m.set(6 + m.rand() % 3);
    } else if (m.see(0, 36, X)) m.act("turn");
    else if (m.see(120, 360, 2000)) m.set(9);
    else if (m.q("notRooted")) m.set(3);
    break;
  case 6: case 7: case 8:
    m.act("turn");
    if (m.ev() & 1) m.hit(0, 36, 8000, 8);
    if (m.ev() & 2) m.hit(0, 36, 8000, 0x40001);
    if (!(m.anim() & 8)) m.set(5);
    break;
  case 0x10: flee(m); break;
  case 0x12: if (!(m.anim() & 0x10)) m.set(3); break;
  case 9:
    m.act("turn");
    if (m.ev() & 1)
      for (int i = 0; i < 2; ++i) { m.unit_ball(); m.act("poolNew"); m.rand(); }
    if (!(m.anim() & 8)) m.set(5);
    break;
  case 0x13: if (m.q("notStunned")) m.set(3); break;  // C:468
  case 0x14: m.act("deadUpdate", {20}); break;
  case 0x17: if (m.c.clock > m.c.deadline) m.set(5); break;
  default: m.set(0);
  }
}

// ---- C:472-479 SeaMonster (MOD:1030-1087) -------------------------------
void seamonster(M &m) {
  const auto s = m.s();
  if (s > 0x18) unwalked("SeaMonster state above 0x18");
  if (!head(m, Class::SeaMonster)) return;
  switch (s) {
  case 0: if (m.sight()) m.set(3); else if (m.q("away")) m.set(1); break;
  case 1: if (m.sight()) m.set(3); else m.act("moveToDest"); break;
  case 3:
    if (!m.target()) m.set(0);
    else if (m.see(0, 36, 5000)) m.set(5);
    else if (m.see(0, 36, X)) m.act("turn");
    else m.act("moveToTarget");
    break;
  case 5:
    if (!m.see(0, 36, 5000)) { if (m.q("notRooted")) m.set(3); }
    else if ((m.rand() & 1) == 0) m.set(7);
    else m.set(m.rand() % 3 == 0 ? 9 : 6);
    break;
  case 6: case 7: case 8:
    if (m.target() && (m.ev() & 1)) m.hit(0, 36, 5000, 8);
    if ((s == 7 || s == 8) && (m.ev() & 2) && (m.rand() & 1) == 0 && m.see(0, 36, 5000))
      m.set(s == 7 ? 8 : 7);
    if (!(m.anim() & 8)) m.set(5);
    break;
  case 9:
    if (m.target() && (m.ev() & 1)) m.hit(0, 36, 5000, 0x80008);
    if (!(m.anim() & 8)) m.set(5);
    break;
  case 0x10: flee(m); break;
  case 0x12: if (!(m.anim() & 0x10)) m.set(5); break;
  case 0x13: if (m.q("notStunned")) m.set(5); break;
  case 0x14: m.act("deadUpdate", {30}); break;
  case 0x17: case 0x18: if (!(m.anim() & 8)) m.set(3); break;
  default: break;
  }
}

// ---- setState entries (C:115-127, C:131-154; MOD:1157-1196) --------------
struct Entry {
  std::optional<std::uint32_t> land;
  std::vector<EntryWrite> writes;
  std::vector<std::string> calls, absent;
  std::vector<std::uint32_t> unwritten;
};
EntryWrite exact(std::uint32_t off, std::int64_t v) { return {EntryWrite::Exact, off, v, 1}; }
EntryWrite clockdraw(std::uint32_t off, std::int64_t base, std::uint32_t mod) {
  // (base, 1) is clock + base with no draw (C:336, C:358: "clock + 120").
  return mod == 1 ? EntryWrite{EntryWrite::Clock, off, base, 1} : EntryWrite{EntryWrite::ClockDraw, off, base, mod};
}
EntryWrite absdraw(std::uint32_t off, std::int64_t base, std::uint32_t mod) { return {EntryWrite::AbsDraw, off, base, mod}; }
Entry none() { Entry e; e.land = 0; return e; }
Entry death(std::vector<std::string> calls, std::vector<std::string> absent = {}) {
  Entry e; e.calls = std::move(calls); e.absent = std::move(absent); return e;
}
Entry w(std::vector<EntryWrite> writes) { Entry e; e.writes = std::move(writes); return e; }
const std::vector<std::string> DEATH = {"dropLoot", "onDeath"};

const std::map<std::uint32_t, Entry> &entries(Class c) {
  static const std::array<std::map<std::uint32_t, Entry>, CLASS_COUNT> t = [] {
    std::array<std::map<std::uint32_t, Entry>, CLASS_COUNT> r;
    const auto cooldown = w({exact(0x318, 60)});  // C:131-132
    r[unsigned(Class::Kobold)] = {{0, w({clockdraw(0xf4, 0, 120)})}, {1, w({clockdraw(0xf4, 120, 120)})},
        {5, w({clockdraw(0xf4, 60, 120)})}, {6, w({exact(0x3c8, 6)})}, {7, w({exact(0x3c8, 7)})},
        {8, w({exact(0x3c8, 8)})}, {0x17, w({exact(0x3c8, 0x17)})}, {0x10, cooldown},
        {4, none()}, {0xa, none()}, {0x11, none()}, {0x14, death(DEATH)}};
    r[unsigned(Class::WarWraith)] = {{0, w({clockdraw(0xf4, 0, 120)})}, {7, w({clockdraw(0x3cc, 300, 120)})},
        {0x11, w({clockdraw(0xf4, 120, 60)})}, {0x14, death({"lootGenerate", "onDeath"})}};
    r[unsigned(Class::Orc)] = {{0, w({clockdraw(0xf4, 120, 60)})}, {4, w({clockdraw(0xf4, 40, 120)})},
        {6, w({absdraw(0xf4, 10, 10)})}, {0x10, cooldown},
        {0x11, w({clockdraw(0xf4, 120, 1), exact(0xa8, 0x200)})},
        {0x17, w({clockdraw(0xf4, 240, 240)})}, {0x14, death(DEATH)}};
    r[unsigned(Class::Goblin)] = {{0, w({clockdraw(0xf4, 120, 60)})}, {4, w({clockdraw(0xf4, 40, 120)})},
        {0x10, cooldown}, {0x11, w({clockdraw(0xf4, 120, 1), exact(0xa8, 0x200)})},
        {0x17, w({clockdraw(0xf4, 240, 240)})}, {0x14, death({"dropLoot"})}};
    r[unsigned(Class::Ogre)] = {{0, w({clockdraw(0xf4, 0, 120)})}, {6, w({exact(0x460, 6)})},
        {7, w({exact(0x460, 7)})}, {8, w({exact(0x460, 8)})}, {9, w({exact(0x460, 9)})},
        {0x10, cooldown}, {4, none()}, {0xa, none()}, {0x11, none()}, {0x17, none()},
        {0x14, death(DEATH)}};
    r[unsigned(Class::Warrior)] = {{0, w({clockdraw(0xf4, 0, 120)})}, {3, w({exact(0x474, 0), exact(0x478, 0)})},
        {5, w({clockdraw(0xf4, 0, 360)})}, {6, w({exact(0x470, 6), exact(0x474, 1)})},
        {7, w({exact(0x474, 1)})}, {8, w({exact(0x470, 8), exact(0x474, 1)})}, {0x10, cooldown},
        {0x17, w({clockdraw(0xf4, 60, 120), exact(0xa8, 0x200)})}, {4, none()}, {0x11, none()},
        {0x14, death(DEATH)}};
    r[unsigned(Class::Clockwork)] = {{6, w({exact(0x3cc, 6)})}, {7, w({exact(0x3cc, 7)})},
        {8, w({exact(0x3cc, 8), clockdraw(0xf4, 120, 1)})},
        {9, w({clockdraw(0xf4, 0, 1), clockdraw(0x4f8, 0, 1)})}, {0x10, cooldown},
        {4, none()}, {0xa, none()}, {0x11, none()}, {0x17, none()}, {0x14, death(DEATH)}};
    // C:151-153: Droid's 0x14 and 0x17 call neither dropLoot nor loot_generateDrops.
    r[unsigned(Class::Droid)] = {{0x10, cooldown}, {4, none()}, {6, none()}, {0xa, none()}, {0x11, none()},
        {0x14, death({"onDeath"}, {"dropLoot", "lootGenerate"})},
        {0x17, death({"onDeath"}, {"dropLoot", "lootGenerate"})}};
    r[unsigned(Class::IronGuardian)] = {{9, w({exact(0x3c8, 0)})}, {10, w({exact(0x3c8, 0)})},
        {0x10, cooldown}, {4, none()}, {0x11, none()}, {0x17, none()}, {0x14, death(DEATH)}};
    r[unsigned(Class::Froglock)] = {{0, w({clockdraw(0xf4, 0, 120)})}, {7, w({absdraw(0xf4, 0, 15)})},
        {8, w({absdraw(0xf4, 0, 15)})}, {0x10, cooldown}, {0x14, death(DEATH)}};
    r[unsigned(Class::Amphian)] = {{0x10, cooldown}, {0x17, w({clockdraw(0xf4, 60, 120), exact(0xa8, 0x200)})},
        {4, none()}, {0xa, none()}, {0x11, none()}, {0x14, death(DEATH)}};
    // C:131-132: SeaMonster's setState(0x10) writes no +0x318.
    Entry sea_flee; sea_flee.unwritten = {0x318};
    r[unsigned(Class::SeaMonster)] = {{0x14, death(DEATH)}, {0x10, sea_flee}};
    return r;
  }();
  return t[unsigned(c)];
}
} // namespace

const ClassInfo &info(Class c) {
  if (unsigned(c) >= CLASS_COUNT) throw Unsupported("creature_ai_m1_3: invalid class");
  return table()[unsigned(c)];
}
std::optional<Class> class_of_constructor(std::uint32_t ctor) {
  for (const auto &i : table()) if (i.constructor == ctor) return i.cls;
  return std::nullopt;
}

const std::vector<LevelCreature> &level_creatures() {
  // C:41-56 / REF:19-42 (Executed factory selection, 807 records).
  static const std::vector<LevelCreature> t = {
    {"blood1", "Kobold", 118, 0x3d0, 0x27f0c0}, {"blood1", "Koboldtough", 8, 0x3d0, 0x27f0c0},
    {"blood1", "koboldshaman", 5, 0x3d0, 0x27f0c0}, {"blood1", "WarWraith", 9, 0x468, 0x2c07a8},
    {"strife", "Orc2Archer", 20, 0x488, 0x2a0954}, {"strife", "Orc2Leader", 2, 0x488, 0x2a0954},
    {"strife", "Orc2Leader1", 2, 0x488, 0x2a0954}, {"strife", "Orc2Shaman", 2, 0x488, 0x2a0954},
    {"strife", "Orc2Soldier", 43, 0x488, 0x2a0954}, {"strife", "Catapult", 2, 0xfc, 0x24d440},
    {"drundar", "GoblinArcher", 23, 0x498, 0x26a624}, {"drundar", "GoblinRecruit", 9, 0x498, 0x26a624},
    {"drundar", "GoblinSoldier", 39, 0x498, 0x26a624}, {"drundar", "Ogre", 14, 0x468, 0x29fad0},
    {"drunare", "UndeadKnight1", 1, 0x478, 0x271c70}, {"drunare", "UndeadKnight2", 1, 0x478, 0x271c70},
    {"drunare", "UndeadKnight3", 1, 0x478, 0x271c70}, {"drunare", "UndeadKnight4", 1, 0x478, 0x271c70},
    {"drunare", "CazicAvatar", 1, 0x3e8, 0x24a670}, {"drunare", "RalosZek", 1, 0x480, 0x2c1f94},
    {"drunarm", "GladiatorBare", 82, 0x480, 0x2bafb4}, {"drunarm", "RalosZek", 2, 0x480, 0x2c1f94},
    {"drunarm", "Catapult", 4, 0xfc, 0x24d440},
    {"innovat1", "clockworkcutter", 23, 0x500, 0x24e124}, {"innovat1", "clockworkhammer", 29, 0x500, 0x24e124},
    {"innovat1", "Rat", 36, 0x448, 0x2547fc}, {"innovat1", "Seagull", 30, 0x448, 0x2547fc},
    {"inmine1", "clockworkcutter", 23, 0x500, 0x24e124}, {"inmine1", "clockworkhammer", 23, 0x500, 0x24e124},
    {"inmine1", "Droid", 30, 0x3d0, 0x27b840},
    {"inmine2", "clockworkcutter", 20, 0x500, 0x24e124}, {"inmine2", "clockworkhammer", 14, 0x500, 0x24e124},
    {"innovat2", "IronGuardian", 1, 0x3e8, 0x27a00c}, {"innovat2", "Rat", 8, 0x448, 0x2547fc},
    {"innovat2", "Seagull", 12, 0x448, 0x2547fc},
    {"islc1", "Froglock", 46, 0x468, 0x263e1c}, {"islc1", "Crab", 39, 0x448, 0x2547fc},
    {"islc2", "Amphian", 37, 0x3c8, 0x234584}, {"islc2", "Crab", 44, 0x448, 0x2547fc},
    {"islc2", "SeaMonster", 2, 0x3d0, 0x2a428c},
  };
  return t;
}
const std::map<std::string, unsigned> &level_record_totals() {
  static const std::map<std::string, unsigned> t = {  // REF:43-45
    {"blood1", 363}, {"strife", 283}, {"drundar", 270}, {"drunare", 39}, {"drunarm", 302},
    {"innovat1", 297}, {"inmine1", 213}, {"inmine2", 118}, {"innovat2", 71}, {"islc1", 262},
    {"islc2", 255}};
  return t;
}
const std::vector<KeyCount> &key_counts() {
  // C:35-39, REF:49-54: key names only. Which RalosZek record fights is an
  // inference from the keys (C:488-491), not ported as behaviour.
  static const std::vector<KeyCount> t = {
    {"drundar", "Ogre", {"miniboss"}, {}, 1},
    {"islc2", "SeaMonster", {"miniboss"}, {}, 2},
    {"drunarm", "RalosZek", {"visualarc", "dropitem"}, {"wait"}, 1},
    {"drunarm", "RalosZek", {"wait"}, {"visualarc", "dropitem"}, 1},
  };
  return t;
}
const LevelCreature &factory_pick(const std::string &level, const std::string &type) {
  for (const auto &r : level_creatures())
    if (level == r.level && type == r.type) return r;
  throw Unsupported("creature_ai_m1_3: type not placed in that level (Unknown)");
}

std::vector<std::uint32_t> Tick::requests() const {
  std::vector<std::uint32_t> r;
  for (const auto &e : events) if (e.name == "set") r.push_back(std::uint32_t(e.args.at(0) / ARG_SCALE));
  return r;
}
bool Tick::has(const std::string &name) const {
  return std::any_of(events.begin(), events.end(), [&](const Event &e) { return e.name == name; });
}

bool droid_fuse_due(std::uint32_t state, std::int64_t self_destruct, std::int64_t clock) {
  return self_destruct < clock && state != 0x14 && state != 0x17;
}

Tick tick(Class c, const Context &ctx, const Answer &answer) {
  M m(ctx, answer);
  switch (c) {
  case Class::Kobold: kobold(m); break;
  case Class::WarWraith: warwraith(m); break;
  case Class::Orc: orc(m); break;
  case Class::Goblin: goblin(m); break;
  case Class::Ogre: ogre(m); break;
  case Class::Warrior: warrior(m); break;
  case Class::Clockwork: clockwork(m); break;
  case Class::Droid: droid(m); break;
  case Class::IronGuardian: ironguardian(m); break;
  case Class::Froglock: froglock(m); break;
  case Class::Amphian: amphian(m); break;
  case Class::SeaMonster: seamonster(m); break;
  default: throw Unsupported("creature_ai_m1_3: invalid class");
  }
  m.out.seed = m.rng.state;
  return m.out;
}

std::int64_t EntryWrite::resolve(std::int64_t clock, const std::function<std::uint32_t()> &draw) const {
  switch (kind) {
  case Exact: return base;
  case Clock: return clock + base;
  case ClockDraw: return clock + base + std::int64_t(draw() % mod);
  case AbsDraw: return base + std::int64_t(draw() % mod);
  }
  return base;
}

SetResult set_state(Class c, const SetContext &sc, std::uint32_t requested) {
  if (c == Class::Kobold && requested == 9)
    throw Unsupported("creature_ai_m1_3: Kobold state 9 entry (FrostArmor) is Unknown");  // C:271-272
  // Every setState row ran with the class variant field at 0 (SetContext).
  if (sc.variant != 0 && (c == Class::Kobold || c == Class::Orc || c == Class::Goblin || c == Class::Clockwork))
    throw Unsupported("creature_ai_m1_3: setState with a nonzero class variant was not executed (Unknown)");
  SetResult r;
  r.requested = requested;
  // Creature_filterState 0x10a9c0 (C:115-122), same order as the audited
  // monsters_1::Model::filter: PvP death refusal, +0x231 wander refusal,
  // 0x14 clears, root, stun, cooldown.
  std::int32_t state = std::int32_t(requested);
  if (sc.global_refuses || (sc.x231 && state == 1)) { r.refused = true; return r; }
  if (state == 0x14)
    for (auto off : {0x31cu, 0x318u, 0x314u, 0x31au}) r.cleared.push_back({off, 0});
  const auto root = state == 0x14 ? 0 : sc.root, stun = state == 0x14 ? 0 : sc.stun;
  auto cooldown = state == 0x14 ? 0 : sc.cooldown;
  if (root && state != 0 && state != 5 && !(state >= 6 && state <= 15)) state = sc.target ? 5 : 0;
  else if (stun && state != 19) state = 19;
  else {
    if (!sc.target) { r.cleared.push_back({0x318, 0}); cooldown = 0; }
    if (cooldown && state != 16) state = 16;
  }
  if (state == 0) r.target_cleared = true;
  r.filtered = std::uint32_t(state);
  const auto &table = entries(c);
  const auto it = table.find(r.filtered);
  // CAI13-3: a landing is executed only for a SET_ENTRY row (MOD:1163-1197,
  // DT:401-436) or for a check_filter landing (DT:442-461). Anything else,
  // for example WarWraith 8 or Droid 9, may have no case and store 0 (C:124).
  constexpr std::uint32_t FILTER_LANDINGS[] = {0, 3, 5, 6, 0x10, 0x13, 0x14};
  if (it == table.end() &&
      std::find(std::begin(FILTER_LANDINGS), std::end(FILTER_LANDINGS), r.filtered) == std::end(FILTER_LANDINGS))
    throw Unsupported("creature_ai_m1_3: setState landing " + std::to_string(r.filtered) +
                      " for this class was not executed (Unknown)");
  r.landed = r.filtered;
  r.entry_known = it != table.end();
  if (it != table.end()) {
    if (it->second.land) r.landed = *it->second.land;
    r.writes = it->second.writes;
    r.calls = it->second.calls;
    r.absent = it->second.absent;
    r.unwritten = it->second.unwritten;
  }
  return r;
}

bool hit_at_reach(const ReachProbe &p) {
  // C:166-179 (Executed boundaries). The facing uses the ported cos16/sin16;
  // the +y asymmetry at exactly 60.0 is Inferred from them (C:176-179), not
  // an added offset.
  const auto px = add(p.attacker[0], mul(cos16(p.facing), p.reach));
  const auto py = add(p.attacker[1], mul(sin16(p.facing), p.reach));
  const auto pz = p.attacker[2];
  const auto dx = sub(p.target[0], px), dy = sub(p.target[1], py), dz = sub(p.target[2], pz);
  const auto d = sqrt(add(add(mul(dx, dx), mul(dy, dy)), mul(dz, dz)));
  if (!lt(d, 0x42700000u)) return false;
  return !(p.target_flags & 8);
}
} // namespace champions::simulation::rta::creature_ai_m1_3
