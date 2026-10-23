// Level objects for the script host: the object factory, and the creature
// classes whose constructor chain is ported (research t3/research-script-session
// 6841b8a, docs/script-session.md sections 3-5).
#include <cctype>
#include "champions/monsters_1.hpp"
#include "champions/script_commands.hpp"
#include <cstring>
#include <limits>
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
// Object fields that hold such a pointer, or that unported code writes
// (Creature_setStats: hit points, maximum, the four statistics and the
// experience value; Creature_init: height and draw byte from the model's
// bounds; the hide mask; the property list pointer). Not copied.
struct Span {
  std::uint32_t offset, size;
};
const Span kNotCopied[] = {
    {0x2c, 4},  {0x34, 4},  {0x368, 4}, {0x228, 4}, {0x22c, 4}, {0xd0, 2}, {0xd2, 2},
    {0xd4, 2},  {0xd8, 4},  {0xe4, 2},  {0x20, 4},  {0x29, 1},  {0x220, 8},
};
// Creature_init 0x10a230 (research tools/ref_creature_init.py).
constexpr std::uint32_t KEY_DEAD = 0x52fac8, KEY_CHANGE = 0x52fad0, KEY_INIT_SCRIPT = 0x52fb58;
constexpr std::uint32_t KEY_WEAPONS[] = {0x52fae0, 0x52faf0, 0x52fb08, 0x52fb18,
                                         0x52fb28, 0x52fb38, 0x52fb48};

// The ported constructors (research docs/spec/monsters-1.md, native
// monsters_1). Badger takes the variant the factory row passes: 1 for the
// SnoBadge row, 0 for the Badger prefix (0x15a4bc, 0x15a504). The type name is
// the class's C++ type-info name (vtables.txt), as the binder compares it.
enum class Class : std::uint8_t { None, SmallSpider, Gnaw, Mosquito, Scorpion, Badger };
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
};
// The SnoBadge equality test (level_factory_table.inc row 39; factory block
// 0x15a494 passes variant 1). The Badger prefix row passes 0.
constexpr std::uint32_t SNOBADGE_TEST = 0x532a30;

struct Refused : std::runtime_error {
  using std::runtime_error::runtime_error;
};

bool live_creature_hp(std::uint32_t raw) {
  float hp;
  static_assert(sizeof(hp) == sizeof(raw), "Creature HP is a 32-bit float");
  std::memcpy(&hp, &raw, sizeof(hp));
  return hp > 0.0f;
}

} // namespace

std::vector<level_object_state::Record> Host::save_built_object_fields() const {
  std::vector<level_object_state::Record> result;
  auto &m = variables_->memory;
  auto put = [](level_object_state::Bytes &out, std::uint32_t word) {
    for (unsigned i = 0; i != 4; ++i) out.push_back(std::uint8_t(word >> (8 * i)));
  };
  constexpr std::uint64_t masks[5] = {0x1000000ull, 0x100000000ull,
                                      0x400000ull, 0x800000ull,
                                      0x2000000000000ull};
  for (const auto &built : built_records_) {
    const auto live = variables_->world->objects.find(built.address);
    if (live == variables_->world->objects.end() || !live->second || !live->second->alive)
      throw std::runtime_error("deleted built-object save policy is unverified");
    level_object_state::Record record;
    record.ordinal = built.ordinal;
    record.cls = built.cls;
    record.position = built.position;
    record.angle = built.angle;
    const auto c = built.address;
    if (!live_creature_hp(m.word(c + 0x228)))
      throw std::runtime_error("dead built-Creature save state is unverified");
    put(record.fields, m.word(c + 0xe8));
    put(record.fields, m.word(c + 0x228));
    put(record.fields, m.word(c + 0x22c));
    const auto raw_flags = std::uint64_t(m.word(c + 0xa8)) |
                           (std::uint64_t(m.word(c + 0xac)) << 32);
    std::uint32_t bits = 0;
    for (unsigned i = 0; i != 5; ++i)
      if (raw_flags & masks[i]) bits |= 1u << i;
    if (m.u8(c + 0x231) & 1u) bits |= 1u << 5;
    if (m.s16(c + 0x312)) bits |= 1u << 6;
    put(record.fields, bits);
    if (bits & (1u << 6)) put(record.fields, std::uint32_t(m.s16(c + 0x312)));
    result.push_back(std::move(record));
  }
  return result;
}

std::size_t Host::restore_built_object_fields(
    const std::vector<level_object_state::Record> &records) {
  // Validate the complete matching set before changing any memory. Missing
  // constructors remain an explicit refusal, not a fabricated object.
  if (records.size() != built_records_.size())
    throw std::runtime_error("saved built-object count differs from the loaded level");
  for (std::size_t i = 0; i != records.size(); ++i) {
    const auto &r = records[i];
    const auto &b = built_records_[i];
    if (r.ordinal != b.ordinal || r.cls != b.cls ||
        r.position != b.position || r.angle != b.angle ||
        (r.fields.size() != 16 && r.fields.size() != 20))
      throw std::runtime_error("saved built-object identity or field layout differs");
    const auto bits = std::uint32_t(r.fields[12]) |
                      (std::uint32_t(r.fields[13]) << 8) |
                      (std::uint32_t(r.fields[14]) << 16) |
                      (std::uint32_t(r.fields[15]) << 24);
    if (bits & ~0x7fu || ((bits & 0x40u) != 0) != (r.fields.size() == 20))
      throw std::runtime_error("invalid saved Creature flag fields");
    if (!live_creature_hp(std::uint32_t(r.fields[4]) |
                          (std::uint32_t(r.fields[5]) << 8) |
                          (std::uint32_t(r.fields[6]) << 16) |
                          (std::uint32_t(r.fields[7]) << 24)))
      throw std::runtime_error("dead built-Creature restore state is unverified");
    if (r.fields.size() == 20) {
      const auto charm = std::uint16_t(r.fields[16]) | (std::uint16_t(r.fields[17]) << 8);
      const auto sign = (charm & 0x8000u) ? 0xffu : 0u;
      if (!charm || r.fields[18] != sign || r.fields[19] != sign)
        throw std::runtime_error("invalid saved Creature charm timer");
    }
  }
  auto &m = variables_->memory;
  auto word = [](const level_object_state::Bytes &v, std::size_t at) {
    return std::uint32_t(v[at]) | (std::uint32_t(v[at + 1]) << 8) |
           (std::uint32_t(v[at + 2]) << 16) | (std::uint32_t(v[at + 3]) << 24);
  };
  constexpr std::uint64_t masks[5] = {0x1000000ull, 0x100000000ull,
                                      0x400000ull, 0x800000ull,
                                      0x2000000000000ull};
  for (std::size_t i = 0; i != records.size(); ++i) {
    const auto c = built_records_[i].address;
    const auto &v = records[i].fields;
    m.word(c + 0xe8, word(v, 0));
    m.word(c + 0x228, word(v, 4));
    m.word(c + 0x22c, word(v, 8));
    const auto bits = word(v, 12);
    auto raw_flags = std::uint64_t(m.word(c + 0xa8)) |
                     (std::uint64_t(m.word(c + 0xac)) << 32);
    for (unsigned bit = 0; bit != 5; ++bit)
      raw_flags = (raw_flags & ~masks[bit]) | ((bits & (1u << bit)) ? masks[bit] : 0);
    m.word(c + 0xa8, std::uint32_t(raw_flags));
    m.word(c + 0xac, std::uint32_t(raw_flags >> 32));
    m.byte(c + 0x231, std::uint8_t((m.u8(c + 0x231) & ~1u) | ((bits >> 5) & 1u)));
    m.w16(c + 0x312, bits & 0x40u ? word(v, 16) : 0);
  }
  return records.size();
}

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
  for (std::size_t ordinal = 0; ordinal < records.size(); ++ordinal) {
    const auto &record = records[ordinal];
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
    if (result == Build::Built) {
      counts_.objects_built += 1;
      built_records_.push_back({std::uint32_t(ordinal), address, record.cls,
                                record.position, record.angle});
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
  model.balance_lookup = [](M1::Model &, Raw) { return M1::Model::Balance{}; };
  // Creature_setStats 0x10a620 (docs/data-tables.md 3.2): its results are not
  // copied (kNotCopied); with rollHp it makes one draw of the object's own
  // generator, which later own draws continue.
  model.callbacks["Creature_setStats"] = [&](M1::Model &m, const std::vector<M1::Argument> &a) {
    const auto self = std::get<std::uint32_t>(a.at(0));
    if (std::get<std::uint32_t>(a.at(1))) {
      const auto seed = static_cast<std::uint32_t>(m.mem.load(self + 0x44, 4)) * 0x19660du + 0x3c6ef35fu;
      m.mem.store(self + 0x44, 4, seed);
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
  auto skipped = [](std::uint32_t off) {
    for (const auto &s : kNotCopied)
      if (off >= s.offset && off < s.offset + s.size)
        return true;
    return false;
  };
  for (std::uint32_t off = 0; off < outcome.size; ++off)
    if (!skipped(off))
      variables_->memory.byte(c + off, static_cast<std::uint8_t>(mem.load(c + off, 1)));
  variables_->memory.byte(CREATURE_SERIAL, static_cast<std::uint8_t>(mem.load(CREATURE_SERIAL, 1)));
  // The host-side copy of the flag word follows the built one.
  static_cast<objects::GameObject &>(*obj).flags = variables_->memory.word(c + 0x0c);
  built_.insert(c);
  counts_.objects_registered += 1;
  return Build::Built;
}

} // namespace champions::simulation::rta::script_commands
