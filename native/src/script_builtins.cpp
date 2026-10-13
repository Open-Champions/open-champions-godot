#include "champions/script_builtins.hpp"
#include <algorithm>
#include <limits>

namespace champions::simulation::rta::builtins {
namespace {
amx::Cell signed_byte(std::uint8_t value) {
  return value < 128 ? value : static_cast<amx::Cell>(value) - 256;
}
std::string lower(std::string name) {
  for (char &ch : name) {
    auto value = static_cast<unsigned char>(ch);
    if (value >= 128)
      throw amx::HostFault("non-ASCII variable name");
    if (value >= 'A' && value <= 'Z')
      ch = static_cast<char>(value + 32);
  }
  return name;
}
amx::Cell clamp(amx::Cell value) {
  return std::max<amx::Cell>(-128, std::min<amx::Cell>(127, value));
}
bool supported(const std::string &name) {
  return name == "getv" || name == "setv" || name == "addv" ||
         name == "getpv" || name == "setpv";
}
amx::Cell argument(const amx::Machine &machine, amx::Address offset) {
  if (machine.params > std::numeric_limits<amx::Address>::max() - offset)
    throw amx::HostFault("host parameter address arithmetic exceeds int64");
  return machine.cell(machine.params + offset);
}
} // namespace
std::uint8_t Memory::read(std::uint32_t address) const {
  auto it = bytes.find(address);
  return it == bytes.end() ? 0 : it->second;
}
std::string Memory::cstr(std::uint32_t address) const {
  std::string result;
  // The pinned Mem.cstr also refuses at 0x4000 bytes.
  for (unsigned i = 0; i < 0x4000; ++i) {
    auto value = read(address++);
    if (!value)
      return result;
    result.push_back(static_cast<char>(value));
  }
  throw amx::HostFault("unterminated memory string");
}
void Memory::write(std::uint32_t address, const std::vector<std::uint8_t> &data) {
  if (data.empty())
    return;
  writes.emplace_back(address, data.size());
  for (auto value : data) {
    // A missing byte means zero. Erasing zeros keeps snapshots canonical.
    if (value)
      bytes[address] = value;
    else
      bytes.erase(address);
    ++address;
  }
}
void Memory::byte(std::uint32_t address, std::uint8_t value) {
  write(address, {value});
}
void Memory::string(std::uint32_t address, const std::string &text) {
  std::vector<std::uint8_t> data(text.begin(), text.end());
  data.push_back(0);
  write(address, data);
}
std::pair<std::uint32_t, bool> Variables::slot(std::uint32_t address,
                                             const std::string &name) const {
  // Authored traversal guard, not a claimed original table capacity.
  for (unsigned i = 0; i < 1024 * 1024; ++i) {
    if (!memory.read(address))
      return {address, false};
    auto entry = memory.cstr(address);
    if (entry == name)
      return {address, true};
    address += static_cast<std::uint32_t>(entry.size() + 2);
  }
  throw amx::ResourceLimit("variable table traversal limit");
}
amx::Cell Variables::get(std::uint32_t table, const std::string &input) {
  auto name = lower(input);
  auto found = slot(table, name);
  auto value_address = found.first + static_cast<std::uint32_t>(name.size()) + 1;
  if (!found.second) {
    memory.string(found.first, name);
    memory.byte(value_address, 0);
    return 0;
  }
  return signed_byte(memory.read(value_address));
}
amx::Cell Variables::quest_get(const std::string &name) { return get(QUEST_VARS, name); }
void Variables::quest_set(const std::string &input, amx::Cell value) {
  auto name = lower(input);
  auto found = slot(QUEST_VARS, name);
  if (!found.second)
    memory.string(found.first, name);
  memory.byte(found.first + static_cast<std::uint32_t>(name.size()) + 1,
              static_cast<std::uint8_t>(clamp(value)));
}
void Variables::quest_clear_all() {
  auto address = QUEST_VARS;
  for (unsigned i = 0; i < 1024 * 1024; ++i) {
    if (!memory.read(address))
      return;
    auto length = static_cast<std::uint32_t>(memory.cstr(address).size());
    memory.byte(address + length + 1, 0);
    address += length + 2;
  }
  throw amx::ResourceLimit("variable table traversal limit");
}
std::optional<std::pair<std::string, amx::Cell>>
Variables::quest_by_index(amx::Address index) const {
  auto address = QUEST_VARS;
  for (unsigned i = 0; i < 1024 * 1024; ++i) {
    if (!memory.read(address))
      return std::nullopt;
    auto name = memory.cstr(address);
    if (index == 0)
      return std::make_pair(name, signed_byte(memory.read(
          address + static_cast<std::uint32_t>(name.size()) + 1)));
    // Avoid signed overflow for an invalid, negative host-sized index.
    if (index > 0)
      --index;
    address += static_cast<std::uint32_t>(name.size() + 2);
  }
  throw amx::ResourceLimit("variable table traversal limit");
}
amx::Cell Variables::profile_get(const std::string &name) { return get(PROFILE_VARS, name); }
void Variables::profile_set(const std::string &input, amx::Cell value, bool save) {
  value = clamp(value);
  auto name = lower(input);
  auto found = slot(PROFILE_VARS, name);
  auto address = found.first + static_cast<std::uint32_t>(name.size()) + 1;
  if (found.second && signed_byte(memory.read(address)) == value)
    return;
  if (!found.second)
    memory.string(found.first, name);
  memory.byte(address, static_cast<std::uint8_t>(value));
  if (save) {
    // Retain the active callable if a reentrant callback replaces its binding.
    auto callback = request_config_save;
    if (!callback)
      throw amx::HostFault("memcard_requestConfigSave callback required");
    callback();
  }
}
amx::Cell Variables::dispatch(const std::string &name, amx::Machine &machine) {
  if (!supported(name))
    throw amx::HostFault("unsupported RTA script built-in: " + name);
  // All five handlers share stack pointer slot 0x20. The count and callback's
  // argument vector are deliberately ignored: read the live parameter cells.
  auto address = argument(machine, 4);
  if (machine.get_addr(address) == amx::ERR_NONE)
    string_slot = address;
  else if (!string_slot)
    throw StackGarbage("refused string address with unknown handler stack slot");
  auto text = machine.get_string(*string_slot);
  if (text.size() >= 160)
    throw amx::HostFault("string overruns 160-byte handler frame");
  if (name == "getv")
    return quest_get(text);
  if (name == "getpv")
    return profile_get(text);
  if (name == "addv") {
    auto previous = quest_get(text);
    auto value = argument(machine, 8);
    auto total = amx::s32(static_cast<std::uint32_t>(previous) +
                         std::uint64_t(static_cast<std::uint32_t>(value)));
    quest_set(text, total);
    return total;
  }
  auto value = argument(machine, 8);
  if (name == "setv") {
    quest_set(text, value);
    return 0;
  }
  if (name == "setpv") {
    profile_set(text, value, true);
    return 0;
  }
  throw amx::HostFault("unsupported RTA script built-in");
}
amx::NativeMap Variables::natives() {
  amx::NativeMap result;
  for (const std::string name : {"getv", "setv", "addv", "getpv", "setpv"})
    result.emplace(name, [this, name](amx::Machine &m, const std::vector<amx::Cell> &)
                   -> std::optional<amx::Address> { return dispatch(name, m); });
  return result;
}
} // namespace champions::simulation::rta::builtins
