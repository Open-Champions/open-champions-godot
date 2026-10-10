#include "champions/amx.hpp"
#include <algorithm>
#include <limits>
#include <utility>
namespace champions::simulation::rta::amx {
namespace {
Address plus(Address a, Address b) {
  if ((b > 0 && a > std::numeric_limits<Address>::max() - b) ||
      (b < 0 && a < std::numeric_limits<Address>::min() - b))
    throw HostFault("host address arithmetic exceeds int64");
  return a + b;
}
thread_local unsigned native_depth = 0;
struct NativeDepth {
  NativeDepth() {
    if (native_depth == 128)
      throw ResourceLimit("native nesting limit");
    ++native_depth;
  }
  ~NativeDepth() { --native_depth; }
};
} // namespace
std::vector<Binding> &Machine::natives() {
  if (!bound) {
    if (!header_read)
      throw HostFault("native table unavailable before header parse");
    std::vector<Binding> result;
    for (Address i = 0; i < num_natives; ++i) {
      if (i > Address(MAX_IMAGE_BYTES / RECORD_SIZE))
        throw ResourceLimit("native record count");
      auto v = header_cell(plus(natives_off, i * RECORD_SIZE));
      result.push_back(
          {v == 0 ? BindingKind::Unbound : BindingKind::Foreign, {}});
    }
    bound = std::move(result);
  }
  return *bound;
}
Cell Machine::register_natives(const NativeMap &values) {
  auto &bindings = natives();
  Cell result = ERR_NONE;
  for (std::size_t i = 0; i < bindings.size(); ++i) {
    if (bindings[i].kind != BindingKind::Unbound)
      continue;
    auto name = native_name(static_cast<Address>(i));
    auto found = values.find(name);
    if (found == values.end() || !found->second)
      result = ERR_NOTFOUND;
    else
      bindings[i] = {BindingKind::Bound, found->second};
  }
  return result;
}
std::pair<Address, Address> Machine::foreign_callback(Address, Address) {
  throw HostFault("image callback is a foreign machine address");
}
std::pair<Address, Address> Machine::call_native(Address index,
                                                 Address address) {
  NativeDepth depth;
  auto &bindings = natives();
  if (index < 0 || static_cast<std::uint64_t>(index) >= bindings.size())
    throw HostFault("native index outside table");
  // Keep a callable alive across nested calls that replace the binding vector.
  auto binding = bindings[static_cast<std::size_t>(index)];
  if (binding.kind != BindingKind::Bound || !binding.function)
    throw HostFault("unbound or foreign native");
  error = 0;
  params = address;
  auto count = cell(address);
  std::vector<Cell> args;
  auto cells = count > 0 ? count / 4 : 0;
  args.reserve(
      std::min<std::size_t>(static_cast<std::size_t>(cells), image.size() / 4));
  for (Cell k = 0; k < cells; ++k)
    args.push_back(cell(plus(address, 4 + Address(k) * 4)));
  auto result = binding.function(*this, args);
  return {error, s32(static_cast<std::uint64_t>(result.value_or(0)))};
}
std::size_t Machine::position(Address address, Address size_) const {
  if (!header_read)
    throw HostFault("script data address unavailable before header parse");
  if (address < 0 || size_ < 0 ||
      (std::uint64_t(address) & (std::uint64_t(size_) - 1)))
    throw HostFault("unaligned or negative script access");
  auto p = plus(data_offset, address);
  if (p < 0 || std::uint64_t(p) > image.size() ||
      std::uint64_t(size_) > image.size() - std::uint64_t(p))
    throw HostFault("script access outside image");
  return static_cast<std::size_t>(p);
}
Cell Machine::cell(Address address) const {
  auto p = position(address, 4);
  std::uint32_t v = 0;
  for (unsigned i = 0; i < 4; ++i)
    v |= std::uint32_t(image[p + i]) << (8 * i);
  return s32(v);
}
void Machine::set_cell(Address address, std::uint64_t value) {
  auto p = position(address, 4);
  for (unsigned i = 0; i < 4; ++i)
    image[p + i] = std::uint8_t(value >> (8 * i));
}
ByteVector Machine::memory() const {
  if (!header_read)
    throw HostFault("script data address unavailable before header parse");
  Address start = data_offset;
  if (start < 0)
    start = std::max<Address>(0, plus(Address(image.size()), start));
  auto p = std::min<std::uint64_t>(std::uint64_t(start), image.size());
  return ByteVector(image.begin() + static_cast<std::ptrdiff_t>(p),
                    image.end());
}
Cell Machine::get_addr(Address address) const {
  return (hea <= address && address < stk) || address >= stp ? ERR_MEMACCESS
                                                             : ERR_NONE;
}
std::string Machine::get_string(Address address) const {
  std::string out;
  auto first = cell(address);
  if (first < 0x100) {
    if (first == 0)
      return out;
    for (;;) {
      out.push_back(static_cast<char>(std::uint32_t(cell(address)) & 255));
      address = plus(address, 4);
      if (cell(address) == 0)
        return out;
    }
  }
  for (;;) {
    auto v = std::uint32_t(cell(address));
    address = plus(address, 4);
    for (int shift : {24, 16, 8, 0}) {
      auto ch = std::uint8_t(v >> shift);
      if (!ch)
        return out;
      out.push_back(static_cast<char>(ch));
    }
  }
}
Cell Machine::set_string(Address address, const std::string &text, bool pack) {
  auto zero = text.find('\0');
  std::size_t length = zero == std::string::npos ? text.size() : zero;
  if (length > MAX_IMAGE_BYTES)
    throw ResourceLimit("host string exceeds image resource limit");
  if (pack) {
    set_cell(plus(address, Address(length / 4) * 4), 0);
    for (std::size_t i = 0; i < length; ++i)
      image[position(plus(address, Address(i)), 1)] =
          static_cast<std::uint8_t>(text[i]);
    for (std::size_t i = length / 4 + 1; i > 0;) {
      --i;
      auto p = position(plus(address, Address(i) * 4), 4);
      std::reverse(image.begin() + static_cast<std::ptrdiff_t>(p),
                   image.begin() + static_cast<std::ptrdiff_t>(p + 4));
    }
  } else {
    for (std::size_t i = 0; i < length; ++i) {
      auto b = static_cast<unsigned char>(text[i]);
      set_cell(plus(address, Address(i) * 4),
               b & 128 ? std::uint32_t(b) - 256u : b);
    }
    set_cell(plus(address, Address(length) * 4), 0);
  }
  return ERR_NONE;
}
Address Machine::str_len(Address address) const {
  auto first = cell(address);
  Address length = 0;
  if (first < 0x100) {
    if (first)
      do {
        ++length;
      } while (cell(plus(address, length * 4)) != 0);
    return length;
  }
  while (image[position(plus(address, length), 1)] != 0)
    ++length;
  auto v = std::uint32_t(cell(plus(address, length & ~Address(3))));
  length &= ~Address(3);
  while (v & 0xff000000u) {
    ++length;
    v <<= 8;
  }
  return length;
}
std::pair<Cell, std::optional<Address>> Machine::allot(Address cells) {
  auto size_ = s32(std::uint32_t(cells) << 2);
  auto room = std::uint32_t(stk) - std::uint32_t(hea) - std::uint32_t(size_);
  if (room < STKMARGIN)
    return {ERR_MEMORY, std::nullopt};
  auto address = hea;
  hea = s32(std::uint32_t(hea) + std::uint32_t(size_));
  return {ERR_NONE, address};
}
Cell Machine::release(Address address) {
  if (address < hea)
    hea = address;
  return ERR_NONE;
}
Cell Machine::raise_error(Address code) {
  error = code;
  return ERR_NONE;
}
Snapshot Machine::snapshot() const {
  return {image,
          {cip, frm, hea, stk, stp, flags, curline, curfile, dbgcode, dbgaddr,
           dbgparam, dbgname, error, hdr_stp}};
}
void Machine::restore(const Snapshot &saved) {
  if (saved.image.size() > MAX_IMAGE_BYTES)
    throw ResourceLimit("snapshot image exceeds resource limit");
  image = saved.image;
  for (std::size_t i = 0; i < std::min<std::size_t>(14, saved.state.size());
       ++i) {
    auto v = saved.state[i];
    if (i == 11) {
      dbgname = v;
      continue;
    }
    if (i == 13) {
      hdr_stp = v;
      continue;
    }
    if (!v)
      throw HostFault("None is unsupported for an integer snapshot field");
    switch (i) {
    case 0:
      cip = *v;
      break;
    case 1:
      frm = *v;
      break;
    case 2:
      hea = *v;
      break;
    case 3:
      stk = *v;
      break;
    case 4:
      stp = *v;
      break;
    case 5:
      if (*v < 0 || std::uint64_t(*v) > M32)
        throw HostFault("snapshot flags outside uint32");
      flags = std::uint32_t(*v);
      break;
    case 6:
      curline = *v;
      break;
    case 7:
      curfile = *v;
      break;
    case 8:
      dbgcode = *v;
      break;
    case 9:
      dbgaddr = *v;
      break;
    case 10:
      dbgparam = *v;
      break;
    case 12:
      error = *v;
      break;
    default:
      break;
    }
  }
}
} // namespace champions::simulation::rta::amx
