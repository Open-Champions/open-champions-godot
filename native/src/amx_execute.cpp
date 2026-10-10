#include "champions/amx.hpp"
#include <algorithm>
#include <limits>

// Original C++ implementation of the published ref_amx.py interpreter contract,
// revision5ad489a. No game image, native implementation or decompiler output.
namespace champions::simulation::rta::amx {
namespace {
Cell wrap_add(Address a, Address b) {
  return s32(std::uint32_t(a) + std::uint32_t(b));
}
Cell wrap_sub(Address a, Address b) {
  return s32(std::uint32_t(a) - std::uint32_t(b));
}
Address checked_add(Address a, Address b) {
  if ((b > 0 && a > std::numeric_limits<Address>::max() - b) ||
      (b < 0 && a < std::numeric_limits<Address>::min() - b))
    throw ResourceLimit("AMX address exceeds int64 representation");
  return a + b;
}
Address arithmetic_shift(Address value, unsigned shift) {
  if (!shift)
    return value;
  const auto bits = std::uint64_t(value) >> shift;
  if (value >= 0)
    return static_cast<Address>(bits);
  const auto filled = bits | (~std::uint64_t(0) << (64 - shift));
  return -1 - static_cast<Address>(~filled);
}
Address remainder(Address a, Address b) {
  if (!b)
    throw ZeroDivisionError("zero divisor");
  if (a == std::numeric_limits<Address>::min() && b == -1)
    return 0;
  return a % b;
}
std::pair<Cell, Cell> floor_wide(Address a, Address b) {
  const auto first = wrap_add(remainder(a, b), b);
  const auto rest = remainder(first, b);
  const auto numerator = wrap_sub(a, rest);
  if (numerator == std::numeric_limits<Cell>::min() && b == -1)
    return {numerator, static_cast<Cell>(rest)};
  return {static_cast<Cell>(numerator / b), static_cast<Cell>(rest)};
}
} // namespace
std::pair<Cell, Cell> trunc_div(Cell a, Cell b) {
  if (!b)
    throw ZeroDivisionError("zero divisor");
  if (a == std::numeric_limits<Cell>::min() && b == -1)
    return {a, 0};
  return {static_cast<Cell>(a / b), static_cast<Cell>(a % b)};
}
std::pair<Cell, Cell> floored_div(Cell a, Cell b) { return floor_wide(a, b); }
std::size_t Machine::block(Address address, std::uint64_t count) const {
  if (address < 0)
    throw HostFault("negative block address");
  const auto start = checked_add(data_offset, address);
  if (start < 0 || std::uint64_t(start) > image.size() ||
      count > image.size() - std::uint64_t(start))
    throw HostFault("block outside image");
  return static_cast<std::size_t>(start);
}
Address Machine::call(const std::string &name, Address initial) {
  const auto found = find_public(name);
  if (found.first)
    return -1;
  const auto result = execute(found.second, {}, initial);
  return result.second.value_or(0);
}
ExecuteResult Machine::execute(Address index,
                               const std::vector<Cell> &arguments,
                               Address initial) {
  if (!callback)
    return {ERR_CALLBACK, std::nullopt};
  flags &= ~FLAG_BROWSE;
  if (!(flags & FLAG_RELOC))
    return {ERR_INIT, std::nullopt};
  if (!parsed || !decoded)
    throw HostFault("image has no prepared code");
  Address stack = stk, reset_stack = stk, heap = hea;
  if (!hdr_stp)
    throw HostFault("missing parsed stack top");
  if (*hdr_stp < stack)
    return {ERR_STACKLOW, std::nullopt};
  if (heap < 0)
    return {ERR_HEAPLOW, std::nullopt};
  Address pc;
  if (index == EXEC_MAIN) {
    pc = start;
    if (pc < 0)
      return {ERR_INDEX, std::nullopt};
  } else {
    if ((index < 0 && index != -2) || index >= num_publics)
      return {ERR_INDEX, std::nullopt};
    // The historical -2 entry hole is deliberately preserved.
    if (index > std::numeric_limits<Address>::max() / RECORD_SIZE)
      throw ResourceLimit("public table offset exceeds int64 representation");
    pc = header_cell(checked_add(publics_off, index * RECORD_SIZE));
  }
  stack = wrap_sub(stack, std::uint64_t(arguments.size()) * 4);
  for (std::size_t i = 0; i < arguments.size(); ++i)
    set_cell(checked_add(stack, static_cast<Address>(i) * 4),
             std::uint32_t(arguments[i]));
  stack = wrap_sub(stack, 4);
  set_cell(stack, std::uint64_t(arguments.size()) * 4);
  stack = wrap_sub(stack, 4);
  set_cell(stack, 0);
  if (stack < wrap_add(heap, STKMARGIN))
    return {ERR_STACKERR, std::nullopt};
  Address pri = s32(initial), alt = 0, frame = 0;
  const auto code = decoded;
  const auto counts = profile;
  const Address top = stp;
  const auto abort = [&](Address error_code) -> ExecuteResult {
    stk = reset_stack;
    return {error_code, std::nullopt};
  };
  const auto invalid_address = [&](Address a) {
    return (heap <= a && a < stack) || a >= top;
  };
  for (;;) {
    const auto found = code->find(pc);
    if (found == code->end())
      throw HostFault("execution reached unprepared code");
    if (budget) {
      if (*budget <= 0)
        throw Limit("instruction budget exhausted");
      --*budget;
    }
    // Copy this instruction: callbacks may mutate or replace the decoded map.
    const auto instruction = found->second;
    const auto op = instruction.opcode;
    pc = instruction.next;
    if (counts) {
      auto &count = (*counts)[op];
      if (count == std::numeric_limits<std::uint64_t>::max())
        throw ResourceLimit("profile counter overflow");
      ++count;
    }
    Cell value = 0;
    if (const auto *v = std::get_if<Cell>(&instruction.operand))
      value = *v;
    switch (op) {
    case 0x01:
      pri = cell(value);
      break;
    case 0x02:
      alt = cell(value);
      break;
    case 0x03:
      pri = cell(wrap_add(frame, value));
      break;
    case 0x04:
      alt = cell(wrap_add(frame, value));
      break;
    case 0x05:
      pri = cell(cell(value));
      break;
    case 0x06:
      alt = cell(cell(value));
      break;
    case 0x07:
      pri = cell(cell(wrap_add(frame, value)));
      break;
    case 0x08:
      alt = cell(cell(wrap_add(frame, value)));
      break;
    case 0x09:
      if (invalid_address(pri))
        return abort(ERR_MEMACCESS);
      pri = cell(pri);
      break;
    case 0x0a:
      if (invalid_address(pri))
        return abort(ERR_MEMACCESS);
      if (value == 1 || value == 2) {
        auto pos = position(pri, value);
        pri = image[pos];
        if (value == 2)
          pri |= Address(image[pos + 1]) << 8;
      } else if (value == 4)
        pri = cell(pri);
      break;
    case 0x0b:
      pri = value;
      break;
    case 0x0c:
      alt = value;
      break;
    case 0x0d:
      pri = wrap_add(frame, value);
      break;
    case 0x0e:
      alt = wrap_add(frame, value);
      break;
    case 0x0f:
      set_cell(value, pri);
      break;
    case 0x10:
      set_cell(value, alt);
      break;
    case 0x11:
      set_cell(wrap_add(frame, value), pri);
      break;
    case 0x12:
      set_cell(wrap_add(frame, value), alt);
      break;
    case 0x13:
      set_cell(cell(value), pri);
      break;
    case 0x14:
      set_cell(cell(value), alt);
      break;
    case 0x15:
      set_cell(cell(wrap_add(frame, value)), pri);
      break;
    case 0x16:
      set_cell(cell(wrap_add(frame, value)), alt);
      break;
    case 0x17:
      if (invalid_address(alt))
        return abort(ERR_MEMACCESS);
      set_cell(alt, pri);
      break;
    case 0x18:
      if (invalid_address(alt))
        return abort(ERR_MEMACCESS);
      if (value == 1 || value == 2 || value == 4) {
        auto pos = position(alt, value);
        for (Cell i = 0; i < value; ++i)
          image[pos + i] = std::uint32_t(pri) >> (i * 8);
      }
      break;
    case 0x19:
    case 0x1a: {
      const auto address = wrap_add(
          std::uint32_t(pri) << (op == 0x19 ? 2 : std::uint32_t(value) & 31),
          alt);
      if (invalid_address(address))
        return abort(ERR_MEMACCESS);
      pri = cell(address);
      break;
    }
    case 0x1b:
      pri = wrap_add(std::uint32_t(pri) << 2, alt);
      break;
    case 0x1c:
      pri = wrap_add(std::uint32_t(pri) << (std::uint32_t(value) & 31), alt);
      break;
    case 0x1d:
      if (value >= 0 && value < 4)
        pri ^= 4 - value;
      break;
    case 0x1e:
      if (value >= 0 && value < 4)
        alt ^= 4 - value;
      break;
    case 0x1f:
      switch (value) {
      case 0:
        pri = cod;
        break;
      case 1:
        pri = dat;
        break;
      case 2:
        pri = heap;
        break;
      case 3:
        pri = top;
        break;
      case 4:
        pri = stack;
        break;
      case 5:
        pri = frame;
        break;
      case 6:
        pri = pc;
        break;
      default:
        break;
      }
      break;
    case 0x20:
      switch (value) {
      case 2:
        heap = pri;
        break;
      case 4:
        stack = pri;
        break;
      case 5:
        frame = pri;
        break;
      case 6:
        pc = pri;
        break;
      default:
        break;
      }
      break;
    case 0x21:
      pri = alt;
      break;
    case 0x22:
      alt = pri;
      break;
    case 0x23:
      std::swap(pri, alt);
      break;
    case 0x24:
      stack = wrap_sub(stack, 4);
      set_cell(stack, pri);
      break;
    case 0x25:
      stack = wrap_sub(stack, 4);
      set_cell(stack, alt);
      break;
    case 0x26:
      for (std::uint64_t i = 0; i < std::uint32_t(value); ++i) {
        stack = wrap_sub(stack, 4);
        set_cell(stack, pri);
      }
      break;
    case 0x27:
      stack = wrap_sub(stack, 4);
      set_cell(stack, value);
      break;
    case 0x28:
    case 0x29: {
      auto v = cell(op == 0x28 ? value : wrap_add(frame, value));
      stack = wrap_sub(stack, 4);
      set_cell(stack, v);
      break;
    }
    case 0x2a:
      pri = cell(stack);
      stack = wrap_add(stack, 4);
      break;
    case 0x2b:
      alt = cell(stack);
      stack = wrap_add(stack, 4);
      break;
    case 0x2c:
      alt = stack;
      stack = wrap_add(stack, value);
      if (stack < wrap_add(heap, STKMARGIN))
        return {ERR_STACKERR, std::nullopt};
      if (!hdr_stp)
        throw HostFault("missing stack top");
      if (*hdr_stp < stack)
        return {ERR_STACKLOW, std::nullopt};
      break;
    case 0x2d:
      alt = heap;
      heap = wrap_add(heap, value);
      if (stack < wrap_add(heap, STKMARGIN))
        return {ERR_STACKERR, std::nullopt};
      if (heap < 0)
        return {ERR_HEAPLOW, std::nullopt};
      break;
    case 0x2e:
      stack = wrap_sub(stack, 4);
      set_cell(stack, frame);
      frame = stack;
      break;
    case 0x2f: {
      frame = cell(stack);
      const auto target = cell(wrap_add(stack, 4));
      stack = wrap_add(stack, 8);
      if (!target) {
        stk = reset_stack;
        return {ERR_NONE, pri};
      }
      pc = target;
      break;
    }
    case 0x30: {
      frame = cell(stack);
      const auto target = cell(wrap_add(stack, 4));
      stack = wrap_add(stack, 8);
      stack = wrap_add(wrap_add(stack, cell(stack)), 4);
      stk = stack;
      if (!target)
        return {ERR_NONE, pri};
      pc = target;
      break;
    }
    case 0x31:
      stack = wrap_sub(stack, 4);
      set_cell(stack, pc);
      pc = value;
      break;
    case 0x32:
      stack = wrap_sub(stack, 4);
      set_cell(stack, pc);
      pc = pri;
      break;
    case 0x33:
      pc = value;
      break;
    case 0x34:
      pc = wrap_add(pc, value);
      break;
    case 0x35:
      if (pri == 0)
        pc = value;
      break;
    case 0x36:
      if (pri != 0)
        pc = value;
      break;
    case 0x37:
      if (pri == alt)
        pc = value;
      break;
    case 0x38:
      if (pri != alt)
        pc = value;
      break;
    case 0x39:
      if (std::uint32_t(pri) < std::uint32_t(alt))
        pc = value;
      break;
    case 0x3a:
      if (std::uint32_t(pri) <= std::uint32_t(alt))
        pc = value;
      break;
    case 0x3b:
      if (std::uint32_t(pri) > std::uint32_t(alt))
        pc = value;
      break;
    case 0x3c:
      if (std::uint32_t(pri) >= std::uint32_t(alt))
        pc = value;
      break;
    case 0x3d:
      if (pri < alt)
        pc = value;
      break;
    case 0x3e:
      if (pri <= alt)
        pc = value;
      break;
    case 0x3f:
      if (pri > alt)
        pc = value;
      break;
    case 0x40:
      if (pri >= alt)
        pc = value;
      break;
    case 0x41:
      pri = s32(std::uint32_t(pri) << (std::uint32_t(alt) & 31));
      break;
    case 0x42:
      pri = s32(std::uint32_t(pri) >> (std::uint32_t(alt) & 31));
      break;
    case 0x43:
      pri = arithmetic_shift(pri, std::uint32_t(alt) & 31);
      break;
    case 0x44:
      pri = s32(std::uint32_t(pri) << (std::uint32_t(value) & 31));
      break;
    case 0x45:
      alt = s32(std::uint32_t(alt) << (std::uint32_t(value) & 31));
      break;
    case 0x46:
      pri = s32(std::uint32_t(pri) >> (std::uint32_t(value) & 31));
      break;
    case 0x47:
      alt = s32(std::uint32_t(alt) >> (std::uint32_t(value) & 31));
      break;
    case 0x48:
    case 0x4b:
      pri = s32(std::uint32_t(pri) * std::uint32_t(alt));
      break;
    case 0x49:
    case 0x4a: {
      const auto divisor = op == 0x49 ? alt : pri,
                 dividend = op == 0x49 ? pri : alt;
      if (!divisor)
        return abort(ERR_DIVIDE);
      auto qr = floor_wide(dividend, divisor);
      pri = qr.first;
      alt = qr.second;
      break;
    }
    case 0x4c:
    case 0x4d: {
      const auto divisor = op == 0x4c ? alt : pri,
                 dividend = op == 0x4c ? pri : alt;
      if (!divisor)
        return abort(ERR_DIVIDE);
      if (!std::uint32_t(divisor))
        throw ZeroDivisionError("unsigned divide has zero low word");
      pri = s32(std::uint32_t(dividend) / std::uint32_t(divisor));
      alt = s32(std::uint32_t(dividend) % std::uint32_t(divisor));
      break;
    }
    case 0x4e:
      pri = wrap_add(pri, alt);
      break;
    case 0x4f:
      pri = wrap_sub(pri, alt);
      break;
    case 0x50:
      pri = wrap_sub(alt, pri);
      break;
    case 0x51:
      pri &= alt;
      break;
    case 0x52:
      pri |= alt;
      break;
    case 0x53:
      pri ^= alt;
      break;
    case 0x54:
      pri = pri == 0;
      break;
    case 0x55:
      pri = wrap_sub(0, pri);
      break;
    case 0x56:
      pri = ~pri;
      break;
    case 0x57:
      pri = wrap_add(pri, value);
      break;
    case 0x58:
      pri = s32(std::uint32_t(pri) * std::uint32_t(value));
      break;
    case 0x59:
      pri = 0;
      break;
    case 0x5a:
      alt = 0;
      break;
    case 0x5b:
      set_cell(value, 0);
      break;
    case 0x5c:
      set_cell(wrap_add(frame, value), 0);
      break;
    case 0x5d:
      if (pri & 128)
        pri = s32(std::uint64_t(pri) | 0xffffff00u);
      break;
    case 0x5e:
      if (alt & 128)
        alt = s32(std::uint64_t(alt) | 0xffffff00u);
      break;
    case 0x5f:
      pri = pri == alt;
      break;
    case 0x60:
      pri = pri != alt;
      break;
    case 0x61:
      pri = std::uint32_t(pri) < std::uint32_t(alt);
      break;
    case 0x62:
      pri = std::uint32_t(pri) <= std::uint32_t(alt);
      break;
    case 0x63:
      pri = std::uint32_t(pri) > std::uint32_t(alt);
      break;
    case 0x64:
      pri = std::uint32_t(pri) >= std::uint32_t(alt);
      break;
    case 0x65:
      pri = pri < alt;
      break;
    case 0x66:
      pri = pri <= alt;
      break;
    case 0x67:
      pri = pri > alt;
      break;
    case 0x68:
      pri = pri >= alt;
      break;
    case 0x69:
      pri = pri == value;
      break;
    case 0x6a:
      pri = alt == value;
      break;
    case 0x6b:
      pri = wrap_add(pri, 1);
      break;
    case 0x6c:
      alt = wrap_add(alt, 1);
      break;
    case 0x6d:
    case 0x6e:
    case 0x6f: {
      const auto address = op == 0x6d   ? value
                           : op == 0x6e ? wrap_add(frame, value)
                                        : pri;
      const auto old = cell(address);
      set_cell(address, std::uint32_t(old) + 1u);
      break;
    }
    case 0x70:
      pri = wrap_sub(pri, 1);
      break;
    case 0x71:
      alt = wrap_sub(alt, 1);
      break;
    case 0x72:
    case 0x73:
    case 0x74: {
      const auto address = op == 0x72   ? value
                           : op == 0x73 ? wrap_add(frame, value)
                                        : pri;
      const auto old = cell(address);
      set_cell(address, std::uint32_t(old) - 1u);
      break;
    }
    case 0x75: {
      const auto count = std::uint32_t(value);
      const auto source = block(pri, count), target = block(alt, count);
      for (std::uint64_t i = 0; i < count; ++i)
        image[target + i] = image[source + i];
      break;
    }
    case 0x76: {
      const auto count = std::uint32_t(value);
      const auto first = block(alt, 0), second = block(pri, 0);
      const auto room = image.size() - std::max(first, second);
      Address difference = 0;
      for (std::uint64_t i = 0; i < std::min<std::uint64_t>(count, room); ++i) {
        difference = Address(image[first + i]) - Address(image[second + i]);
        if (difference)
          break;
      }
      if (!difference && count > room)
        throw HostFault("CMPS extends beyond image");
      pri = difference;
      break;
    }
    case 0x77: {
      auto count = std::uint32_t(value);
      auto address = alt;
      while (count >= 4) {
        set_cell(address, pri);
        address = wrap_add(address, 4);
        count -= 4;
      }
      break;
    }
    case 0x78:
      stk = reset_stack;
      return {value, pri};
    case 0x79:
      if (pri > value || pri < 0)
        return abort(ERR_BOUNDS);
      break;
    case 0x7a:
    case 0x7b: {
      if (!callback)
        throw HostFault("native call without callback");
      cip = pc;
      frm = frame;
      hea = heap;
      stk = stack;
      // Copy callable before invoking: it may replace Machine.callback itself.
      const auto invoke = callback;
      auto result = invoke(op == 0x7a ? pri : value, stack);
      pri = s32(result.second);
      if (result.first)
        return abort(result.first);
      break;
    }
    case 0x7c:
      break;
    case 0x7d: {
      auto pair = std::get<std::pair<Cell, Cell>>(instruction.operand);
      curline = pair.first;
      curfile = pair.second;
      break;
    }
    case 0x7e: {
      auto symbol = std::get<DebugSymbol>(instruction.operand);
      dbgaddr = symbol.address;
      dbgparam = symbol.flags;
      dbgname = symbol.name;
      break;
    }
    case 0x7f: {
      auto pair = std::get<std::pair<Cell, Cell>>(instruction.operand);
      dbgaddr = pair.first;
      dbgparam = pair.second;
      break;
    }
    case 0x80:
      pc = pri;
      break;
    case 0x81: {
      auto table = code->find(value);
      if (table == code->end() || table->second.opcode != 0x82)
        throw HostFault("SWITCH target is not a prepared case table");
      const auto *cases = std::get_if<CaseTable>(&table->second.operand);
      if (!cases || cases->count < 0 || !cases->default_target)
        throw HostFault("invalid SWITCH case table");
      pc = *cases->default_target;
      for (const auto &entry : cases->cases)
        if (entry.first == pri) {
          pc = entry.second;
          break;
        }
      break;
    }
    case 0x82:
      throw HostFault("CASETBL executed as an instruction");
    default:
      return abort(ERR_INVINSTR);
    }
  }
}
} // namespace champions::simulation::rta::amx
