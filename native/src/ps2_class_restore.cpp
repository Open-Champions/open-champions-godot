#include "champions/ps2_class_restore.hpp"
#include "champions/core_math.hpp"
#include <limits>

namespace champions::simulation::rta::ps2_save {
namespace {
// All class contracts below are pinned to research-death-reload
// 0e3afdb8ce3cdf79746f535d8a2654db37c9e97b. References are relative to it.
template<class F> const F &need(const F &f, const char *name) {
  if (!f) throw Unknown(std::string("class reload refused: unbound ") + name);
  return f;
}
struct Reader {
  const Bytes &data;
  std::size_t at = 0;
  std::uint64_t get(unsigned n) {
    if (at > data.size() || n > data.size()-at) throw Error("truncated class reload payload");
    std::uint64_t v = 0;
    for (unsigned i = 0; i < n; ++i) v |= std::uint64_t(data[at++]) << (8*i);
    return v;
  }
};
level_flow::Pieces &pieces(const ClassLoadServices &s) {
  if (!s.pieces) throw Unknown("class reload refused: world pieces unavailable");
  return *s.pieces;
}
// Host-side admission checks are deliberately stricter than a general loader.
// Parse the whole bounded payload before writes or service calls; this avoids
// leaving an object half-restored for a truncated or unsupported pose/encoding.
// This is not rollback for a service throwing: supported prefixes remain applied.
void admit(builtins::Memory &m, std::uint32_t c, const std::string &cls,
           Reader r, const ClassLoadServices &s) {
  if (cls == "Trigger") { // docs/death-reload.md:75
    r.get(4); r.get(1);
  } else if (cls == "Creature") { // docs/death-reload.md:76; docs/save-world.md:120
    r.get(4);
    const auto ratio = Raw(r.get(4));
    if ((ratio & 0x7f800000u) == 0x7f800000u)
      throw Unknown("Creature nonfinite HP ratio outside researched domain");
    if (Fpu::compare(ratio, ZERO) <= 0) r.get(4);
    const auto bits = r.get(4);
    if (bits & ~0x7full) throw Unknown("Creature packed flags outside documented domain");
    if (bits & 64) r.get(4);
    else if (m.u16(c+0x312) != 0)
      // The pin starts +0x312 at zero and only proves the optional write.
      // tools/difftest_death_reload.py:239,264-272; death-reload.md:76.
      throw Unknown("Creature nonzero constructor charm without optional word Unknown");
  } else if (cls == "Chest" || cls == "AutoChest") { // docs/death-reload.md:77-78
    r.get(4); r.get(2); r.get(1); r.get(1); r.get(4);
  } else if (cls == "DoorSwing" || cls == "DoorSecret" || cls == "RollDoor") {
    // docs/death-reload.md:79-81,85-89. No negative/missing piece was probed.
    std::int32_t index = m.s16(c+0x4c);
    bool needs_piece = true;
    std::uint16_t angle = 0;
    if (cls == "DoorSwing") { angle = std::uint16_t(r.get(2)); r.get(2); r.get(4); }
    else if (cls == "DoorSecret") needs_piece = r.get(1) != 0;
    else { r.get(4); index = std::int32_t(r.get(4)); r.get(4); r.get(4); }
    if (needs_piece) {
      const auto &world = pieces(s);
      if (index < 0 || std::size_t(index) >= world.data().size())
        throw Unknown("door piece index outside constructed world");
      if (cls == "DoorSwing" && ((angle != 0 && angle != 32768) ||
          world.data()[std::size_t(index)].tx || world.data()[std::size_t(index)].ty))
        throw Unknown("DoorSwing arbitrary-angle/origin reconstruction Unknown");
    }
  } else if (cls == "JumpGate") { // docs/death-reload.md:82
    r.get(1); r.get(1);
  } else if (cls == "Prop" || cls == "ItemProp") {
    // docs/death-reload.md:83; docs/save-world.md:123;
    // tools/difftest_death_reload.py:291-336. Identity quaternion/facing only.
    if (cls == "ItemProp") {
      const auto index = r.get(4);
      if (index > (std::numeric_limits<std::uint32_t>::max()-0x3f7650u)/0x40u)
        throw Unknown("ItemProp definition index outside addressable table");
      for (unsigned i = 4; i < 0x48; ++i) r.get(1);
      if (std::uint16_t(m.u16(c+0x38)+0x4000) != 0)
        throw Unknown("ItemProp arbitrary facing reconstruction Unknown");
    }
    r.get(4); r.get(4); r.get(4);
    const auto tag = r.get(1);
    if (tag == 255) {
      if (r.get(3) != 0xffffff) throw Unknown("Prop mask encoding outside executed domain");
      r.get(8);
      if (r.get(1) != 0) throw Unknown("Prop mask suffix outside executed domain");
    } else if (tag >= 64) throw Unknown("Prop mask bit outside documented width");
    const auto x = r.get(1), y = r.get(1), z = r.get(1), w = r.get(2);
    if (x || y || z || w != 10000)
      throw Unknown("Prop quaternion outside executed identity domain; arbitrary pose Unknown");
    r.get(2); r.get(2); r.get(2);
  } else throw Unknown("class reload refused: undocumented class reconstruction");
}
// docs/death-reload.md:83; tools/difftest_death_reload.py:333-336.
void matrix(builtins::Memory &m, std::uint32_t c, const Mat34 &v) {
  for (unsigned i = 0; i < v.size(); ++i) m.word(c+0x110+4*i, v[i]);
}
// docs/death-reload.md:83 and tools/difftest_death_reload.py:291-336;
// docs/save-world.md:123. Only the executed identity-quaternion domain is
// admitted. Arbitrary quantized quaternion reconstruction remains Unknown.
std::size_t prop(builtins::Memory &m, std::uint32_t c, Reader &r, const ClassLoadServices &s) {
  std::string strings[3];
  for (auto &str : strings) {
    const auto handle = std::uint32_t(r.get(4));
    str = need(s.string, "world string resolution")(handle);
  }
  // Pin docs/objects.md:333-337 and docs/death-reload.md:83: the names
  // themselves are copied, not only resolved pointers. Host admission stays
  // inside the known adjacent fields; it does not emulate overflow/truncation.
  constexpr std::uint32_t offsets[] = {0x54, 0x64, 0xa4};
  constexpr std::size_t rooms[] = {0x64-0x54, 0xa4-0x64, 0xe8-0xa4};
  for (unsigned i=0; i<3; ++i)
    if (strings[i].size() >= rooms[i] || strings[i].find('\0') != std::string::npos)
      throw Unknown("Prop resource name outside bounded field");
  for (unsigned i=0; i<3; ++i) m.string(c+offsets[i], strings[i]);
  m.word(c+0x4c, need(s.asset, "model asset lookup")(strings[0], strings[1]));
  m.word(c+0x50, need(s.asset, "texture asset lookup")(strings[0], strings[2]));
  auto tag = r.get(1);
  std::uint64_t mask;
  if (tag == 255) {
    if (r.get(3) != 0xffffff) throw Unknown("Prop mask encoding outside executed domain");
    mask = r.get(8);
    if (r.get(1) != 0) throw Unknown("Prop mask suffix outside executed domain");
  } else {
    if (tag >= 64) throw Unknown("Prop mask bit outside documented width");
    mask = std::uint64_t(1) << tag;
  }
  m.w64(c+0xe8, mask);
  const auto x = r.get(1), y = r.get(1), z = r.get(1), w = r.get(2);
  if (x || y || z || w != 10000)
    throw Unknown("Prop quaternion outside executed identity domain; arbitrary pose Unknown");
  Vec3 position;
  for (auto &v : position) v = Fpu{}.from_int(std::uint32_t(s16(std::uint32_t(r.get(2)))));
  matrix(m, c, mat34_set_translation(position));
  m.word(c+0xc, m.word(c+0xc) | 0x1000);
  return r.at;
}
}
std::size_t restore_class_fields(builtins::Memory &m, std::uint32_t c,
    const std::string &cls, const Bytes &data, const ClassLoadServices &s) {
  Reader r{data};
  admit(m, c, cls, r, s);
  if (cls == "Trigger") {
    // docs/death-reload.md:75; tools/difftest_death_reload.py:132-137.
    m.word(c+0x64, std::uint32_t(r.get(4)));
    m.byte(c+0x60, std::uint8_t(r.get(1)));
  } else if (cls == "Creature") {
    // docs/death-reload.md:76; tools/difftest_death_reload.py:226-285;
    // docs/save-world.md:120 for the serialized masks and payload layout.
    m.word(c+0xe8, std::uint32_t(r.get(4)));
    need(s.variant, "Creature variant callback")(c);
    const auto ratio = Raw(r.get(4));
    m.word(c+0x228, Fpu{}.mul(ratio, m.word(c+0x22c)));
    if (Fpu::compare(ratio, ZERO) <= 0) {
      const auto handle = std::int32_t(r.get(4));
      m.w16(c+0x232, 20);
      m.word(c+0xc, m.word(c+0xc) | 0x2009);
      need(s.stop, "Creature stop/list effects")(c);
      const auto clip = need(s.dead_clip, "Creature dead animation resolver")(c, handle);
      if (clip) {
        need(s.reset_animation, "Creature animation reset")(c+0x114);
        need(s.start_animation, "Creature animation start")(c+0x114, *clip, 4);
        need(s.end_animation, "Creature animation end")(c+0x114);
        need(s.motion, "Creature virtual motion callback")(c, 1);
        m.w16(c+0x11a, m.u16(c+0x11a) | 0x80);
      }
      if (!need(s.eligible_creature, "Creature type/eligibility effects")(c))
        throw Unknown("Creature reconstruction eligibility branch Unknown");
    }
    const auto bits = std::uint32_t(r.get(4));
    if (bits & ~0x7fu) throw Unknown("Creature packed flags outside documented domain");
    constexpr std::uint64_t masks[] = {0x1000000ull, 0x100000000ull, 0x400000ull,
        0x800000ull, 0x2000000000000ull};
    auto flags = m.u64(c+0xa8);
    for (unsigned i = 0; i < 5; ++i)
      flags = (flags & ~masks[i]) | ((bits & (1u<<i)) ? masks[i] : 0);
    m.w64(c+0xa8, flags);
    m.byte(c+0x231, (m.u8(c+0x231) & ~1u) | ((bits>>5)&1));
    if (bits & 64) {
      if (!need(s.eligible_creature, "Creature optional-word eligibility")(c))
        throw Unknown("Creature optional-word eligibility branch Unknown");
      m.w16(c+0x312, r.get(4));
      need(s.unlink, "Creature optional-word unlink")(c+0x234);
    } // No invented clear of a nonserialized constructor timer.
  } else if (cls == "Chest" || cls == "AutoChest") {
    // docs/death-reload.md:77-78; tools/difftest_death_reload.py:138-160,340-365;
    // docs/save-world.md:119 for AutoChest's distinct field layout.
    const bool auto_chest = cls == "AutoChest";
    m.word(c+(auto_chest ? 0xc8 : 0x54), std::uint32_t(r.get(4)));
    const auto gold = std::uint16_t(r.get(2));
    m.w16(c+(auto_chest ? 0xdc : 0x4c), gold);
    m.byte(c+(auto_chest ? 0xe4 : 0x58), std::uint8_t(r.get(1)));
    const auto variant = std::uint8_t(r.get(1));
    m.byte(c+(auto_chest ? 0xe5 : 0x59), variant);
    const auto opened = std::uint32_t(r.get(4));
    m.word(c+(auto_chest ? 0xd8 : 0x84), opened);
    if (opened) {
      const auto track = c+(auto_chest ? 0x98 : 0x5c);
      const auto clip = auto_chest ? m.word(c+0xf4) : m.word(m.word(c+0x24)+0x1c);
      need(s.start_animation, "Chest clip start")(track, clip, 0x204);
      need(s.end_animation, "Chest clip end")(track);
      if (s16(gold) <= 0) m.word(c+0xc, m.word(c+0xc) & ~0x100000u);
    }
    if (!auto_chest) {
      const auto definition = need(s.class_definition, "Chest variant definition")(variant);
      need(s.set_class, "Chest class/cache setter")(c, definition);
    }
  } else if (cls == "DoorSwing") {
    // docs/death-reload.md:79,85-89; tools/difftest_death_reload.py:178-208;
    // docs/save-world.md:122. Bound to the executed zero-XY/0-or-half-turn domain.
    const auto angle = std::uint16_t(r.get(2));
    m.w16(c+0x50, angle);
    m.byte(c+0x58, std::uint8_t(r.get(2)));
    m.word(c+0x54, std::uint32_t(r.get(4)));
    const auto index = m.s16(c+0x4c);
    auto &world = pieces(s);
    if ((angle != 0 && angle != 32768) || world.data().at(std::size_t(index)).tx ||
        world.data().at(std::size_t(index)).ty)
      throw Unknown("DoorSwing arbitrary-angle/origin reconstruction Unknown");
    world.collision(index, false);
    world.move(index, {}, angle);
    if (angle) {
      m.word(c+0xc, (m.word(c+0xc) & ~0x100000u) | 1);
      need(s.update, "DoorSwing update queue")(c);
    }
  } else if (cls == "DoorSecret") {
    // docs/death-reload.md:80; tools/difftest_death_reload.py:209-215.
    const auto opened = std::uint8_t(r.get(1));
    m.byte(c+0x58, opened);
    if (opened) {
      auto &world = pieces(s);
      const auto index = m.s16(c+0x4c);
      world.collision(index, false);
      world.move(index, {ZERO, ZERO, Fpu{}.from_int(std::uint32_t(-300))}, 0);
      m.word(c+0xc, m.word(c+0xc) & ~0x100000u);
    }
  } else if (cls == "RollDoor") {
    // docs/death-reload.md:81; tools/difftest_death_reload.py:216-222.
    const auto opened = std::uint32_t(r.get(4));
    m.w16(c+0x5a, opened);
    const auto index = std::int32_t(r.get(4));
    m.w16(c+0x4c, index);
    m.word(c+0x50, std::uint32_t(r.get(4)));
    m.word(c+0x54, std::uint32_t(r.get(4)));
    auto &world = pieces(s);
    world.collision(index, true);
    m.w16(c+0x4e, std::uint64_t(world.height(index)));
    if (opened) need(s.update, "RollDoor update queue")(c);
  } else if (cls == "JumpGate") {
    // docs/death-reload.md:82; tools/difftest_death_reload.py:161-174.
    m.byte(c+0x4d, std::uint8_t(r.get(1)));
    const auto active = std::uint8_t(r.get(1));
    m.byte(c+0x4e, active);
    if (active) {
      need(s.light, "JumpGate light effects")(c+0x100);
      const auto clip = need(s.gate_asset, "JumpGate asset lookup")(c+0x90, 0x547ee0);
      need(s.start_animation, "JumpGate animation effects")(c+0xd8, clip, 3);
    }
    need(s.update, "JumpGate update queue")(c);
  } else if (cls == "Prop") {
    prop(m, c, r, s);
  } else if (cls == "ItemProp") {
    // docs/death-reload.md:83; difftest_death_reload.py:288-338.
    if (data.size() < 0x48) throw Error("truncated ItemProp item");
    Bytes item(data.begin(), data.begin()+0x48);
    const auto index = std::uint32_t(r.get(4));
    // ItemProp's definition table uses 0x40 stride, distinct from card item
    // import's table (no use of definition_pointer here).
    if (index > (std::numeric_limits<std::uint32_t>::max()-0x3f7650u)/0x40u)
      throw Unknown("ItemProp definition index outside addressable table");
    const auto definition = 0x3f7650u+index*0x40u;
    for (unsigned i = 0; i < 4; ++i) item[i] = std::uint8_t(definition>>(8*i));
    m.write(c+0x148, item);
    r.at = 0x48;
    m.word(c+0xc, (m.word(c+0xc) | 0x100000u) & ~0x800u);
    prop(m, c, r, s);
    if (m.u64(c+0x150) & (std::uint64_t(1)<<39))
      m.word(c+0xc, m.word(c+0xc) & ~0x3000u);
    if (std::uint16_t(m.u16(c+0x38)+0x4000) != 0)
      throw Unknown("ItemProp arbitrary facing reconstruction Unknown");
    matrix(m, c, mat34_set_translation({m.word(c+0x10), m.word(c+0x14), m.word(c+0x18)}));
  } else throw Unknown("class reload refused: undocumented class reconstruction");
  return r.at;
}
} // namespace champions::simulation::rta::ps2_save
