// Level objects for the script host: the object factory, and the creature
// classes whose constructor chain is ported (research t3/research-script-session
// 6841b8a, docs/script-session.md sections 3-5).
#include "champions/arena_monsters.hpp"
#include <cctype>
#include "champions/monsters_1.hpp"
#include "champions/combat_stats.hpp"
#include "champions/script_commands.hpp"
#include "champions/terrain_file.hpp"
#include <cstring>
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace champions::simulation::rta::script_commands {
namespace {
namespace M1 = monsters_1;
// GameObject_ctor 0x14cb10 (research docs/spec/object-system.md 2.2).
constexpr std::uint32_t VT_GAMEOBJECT = 0x5122a8;
constexpr std::uint32_t RADIUS_24 = 0x41c00000, HEIGHT_72 = 0x42900000;
// ScriptGenerator_ctor 0x265f54: research-items-gaps ecf9742
// docs/monsters-2.md:7660-7667 and symbols/monsters.tsv:262 pin table
// 0x514e60, size 0x4c, facing and flags |= 9. GameObject_ctor's 'hide'
// lookup (string 0x5321a8) is in docs/spec/hide-monster.md section 5.
constexpr std::uint32_t VT_SCRIPT_GENERATOR = 0x514e60, KEY_HIDE = 0x5321a8;
// Creature serial counter (zeroed at a level change, docs/issues/arena-entry.md
// timeline row 7; Creature_ctor takes the next one).
constexpr std::uint32_t CREATURE_SERIAL = 0x4f85c4;
// Scratch addresses in the constructor's memory for the class string and the
// property values its property getters hand back. They are not original
// addresses, so the words that hold them are not copied into the binder.
constexpr std::uint32_t SCRATCH = 0x01f00000, SCRATCH_END = 0x01f80000;
// Object fields that hold a host scratch pointer, or that unported code
// writes (Creature_init: height and draw byte from the model's bounds; the
// hide mask; the property list pointer). Statistics are copied only when the
// selected level's balance table and monster level have been supplied.
struct Span {
  std::uint32_t offset, size;
};
const Span kNotCopied[] = {
    {0x2c, 4},  {0x34, 4},  {0x368, 4}, {0x20, 4},  {0x29, 1},  {0x220, 8},
};
const Span kStats[] = {
    {0x228, 4}, {0x22c, 4}, {0xd0, 2}, {0xd2, 2},
    {0xd4, 2}, {0xd8, 4}, {0xe4, 2},
};
constexpr std::uint32_t LEVEL_TABLE = 0x4f7210;
constexpr std::uint32_t PERCENT_001 = 0x3c23d70a;
std::uint16_t balance_hash(const std::string &name) {
  std::uint32_t h = 0;
  for (unsigned char c : name) {
    int value = c >= 128 ? int(c) - 256 : int(c);
    if (c >= 'A' && c <= 'Z') value += 32;
    h = ((h + value) & 0xffffu) * 0x401u & 0xffffu;
    h ^= h >> 6;
  }
  h = h * 9u & 0xffffu;
  return static_cast<std::uint16_t>(((h >> 11) ^ h) * 0x8001u);
}
bool in_spans(std::uint32_t off, const Span *spans, std::size_t count) {
  for (std::size_t i = 0; i < count; ++i)
    if (off >= spans[i].offset && off < spans[i].offset + spans[i].size)
      return true;
  return false;
}
// Creature_init 0x10a230 (research tools/ref_creature_init.py).
constexpr std::uint32_t KEY_DEAD = 0x52fac8, KEY_CHANGE = 0x52fad0, KEY_INIT_SCRIPT = 0x52fb58;
constexpr std::uint32_t KEY_WEAPONS[] = {0x52fae0, 0x52faf0, 0x52fb08, 0x52fb18,
                                         0x52fb28, 0x52fb38, 0x52fb48};

// The ported constructors (research docs/spec/monsters-1.md, native
// monsters_1). Badger takes the variant the factory row passes: 1 for the
// SnoBadge row, 0 for the Badger prefix (0x15a4bc, 0x15a504). The type name is
// the class's C++ type-info name (vtables.txt), as the binder compares it.
enum class Class : std::uint8_t { None, SmallSpider, Gnaw, Mosquito, Scorpion, Badger,
                                  Skelight, Zek, Horsemen };
struct Ported {
  std::uint32_t ctor;
  Class cls;
  const char *type_name;
};
const Ported kPorted[] = {
    {0x2ab450, Class::SmallSpider, "11SmallSpider"},
    {0x26741c, Class::Gnaw, "4Gnaw"},
    {0x297788, Class::Mosquito, "8Mosquito"},
    {0x2a4d68, Class::Scorpion, "8Scorpion"},
    {0x244018, Class::Badger, "6Badger"},
    {0x2a8770, Class::Skelight, "8Skelight"},
    {0x2c1f94, Class::Zek, "3Zek"},
    {0x271c70, Class::Horsemen, "8Horsemen"},
};
// The SnoBadge equality test (level_factory_table.inc row 39; factory block
// 0x15a494 passes variant 1). The Badger prefix row passes 0.
constexpr std::uint32_t SNOBADGE_TEST = 0x532a30;
constexpr std::uint32_t HORSEMEN_RIDER_FIRST = 0x532cc8;
constexpr std::uint32_t HORSEMEN_FOOT_FIRST = 0x532dc8;
// Horsemen_ctor and Nightmare_ctor (research docs/spec/horsemen-mount.md section 1). The rider
// rows' name tests give the variant: 0x532cc8, 0x532cd8, 0x532ce8, 0x532cf8 -> 0..3.
constexpr std::uint32_t HORSEMEN_CTOR = 0x271c70, NIGHTMARE_CTOR = 0x270968;
constexpr std::uint32_t RIDER_TESTS[] = {0x532cc8, 0x532cd8, 0x532ce8, 0x532cf8};
constexpr std::uint32_t ON_FOOT_TESTS[] = {0x532dc8, 0x532dd8, 0x532de8, 0x532df8};
constexpr std::uint32_t MOUNT_BLOCK_SIZE = 0x3d8;

struct Refused : std::runtime_error {
  using std::runtime_error::runtime_error;
};

} // namespace

level_factory::Executable executable_view(std::shared_ptr<const arena_modes::Executable> exe) {
  if (!exe)
    throw std::invalid_argument("no executable");
  level_factory::Executable view;
  view.string = [exe](std::uint32_t va) { return exe->string(va); };
  view.byte = [exe](std::uint32_t va) { return exe->byte(va); };
  return view;
}

void Host::set_executable(level_factory::Executable exe) {
  exe_ = std::make_unique<level_factory::Executable>(std::move(exe));
  // ScriptGenerator_spawn's world_createObject goes through the factory row
  // and a ported constructor chain; anything else refuses.
  items_->create_object = [this](const std::string &name, const std::array<std::uint32_t, 3> &pos,
                                 std::int32_t angle, std::uint32_t props) {
    return create_object(name, pos, angle, props);
  };
  items_->dynamic_type = [this](std::uint32_t address) {
    const auto *obj = variables_->obj_at(address);
    return obj ? obj->type_name : std::string();
  };
}

std::uint32_t Host::create_object(const std::string &class_name, const std::array<std::uint32_t, 3> &pos,
                                  std::int32_t angle, std::uint32_t props_word) {
  // The generator's property-list word is the generator's own address here
  // (build_generator): the list lives in the host, not in the binder memory.
  const auto *generator = variables_->obj_at(props_word);
  if (!exe_ || !generator || !generator->props)
    throw script_items::Refused("world_createObject: the property list is not the host's");
  const auto outcome = level_factory::classify(class_name, generator->props.get(), *exe_);
  if (outcome.kind == level_factory::Kind::Nothing)
    return 0; // the factory returns 0: nothing is created
  if (outcome.kind != level_factory::Kind::Object)
    throw script_items::Refused("world_createObject: the factory row calls a function and builds no object");
  bool ported = false;
  for (const auto &p : kPorted)
    ported = ported || p.ctor == outcome.constructor();
  if (!ported)
    throw script_items::Refused("world_createObject: the constructor of this class is not ported");
  if (!external_seed_)
    throw script_items::Refused("world_createObject: GameObject_ctor draws from the shared seed, which is not bound");
  const auto address = variables_->world->pool.allocate(outcome.size);
  if (!address)
    return 0; // pool_new returned 0
  level_flow::Record record;
  record.cls = class_name;
  record.angle = static_cast<std::uint16_t>(angle);
  record.position = pos;
  record.props = *generator->props;
  const auto result = build(record, outcome, address, record.props);
  if (result != Build::Built)
    throw script_items::Refused("world_createObject: the constructor chain is not run here");
  counts_.objects_built += 1;
  return address;
}

Host::Build Host::build_generator(const level_factory::Outcome &outcome, const level_flow::Record &record,
                                  std::uint32_t c, const std::vector<std::string> &props) {
  if (!external_seed_) {
    counts_.build_refusals["no session seed"] += 1;
    return Build::NotBuilt;
  }
  auto &exe = *exe_;
  if (const auto hide = level_factory::prop_find(&props, exe.string(KEY_HIDE), exe);
      hide && level_factory::atoi(*hide, exe)) {
    counts_.build_refusals["ScriptGenerator hide"] += 1;
    return Build::NotBuilt;
  }
  auto list = std::make_shared<std::vector<std::string>>(props);
  const auto obj = variables_->new_object({}, 0, record.position, record.angle, "15ScriptGenerator", list, c,
                                          outcome.size);
  auto &memory = variables_->memory;
  const std::pair<std::uint32_t, std::uint32_t> words[] = {
      {0x00, 0}, {0x04, 0}, {0x08, 0}, {0x48, VT_GAMEOBJECT}, {0x1c, RADIUS_24}, {0x20, HEIGHT_72},
      {0x0c, 0}, {0x10, record.position[0]}, {0x14, record.position[1]}, {0x18, record.position[2]},
      {0x34, 0}, {0x24, 0}};
  for (const auto &w : words)
    memory.word(c + w.first, w.second);
  memory.byte(c + 0x28, 25);
  memory.byte(c + 0x29, 73);
  // GameObject_ctor step 6: one draw from the shared seed seeds the object's own generator.
  memory.word(c + 0x44, external_seed_->draw());
  // The property-list word +0x2c is the host's identity for the list.
  memory.word(c + 0x2c, c);
  // ScriptGenerator_ctor 0x265f54.
  memory.word(c + 0x48, VT_SCRIPT_GENERATOR);
  memory.word(c + 0x0c, memory.word(c + 0x0c) | 9u);
  memory.byte(c + 0x38, static_cast<std::uint8_t>(record.angle & 0xff));
  memory.byte(c + 0x39, static_cast<std::uint8_t>(record.angle >> 8));
  static_cast<objects::GameObject &>(*obj).flags = memory.word(c + 0x0c);
  mark_built(c);
  counts_.objects_registered += 1;
  return Build::Built;
}

void Host::set_level_stats(const std::string &name, const std::uint8_t *data,
                           std::size_t size) {
  stats_ready_ = false;
  balance_rows_.clear();
  level_name_.clear();
  if (!exe_ || !data || size < 2 || name.empty())
    throw std::invalid_argument("level statistics need executable, level and monsters.bal");
  const auto count = std::uint32_t(data[0]) | (std::uint32_t(data[1]) << 8);
  if (count > (size - 2) / 12)
    throw std::invalid_argument("truncated monsters.bal");
  std::vector<level_flow::LevelRow> rows;
  rows.reserve(91);
  auto word = [&](std::uint32_t at) {
    std::uint32_t v = 0;
    for (unsigned i = 0; i < 4; ++i)
      v |= std::uint32_t(exe_->byte(at + i)) << (8 * i);
    return v;
  };
  for (unsigned i = 0; i <= 90; ++i) {
    const auto at = LEVEL_TABLE + 0x30u * i;
    const auto ptr = word(at);
    level_flow::LevelRow row;
    row.name = ptr ? exe_->string(ptr) : std::string();
    row.monster_level = static_cast<std::int8_t>(exe_->byte(at + 4));
    rows.push_back(row);
    if (!ptr) break;
  }
  if (rows.empty() || !rows.back().name.empty())
    throw std::invalid_argument("unterminated level table");
  // mainloop_loadRequestedWorld splits the request at '_' and stores only the
  // base name (research a684d7d, difftest_level_name_split.py: g_levelName and
  // g_levelTexName 0x44fe18 both hold the base). Only the base is written.
  const auto request = level_flow::split_request(name);
  // monsters.bal is loaded "on every level load outside the front end"
  // (research-creature-stats docs/data-tables.md:216-217); `intro` is the front
  // end (docs/issues/live-rig.md:164,220). Nothing supports applying the table
  // there, so refuse: creatures keep their hit points unreadable.
  if (request.first == "intro") {
    counts_.build_refusals["Front-end level has no balance table"] += 1;
    return;
  }
  // The executable's own table supplies the longest name this image uses.
  // Refuse base names longer than that known-safe domain; the capacity of
  // the global at 0x44fe18 is not documented.
  std::size_t max_level_name = 0;
  for (const auto &entry : rows)
    max_level_name = std::max(max_level_name, entry.name.size());
  if (request.first.size() > max_level_name)
    throw std::invalid_argument("level name exceeds executable table names");
  // A short arbitrary caller string is not proof that it fits the original
  // allocation. Accept only a base name present in the executable table;
  // the capacity of 0x44fe18 itself remains unpinned.
  if (request.first.empty() || std::none_of(rows.begin(), rows.end(), [&](const level_flow::LevelRow &entry) {
        return entry.name == request.first;
      }))
    throw std::invalid_argument("level name is not in executable table");
  const auto row = level_flow::find_level_row(rows, request.first, request.second);
  // Single-player only: difficulty 1 and pvp=false are assumed because
  // Host::start_single_player is the only mode this host models; the PvP
  // mode B level (3.0, data-tables.md section 5) is not reachable here.
  const auto level = level_flow::monster_level(row, 1, request.first);
  std::vector<BalanceRow> parsed;
  parsed.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    const auto at = 2 + 12 * i;
    auto half = [&](std::size_t off) {
      return std::uint16_t(std::uint16_t(data[at + off]) |
                           (std::uint16_t(data[at + off + 1]) << 8));
    };
    BalanceRow b;
    b.hash = half(0);
    for (unsigned j = 0; j < 5; ++j)
      b.factors[j] = combat::fmul(combat::itof(half(2 + 2 * j)), PERCENT_001);
    parsed.push_back(b);
  }
  balance_rows_ = std::move(parsed);
  monster_level_ = level;
  // Base name: the stomedal/innpit override, Skelight's blomedal test and
  // 0x44fe18 all see it (research a684d7d).
  level_name_ = request.first;
  stats_ready_ = true;
}

void Host::set_level_world(const std::uint8_t *data, std::size_t size) {
  if (!data || !size) throw std::invalid_argument("empty level WORLD");
  terrain_level_ = std::make_unique<world_queries::Level>(
      terrain_file::decode(std::vector<std::uint8_t>(data, data + size)));
}

void Host::advance_first_level_items(const std::string &level_name,
                                     std::optional<bool> hud_lmp_resident) {
  // Executed gate: game_loadLevel 0x1e5690 compares against intro;
  // 0x1e56a4 queries hud.lmp residency (research 7a66b58, and 938ccb8 for both
  // hud.lmp outcomes). The loader's 223 draws: research-items docs/startup.md:341
  // and docs/issues/sync-breakers.md:383.
  if (level_name == "intro") {
    // Front end: no balance table is loaded (see set_level_stats).
    stats_ready_ = false;
    seed_gate_observed_ = true;
    return;
  }
  if (level_name.empty() || !hud_lmp_resident) {
    counts_.item_load_gate_refusals += 1;
    counts_.item_load_draws_inferred += 223;
    stats_built_.clear();
    return;
  }
  if (*hud_lmp_resident) {
    seed_gate_observed_ = true;
    return;
  }
  if (!external_seed_) throw std::invalid_argument("item loader needs session seed");
  // item_loadResources parses every base definition with variant -1.
  for (unsigned i = 0; i < 223; ++i) external_seed_->draw();
  seed_gate_observed_ = true;
}

void Host::start_single_player() {
  // docs/script-session.md section 2.
  variables_->memory.word(0x4d8ae0, 1u);
  // Difficulty is initialized once by Host from script_items::DIFFICULTY_IMAGE.
  mirror();
}

void Host::receive_launch(const net_launch::Record &record0) {
  const auto got = net_launch::launch_receive(record0);
  auto &memory = variables_->memory;
  memory.word(script_items::G_DIFFICULTY, got.difficulty);
  memory.byte(0x4f8408, got.kill_limit);
  memory.word(0x4f711c, got.pvp_mode_a ? 1u : 0u);
  memory.word(0x4f7120, got.pvp_mode_b ? 1u : 0u);
  memory.word(0x4f7124, got.mode_3 ? 1u : 0u);
  if (got.set_cm1)
    script_items::set_cm1_word(memory, *got.set_cm1,
                               [this](const std::string &name) { return variables_->profile_get(name); });
  mirror();
}

void Host::stub(const level_flow::Record &record, std::uint32_t address,
                std::vector<std::string> props, std::int64_t size) {
  auto list = std::make_shared<std::vector<std::string>>(std::move(props));
  const auto obj = variables_->new_object({}, 0, record.position, record.angle, record.cls, list,
                                          address, size);
  stubs_.insert(obj->addr);
  counts_.objects_registered += 1;
}

// The factory's order is the file order (world_loadRecords 0x14e808). Every
// record the factory constructs takes its pool block, built or not, so the
// pool runs out where the original's would.
bool Host::adopt_classified(const std::vector<level_flow::Record> &records) {
  record_addresses_.assign(records.size(), 0);
  for (std::size_t index = 0; index < records.size(); ++index) {
    const auto &record = records[index];
    // GameObject_ctor step 7: name= values in double quotes are unquoted in place.
    std::vector<std::string> props = record.props;
    for (auto &entry : props)
      if (entry.compare(0, 5, "name=") == 0)
        entry = "name=" + objects::strip_quotes(entry.substr(5));
    const auto outcome = level_factory::classify(record.cls, &record.props, *exe_);
    if (outcome.kind != level_factory::Kind::Object) {
      counts_.objects_not_created += 1;
      // A record with a dialogScript that makes no object has no talker (the
      // TestLocation records: docs/spec/dialog-gaps.md 6). Nothing is created and
      // nothing is refused.
      for (const auto &entry : record.props) {
        std::string key = entry.substr(0, entry.find('='));
        for (auto &c : key)
          c = char(std::tolower(static_cast<unsigned char>(c)));
        if (key == "dialogscript" && entry.find('=') != std::string::npos) {
          dialog_counts_.no_object_talker_records += 1;
          break;
        }
      }
      continue;
    }
    const auto address = variables_->world->pool.allocate(outcome.size);
    if (!address) {
      counts_.objects_pool_refused += 1;
      continue;
    }
    const auto result = build(record, outcome, address, props);
    if (variables_->object_properties.count(address))
      record_addresses_[index] = address;
    if (result == Build::Built) {
      counts_.objects_built += 1;
      continue;
    }
    if (result == Build::NotBuilt) {
      stub(record, address, props, outcome.size);
    // Creature_ctor also applies hide/wait at load (hide-monster.md section
    // 3.4). A stub has not run that constructor; keep hide/show refused if
    // either property is present, including an unparsed value.
    auto creature = level_factory::creature_kind(outcome);
    bool load_property_refused = false;
    if (creature == level_factory::Creature::Yes) {
      load_property_refused = std::any_of(props.begin(), props.end(), [](const std::string &p) {
        return p.rfind("hide=", 0) == 0 || p.rfind("wait=", 0) == 0;
      });
      if (load_property_refused) {
        creature = level_factory::Creature::Unknown;
        counts_.build_refusals["creature stub with hide/wait load property"] += 1;
      }
    }
    stub_info_[address] = {creature, outcome.constructor(), load_property_refused};
      record_addresses_[index] = address;
    }
    counts_.objects_unbuilt += 1;
    // A mounted record that reached its refusal after both GameObject_ctor draws
    // has made every shared draw it owes (research horsemen-mount 1.1).
    if (!(result == Build::Stubbed && last_draws_complete_))
      counts_.load_draws_unmodelled += 1;
    counts_.unbuilt_constructors[outcome.constructor()] += 1;
  }
  return true;
}

Host::Build Host::build(const level_flow::Record &record, const level_factory::Outcome &outcome,
                        std::uint32_t c, const std::vector<std::string> &props) {
  last_draws_complete_ = false;
  // Both sets of Horsemen rows call the same constructor. The factory supplies
  // the mounted argument; properties always come from the individual record.
  bool rider_row = false;
  for (const auto t : RIDER_TESTS)
    rider_row = rider_row || outcome.test == t;
  for (const auto t : ON_FOOT_TESTS)
    rider_row = rider_row || outcome.test == t;
  if ((outcome.constructor() == HORSEMEN_CTOR && rider_row) || outcome.constructor() == NIGHTMARE_CTOR)
    return build_mounted(record, outcome, c, props);
  if (outcome.constructor() == SCRIPT_GENERATOR_CTOR)
    return build_generator(outcome, record, c, props);
  // Campaign-combat r7 observed both class constructors entering Creature_ctor,
  // then GameObject_ctor, which called res_find. Its 46 resource returns are
  // addresses in one scripted-entry emulator session, not addresses allocated
  // by this host. Registration on the 60-frame list and direct runObjects to
  // WarBoar_run dispatch were observed in round 8; Minotaur dispatch was not
  // captured. Definition names come from the selected executable, never the
  // capture's returned pointers. This verifies the native resource service
  // separately from the still-unported actor execution.
  if (outcome.constructor() == 0x2ba03c && creature_builder_)
    return build_creature_family(record, outcome, c, props);
  if (outcome.constructor() == 0x2ba03c || outcome.constructor() == 0x288fdc) {
    const bool boar = outcome.constructor() == 0x2ba03c;
    const std::uint32_t definition = boar ? 0x502f80 : 0x501200;
    const unsigned slots = boar ? 20 : 26;
    auto word = [this](std::uint32_t at) {
      std::uint32_t value = 0;
      for (unsigned i = 0; i < 4; ++i)
        value |= std::uint32_t(exe_->byte(at + i)) << (i * 8);
      return value;
    };
    const auto lump = exe_->string(word(definition));
    bool complete = !lump.empty();
    for (unsigned slot = 0; slot < slots && complete; ++slot) {
      const auto name = exe_->string(word(definition + 8 + 8 * slot));
      const auto handle = resources_.find(lump, name);
      complete = !name.empty() && handle && resources_.bytes(*handle).has_value();
    }
    if (!complete || word(definition + 8 + 8 * slots) != 0) {
      counts_.build_refusals[boar ? "WarBoar named resources unavailable" :
                                    "Minotaur named resources unavailable"] += 1;
      return Build::NotBuilt;
    }
    counts_.build_refusals[boar ? "WarBoar class tick and shared actor runtime unavailable" :
                                  "Minotaur: base Creature_ctor flags, hide variant and hand writes Unknown; Minotaur_run not in the research"] += 1;
    return Build::NotBuilt;
  }
  if (outcome.constructor() == 0x158928) {
    if (!seed_gate_observed_ || counts_.item_load_gate_refusals ||
        counts_.load_draws_unmodelled) {
      counts_.build_refusals["Player1 placement needs faithful seed"] += 1;
      return Build::NotBuilt;
    }
    if (!external_seed_ || !terrain_level_) {
      counts_.build_refusals["Player1 needs session seed and WORLD"] += 1;
      return Build::NotBuilt;
    }
    // The ranking's inputs (previous level, entrance, gate travel, save load)
    // are not derived by this host; the caller must have supplied them.
    if (!player1_entry_) {
      counts_.build_refusals["Player1 needs caller-supplied level entry"] += 1;
      return Build::NotBuilt;
    }
    const auto &entry = *player1_entry_;
    bool world_seed_active = false;
    // The original writes all four spawn slots and the rank or none on a
    // completed ranking; a refused placement restores them.
    auto &placement = *variables_->world;
    const auto saved_spawn_pos = placement.spawn_pos;
    const auto saved_spawn_facing = placement.spawn_facing;
    const auto saved_start_rank = placement.start_rank;
    try {
    auto list = std::make_shared<std::vector<std::string>>(props);
    auto obj = variables_->new_object({}, 3, record.position, record.angle,
                                      "7Player1", list, c, outcome.size);
    obj->vtable = objects::VT_PLAYER1;
    obj->seed = external_seed_->draw(); // GameObject_ctor, before marker ranking.
    variables_->memory.word(c + 0x44, obj->seed);
    variables_->memory.word(c + 0x0c, 3);
    variables_->memory.word(c + 0x48, objects::VT_PLAYER1);
    variables_->memory.byte(c + 0x38, static_cast<std::uint8_t>(record.angle));
    variables_->memory.byte(c + 0x39, static_cast<std::uint8_t>(record.angle >> 8));
    auto &world = *variables_->world;
    world.shared.state = external_seed_->state;
    world_seed_active = true;
    int rank = world.start_rank;
    if (entry.loaded_from_save) rank = 3;
    bool take = false;
    if (rank != 3) {
      auto key = [&](const char *name) { return level_factory::prop_find(&props, name, *exe_); };
      if (rank <= 0 || (rank == 1 && key("default") &&
                        level_factory::atoi(*key("default"), *exe_) != 0)) {
        take = true; rank = 1;
      }
      if (const auto from = key("from")) {
        if (!entry.gate_travel && *from == entry.previous_level) { take = true; rank = 2; }
        else if (entry.gate_travel && *from == "gate") { take = true; rank = 3; }
      }
      if (!entry.gate_travel)
        if (const auto name = key("name"))
          if (*name == entry.entrance) { take = true; rank = 3; }
    }
    world.start_rank = rank;
    if (take) {
      auto blocked = [&](Raw radius, const objects::Vec3 &pos) {
        return world_queries::terrain_test_flag1(*terrain_level_, radius, pos);
      };
      auto height = [&](Raw radius, const objects::Vec3 &pos) {
        return world_queries::terrain_height(*terrain_level_, radius, pos);
      };
      for (unsigned slot = 0; slot < 4; ++slot) {
        const auto spot = world.find_free_spot(record.position, 36, 24, false, blocked, height);
        world.spawn_pos[slot] = spot.first;
        world.spawn_facing[slot] = (std::uint32_t(record.angle) + world.shared.draw() % 4000u - 2000u) & 65535u;
      }
      external_seed_->state = world.shared.state;
    }
    mark_built(c);
    counts_.objects_registered += 1;
    return Build::Built;
    } catch (const std::exception &error) {
      if (world_seed_active)
        external_seed_->state = variables_->world->shared.state;
      placement.spawn_pos = saved_spawn_pos;
      placement.spawn_facing = saved_spawn_facing;
      placement.start_rank = saved_start_rank;
      // WORLD queries can refuse a geometry shape outside their executed
      // domain. A failed Player1 placement must not abort every later record.
      const char *reason = "Player1 placement other refusal";
      if (dynamic_cast<const world_queries::ResourceLimit *>(&error))
        reason = "Player1 WORLD query budget";
      else if (dynamic_cast<const world_queries::Error *>(&error))
        reason = "Player1 WORLD query unsupported";
      else if (dynamic_cast<const objects::ResourceLimit *>(&error))
        reason = "Player1 object query budget";
      else if (dynamic_cast<const objects::ObjectError *>(&error))
        reason = "Player1 object query unsupported";
      counts_.build_refusals[reason] += 1;
      if (variables_->world->objects.count(c)) {
        stubs_.insert(c);
        counts_.objects_registered += 1;
        return Build::Stubbed;
      }
      return Build::NotBuilt;
    }
  }
  const Ported *ported = nullptr;
  for (const auto &p : kPorted)
    if (p.ctor == outcome.constructor())
      ported = &p;
  if (!ported)
    return Build::NotBuilt;
  if (!external_seed_) {
    counts_.build_refusals["no session seed"] += 1;
    return Build::NotBuilt;
  }
  if (stats_ready_ &&
      (variables_->memory.u32(0x4d8ae0) != 1 ||
       variables_->memory.u32(script_items::G_DIFFICULTY) != 1)) {
    counts_.build_refusals["stats require one player at difficulty 1"] += 1;
    return Build::NotBuilt;
  }
  // Mounted Horsemen reach Nightmare_ctor, whose body is not yet in the
  // executed research set. Keep the entire record unbuilt until it is.
  if (ported->cls == Class::Horsemen && outcome.test >= HORSEMEN_RIDER_FIRST &&
      outcome.test <= HORSEMEN_RIDER_FIRST + 0x30) {
    counts_.build_refusals["mounted Horsemen needs Nightmare_ctor"] += 1;
    return Build::NotBuilt;
  }
  auto &exe = *exe_;
  auto prop = [&](std::uint32_t key) {
    return level_factory::prop_find(&props, exe.string(key), exe);
  };
  // Creature_init paths that call the class or run a script (ref_creature_init
  // steps 1, 3, 6) are not run here: such a record stays a stub.
  if (prop(KEY_DEAD)) {
    counts_.build_refusals["Creature_init dead"] += 1;
    return Build::NotBuilt;
  }
  if (prop(KEY_INIT_SCRIPT)) {
    counts_.build_refusals["Creature_init initScript"] += 1;
    return Build::NotBuilt;
  }
  // The constructor's memory: the executable image under it, the binder's
  // creature serial poked in.
  M1::Memory mem({}, [&exe](creature_hp::Address a, std::size_t n) -> std::uint64_t {
    std::uint64_t v = 0;
    for (std::size_t i = n; i-- > 0;) {
      std::uint8_t b = 0;
      try {
        b = exe.byte(static_cast<std::uint32_t>(a + i));
      } catch (const std::exception &) {
        b = 0;
      }
      v = (v << 8) | b;
    }
    return v;
  });
  mem.poke(CREATURE_SERIAL, 1, variables_->memory.u8(CREATURE_SERIAL));
  if (stats_ready_) {
    mem.poke(0x4f714c, 4, monster_level_);
    for (std::size_t i = 0; i < level_name_.size(); ++i)
      mem.poke(0x44fe18 + static_cast<std::uint32_t>(i), 1,
               static_cast<unsigned char>(level_name_[i]));
    mem.poke(0x44fe18 + static_cast<std::uint32_t>(level_name_.size()), 1, 0);
  }
  std::uint32_t scratch = SCRATCH;
  auto place = [&](const std::string &text) -> std::uint32_t {
    const auto at = scratch;
    if (at + text.size() + 1 >= SCRATCH_END)
      throw Refused("scratch strings exhausted");
    for (std::size_t i = 0; i < text.size(); ++i)
      mem.poke(at + static_cast<std::uint32_t>(i), 1, static_cast<unsigned char>(text[i]));
    mem.poke(at + static_cast<std::uint32_t>(text.size()), 1, 0);
    scratch = (at + static_cast<std::uint32_t>(text.size()) + 16) & ~15u;
    return at;
  };
  const auto type_ptr = place(record.cls);
  M1::Model model(mem, M1::CallbackPolicy::RequireHandlers);
  std::shared_ptr<builtins::Obj> obj;
  auto prop_text = [&](const M1::Argument &key) -> std::optional<std::string> {
    return level_factory::prop_find(&props, exe.string(std::get<std::uint32_t>(key)), exe);
  };
  // GameObject_ctor: research docs/spec/object-system.md 2.2, steps 1, 3, 5-7.
  model.callbacks["GameObject_ctor"] = [&](M1::Model &m, const std::vector<M1::Argument> &a) {
    const auto self = std::get<std::uint32_t>(a.at(0));
    const auto pos = std::get<M1::Vec3>(a.at(1));
    const auto def = std::get<std::uint32_t>(a.at(2));
    if (self != c)
      throw Refused("GameObject_ctor on another block");
    auto list = std::make_shared<std::vector<std::string>>(props);
    obj = variables_->new_object({}, 0, pos, 0, ported->type_name, list, c, outcome.size);
    const std::pair<std::uint32_t, std::uint64_t> words[] = {
        {0x00, 0}, {0x04, 0}, {0x08, 0}, {0x48, VT_GAMEOBJECT}, {0x1c, RADIUS_24},
        {0x20, HEIGHT_72}, {0x0c, 0}, {0x10, pos[0]}, {0x14, pos[1]}, {0x18, pos[2]},
        {0x34, 0}, {0x24, def}};
    for (const auto &w : words)
      m.mem.store(self + w.first, 4, w.second);
    m.mem.store(self + 0x28, 1, 25);
    m.mem.store(self + 0x29, 1, 73);
    // Step 6: one draw from the shared seed; the object's own generator is the
    // new seed >> 16.
    m.mem.store(self + 0x44, 4, external_seed_->draw());
    return std::uint64_t{0};
  };
  model.callbacks["obj_getPropInt"] = [&](M1::Model &, const std::vector<M1::Argument> &a) {
    const auto text = prop_text(a.at(1));
    return std::uint64_t{text ? level_factory::atoi(*text, exe) : 0u};
  };
  model.callbacks["obj_getPropStr"] = [&](M1::Model &, const std::vector<M1::Argument> &a) {
    const auto text = prop_text(a.at(1));
    return std::uint64_t{text ? place(*text) : 0u};
  };
  model.balance_lookup = [&](M1::Model &, Raw) {
    if (stats_ready_)
      for (const auto &row : balance_rows_)
        if (row.hash == balance_hash(record.cls))
          return M1::Model::Balance{1u, row.factors[0], row.factors[1],
                                    row.factors[2], row.factors[3], row.factors[4]};
    return M1::Model::Balance{};
  };
  // Creature_setStats 0x10a620, executed in research and already modelled by
  // combat::monster_stats. Its own-generator draw precedes the stat stores.
  // Without selected level data, retain only the draw and mark stats unknown.
  model.callbacks["Creature_setStats"] = [&](M1::Model &m, const std::vector<M1::Argument> &a) {
    const auto self = std::get<std::uint32_t>(a.at(0));
    const bool roll = std::get<std::uint32_t>(a.at(1)) != 0;
    std::uint32_t draw = 0;
    if (roll) {
      const auto seed = static_cast<std::uint32_t>(m.mem.load(self + 0x44, 4)) * 0x19660du + 0x3c6ef35fu;
      m.mem.store(self + 0x44, 4, seed);
      draw = seed >> 16;
    }
    if (stats_ready_) {
      combat::Tables tables;
      tables.fixed_levels = {U"stomedal", U"innpit"};
      combat::World world;
      world.monster_level = static_cast<Raw>(m.mem.load(0x4f714c, 4));
      world.num_players = variables_->memory.u32(0x4d8ae0);
      world.difficulty = variables_->memory.u32(script_items::G_DIFFICULTY);
      world.medal_mission = variables_->memory.u32(0x4f713c);
      for (unsigned char ch : level_name_) world.level_name.push_back(ch);
      std::array<Raw, 5> factors{};
      for (unsigned i = 0; i < 5; ++i)
        factors[i] = std::get<std::uint32_t>(a.at(i + 2));
      // Creature_ctor runs this before Creature_init, so the MiniBoss refusal
      // (the Creature_init callback below) comes after it; the exception
      // leaves the record unbuilt and nothing is copied. Rank 2 for named
      // bosses changes only the multiple-player damage rule, and this host
      // starts one player. No constructor here has a Dead property.
      const auto result = combat::monster_stats(tables, world, factors, 0,
                                               std::u32string{}, roll, draw);
      if (result.hp) m.mem.store(self + 0x228, 4, *result.hp);
      if (result.max_hp) m.mem.store(self + 0x22c, 4, *result.max_hp);
      m.mem.store(self + 0xe4, 2, static_cast<std::uint16_t>(result.armor));
      m.mem.store(self + 0xd0, 2, static_cast<std::uint16_t>(result.dmg_low));
      m.mem.store(self + 0xd2, 2, static_cast<std::uint16_t>(result.dmg_high));
      m.mem.store(self + 0xd4, 2, static_cast<std::uint16_t>(result.attack));
      m.mem.store(self + 0xd8, 4, result.exp_value);
    }
    return std::uint64_t{0};
  };
  // Creature_init 0x10a230 (tools/ref_creature_init.py), the paths that call
  // nothing of the class: step 2's height and draw byte are not copied; step 4
  // stores the change mask (the hide mask is not copied); step 5 needs a
  // weapon, which these classes do not have.
  model.callbacks["Creature_init"] = [&](M1::Model &m, const std::vector<M1::Argument> &a) {
    const auto self = std::get<std::uint32_t>(a.at(0));
    if (m.mem.load(self + 0xa8, 8) & 1)
      throw Refused("Creature_init MiniBoss");
    std::uint32_t mask = 0;
    const auto format = exe.string(KEY_CHANGE);
    const auto at = format.find("%d");
    if (at == std::string::npos)
      throw Refused("change key format");
    for (int n = 0; n < 32; ++n) {
      auto key = format;
      key.replace(at, 2, std::to_string(n));
      if (level_factory::prop_find(&props, key, exe))
        mask |= 1u << n;
    }
    if (mask)
      m.mem.store(self + 0xe8, 4, mask);
    if (m.mem.load(self + 0x50, 4))
      for (const auto key : KEY_WEAPONS)
        if (const auto text = level_factory::prop_find(&props, exe.string(key), exe))
          if (level_factory::atoi(*text, exe))
            throw Refused("Creature_init weapon element");
    return std::uint64_t{0};
  };
  model.callbacks["obj_moveOffGrid"] = [&](M1::Model &, const std::vector<M1::Argument> &) {
    if (!obj)
      throw Refused("grid call before GameObject_ctor");
    variables_->world->move_off_grid(*obj);
    return std::uint64_t{0};
  };
  model.callbacks["obj_runEvery60"] = [&](M1::Model &, const std::vector<M1::Argument> &) {
    if (!obj)
      throw Refused("run list before GameObject_ctor");
    variables_->world->run_every_60(*obj);
    return std::uint64_t{0};
  };
  model.callbacks["obj_runEveryFrame"] = [&](M1::Model &, const std::vector<M1::Argument> &) {
    if (!obj)
      throw Refused("run list before GameObject_ctor");
    variables_->world->run_every_frame(*obj);
    return std::uint64_t{0};
  };
  // handle_clear 0x1a1698 removes every node on a list (research docs/objects.md,
  // "Handle lists"). The set-states call it on the target list +0x234, which
  // Creature_ctor has just emptied: nothing to remove. A list with nodes is
  // not modelled here.
  model.callbacks["handle_clear"] = [&](M1::Model &m, const std::vector<M1::Argument> &a) {
    if (m.mem.load(std::get<std::uint32_t>(a.at(0)), 4))
      throw Refused("handle_clear on a list with nodes");
    return std::uint64_t{0};
  };
  // The two Holder.v1 bodies are executed (research-creature-stats 3469927,
  // creature-stats-port-check.md, "Weapon holder slot 1"), but that pinned
  // research does not read slot 1 from any table. Refuse before choosing a
  // body from the holder's class name or first word.
  model.callbacks["Holder.v1"] = [](M1::Model &, const std::vector<M1::Argument> &) -> std::uint64_t {
    throw Refused("Holder.v1 table slot 1 is untraced");
  };
  // The constructors discard lump_load's result; only its side effects are
  // outside this model, so this return cannot feed retained object bytes.
  model.callbacks["lump_load"] = [](M1::Model &, const std::vector<M1::Argument> &) {
    return std::uint64_t{0};
  };
  // The animation calls write the animation state at +0x114 and draw the
  // cosmetic seed; model_buildHideMask's answer goes to +0x220 (not copied).
  for (const char *name : {"anim_clear", "anim_push", "anim_randomizeTopTime", "anim_start",
                           "anim_startBlendInOut", "anim_syncToSimFrame", "model_buildHideMask"})
    model.callbacks[name] = [](M1::Model &, const std::vector<M1::Argument> &) {
      return std::uint64_t{0};
    };
  const auto facing = static_cast<Raw>(static_cast<std::int32_t>(static_cast<std::int16_t>(record.angle)));
  try {
    switch (ported->cls) {
    case Class::SmallSpider:
      model.smallspider_ctor(c, type_ptr, record.position, facing, 0);
      break;
    case Class::Gnaw:
      model.gnaw_ctor(c, type_ptr, record.position, facing, 0);
      break;
    case Class::Mosquito:
      model.mosquito_ctor(c, record.position, facing, 0, type_ptr);
      break;
    case Class::Scorpion:
      model.scorpion_ctor(c, type_ptr, record.position, facing, 0);
      break;
    case Class::Badger: {
      model.badger_ctor(c, type_ptr, record.position, facing, 0,
                        outcome.test == SNOBADGE_TEST ? 1u : 0u);
      break;
    }
    case Class::Skelight:
      // Factory call 0x15b608, delay slot 0x15b60c: fifth argument = 1.
      // Executed call-site difftest: research 7a66b58 (factory-skelight).
      model.skelight_ctor(c, record.position, facing, 0, 1);
      break;
    case Class::Zek:
      model.zek_ctor(c, type_ptr, record.position, facing, 0);
      break;
    case Class::Horsemen: {
      // On-foot rows 0x532dc8..0x532df8 reach call 0x15b900 with variant 0..3
      // and mounted 0 (executed, research dfcc8d3, difftest_horsemen_factory.py).
      // The rider rows 0x532cc8..0x532cf8 reach 0x15b3d4 with mounted 1; that
      // mount lifecycle is not ported, so they stay refused below.
      if (outcome.test < HORSEMEN_FOOT_FIRST ||
          outcome.test > HORSEMEN_FOOT_FIRST + 0x30 ||
          (outcome.test - HORSEMEN_FOOT_FIRST) % 0x10)
        throw Refused("Horsemen factory variant outside executed rows");
      const auto variant = (outcome.test - HORSEMEN_FOOT_FIRST) / 0x10;
      model.horsemen_ctor(c, type_ptr, record.position, facing, 0, variant, 0);
      break;
    }
    case Class::None:
      return Build::NotBuilt;
    }
  } catch (const std::exception &error) {
    counts_.build_refusals[error.what()] += 1;
    if (!obj)
      return Build::NotBuilt;
    // GameObject_ctor ran and registered the object (it exists in the
    // original too); it stays a stub, with its draw made.
    stubs_.insert(obj->addr);
    counts_.objects_registered += 1;
    return Build::Stubbed;
  }
  if (!obj)
    throw std::logic_error("constructor did not call GameObject_ctor");
  // Copy the object's bytes and the creature serial into the binder.
  auto skipped = [&](std::uint32_t off) {
    return in_spans(off, kNotCopied, std::size(kNotCopied)) ||
           (!stats_ready_ && in_spans(off, kStats, std::size(kStats)));
  };
  for (std::uint32_t off = 0; off < outcome.size; ++off)
    if (!skipped(off))
      variables_->memory.byte(c + off, static_cast<std::uint8_t>(mem.load(c + off, 1)));
  variables_->memory.byte(CREATURE_SERIAL, static_cast<std::uint8_t>(mem.load(CREATURE_SERIAL, 1)));
  // The host-side copy of the flag word follows the built one.
  // The collision radius (+0x1c) is an immediate of each class constructor
  // (research data-tables.md:211,319); Player1's free-spot box query tests it
  // for every earlier object. Creature_init does not write it.
  static_cast<objects::GameObject &>(*obj).flags = variables_->memory.word(c + 0x0c);
  static_cast<objects::GameObject &>(*obj).radius = variables_->memory.word(c + 0x1c);
  mark_built(c);
  if (stats_ready_ && seed_gate_observed_ && !counts_.item_load_gate_refusals &&
      !counts_.load_draws_unmodelled) stats_built_.insert(c);
  counts_.objects_registered += 1;
  return Build::Built;
}

// WarBoar / WarBoarSmall (constructor 0x2ba03c; research t3/research-campaign-combat
// 328b072, handoff rounds 2-8). Order as in the original: GameObject_ctor (one
// shared draw), the executed base Creature_ctor with max-HP argument 10.0 and
// class definition 0x502f80, then the class part (creature_builder_). Every
// service the class part needs that the research does not establish makes the
// builder throw, which leaves the object a labelled stub with its draw made.
Host::Build Host::build_creature_family(const level_flow::Record &record,
                                        const level_factory::Outcome &outcome, std::uint32_t c,
                                        const std::vector<std::string> &props) {
  namespace AM = arena_monsters;
  constexpr std::uint32_t BOAR_DEFINITION = 0x502f80;
  constexpr Raw BOAR_MAX_HP_ARGUMENT = 0x41200000; // 10.0 (monsters-2.md WarBoar)
  if (!external_seed_) {
    counts_.build_refusals["no session seed"] += 1;
    return Build::NotBuilt;
  }
  if (!stats_ready_) {
    counts_.build_refusals["WarBoar needs monsters.bal and the level monster level"] += 1;
    return Build::NotBuilt;
  }
  if (variables_->memory.u32(0x4d8ae0) != 1 ||
      variables_->memory.u32(script_items::G_DIFFICULTY) != 1) {
    counts_.build_refusals["stats require one player at difficulty 1"] += 1;
    return Build::NotBuilt;
  }
  auto &exe = *exe_;
  M1::Memory mem({}, [&exe](creature_hp::Address a, std::size_t n) -> std::uint64_t {
    std::uint64_t v = 0;
    for (std::size_t i = n; i-- > 0;) {
      std::uint8_t b = 0;
      try {
        b = exe.byte(static_cast<std::uint32_t>(a + i));
      } catch (const std::exception &) {
        b = 0;
      }
      v = (v << 8) | b;
    }
    return v;
  });
  mem.poke(CREATURE_SERIAL, 1, variables_->memory.u8(CREATURE_SERIAL));
  mem.poke(0x4f714c, 4, monster_level_);
  for (std::size_t i = 0; i < level_name_.size(); ++i)
    mem.poke(0x44fe18 + static_cast<std::uint32_t>(i), 1, static_cast<unsigned char>(level_name_[i]));
  mem.poke(0x44fe18 + static_cast<std::uint32_t>(level_name_.size()), 1, 0);
  std::uint32_t scratch = SCRATCH;
  auto place = [&](const std::string &text) -> std::uint32_t {
    const auto at = scratch;
    if (at + text.size() + 1 >= SCRATCH_END)
      throw Refused("scratch strings exhausted");
    for (std::size_t i = 0; i < text.size(); ++i)
      mem.poke(at + static_cast<std::uint32_t>(i), 1, static_cast<unsigned char>(text[i]));
    mem.poke(at + static_cast<std::uint32_t>(text.size()), 1, 0);
    scratch = (at + static_cast<std::uint32_t>(text.size()) + 16) & ~15u;
    return at;
  };
  const auto type_ptr = place(record.cls);
  AM::Model model(mem);
  std::shared_ptr<builtins::Obj> obj;
  auto num = [](const AM::Value &v) { return std::uint32_t(std::get<std::int64_t>(v)); };
  auto prop_text = [&](std::uint32_t key) -> std::optional<std::string> {
    return level_factory::prop_find(&props, exe.string(key), exe);
  };
  model.callbacks["GameObject_ctor"] = [&](AM::Model &m, const std::vector<AM::Value> &a) -> AM::Value {
    const auto self = num(a.at(0));
    const auto pos = std::get<AM::Vec3>(a.at(1));
    const auto def = num(a.at(2));
    if (self != c)
      throw Refused("GameObject_ctor on another block");
    auto list = std::make_shared<std::vector<std::string>>(props);
    obj = variables_->new_object({}, 0, pos, 0, "7WarBoar", list, c, outcome.size);
    const std::pair<std::uint32_t, std::uint64_t> words[] = {
        {0x00, 0}, {0x04, 0}, {0x08, 0}, {0x48, VT_GAMEOBJECT}, {0x1c, RADIUS_24},
        {0x20, HEIGHT_72}, {0x0c, 0}, {0x10, pos[0]}, {0x14, pos[1]}, {0x18, pos[2]},
        {0x34, 0}, {0x24, def}};
    for (const auto &w : words)
      m.mem.store(self + w.first, 4, w.second);
    m.mem.store(self + 0x28, 1, 25);
    m.mem.store(self + 0x29, 1, 73);
    m.mem.store(self + 0x44, 4, external_seed_->draw());
    return std::int64_t(0);
  };
  model.callbacks["obj_getPropInt"] = [&](AM::Model &, const std::vector<AM::Value> &a) -> AM::Value {
    const auto text = prop_text(num(a.at(1)));
    return std::int64_t(text ? level_factory::atoi(*text, exe) : 0u);
  };
  model.callbacks["obj_getPropStr"] = [&](AM::Model &, const std::vector<AM::Value> &a) -> AM::Value {
    const auto text = prop_text(num(a.at(1)));
    return std::int64_t(text ? place(*text) : 0u);
  };
  model.callbacks["obj_moveOffGrid"] = [&](AM::Model &, const std::vector<AM::Value> &) -> AM::Value {
    if (!obj)
      throw Refused("grid call before GameObject_ctor");
    variables_->world->move_off_grid(*obj);
    return std::int64_t(0);
  };
  model.callbacks["balance_lookup"] = [&](AM::Model &, const std::vector<AM::Value> &) -> AM::Value {
    for (const auto &row : balance_rows_)
      if (row.hash == balance_hash(record.cls)) {
        AM::Balance b;
        b.found = 1;
        for (unsigned i = 0; i < 5; ++i)
          b.factors[i] = row.factors[i];
        return b;
      }
    return AM::Balance{};
  };
  // Creature_setStats 0x10a620: combat::monster_stats on the selected level's
  // table, with its own-generator HP draw first (as build()).
  model.callbacks["Creature_setStats"] = [&](AM::Model &m, const std::vector<AM::Value> &a) -> AM::Value {
    const auto self = num(a.at(0));
    const bool roll = num(a.at(1)) != 0;
    std::uint32_t draw = 0;
    if (roll) {
      const auto seed = static_cast<std::uint32_t>(m.mem.load(self + 0x44, 4)) * 0x19660du + 0x3c6ef35fu;
      m.mem.store(self + 0x44, 4, seed);
      draw = seed >> 16;
    }
    combat::Tables tables;
    tables.fixed_levels = {U"stomedal", U"innpit"};
    combat::World world;
    world.monster_level = static_cast<Raw>(m.mem.load(0x4f714c, 4));
    world.num_players = variables_->memory.u32(0x4d8ae0);
    world.difficulty = variables_->memory.u32(script_items::G_DIFFICULTY);
    world.medal_mission = variables_->memory.u32(0x4f713c);
    for (unsigned char ch : level_name_) world.level_name.push_back(ch);
    std::array<Raw, 5> factors{};
    for (unsigned i = 0; i < 5; ++i)
      factors[i] = num(a.at(i + 2));
    const auto result = combat::monster_stats(tables, world, factors, 0, std::u32string{}, roll, draw);
    if (result.hp) m.mem.store(self + 0x228, 4, *result.hp);
    if (result.max_hp) m.mem.store(self + 0x22c, 4, *result.max_hp);
    m.mem.store(self + 0xe4, 2, static_cast<std::uint16_t>(result.armor));
    m.mem.store(self + 0xd0, 2, static_cast<std::uint16_t>(result.dmg_low));
    m.mem.store(self + 0xd2, 2, static_cast<std::uint16_t>(result.dmg_high));
    m.mem.store(self + 0xd4, 2, static_cast<std::uint16_t>(result.attack));
    m.mem.store(self + 0xd8, 4, result.exp_value);
    return std::int64_t(0);
  };
  const auto facing = static_cast<Raw>(static_cast<std::int32_t>(static_cast<std::int16_t>(record.angle)));
  try {
    model.creature_ctor(c, type_ptr, record.position, facing, BOAR_DEFINITION, 0, BOAR_MAX_HP_ARGUMENT);
  } catch (const std::exception &error) {
    counts_.build_refusals[error.what()] += 1;
    if (!obj)
      return Build::NotBuilt;
    stubs_.insert(obj->addr);
    counts_.objects_registered += 1;
    return Build::Stubbed;
  }
  if (!obj)
    throw std::logic_error("constructor did not call GameObject_ctor");
  // Copy the base-constructed bytes into the shared memory. The fields the
  // base does not model (pointers into host scratch, the property list word)
  // are skipped exactly as for the other ported classes.
  for (std::uint32_t off = 0; off < outcome.size; ++off)
    if (!in_spans(off, kNotCopied, std::size(kNotCopied)))
      variables_->memory.byte(c + off, static_cast<std::uint8_t>(mem.load(c + off, 1)));
  variables_->memory.byte(CREATURE_SERIAL, static_cast<std::uint8_t>(mem.load(CREATURE_SERIAL, 1)));
  static_cast<objects::GameObject &>(*obj).flags = variables_->memory.word(c + 0x0c);
  static_cast<objects::GameObject &>(*obj).radius = variables_->memory.word(c + 0x1c);
  static_cast<objects::GameObject &>(*obj).seed = variables_->memory.word(c + 0x44);
  // Class part: the builder runs over the shared bytes. A refusal leaves the
  // object a labelled stub (GameObject_ctor ran; its draws are made).
  try {
    CreatureRequest request;
    request.class_name = record.cls;
    request.small = record.cls.compare(0, 12, "WarBoarSmall") == 0;
    request.address = c;
    request.angle = record.angle;
    request.object = obj.get();
    creature_builder_(request);
  } catch (const std::exception &error) {
    counts_.build_refusals[error.what()] += 1;
    stubs_.insert(obj->addr);
    counts_.objects_registered += 1;
    return Build::Stubbed;
  }
  mark_built(c);
  if (seed_gate_observed_ && !counts_.item_load_gate_refusals && !counts_.load_draws_unmodelled)
    stats_built_.insert(c);
  counts_.objects_registered += 1;
  return Build::Built;
}

// The mounted Horsemen and Nightmare rows, from research t3/research-horsemen-mount
// 338d459, docs/spec/horsemen-mount.md sections 1-2. The four addressed lists
// each contain miniboss=1 and a distinct name. One rider record consumes two
// pool blocks and two shared draws; a placed Nightmare consumes one of each.
// Mounted riders receive a fixed MiniBoss mount list; on-foot rows consume
// their own record properties and construct no mount.
Host::Build Host::build_mounted(const level_flow::Record &record, const level_factory::Outcome &outcome,
                                std::uint32_t c, const std::vector<std::string> &props) {
  namespace AM = arena_monsters;
  const bool rider = outcome.constructor() == HORSEMEN_CTOR;
  bool mounted = false;
  if (!external_seed_) {
    counts_.build_refusals["no session seed"] += 1;
    return Build::NotBuilt;
  }
  auto &exe = *exe_;
  std::uint32_t variant = 0;
  if (rider) {
    bool known = false;
    for (std::uint32_t k = 0; k < 4; ++k)
      if (outcome.test == RIDER_TESTS[k]) {
        variant = k;
        known = true;
        mounted = true;
      } else if (outcome.test == ON_FOOT_TESTS[k]) {
        variant = k;
        known = true;
      }
    if (!known) {
      counts_.build_refusals["rider row not recognised"] += 1;
      return Build::NotBuilt;
    }
  }
  constexpr const char *MOUNT_NAMES[] = {"horsedisease", "horsewar", "horsepestilence", "horsedeath"};
  M1::Memory mem({}, [&exe](creature_hp::Address a, std::size_t n) -> std::uint64_t {
    std::uint64_t v = 0;
    for (std::size_t i = n; i-- > 0;) {
      std::uint8_t b = 0;
      try {
        b = exe.byte(static_cast<std::uint32_t>(a + i));
      } catch (const std::exception &) {
        b = 0;
      }
      v = (v << 8) | b;
    }
    return v;
  });
  mem.poke(CREATURE_SERIAL, 1, variables_->memory.u8(CREATURE_SERIAL));
  std::uint32_t scratch = SCRATCH;
  auto place = [&](const std::string &text) -> std::uint32_t {
    const auto at = scratch;
    if (at + text.size() + 1 >= SCRATCH_END)
      throw Refused("scratch strings exhausted");
    for (std::size_t i = 0; i < text.size(); ++i)
      mem.poke(at + static_cast<std::uint32_t>(i), 1, static_cast<unsigned char>(text[i]));
    mem.poke(at + static_cast<std::uint32_t>(text.size()), 1, 0);
    scratch = (at + static_cast<std::uint32_t>(text.size()) + 16) & ~15u;
    return at;
  };
  const auto type_ptr = place(record.cls);
  AM::Model model(mem);
  model.nightmare_native = true;
  std::map<std::uint32_t, std::shared_ptr<builtins::Obj>> objects;
  std::uint32_t mount_block = 0;
  std::uint64_t draws = 0;
  auto is_rider_mount = [&](std::uint32_t self) { return rider && self == mount_block && mount_block; };
  auto prop_text = [&](std::uint32_t key) -> std::optional<std::string> {
    return level_factory::prop_find(&props, exe.string(key), exe);
  };
  auto num = [](const AM::Value &v) { return std::uint32_t(std::get<std::int64_t>(v)); };
  model.callbacks["pool_new"] = [&](AM::Model &, const std::vector<AM::Value> &a) -> AM::Value {
    if (num(a.at(0)) != MOUNT_BLOCK_SIZE)
      throw Refused("pool_new of an unexpected size");
    const auto block = variables_->world->pool.allocate(MOUNT_BLOCK_SIZE);
    if (!block) {
      counts_.objects_pool_refused += 1;
      return std::int64_t(0);
    }
    mount_block = block;
    return std::int64_t(block);
  };
  model.callbacks["GameObject_ctor"] = [&](AM::Model &m, const std::vector<AM::Value> &a) -> AM::Value {
    const auto self = num(a.at(0));
    const auto pos = std::get<AM::Vec3>(a.at(1));
    const auto def = num(a.at(2));
    if (self != c && self != mount_block)
      throw Refused("GameObject_ctor on another block");
    // Observed 12-byte lists at 0x500848 + 12*variant, each ending in null.
    const auto list_props = is_rider_mount(self)
        ? std::vector<std::string>{"miniboss=1", std::string("name=") + MOUNT_NAMES[variant]}
        : props;
    auto list = std::make_shared<std::vector<std::string>>(list_props);
    const bool is_rider_object = rider && self == c;
    const auto size = self == c ? outcome.size : MOUNT_BLOCK_SIZE;
    objects[self] = variables_->new_object({}, 0, pos, record.angle, is_rider_object ? "8Horsemen" : "9Nightmare", list, self, size);
    const std::pair<std::uint32_t, std::uint64_t> words[] = {
        {0x00, 0}, {0x04, 0}, {0x08, 0}, {0x48, VT_GAMEOBJECT}, {0x1c, RADIUS_24},
        {0x20, HEIGHT_72}, {0x0c, 0}, {0x10, pos[0]}, {0x14, pos[1]}, {0x18, pos[2]},
        {0x34, 0}, {0x24, def}};
    for (const auto &w : words)
      m.mem.store(self + w.first, 4, w.second);
    m.mem.store(self + 0x28, 1, 25);
    m.mem.store(self + 0x29, 1, 73);
    m.mem.store(self + 0x44, 4, external_seed_->draw());
    ++draws;
    if ((mounted && draws == 2) || (!mounted && draws == 1))
      last_draws_complete_ = true;
    return std::int64_t(0);
  };
  model.callbacks["obj_getPropInt"] = [&](AM::Model &, const std::vector<AM::Value> &a) -> AM::Value {
    if (is_rider_mount(num(a.at(0))))
      return std::int64_t(num(a.at(1)) == 0x52fa60 ? 1 : 0); // MiniBoss key in Creature_ctor.
    const auto text = prop_text(num(a.at(1)));
    return std::int64_t(text ? level_factory::atoi(*text, exe) : 0u);
  };
  model.callbacks["obj_getPropStr"] = [&](AM::Model &, const std::vector<AM::Value> &a) -> AM::Value {
    if (is_rider_mount(num(a.at(0))))
      return std::int64_t(exe.string(num(a.at(1))) == "name" ? place(MOUNT_NAMES[variant]) : 0u);
    const auto text = prop_text(num(a.at(1)));
    return std::int64_t(text ? place(*text) : 0u);
  };
  // As build(): the balance row only feeds Creature_setStats, whose results are not copied.
  model.callbacks["balance_lookup"] = [](AM::Model &, const std::vector<AM::Value> &) -> AM::Value {
    return AM::Balance{};
  };
  model.callbacks["Creature_setStats"] = [&](AM::Model &m, const std::vector<AM::Value> &a) -> AM::Value {
    const auto self = num(a.at(0));
    if (num(a.at(1))) {
      const auto seed = static_cast<std::uint32_t>(m.mem.load(self + 0x44, 4)) * 0x19660du + 0x3c6ef35fu;
      m.mem.store(self + 0x44, 4, seed);
    }
    return std::int64_t(0);
  };
  model.callbacks["Creature_init"] = [&](AM::Model &m, const std::vector<AM::Value> &a) -> AM::Value {
    const auto self = num(a.at(0));
    if (!is_rider_mount(self) && prop_text(KEY_DEAD)) throw Refused("Creature_init dead");
    if (!is_rider_mount(self) && prop_text(KEY_INIT_SCRIPT)) throw Refused("Creature_init initScript");
    // Creature_init first calls virtual slot 18 for a MiniBoss. Nightmare
    // inherits the documented empty Creature slot 18 (docs/creatures.md).
    std::uint32_t mask = 0;
    const auto format = exe.string(KEY_CHANGE);
    const auto at = format.find("%d");
    if (at == std::string::npos)
      throw Refused("change key format");
    for (int n = 0; n < 32; ++n) {
      auto key = format;
      key.replace(at, 2, std::to_string(n));
      if (!is_rider_mount(self) && level_factory::prop_find(&props, key, exe))
        mask |= 1u << n;
    }
    if (mask)
      m.mem.store(self + 0xe8, 4, mask);
    if (!is_rider_mount(self) && m.mem.load(self + 0x50, 4))
      for (const auto key : KEY_WEAPONS)
        if (const auto text = level_factory::prop_find(&props, exe.string(key), exe))
          if (level_factory::atoi(*text, exe))
            throw Refused("Creature_init weapon element");
    return std::int64_t(0);
  };
  auto object_of = [&](const std::vector<AM::Value> &a) -> builtins::Obj & {
    const auto it = objects.find(num(a.at(0)));
    if (it == objects.end())
      throw Refused("run list before GameObject_ctor");
    return *it->second;
  };
  model.callbacks["obj_runEvery60"] = [&](AM::Model &, const std::vector<AM::Value> &a) -> AM::Value {
    variables_->world->run_every_60(object_of(a));
    return std::int64_t(0);
  };
  model.callbacks["handle_clear"] = [&](AM::Model &m, const std::vector<AM::Value> &a) -> AM::Value {
    if (m.mem.load(num(a.at(0)), 4))
      throw Refused("handle_clear on a list with nodes");
    return std::int64_t(0);
  };
  model.callbacks["handle_link"] = [&](AM::Model &m, const std::vector<AM::Value> &a) -> AM::Value {
    const auto head = num(a.at(0)), target = num(a.at(1));
    if (!rider || (head != c + 0x3c8 && head != mount_block + 0x3c8) ||
        (target != c && target != mount_block) || num(a.at(2)) != target)
      throw Refused("rider-mount handle_link outside observed pair");
    auto found = objects.find(target);
    if (found == objects.end()) throw Refused("rider-mount handle_link target absent");
    auto &cell = mounted_cells_[head];
    if (!cell) cell = std::make_unique<Cell>();
    auto *node = variables_->world->handles.link(*cell, found->second->heads[0], found->second.get());
    const auto word = HandlePool::NODE_BASE + HandlePool::NODE_SIZE * static_cast<std::uint32_t>(node->index);
    m.mem.store(head, 4, word);
    m.mem.store(word + 12, 4, target);
    variables_->memory.word(word + 12, target);
    return std::int64_t(0);
  };
  // Resource and animation calls: their results are not stored by these constructors.
  for (const char *name : {"anim_clear", "anim_push", "anim_randomizeTopTime", "anim_start", "anim_startBlendInOut",
                           "anim_syncToSimFrame", "anim_pushChained", "model_buildHideMask", "lump_load",
                           // Holder.v1 puts an item model in a hand (research arena EXTERNALS): held-part
                           // presentation. Rider records are refused later and nothing is copied.
                           "Holder.v1", "Holder.v2", "Holder.v4", "Holder.v6"})
    model.callbacks[name] = [](AM::Model &, const std::vector<AM::Value> &) -> AM::Value { return std::int64_t(0); };
  const auto facing = static_cast<Raw>(static_cast<std::int32_t>(static_cast<std::int16_t>(record.angle)));
  try {
    if (rider)
      model.horsemen_ctor(c, type_ptr, record.position, facing, 0, variant, mounted ? 1 : 0);
    else
      model.nightmare_ctor(c, type_ptr, record.position, facing, 0);
  } catch (const std::exception &error) {
    counts_.build_refusals[error.what()] += 1;
    if (objects.empty())
      return Build::NotBuilt;
    // Each constructed object registered and drew once; they stay stubs.
    for (const auto &kv : objects) {
      stubs_.insert(kv.second->addr);
      counts_.objects_registered += 1;
    }
    return Build::Stubbed;
  }
  // Failed mount allocation leaves an on-foot rider in state 0 (spec 1.1).
  // Keep that partial constructor as a labelled stub; never abort level load.
  if (mounted && !mount_block) {
    last_draws_complete_ = draws == 1;
    counts_.build_refusals["rider on foot (mount pool exhausted)"] += 1;
    stubs_.insert(objects.at(c)->addr);
    counts_.objects_registered += 1;
    return Build::Stubbed;
  }
  // Copy both completed objects (or the placed Nightmare) into the binder.
  auto skipped = [](std::uint32_t off) {
    for (const auto &sp : kNotCopied)
      if (off >= sp.offset && off < sp.offset + sp.size)
        return true;
    return false;
  };
  for (const auto &entry : objects) {
    const auto self = entry.first;
    const auto size = self == c ? outcome.size : MOUNT_BLOCK_SIZE;
    for (std::uint32_t off = 0; off < size; ++off)
      if (!skipped(off))
        variables_->memory.byte(self + off, static_cast<std::uint8_t>(mem.load(self + off, 1)));
    static_cast<objects::GameObject &>(*entry.second).flags = variables_->memory.word(self + 0x0c);
    mark_built(self);
  }
  variables_->memory.byte(CREATURE_SERIAL, static_cast<std::uint8_t>(mem.load(CREATURE_SERIAL, 1)));
  counts_.objects_registered += objects.size();
  return Build::Built;
}

} // namespace champions::simulation::rta::script_commands
