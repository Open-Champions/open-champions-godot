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
const std::vector<std::string> campaign = {
 "getEvil", "setEvil", "getMadeDecision", "setMadeDecision", "grantMission",
 "getMissionComplete", "setMissionAvailable", "getMissionAvailable", "grantGold",
 "grantSilver", "grantBronze", "getMedal", "addQuest", "removeQuest"};
constexpr std::uint32_t NUM_PLAYERS=0x4d8ae0, DIFFICULTY=0x4f719c;
bool supported(const std::string &name) {
  return name == "getv" || name == "setv" || name == "addv" ||
         name == "getpv" || name == "setpv" ||
         std::find(campaign.begin(), campaign.end(), name) != campaign.end();
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
std::uint32_t Memory::word(std::uint32_t address) const {
  std::uint32_t value = 0;
  for (unsigned i=0; i<4; ++i) value |= std::uint32_t(read(address+i)) << (8*i);
  return value;
}
void Memory::word(std::uint32_t address, std::uint32_t value) {
  write(address, {std::uint8_t(value), std::uint8_t(value>>8),
                  std::uint8_t(value>>16), std::uint8_t(value>>24)});
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
std::uint32_t Variables::player_record(amx::Cell slot) const {
  return 0x4d8af0u + std::uint32_t(slot)*0x4c30u;
}
std::uint32_t Variables::mission_byte(amx::Cell slot, amx::Cell difficulty, amx::Cell mission) const {
  return player_record(slot)+0x4bbfu+std::uint32_t(difficulty)*13u+std::uint32_t(mission);
}
amx::Cell Variables::mission_available(amx::Cell d, amx::Cell m) const {
  return m<13 ? memory.read(mission_byte(0,d,m)) & 1 : 0;
}
amx::Cell Variables::mission_complete(amx::Cell d, amx::Cell m) const {
  return m<13 ? (memory.read(mission_byte(0,d,m))>>1) & 1 : 0;
}
void Variables::mission_set_available(amx::Cell d, amx::Cell m, amx::Cell on) {
  if (m>=13) return;
  const auto players=amx::s32(memory.word(NUM_PLAYERS));
  for (amx::Cell slot=0; slot<players; ++slot) {
    auto where=mission_byte(slot,d,m);
    memory.byte(where, (memory.read(where)&0xfe) | (std::uint32_t(on)&1));
  }
}
void Variables::mission_set_complete(amx::Cell d, amx::Cell m, amx::Cell value) {
  if (m>=13) return;
  for (amx::Cell slot=0; slot<4; ++slot) {
    auto where=mission_byte(slot,d,m);
    auto byte=memory.read(where);
    memory.byte(where, value==-1 ? byte|2 : (byte&0xfd)|((std::uint32_t(value)&1)<<1));
  }
}
void Variables::mission_grant_medal(amx::Cell m, amx::Cell level) {
  if (m>=13) return;
  for (amx::Cell slot=0; slot<amx::s32(memory.word(NUM_PLAYERS)); ++slot) {
    auto where=player_record(slot)+0x4c0du+std::uint32_t(m);
    std::int64_t have=signed_byte(memory.read(where));
    if (have>=level) continue;
    memory.byte(0x4f7149, std::uint8_t(slot));
    do {
      auto callback=run_reward_script;
      if (!callback) throw amx::HostFault("script_runByName callback required");
      callback(have==0 ? "firstBronze" : have==1 ? "firstSilver" : "firstGold");
      ++have;
    } while (have<level);
    memory.byte(where, std::uint8_t(level));
  }
}
amx::Cell Variables::mission_get_medal(amx::Cell m, amx::Cell level, amx::Cell flag) {
  amx::Cell slot=0;
  if (flag==0 || memory.read(0x402791)==0) {
    auto callback=local_player_slot;
    if (!callback) throw amx::HostFault("net_getLocalPlayerSlot callback required");
    slot=callback();
  }
  if (m>=13) return 0;
  auto medal=signed_byte(memory.read(player_record(slot)+0x4c0du+std::uint32_t(m)));
  return level==-1 ? medal : medal>=level;
}
void Variables::quest_add(const std::string &title, const std::string &text) {
  for (amx::Cell slot=0; slot<amx::s32(memory.word(NUM_PLAYERS)); ++slot) {
    auto rec=player_record(slot);
    auto count=amx::s32(memory.word(rec+0x43d0));
    if (count>7) return;
    for (amx::Cell i=0; i<count; ++i)
      if (memory.cstr(rec+0x41d0+std::uint32_t(i)*0x40)==title) return;
    auto entry=rec+0x41d0+std::uint32_t(count)*0x40;
    memory.string(entry,title);
    memory.string(entry+0x20,text);
    memory.word(rec+0x43d0,std::uint32_t(count)+1);
  }
}
void Variables::quest_remove(const std::string &title) {
  const auto players=amx::s32(memory.word(NUM_PLAYERS));
  for (amx::Cell slot=0; slot<players; ++slot) {
    auto rec=player_record(slot);
    amx::Cell last=-1;
    for (amx::Cell i=0; i<amx::s32(memory.word(rec+0x43d0)); ++i) {
      auto entry=rec+0x41d0+std::uint32_t(i)*0x40;
      if (memory.cstr(entry)==title) {
        // The placeholder already includes its terminator: exactly nine bytes.
        memory.string(entry,"NoQuests"); memory.string(entry+0x20,"NoQuests"); last=i;
      }
    }
    if (last==-1) continue;
    while (std::int64_t(last)<std::int64_t(amx::s32(memory.word(rec+0x43d0)))-1) {
      auto entry=rec+0x41d0+std::uint32_t(last)*0x40;
      memory.string(entry,memory.cstr(entry+0x40));
      memory.string(entry+0x20,memory.cstr(entry+0x60));
      ++last;
    }
    memory.word(rec+0x43d0,memory.word(rec+0x43d0)-1);
  }
}
amx::Cell Variables::choice_get(std::uint32_t offset) const {
  auto mask=1u<<(memory.word(DIFFICULTY)&31);
  return memory.read(player_record(0)+offset)&mask;
}
void Variables::choice_set(std::uint32_t offset, amx::Cell on) {
  auto mask=std::uint8_t(1u<<(memory.word(DIFFICULTY)&31));
  const auto players=amx::s32(memory.word(NUM_PLAYERS));
  for (amx::Cell slot=0; slot<players; ++slot) {
    auto where=player_record(slot)+offset;
    auto byte=memory.read(where);
    memory.byte(where,on!=0 ? byte|mask : byte&~mask);
  }
}
std::string Variables::string_argument(amx::Machine &machine, amx::Address offset,
                                      std::optional<amx::Cell> &slot) {
  auto address=argument(machine,offset);
  if (machine.get_addr(address)==amx::ERR_NONE) slot=address;
  else if (!slot) throw StackGarbage("unknown handler string slot");
  auto text=machine.get_string(*slot);
  if (text.size()>=160) throw amx::HostFault("string overruns 160-byte handler frame");
  return text;
}
amx::Cell Variables::dispatch(const std::string &name, amx::Machine &machine) {
  if (!supported(name))
    throw amx::HostFault("unsupported RTA script built-in: " + name);
  // A handler frame preserves only its own string-pointer slot. Numeric
  // handlers overwrite both words, as GameState._bind specifies.
  if (name=="addQuest") string_slot.reset();
  else if (name=="removeQuest" || name=="getv" || name=="setv" ||
           name=="addv" || name=="getpv" || name=="setpv") quest_string_slot.reset();
  else forget_string_slot();
  auto difficulty=amx::s32(memory.word(DIFFICULTY));
  if (name=="getEvil") return choice_get(0x4c1a);
  if (name=="getMadeDecision") return choice_get(0x4c1b);
  if (name=="setEvil") { auto on=argument(machine,4); if(on) choice_set(0x4c1a,on); return 0; }
  if (name=="setMadeDecision") { choice_set(0x4c1b,1); return 0; }
  if (name=="grantMission") { mission_set_complete(difficulty,argument(machine,4),-1); return 0; }
  if (name=="getMissionComplete") return mission_complete(difficulty,argument(machine,4));
  if (name=="getMissionAvailable") return mission_available(difficulty,argument(machine,4));
  if (name=="setMissionAvailable") {
    auto mission=argument(machine,4); auto on=argument(machine,8);
    mission_set_available(difficulty,mission,on); return 0;
  }
  if (name=="grantGold" || name=="grantSilver" || name=="grantBronze") {
    mission_grant_medal(argument(machine,4),name=="grantGold" ? 3 : name=="grantSilver" ? 2 : 1); return 0;
  }
  if (name=="getMedal") return mission_get_medal(argument(machine,4),-1,0);
  if (name=="addQuest") {
    auto title=string_argument(machine,4,quest_string_slot);
    auto text=string_argument(machine,8,quest_string_slot);
    quest_add(title,text); return 0;
  }
  if (name=="removeQuest") { quest_remove(string_argument(machine,4,string_slot)); return 0; }
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
  for (const auto &name : campaign)
    result.emplace(name, [this,name](amx::Machine &m,const std::vector<amx::Cell> &)
                   -> std::optional<amx::Address> { return dispatch(name,m); });
  return result;
}
} // namespace champions::simulation::rta::builtins
