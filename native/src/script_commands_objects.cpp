// Level objects for the script host: the object factory, and the creature
// classes whose constructor chain is ported (research t3/research-script-session
// 6841b8a, docs/script-session.md sections 3-5).
#include "champions/arena_monsters.hpp"
#include <cctype>
#include "champions/monsters_1.hpp"
#include "champions/script_commands.hpp"
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
// Horsemen_ctor and Nightmare_ctor (research docs/spec/horsemen-mount.md section 1). The rider
// rows' name tests give the variant: 0x532cc8, 0x532cd8, 0x532ce8, 0x532cf8 -> 0..3.
constexpr std::uint32_t HORSEMEN_CTOR = 0x271c70, NIGHTMARE_CTOR = 0x270968;
constexpr std::uint32_t RIDER_TESTS[] = {0x532cc8, 0x532cd8, 0x532ce8, 0x532cf8};
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
      continue;
    }
    if (result == Build::NotBuilt)
      stub(record, address, props, outcome.size);
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
  // Only the four rider rows the research documents (mounted 1) and the Nightmare row. The
  // other rows that call the same constructor (0x532dc8..0x532df8) are not documented there.
  bool rider_row = false;
  for (const auto t : RIDER_TESTS)
    rider_row = rider_row || outcome.test == t;
  if ((outcome.constructor() == HORSEMEN_CTOR && rider_row) || outcome.constructor() == NIGHTMARE_CTOR)
    return build_mounted(record, outcome, c, props);
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

// The mounted Horsemen and Nightmare rows, from research t3/research-horsemen-mount
// 338d459, docs/spec/horsemen-mount.md sections 1-2. The four addressed lists
// each contain miniboss=1 and a distinct name. One rider record consumes two
// pool blocks and two shared draws; a placed Nightmare consumes one of each.
Host::Build Host::build_mounted(const level_flow::Record &record, const level_factory::Outcome &outcome,
                                std::uint32_t c, const std::vector<std::string> &props) {
  namespace AM = arena_monsters;
  const bool rider = outcome.constructor() == HORSEMEN_CTOR;
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
    if ((rider && draws == 2) || (!rider && draws == 1))
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
      model.horsemen_ctor(c, type_ptr, record.position, facing, 0, variant, 1);
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
  if (rider && !mount_block) {
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
    built_.insert(self);
  }
  variables_->memory.byte(CREATURE_SERIAL, static_cast<std::uint8_t>(mem.load(CREATURE_SERIAL, 1)));
  counts_.objects_registered += objects.size();
  return Build::Built;
}

} // namespace champions::simulation::rta::script_commands
