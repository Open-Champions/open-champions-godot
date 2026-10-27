#include "champions/mission1_scripts.hpp"
#include "champions/objects.hpp"
#include <algorithm>
#include <set>
#include <stdexcept>

namespace champions::simulation::rta::mission1_scripts {
namespace {
void require(bool ok) {
  if (!ok) throw std::runtime_error("Mission 1 addressed script contract mismatch");
}
struct Reader {
  const amx::Machine &m;
  Contract &c;
  mutable std::uint32_t last = 0;
  std::uint32_t word(std::uint32_t off) const {
    last = off;
    require((off & 3) == 0 && m.dat >= m.cod && off + 4ull <= std::uint64_t(m.dat - m.cod));
    const auto at = std::uint64_t(amx::STRUCT_SIZE) + m.cod + off;
    require(at + 4 <= m.image.size());
    std::uint32_t v = 0;
    for (unsigned i = 0; i < 4; ++i) v |= std::uint32_t(m.image[at + i]) << (8 * i);
    return v;
  }
  void cells(std::uint32_t off, std::initializer_list<std::uint32_t> values) const {
    for (const auto value : values) { require(word(off) == value); off += 4; }
  }
  std::string entry(std::uint32_t off) {
    std::string name;
    for (const auto &p : m.publics()) if (std::uint32_t(p.second) == off) {
      require(name.empty()); name = p.first;
    }
    require(!name.empty()); c.entries.emplace(off, name); return name;
  }
  Site call(std::uint32_t off, const std::string &command, unsigned count,
            std::initializer_list<unsigned> strings = {}) {
    require(word(off) == 0x7b);
    require(m.decoded && m.decoded->count(off) && m.decoded->at(off).opcode == 0x7b);
    require(m.native_name(word(off + 4)) == command);
    Site site{off, command, {}, {}};
    for (unsigned i = 0; i <= count; ++i) require(word(off - 8 * (i + 1)) == 0x27);
    require(word(off - 4) == count * 4);
    for (unsigned i = 0; i < count; ++i)
      site.arguments.push_back(amx::s32(word(off - 12 - 8 * i)));
    for (const auto index : strings) {
      require(index < count);
      const auto text = m.get_string(site.arguments.at(index));
      require(!text.empty()); site.strings[index] = text;
    }
    c.sites.emplace(off, site);
    return site;
  }
  std::string text(std::uint32_t off, const std::string &name, unsigned n, unsigned arg = 0) {
    return call(off, name, n, {arg}).strings.at(arg);
  }
};
std::string prop(const level_flow::Record &r, const char *key) {
  const auto p = r.prop(key);
  return p ? *p : std::string();
}
std::vector<const level_flow::Record *> by_ctor(const std::vector<level_flow::Record> &records,
                                               const level_factory::Executable &exe, std::uint32_t ctor) {
  std::vector<const level_flow::Record *> out;
  for (const auto &r : records) {
    const auto o = level_factory::classify(r.cls, &r.props, exe);
    if (o.kind == level_factory::Kind::Object && o.constructor() == ctor) out.push_back(&r);
  }
  return out;
}
} // namespace

std::optional<Contract> Contract::inspect(const amx::Machine &m) {
  const auto start = m.find_public("start");
  if (start.first != amx::ERR_NONE) return std::nullopt;
  const auto start_offset = std::uint32_t(m.public_entry(start.second).second);
  const bool strife = m.num_publics == 47 && m.num_natives == 21 && start_offset == 0x638;
  const bool drundar = m.num_publics == 36 && m.num_natives == 20 && start_offset == 0x150;
  if (!strife && !drundar) {
    const bool candidate = (m.num_publics == 47 && m.num_natives == 21) ||
        (m.num_publics == 36 && m.num_natives == 20) ||
        (start_offset == 0x638 && (m.num_publics == 47 || m.num_natives == 21)) ||
        (start_offset == 0x150 && (m.num_publics == 36 || m.num_natives == 20));
    require(!candidate);
    return std::nullopt;
  }
  Contract out(strife ? Level::Strife : Level::Drundar);
  Reader r{m, out};
  try {
  r.entry(start_offset);
  if (strife) {
    r.entry(0x568);
    r.call(0x594, "preloadResource", 1, {0});
    out.counter_key = r.text(0x5e8, "getv", 1);
    r.cells(0x5f8, {0x0f, 0x14c});
    r.cells(0x15a0, {0x14c, 0x27});
    require(m.get_string(r.word(0x15a8)) == out.counter_key);
    r.cells(0x15b4, {0x7b});
    require(m.native_name(r.word(0x15b8)) == "setv");
    // Dynamic value pushed from the counter: validate it at the live call.
    out.sites.emplace(0x15b4, Site{0x15b4, "setv", {}, {{0, out.counter_key}}});
    for (const auto at : {0x678u, 0x760u, 0x844u, 0x928u})
      out.stage_keys.push_back(r.text(at, "getv", 1));
    require(out.stage_keys[0] == out.stage_keys[1] &&
            std::set<std::string>(out.stage_keys.begin(), out.stage_keys.end()).size() == 3);
    r.cells(0x688, {0x54, 0x35, 0x744});
    r.cells(0x770, {0x35, 0x828}); r.cells(0x854, {0x35, 0x90c}); r.cells(0x938, {0x35, 0x9f0});
    const std::uint32_t show[4][4] = {{0x6b0,0x6dc,0x708,0x734}, {0x794,0x7c0,0x7ec,0x818},
                                     {0x878,0x8a4,0x8d0,0x8fc}, {0x95c,0x988,0x9b4,0x9e0}};
    for (unsigned group = 0; group < 4; ++group) for (unsigned i = 0; i < 4; ++i) {
      const auto target = r.text(show[group][i], group == i ? "showMonster" : "hideMonster", 1);
      if (group == 0) out.scouts.push_back(target); else require(target == out.scouts[i]);
    }
    require(std::set<std::string>(out.scouts.begin(), out.scouts.end()).size() == 4);
    r.entry(0x200c);
    const auto first_key = r.text(0x1f44, "getv", 1);
    const auto flag = r.call(0x2040, "setv", 2, {0});
    require(flag.strings.at(0) == first_key && flag.arguments[1] == 1);
    require(r.text(0x20a4, "callScript", 1) == r.entry(0xc94));
    const auto quest = r.call(0x20d8, "addQuest", 2, {0,1});
    for (const auto at : {0x1c90u,0x1df0u,0x1ed4u}) {
      out.bridges.push_back(r.text(at, "trigger", 2));
    }
    require(std::set<std::string>(out.bridges.begin(), out.bridges.end()).size() == 3);
    require(r.text(0x1f00, "removeQuest", 1) == quest.strings.at(0));
    r.entry(0x15cc); r.entry(0x156c);
    out.stage1_init_ = r.entry(0x88); out.stage1_death_ = r.entry(0xcc);
    out.stage2_init_ = r.entry(0x3dc); out.stage2_death_ = r.entry(0x420);
    out.catapult_death_ = r.entry(0x138);
    r.cells(0x1668, {0x01,0x14c,0x69,2,0x35,0x1690}); r.cells(0x1694, {0x35,0x1770});
    const auto complete = r.call(0x16f4, "setv", 2, {0});
    require(complete.strings.at(0) == out.stage_keys[3] && complete.arguments[1] == 1);
    require(r.text(0x1720, "callScript", 1) == r.entry(0x17f8));
    for (const auto at : {0x1c90u,0x1df0u,0x1ed4u,0x1f00u,0x1f44u}) {
      // Public owner of each addressed effect, resolved from this image.
      std::uint32_t owner = 0;
      for (const auto &p : m.publics()) if (std::uint32_t(p.second) <= at)
        owner = std::max(owner, std::uint32_t(p.second));
      r.entry(owner);
    }
  } else {
    for (const auto at : {0x18cu,0x1c8u,0x204u}) r.call(at, "setMonsterVal", 3, {0});
    require(out.sites.at(0x18c).strings == out.sites.at(0x1c8).strings &&
            out.sites.at(0x18c).strings == out.sites.at(0x204).strings);
    out.door_flag = r.text(0x338, "getv", 1);
    r.cells(0x348, {0x35,0x3b8});
    require(r.call(0x374, "autoPropSetState", 2, {0}).arguments[1] == 1);
    require(r.call(0x3a8, "autoPropSetState", 2, {0}).arguments[1] == 1);
    const std::uint32_t gets[] = {0x47c,0x4f0,0x564,0x5d8,0x64c,0x6c0,0x734,0x7a8};
    const std::uint32_t effects[] = {0x4b0,0x524,0x598,0x60c,0x680,0x6f4,0x768,0x7dc};
    const std::uint32_t skips[] = {0x4d4,0x548,0x5bc,0x630,0x6a4,0x718,0x78c,0x800};
    std::map<std::string, std::string> restore;
    for (unsigned i = 0; i < 8; ++i) {
      const auto key = r.text(gets[i], "getv", 1);
      const auto effect = r.text(effects[i], "callScript", 1);
      require(m.find_public(effect).first == amx::ERR_NONE);
      require(restore.emplace(key, effect).second);
      out.restore_keys.emplace(effects[i], key);
      r.cells(gets[i]+0x10, {0x35,skips[i]});
    }
    const std::uint32_t keys[] = {0x1d38,0x1e08,0x1ed8,0x1fa8,0x2078,0x2148,0x2218,0x22e8};
    const std::uint32_t actions[] = {0x1cac,0x1d7c,0x1e4c,0x1f1c,0x1fec,0x20bc,0x218c,0x225c};
    for (unsigned i = 0; i < 8; ++i) {
      r.entry(keys[i]); r.entry(actions[i]);
      const auto key = r.text(keys[i]+0x2c, "getv", 1);
      r.cells(keys[i]+0x3c, {0x54}); r.cells(actions[i]+0x1c, {0x6d,0});
      const auto set = r.call(actions[i]+0x48, "setv", 2, {0});
      require(set.strings.at(0) == key && set.arguments[1] == 1);
      const auto effect = r.text(actions[i]+0x74, "callScript", 1);
      require(restore.count(key) && restore.at(key) == effect);
      out.switches.push_back({keys[i],actions[i],key,effect});
    }
    std::set<std::string> unique;
    for (const auto &s : out.switches) unique.insert(s.persistent);
    require(unique.size() == 8);
    r.cells(0x868, {0x01,0,0x69,8,0x35,0x900}); r.cells(0x1c54, {0x01,0,0x69,8});
    const auto inhibit = r.call(0x89c, "setSaveInhibit", 1);
    require(inhibit.arguments[0] == 1);
    require(r.text(0x8dc, "addDeferedCall", 1) == r.entry(0x934));
    require(r.text(0x9e4, "callScript", 1) == r.entry(0xa3c));
    require(r.call(0xaa0, "setSaveInhibit", 1).arguments[0] == 0);
    r.call(0xacc, "cutScene", 1, {0});
    require(r.text(0xa68, "getv", 1) == out.door_flag);
    r.cells(0xa7c, {0x35,0xb10});
    const auto saved = r.call(0xb00, "setv", 2, {0});
    require(saved.strings.at(0) == out.door_flag && saved.arguments[1] == 1);
    for (const auto at : {0x868u,0x1c54u}) {
      std::uint32_t owner = 0;
      for (const auto &p : m.publics()) if (std::uint32_t(p.second) <= at)
        owner = std::max(owner, std::uint32_t(p.second));
      r.entry(owner);
    }
  }
  } catch (const std::exception &) {
    throw std::runtime_error("Mission 1 addressed contract mismatch near code cell " + std::to_string(r.last));
  }
  return out;
}

void Contract::validate_objects(const std::vector<level_flow::Record> &records,
                                 const level_factory::Executable &exe) {
  objects_validated = false;
  Census next;
  next.records = records.size();
  for (const auto &record : records) {
    const auto o = level_factory::classify(record.cls, &record.props, exe);
    if (o.kind != level_factory::Kind::Object) { ++next.not_created; continue; }
    ++next.selected; ++next.constructors[o.constructor()];
    if (o.constructor() == 0x2a0954 || o.constructor() == 0x26a624 || o.constructor() == 0x29fad0)
      ++next.hostiles;
  }
  const bool s = level == Level::Strife;
  require(next.records == (s ? 283u : 270u) && next.selected == (s ? 267u : 254u));
  // Light-bearing fallback is selected by the existing executed factory
  // (research-script-session 6841b8a), not the unlit-only research summary.
  const std::map<std::uint32_t,std::size_t> expected = s
      ? std::map<std::uint32_t,std::size_t>{{0x2a0954,69},{0x24d440,2},{0x261dbc,15},{0x2b1e80,70},
          {0x23d93c,73},{0x23ad18,6},{0x23c44c,8},{0x29df04,4},{0x28f15c,6},{0x28cb90,3},
          {0x27c694,1},{0x1ec900,2},{0x1ecdf4,2},{0x1d7778,1},{0x158928,2},
          {0x158bcc,1},{0x158c6c,1},{0x158d0c,1}}
      : std::map<std::uint32_t,std::size_t>{{0x26a624,71},{0x29fad0,14},{0x290c24,29},{0x23d93c,70},
          {0x23ad18,3},{0x23c44c,16},{0x2416c4,8},{0x243594,4},{0x28f15c,12},{0x28cb90,3},{0x27c694,1},
          {0x1ec900,13},{0x1ecdf4,6},{0x158928,1},{0x158bcc,1},{0x158c6c,1},{0x158d0c,1}};
  if (next.constructors != expected) {
    std::string detail;
    for (const auto &p : next.constructors) {
      const auto wanted = expected.find(p.first);
      if (wanted == expected.end() || wanted->second != p.second)
        detail += " ctor " + std::to_string(p.first) + " count " + std::to_string(p.second);
    }
    throw std::runtime_error("Mission 1 factory census mismatch:" + detail);
  }
  require(next.hostiles == (s ? 69u : 85u) && next.constructors.count(0x271c70) == 0);
  if (s) {
    const auto npcs = by_ctor(records, exe, 0x29df04);
    require(npcs.size() == 4);
    for (unsigned i = 0; i < 4; ++i) require(objects::strip_quotes(prop(*npcs[i], "name")) == scouts[i]);
    std::set<std::string> targets;
    for (const auto *p : by_ctor(records, exe, 0x23c44c)) {
      const auto name = objects::strip_quotes(prop(*p, "name"));
      if (std::find(bridges.begin(), bridges.end(), name) != bridges.end()) targets.insert(name);
    }
    require(targets == std::set<std::string>(bridges.begin(), bridges.end()));
    for (const auto *p : by_ctor(records, exe, 0x2a0954)) {
      if (prop(*p, "initscript").empty() && prop(*p, "deathscript").empty()) continue;
      // Linked stage rows are compared through the image's entry offsets below.
      const auto init = prop(*p, "initscript"), death = prop(*p, "deathscript");
      require(!init.empty() && !death.empty());
      if (init == stage1_init_ && death == stage1_death_) ++next.stage1;
      else if (init == stage2_init_ && death == stage2_death_) ++next.stage2;
      else require(false);
    }
    for (const auto *p : by_ctor(records, exe, 0x24d440)) {
      require(prop(*p, "deathscript") == catapult_death_); ++next.catapults;
    }
    require(next.stage1 == 13 && next.stage2 == 2 && next.catapults == 2);
  } else {
    std::set<std::string> seen;
    for (const auto &record : records) {
      const auto key = prop(record, "key"), action = prop(record, "action1");
      for (const auto &sw : switches) if (key == entries.at(sw.key_entry) && action == entries.at(sw.action_entry)) {
        require(seen.insert(key).second); ++next.switches;
      }
    }
    if (next.switches != 8) throw std::runtime_error("Mission 1 switch pair count " + std::to_string(next.switches));
    const auto doors = by_ctor(records, exe, 0x243594);
    if (doors.size() != 4) throw std::runtime_error("Mission 1 door record count " + std::to_string(doors.size()));
    for (const auto *door : doors)
      if (objects::strip_quotes(prop(*door, "name")) != sites.at(0x374).strings.at(0))
        throw std::runtime_error("Mission 1 addressed door target mismatch");
    std::size_t stairs = 0, boss = 0;
    for (const auto &record : records) {
      const auto name = objects::strip_quotes(prop(record, "name"));
      if (!name.empty() && name == sites.at(0x3a8).strings.at(0)) ++stairs;
      if (!name.empty() && name == sites.at(0x18c).strings.at(0) && prop(record, "miniboss") == "1") ++boss;
    }
    if (stairs != 1 || boss != 1) throw std::runtime_error("Mission 1 named stairs count " + std::to_string(stairs) + " boss count " + std::to_string(boss));
  }
  census = std::move(next);
  objects_validated = true;
}

std::optional<std::uint32_t> Contract::offset_of(const std::string &entry) const {
  for (const auto &p : entries) if (p.second == entry) return p.first;
  return std::nullopt;
}
bool Contract::allows(const amx::Machine &m, const std::string &command,
                       const std::vector<amx::Cell> &args) const {
  if (m.cip < 8 || std::uint64_t(m.cip - 8) > UINT32_MAX) return false;
  const auto found = sites.find(std::uint32_t(m.cip - 8));
  if (found == sites.end() || found->second.command != command) return false;
  const auto &site = found->second;
  if (site.arguments.empty()) {
    if (command != "setv" || args.size() != 2 || m.get_string(args[0]) != counter_key) return false;
  } else if (args != site.arguments) return false;
  for (const auto &str : site.strings)
    if (str.first >= args.size() || m.get_string(args[str.first]) != str.second) return false;
  if (command == "callScript" || command == "addDeferedCall")
    return offset_of(m.get_string(args.at(0))).has_value();
  return true;
}
} // namespace champions::simulation::rta::mission1_scripts
