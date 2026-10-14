// MIT; Copyright (c) 2026 TrustlessMike and contributors.
#pragma once
#include <cstdint>

namespace champions::simulation::rta::detail::skill_handle_kernel {
// Adapters retain address validation, history policy and all list read schedules.
// Each dependent read/offset/store remains at its original mutation boundary.
template<class Address, class Read, class Write, class Offset>
auto take(Address free_head, Read read, Write write, Offset offset) {
  const auto node = read(free_head);
  if (node) {
    write(free_head, 4, read(node));
    write(offset(node, 0x10), 2, 0);
  }
  return node;
}
template<class Address, class Node, class Read, class Write>
void recycle(Address free_head, Node node, Read read, Write write) {
  write(node, 4, read(free_head));
  write(free_head, 4, static_cast<std::uint32_t>(node));
}
template<class Address, class Object, class Take, class Insert, class Write, class Offset>
auto paired_link(Address head, Address back, Object object, Take take,
                 Insert insert, Write write, Offset offset) {
  const auto node = take();
  const auto partner = take();
  insert(head, node);
  insert(back, partner);
  write(offset(node, 0xc), 4, object);
  write(offset(node, 8), 4, partner);
  write(offset(partner, 0xc), 4, 0);
  write(offset(partner, 8), 4, node);
  return node;
}
}
