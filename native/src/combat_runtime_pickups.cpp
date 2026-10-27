// RTA US v2.00, research-loot-drop 77c2f70 sections 1-3 and 6. These
// entry points use the existing PhProp solver, pool, grid and deletion lists.
#include "combat_runtime_internal.hpp"
namespace champions::simulation::rta::combat_runtime {
void World::load_runtime_bytes(Address a, const std::vector<std::uint8_t> &bytes) {
  s_->mem.write_host_bytes(a, bytes);
}
Address World::spawn_item(const Vec3 &position, Address lump, Address model,
                          Address texture, const std::array<std::uint8_t, 0x48> &record) {
  auto &s = *s_;
  if (!s.full()) throw Unsupported("loot props require Full composition");
  const auto block = s.pool_new(0x248);
  if (!block) return 0;
  s.install_ctor_services();
  // Host scratch identity, outside the pool and original executable image.
  constexpr Address scratch = 0x01f30000;
  s.mem.write_host_bytes(scratch, {record.begin(), record.end()});
  s.service("PhProp_ctor", {iv(ONE), iv(block), position, iv(lump), iv(model),
                            iv(texture), iv(1), iv(0x441), iv(scratch)});
  // Settling reaches this table through the already ported ctor3 body.
  s.w(0x511c38 + 12, 0x3c2ed8);
  return block;
}
void World::throw_item(Address object, Raw angle, Raw speed) {
  auto &s = *s_;
  s.actor(object);
  s.w(object + 0xfc, mul(mul(cos16(angle), 0x41c00000), speed));
  s.w(object + 0x100, mul(mul(sin16(angle), 0x41c00000), speed));
  combat_ctors::phprop_init_physics(s.model, object, 0x441);
}
Address World::spawn_resting_item(const Vec3 &position, Address lump, Address model,
                                  Address texture, const std::array<std::uint8_t, 0x48> &item,
                                  const Mat34 &matrix) {
  auto &s = *s_;
  const auto block = s.pool_new(0x190);
  if (!block) return 0;
  s.construct(block, position, State::Kind::Effect);
  const auto copy = [&](Address dst, Address src) {
    for (unsigned i = 0; i < 256; ++i) {
      const auto c = s.b(src + i);
      s.w(dst + i, 1, c);
      if (!c) return;
    }
    throw Error("ItemProp resource name exceeds 255 bytes");
  };
  copy(block + 0x54, lump); copy(block + 0x64, model); copy(block + 0xa4, texture);
  s.w(block + 0x4c, s.resource_text(s.cstr(lump, 256), s.cstr(model, 256)));
  s.w(block + 0x50, s.resource_text(s.cstr(lump, 256), s.cstr(texture, 256)));
  s.mem.write_host_bytes(block + 0x148, {item.begin(), item.end()});
  s.w(block + 0x48, 0x511c38); s.w(0x511c38 + 12, 0x3c2ed8);
  s.w(block + 0xe8, 8, UINT64_MAX);
  s.w(block + 0x140, 2, 1); s.w(block + 0x1c, 0x40800000);
  for (unsigned k = 0; k < 12; ++k) s.w(block + 0x110 + 4 * k, matrix[k]);
  auto flags = Raw(0x1809 | 0x102000) & ~0x1800u;
  if (s.q(block + 0x150) & (UINT64_C(1) << 39)) flags &= ~0x3000u;
  s.w(block + 12, flags); s.grid(block);
  return block;
}
Address World::spawn_gold(const Vec3 &position, Raw amount) {
  if (s32(amount) <= 0) return 0;
  auto &s = *s_;
  const auto block = s.pool_new(0xc0);
  if (!block) return 0;
  s.install_ctor_services();
  s.service("Gold_ctor2", {iv(block), position, iv(amount)});
  return block;
}
void World::add_gold_floater(Raw slot, const Vec3 &position, Raw amount) {
  if (slot >= 4) throw Error("Gold recipient outside 0..3");
  combat_ctors::hud_add_gold_floater(s_->model, slot, position, amount);
}
Address World::broken_container_gold(Address c, Raw factor) {
  auto &s = *s_;
  s.actor(c);
  if (s.u(c + 0x13c) != 1 || !(s.u(c + 0x124) & 1) || (s.u(c + 0x124) & 4))
    throw Unsupported("AutoContainer Gold outside the traced broken nonexplosive branch");
  if (s.own(c) >= 24000) return 0;
  const auto block = s.pool_new(0xc0);
  if (!block) return 0; // no amount draw on allocation failure
  const auto amount = Raw(s32(to_int(mul(from_int(s.own(c) & 3), factor))) + 1);
  s.install_ctor_services();
  s.service("Gold_ctor2", {iv(block), s.vec(c + 0x10), iv(amount)});
  return block;
}
} // namespace champions::simulation::rta::combat_runtime
