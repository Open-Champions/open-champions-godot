// Level objects for the script host: the object factory, and the creature
// classes whose constructor chain is ported (research t3/research-script-session
// 6841b8a, docs/script-session.md sections 3-5).
#include "champions/monsters_1.hpp"
#include "champions/combat_stats.hpp"
#include "champions/script_commands.hpp"
#include "champions/terrain_file.hpp"
#include <cstring>
#include <stdexcept>
#include <utility>

namespace champions::simulation::rta::script_commands {
namespace {
namespace M1 = monsters_1;
// GameObject_ctor 0x14cb10 (research docs/spec/object-system.md 2.2).
constexpr std::uint32_t VT_GAMEOBJECT = 0x5122a8;
constexpr std::uint32_t RADIUS_24 = 0x41c00000, HEIGHT_72 = 0x42900000;
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
  const auto request = level_flow::split_request(name);
  const auto row = level_flow::find_level_row(rows, request.first, request.second);
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
  level_name_ = name;
  stats_ready_ = true;
}

void Host::set_level_world(const std::uint8_t *data, std::size_t size) {
  if (!data || !size) throw std::invalid_argument("empty level WORLD");
  terrain_level_ = std::make_unique<world_queries::Level>(
      terrain_file::decode(std::vector<std::uint8_t>(data, data + size)));
}

void Host::advance_first_level_items() {
  if (!external_seed_) throw std::invalid_argument("item loader needs session seed");
  // item_loadResources parses every base definition with variant -1. The
  // executed seed-side contract is exactly 223 shared draws for this build.
  for (unsigned i = 0; i < 223; ++i) external_seed_->draw();
}

void Host::start_single_player() {
  // docs/script-session.md section 2.
  variables_->memory.word(0x4d8ae0, 1u);
  // Difficulty is initialized once by Host from script_items::DIFFICULTY_IMAGE.
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
  for (const auto &record : records) {
    // GameObject_ctor step 7: name= values in double quotes are unquoted in place.
    std::vector<std::string> props = record.props;
    for (auto &entry : props)
      if (entry.compare(0, 5, "name=") == 0)
        entry = "name=" + objects::strip_quotes(entry.substr(5));
    const auto outcome = level_factory::classify(record.cls, &record.props, *exe_);
    if (outcome.kind != level_factory::Kind::Object) {
      counts_.objects_not_created += 1;
      continue;
    }
    const auto address = variables_->world->pool.allocate(outcome.size);
    if (!address) {
      counts_.objects_pool_refused += 1;
      continue;
    }
    const auto result = build(record, outcome, address, props);
    if (result == Build::Built) {
      counts_.objects_built += 1;
      continue;
    }
    if (result == Build::NotBuilt)
      stub(record, address, props, outcome.size);
    counts_.objects_unbuilt += 1;
    counts_.load_draws_unmodelled += 1;
    counts_.unbuilt_constructors[outcome.constructor()] += 1;
  }
  return true;
}

Host::Build Host::build(const level_flow::Record &record, const level_factory::Outcome &outcome,
                        std::uint32_t c, const std::vector<std::string> &props) {
  if (outcome.constructor() == 0x158928) {
    if (!external_seed_ || !terrain_level_) {
      counts_.build_refusals["Player1 needs session seed and WORLD"] += 1;
      return Build::NotBuilt;
    }
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
    int rank = world.start_rank;
    if (flow_.loaded_from_save) rank = 3;
    bool take = false;
    if (rank != 3) {
      auto key = [&](const char *name) { return level_factory::prop_find(&props, name, *exe_); };
      if (rank <= 0 || (rank == 1 && key("default") &&
                        level_factory::atoi(*key("default"), *exe_) != 0)) {
        take = true; rank = 1;
      }
      if (const auto from = key("from")) {
        if (!flow_.arrived_by_recall && *from == flow_.prev) { take = true; rank = 2; }
        else if (flow_.arrived_by_recall && *from == "gate") { take = true; rank = 3; }
      }
      if (!flow_.arrived_by_recall)
        if (const auto name = key("name"))
          if (*name == flow_.entrance) { take = true; rank = 3; }
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
    built_.insert(c);
    counts_.objects_registered += 1;
    return Build::Built;
    } catch (const std::exception &error) {
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
      // The MiniBoss branch is refused before this point. Named boss rank 2
      // changes only the multiple-player damage rule; this host starts one
      // player. No constructor here has a Dead property.
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
  // Holder slot 1, executed and difftested on both concrete tables in
  // t3/research-creature-stats (3469927). The holder is embedded in c.
  model.callbacks["Holder.v1"] = [&](M1::Model &m, const std::vector<M1::Argument> &a) {
    const auto self = std::get<std::uint32_t>(a.at(0));
    const auto hand = std::get<std::uint32_t>(a.at(1));
    const auto entry = std::get<std::uint32_t>(a.at(2)) & 0xffu;
    const auto table = m.mem.load(self, 4);
    const bool shield = table == 0x50cda0;
    if (hand != 1 && !(shield && hand == 2)) return std::uint64_t{1};
    m.mem.store(self + (hand == 1 ? 8u : 0x98u), 1, entry);
    auto flags = static_cast<std::uint32_t>(m.mem.load(self + 4, 1));
    const auto bit = hand == 1 ? 1u : 2u;
    flags = entry ? flags | bit : flags & ~bit;
    m.mem.store(self + 4, 1, flags);
    return std::uint64_t{flags};
  };
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
      model.skelight_ctor(c, record.position, facing, 0, 1);
      break;
    case Class::Zek:
      model.zek_ctor(c, type_ptr, record.position, facing, 0);
      break;
    case Class::Horsemen: {
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
  static_cast<objects::GameObject &>(*obj).flags = variables_->memory.word(c + 0x0c);
  built_.insert(c);
  if (stats_ready_) stats_built_.insert(c);
  counts_.objects_registered += 1;
  return Build::Built;
}

} // namespace champions::simulation::rta::script_commands
