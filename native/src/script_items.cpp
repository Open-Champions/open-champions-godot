#include "champions/script_items.hpp"
#include "champions/messages.hpp"
#include "champions/core_math.hpp"
#include "champions/item_generation.hpp"
#include "champions/objects.hpp"
#include <cstring>
#include <stdexcept>

namespace champions::simulation::rta::script_items {
namespace {
namespace msg = champions::messages::rta;
using builtins::Variables;

constexpr std::uint32_t CSTR_LIMIT = 0x4000;
// ref_items (research-items 49bc578 tools/ref_items.py): the table addresses
// item_parseDescriptor and item_getWeight read.
constexpr std::uint32_t A_PREFIXES = 0x400e50, A_SUFFIXES = 0x4019e0;
constexpr std::uint32_t A_GOLD_SCALE_MP = 0x520048;
constexpr std::uint32_t S_ARROWS = 0x520040, S_NAME_SUFFIX = 0x520058, S_SPACE = 0x520060,
                        S_OF_KEY = 0x520068, S_THROWN_STEM = 0x520070, S_POTION_STEM = 0x520180,
                        S_CIRCLET_STEM = 0x520188, S_RECALL = 0x52b798, S_CIRCLET_STEM_EQUIP = 0x5385e8;
constexpr std::array<std::pair<std::uint32_t, int>, 43> TOKEN_FORMATS = {{
    {0x520190, 0x23}, {0x5201a8, 0x1e}, {0x5201c0, 0x1d}, {0x5201d0, 0x1c}, {0x5201e0, 0x1b},
    {0x5201f0, 0x1a}, {0x520200, 0x19}, {0x520210, 0x15}, {0x520220, 0x16}, {0x520230, 0x18},
    {0x520248, 0x17}, {0x520260, 0x14}, {0x520270, 0x0e}, {0x520280, 0x0f}, {0x520298, 0x10},
    {0x5202b0, 0x11}, {0x5202c8, 0x12}, {0x5202e0, 0x13}, {0x5202f0, 0x0d}, {0x520300, 0x0b},
    {0x520318, 0x0c}, {0x520330, 0x05}, {0x520340, 0x06}, {0x520350, 0x07}, {0x520360, 0x08},
    {0x520378, 0x09}, {0x520390, 0x0a}, {0x5203a0, 0x00}, {0x5203b0, 0x01}, {0x5203c0, 0x02},
    {0x5203d0, 0x03}, {0x5203e0, 0x04}, {0x5203f0, 0x21}, {0x520408, 0x1f}, {0x520420, 0x20},
    {0x520438, 0x22}, {0x520450, 0x24}, {0x520468, 0x25}, {0x520478, 0x27}, {0x520488, 0x28},
    {0x5204a0, 0x2a}, {0x5204b0, -1}, {0x5204b8, 0x26}}};
constexpr std::size_t TABLE_ROW_LIMIT = 65536;

std::uint32_t word_arg(const std::vector<Variables::InventoryValue> &args, std::size_t k, const char *what) {
  if (k >= args.size())
    throw Refused(std::string(what) + ": argument count");
  if (const auto *word = std::get_if<std::uint32_t>(&args[k]))
    return *word;
  throw Refused(std::string(what) + ": argument is not a word");
}
std::string text_arg(const std::vector<Variables::InventoryValue> &args, std::size_t k, const char *what) {
  if (k >= args.size())
    throw Refused(std::string(what) + ": argument count");
  if (const auto *text = std::get_if<std::string>(&args[k]))
    return *text;
  throw Refused(std::string(what) + ": argument is not a string");
}
std::int32_t sword(std::uint32_t v) { return static_cast<std::int32_t>(v); }

// The handler's stack record served from the service, everything else from
// the binder memory.
class ScratchView : public View {
public:
  ScratchView(builtins::Memory &memory, std::array<std::uint8_t, ITEM_SIZE> &scratch)
      : binder_(memory), scratch_(scratch) {}
  std::uint64_t load(std::uint32_t a, unsigned n) const override {
    std::uint64_t v = 0;
    for (unsigned i = 0; i < n; ++i)
      v |= std::uint64_t(byte(a + i)) << (8 * i);
    return v;
  }
  void store(std::uint32_t a, unsigned n, std::uint64_t v) override {
    for (unsigned i = 0; i < n; ++i) {
      const auto at = a + i;
      if (inside(at))
        scratch_[at - Service::SCRATCH] = std::uint8_t(v >> (8 * i));
      else
        binder_.store(at, 1, (v >> (8 * i)) & 0xff);
    }
  }

private:
  static bool inside(std::uint32_t a) { return a >= Service::SCRATCH && a - Service::SCRATCH < ITEM_SIZE; }
  std::uint8_t byte(std::uint32_t a) const {
    return inside(a) ? scratch_[a - Service::SCRATCH] : std::uint8_t(binder_.load(a, 1));
  }
  BinderView binder_;
  std::array<std::uint8_t, ITEM_SIZE> &scratch_;
};

// messages::rta::Memory as a View; its writes are flushed to the binder after
// the machine call.
class OverlayView : public View {
public:
  explicit OverlayView(msg::Memory &memory) : memory_(memory) {}
  std::uint64_t load(std::uint32_t a, unsigned n) const override { return memory_.load(a, n); }
  void store(std::uint32_t a, unsigned n, std::uint64_t v) override { memory_.store(a, n, v); }

private:
  msg::Memory &memory_;
};
} // namespace

std::uint32_t rec(std::int32_t slot) { return G_PLAYERS + static_cast<std::uint32_t>(slot) * REC_SIZE; }
std::uint32_t bag_entry(std::int32_t slot, std::int32_t bag, std::int32_t index) {
  return rec(slot) + P_BAGS + static_cast<std::uint32_t>(bag) * BAG_SIZE + static_cast<std::uint32_t>(index) * ITEM_SIZE;
}
std::uint32_t bag_count_at(std::int32_t slot, std::int32_t bag) {
  return rec(slot) + P_BAGS + static_cast<std::uint32_t>(bag) * BAG_SIZE + BAG_COUNT;
}

std::int32_t View::s8(std::uint32_t a) const { return items::s8(std::uint32_t(load(a, 1))); }
std::int32_t View::s16(std::uint32_t a) const { return items::s16(std::uint32_t(load(a, 2))); }
std::int32_t View::s32(std::uint32_t a) const { return items::s32(std::uint32_t(load(a, 4))); }
std::string View::cstr(std::uint32_t address) const {
  std::string out;
  while (out.size() < CSTR_LIMIT) {
    const auto b = static_cast<char>(load(address + static_cast<std::uint32_t>(out.size()), 1));
    if (b == 0)
      return out;
    out.push_back(b);
  }
  throw Refused("string has no terminator");
}
std::uint64_t BinderView::load(std::uint32_t a, unsigned n) const {
  std::uint64_t v = 0;
  for (unsigned i = 0; i < n; ++i)
    v |= std::uint64_t(memory_.u8(a + i)) << (8 * i);
  return v;
}
void BinderView::store(std::uint32_t a, unsigned n, std::uint64_t v) { memory_.store(a, n, v); }

// ---- looking ------------------------------------------------------------------------------
std::int32_t count_item_by_name(const View &v, const std::string &name, std::int32_t slot) {
  std::uint32_t total = 0;
  for (std::int32_t bag = 0; bag < BAGS; ++bag)
    for (std::int32_t i = 0; i < v.s32(bag_count_at(slot, bag)); ++i) {
      const auto entry = bag_entry(slot, bag, i);
      const auto definition = std::uint32_t(v.load(entry, 4));
      if (v.cstr(std::uint32_t(v.load(definition, 4))) == name)
        total += static_cast<std::uint32_t>(v.s16(entry + 4));
    }
  return sword(total);
}
Found find_item_by_substring(const View &v, std::int32_t slot, const std::string &text) {
  Found found;
  for (std::int32_t bag = 0; bag < BAGS; ++bag)
    for (std::int32_t i = 0; i < v.s32(bag_count_at(slot, bag)); ++i) {
      const auto entry = bag_entry(slot, bag, i);
      const auto name = v.cstr(std::uint32_t(v.load(std::uint32_t(v.load(entry, 4)), 4)));
      if (text.find(name) != std::string::npos) {
        found.index = i;
        found.bag = bag;
      }
    }
  return found;
}
std::int32_t passive_level(const View &v, std::int32_t slot, std::uint64_t mask) {
  if (static_cast<std::uint32_t>(slot) >= 5)
    return 0;
  const auto r = rec(slot);
  const auto table = G_CLASS_SKILLS + static_cast<std::uint32_t>(v.s32(r + P_CLASS)) * CLASS_SKILLS_SIZE;
  for (std::uint32_t row = 0; row < 48; ++row)
    if (v.load(table + row * CLASS_SKILL_ROW, 8) == mask) {
      const auto level = v.s8(r + P_PASSIVES + row);
      return level >= 0 ? level : 0;
    }
  return 0;
}
std::int32_t skill_rank(const View &v, std::int32_t slot, std::int32_t skill) {
  const auto r = rec(slot);
  for (std::uint32_t at = r + P_SKILL_PAIRS; at - r < REC_SIZE; at += 4) {
    const auto ident = v.s16(at);
    if (ident == -1)
      return 0;
    if (ident == items::s16(static_cast<std::uint32_t>(skill)))
      return v.s16(at + 2);
  }
  throw Refused("player_getSkillRank walked past the record without an id -1");
}

// ---- weight and load ------------------------------------------------------------------------------
Raw item_get_weight(const View &v, const items::Tables &tables, std::uint32_t item) {
  std::vector<std::uint8_t> data(ITEM_SIZE);
  for (std::uint32_t k = 0; k < ITEM_SIZE; ++k)
    data[k] = std::uint8_t(v.load(item + k, 1));
  const auto unpacked = items::Item::unpack(tables, data);
  if (!unpacked.definition)
    throw Refused("item record points at no definition row");
  return items::item_weight(unpacked);
}
Raw item_weight(const View &v, const items::Tables *tables, const Complements &complements,
                std::int32_t slot, std::uint32_t item) {
  if (item == 0 || v.load(item, 4) == 0)
    return items::F_ONE;
  const auto definition = std::uint32_t(v.load(item, 4));
  Raw f = items::F_ONE;
  if (v.s8(definition + 0x1c) == ITEM_SLOT_AMMO) {
    if (passive_level(v, slot, PASSIVE_AMMO))
      f = items::F_HALF;
    if (passive_level(v, slot, PASSIVE_AMMO))
      f = items::fmul(f, items::F_HALF);
    const auto rank = skill_rank(v, slot, SKILL_LIGHT_QUIVER);
    if (rank != 0) {
      if (!complements.light_quiver)
        throw Refused("Light Quiver's weight complement is not bound");
      const auto obj = std::uint32_t(v.load(G_SKILL_OBJECTS + 4 * SKILL_LIGHT_QUIVER, 4));
      f = items::fmul(f, complements.light_quiver(obj, rank));
    }
  } else if (v.load(item + 8, 8) & ITEM_FLAG_THROWN_AXE) {
    const auto rank = skill_rank(v, slot, SKILL_AXE_BANDOLIER);
    if (rank != 0) {
      if (!complements.axe_bandolier)
        throw Refused("Axe Bandolier's weight complement is not bound");
      const auto obj = std::uint32_t(v.load(G_SKILL_OBJECTS + 4 * SKILL_AXE_BANDOLIER, 4));
      f = items::fmul(f, complements.axe_bandolier(obj, rank));
    }
  }
  if (!tables)
    throw Refused("item tables are not bound");
  return items::fmul(f, item_get_weight(v, *tables, item));
}
void recalc_load(View &v, const items::Tables *tables, const Complements &complements, std::int32_t slot) {
  const auto r = rec(slot);
  v.store(r + P_LOAD, 4, 0);
  for (std::int32_t bag = 0; bag < BAGS; ++bag)
    for (std::int32_t i = 0; i < v.s32(bag_count_at(slot, bag)); ++i) {
      const auto w = item_weight(v, tables, complements, slot, bag_entry(slot, bag, i));
      v.store(r + P_LOAD, 4, items::fadd(Raw(v.load(r + P_LOAD, 4)), w));
    }
}
Raw load_limit(std::int32_t strength, std::int32_t level, std::int32_t bonus) {
  auto limit = items::fint(static_cast<std::uint32_t>(strength) + static_cast<std::uint32_t>(LOAD_STRENGTH_BASE));
  limit = items::fadd(limit, items::fmul(items::fint(static_cast<std::uint32_t>(level)), F_15));
  if (bonus != 0)
    limit = items::fadd(limit, F_5000);
  if (items::flt(F_9999, limit))
    limit = F_9999;
  return limit;
}
void recalc_load_limits(View &v) {
  for (std::int32_t slot = 0; slot < v.s32(G_NUM_PLAYERS); ++slot) {
    const auto r = rec(slot);
    const auto obj = std::uint32_t(v.load(r + P_OBJECT, 4));
    if (obj == 0)
      continue;
    const auto level = passive_level(v, v.s32(obj + OBJ_SLOT), PASSIVE_LOAD);
    v.store(r + P_LOAD_LIMIT, 4, load_limit(v.s32(r + P_EFF_STRENGTH), level, LOAD_BONUS_VALUE));
  }
}
bool fits(const View &v, std::int32_t slot, Raw weight, bool strict) {
  const auto r = rec(slot);
  if (items::flt(items::fadd(Raw(v.load(r + P_LOAD, 4)), weight), Raw(v.load(r + P_LOAD_LIMIT, 4))))
    return true;
  return !strict && (weight & 0x7fffffffu) == 0;
}

// ---- game_scanCm1Vars ------------------------------------------------------------------------------
std::int32_t scan_cm1_vars(std::int32_t recompute, const std::function<std::int32_t(const std::string &)> &profile_get,
                           std::uint32_t word) {
  if (recompute == 0)
    return sword(word);
  if (!profile_get)
    throw Refused("game_scanCm1Vars(1) needs this machine's profile");
  for (std::size_t index = 0; index < CM1_NAMES.size(); ++index)
    if (profile_get(CM1_NAMES[index]) != 0)
      return static_cast<std::int32_t>(index);
  return CM1_NONE;
}
void set_cm1_word(builtins::Memory &memory, std::int32_t value,
                  const std::function<std::int32_t(const std::string &)> &profile_get) {
  if (value == -1)
    value = scan_cm1_vars(1, profile_get, 0);
  memory.word(G_CM1_WORD, static_cast<std::uint32_t>(value));
}

// ---- tables from an image ------------------------------------------------------------------------------
std::shared_ptr<items::Tables> image_tables(const ImageReader &read) {
  if (!read)
    throw Refused("no image reader");
  const auto bytes = [&](std::uint32_t a, std::size_t n) {
    auto out = read(a, n);
    if (out.size() != n)
      throw Refused("image read is short");
    return out;
  };
  const auto u32 = [&](std::uint32_t a) {
    const auto b = bytes(a, 4);
    return std::uint32_t(b[0]) | std::uint32_t(b[1]) << 8 | std::uint32_t(b[2]) << 16 | std::uint32_t(b[3]) << 24;
  };
  const auto u64 = [&](std::uint32_t a) { return std::uint64_t(u32(a)) | std::uint64_t(u32(a + 4)) << 32; };
  const auto h = [&](std::uint32_t a) { return items::s16(u32(a) & 0xffffu); };
  const auto b8 = [&](std::uint32_t a) { return items::s8(bytes(a, 1)[0]); };
  const auto str = [&](std::uint32_t a) {
    std::string out;
    for (;;) {
      const auto c = bytes(a + static_cast<std::uint32_t>(out.size()), 1)[0];
      if (!c)
        return out;
      out.push_back(static_cast<char>(c));
      if (out.size() >= CSTR_LIMIT)
        throw Refused("image string has no terminator");
    }
  };
  auto t = std::make_shared<items::Tables>();
  for (int preset = 0; preset < 2; ++preset) {
    auto &rows = preset ? t->presets : t->defs;
    for (std::uint32_t a = preset ? items::A_PRESETS : items::A_DEFS; u32(a); a += 0x40) {
      if (rows.size() >= TABLE_ROW_LIMIT)
        throw Refused("item definition count limit");
      auto d = std::make_shared<items::Definition>();
      d->addr = a;
      d->index = static_cast<std::int32_t>(rows.size());
      d->preset = preset != 0;
      d->name = str(u32(a));
      d->level = h(a + 4);
      d->weight = u32(a + 8);
      d->count = h(a + 0xc);
      d->damage_min = h(a + 0xe);
      d->damage_max = h(a + 0x10);
      d->price = items::s32(u32(a + 0x14));
      d->armor = h(a + 0x18);
      d->attack = h(a + 0x1a);
      d->slot = b8(a + 0x1c);
      d->category = b8(a + 0x1d);
      d->subtype = b8(a + 0x1e);
      d->damage_bits = u32(a + 0x20);
      d->flags = u64(a + 0x28);
      if (u32(a + 0x30))
        d->picture = str(u32(a + 0x30));
      d->model_row = h(a + 0x34);
      rows.push_back(d);
    }
  }
  for (std::uint32_t a = A_PREFIXES; u32(a); a += 0x28) {
    if (t->prefixes.size() >= TABLE_ROW_LIMIT)
      throw Refused("item prefix count limit");
    items::Prefix p;
    p.addr = a;
    p.index = static_cast<std::int32_t>(t->prefixes.size());
    p.key = str(u32(a));
    p.level = h(a + 4);
    p.mask = u64(a + 0x18);
    p.damage_mask = u32(a + 0x20);
    t->prefixes.push_back(p);
  }
  for (std::uint32_t a = A_SUFFIXES; u32(a); a += 0x14) {
    if (t->suffixes.size() >= TABLE_ROW_LIMIT)
      throw Refused("item suffix count limit");
    items::Suffix s;
    s.addr = a;
    s.index = static_cast<std::int32_t>(t->suffixes.size());
    s.key = str(u32(a));
    s.level = h(a + 4);
    t->suffixes.push_back(s);
  }
  for (const auto &f : TOKEN_FORMATS)
    t->formats.push_back({str(f.first), f.second < 0 ? std::nullopt : std::optional<std::int32_t>(f.second)});
  t->potion_stem = str(S_POTION_STEM);
  t->thrown_stem = str(S_THROWN_STEM);
  t->circlet_stem = str(S_CIRCLET_STEM);
  t->circlet_stem_equip = str(S_CIRCLET_STEM_EQUIP);
  t->arrows = str(S_ARROWS);
  t->recall = str(S_RECALL);
  t->name_key_format = str(S_NAME_SUFFIX);
  t->name_space = str(S_SPACE);
  t->of_key = str(S_OF_KEY);
  t->gold_scale_multiplayer = u64(A_GOLD_SCALE_MP);
  // item_generateWithAffixes reads the weapon value multipliers (a float per subtype
  // 0-6, docs/data-tables.md; a subtype past the table refuses) and the loot quality
  // rows (12 rows of 3 floats, docs/spec/items.md 3).
  for (std::uint32_t k = 0; k < WEAPON_MULT_ROWS; ++k)
    t->weapon_multipliers.push_back(u32(A_WEAPON_MULT + 4 * k));
  for (std::uint32_t row = 0; row < QUALITY_ROWS; ++row)
    t->quality_rows.push_back({u32(A_QUALITY + 12 * row), u32(A_QUALITY + 12 * row + 4), u32(A_QUALITY + 12 * row + 8)});
  return t;
}

// ---- the service ------------------------------------------------------------------------------
Service::Service(Variables &variables) : variables_(variables) {
  variables_.inventory_service = [this](const std::string &name, const std::vector<Variables::InventoryValue> &args) {
    return call(name, args);
  };
  auto scene = variables_.scene_service;
  variables_.scene_service = [this, scene](const std::string &name,
                                           const std::vector<Variables::ServiceValue> &args) -> std::uint32_t {
    if (name == "ScriptGenerator_spawn") {
      if (args.size() < 2)
        throw Refused("ScriptGenerator_spawn: argument count");
      const auto *generator = std::get_if<std::uint32_t>(&args[0]);
      const auto *type_name = std::get_if<std::string>(&args[1]);
      if (!generator || !type_name)
        throw Refused("ScriptGenerator_spawn: argument kinds");
      // The binder passes the interned bytes. The pointer the original stores
      // at +0x34 is the table entry those bytes were read from.
      std::uint32_t at = 0;
      const auto count = items::s8(variables_.memory.u8(0x4f8409));
      for (std::int32_t k = 0; k < count && !at; ++k)
        if (variables_.memory.cstr(0x5d32c8 + 24u * static_cast<std::uint32_t>(k)) == *type_name)
          at = 0x5d32c8 + 24u * static_cast<std::uint32_t>(k);
      if (!at)
        throw Refused("ScriptGenerator_spawn: the type name is not in the generateMonster table");
      return generator_spawn(*generator, at, *type_name);
    }
    if (scene)
      return scene(name, args);
    throw amx::HostFault("undocumented scene service");
  };
}

void Service::bind_tables(std::shared_ptr<const items::Tables> tables) { tables_ = std::move(tables); }

std::uint64_t Service::delegate(const std::string &name, const std::vector<std::uint64_t> &args) {
  calls.push_back({name, args});
  const auto found = delegates.find(name);
  if (found != delegates.end())
    return found->second(args);
  if (name == "player_countPotions" || name == "player_resetIdleAnim" || name == "inventory_selectIdleAnim" ||
      name == "hud_showHelpMessage" || name == "hud_showPlayerNotice" || name == "text_lookup" ||
      name == "res_find" || name == "sound_playOnObject") {
    counts.deferred[name] += 1;
    return 0;
  }
  if (name == "net_isLocalSlot") {
    if (!variables_.local_player_slot)
      throw Refused("net_isLocalSlot: no local slot");
    return args.at(0) == static_cast<std::uint32_t>(variables_.local_player_slot()) ? 1 : 0;
  }
  if (name == "pool_new")
    throw Refused("ground item: ItemProp_ctor2 and its draw are open in the research");
  if (name == "inventory_equipItem")
    throw Refused("inventory_equipItem needs the item tables");
  if (name == "skill_lightQuiverComplement" || name == "skill_axeBandolierComplement")
    throw Refused(name + " is not bound");
  throw Refused(name + " is not part of the item contract");
}

std::uint64_t Service::machine_service(View &v, const std::string &name, const std::vector<std::uint64_t> &args) {
  if (name == "player_recalcLoad") {
    Complements c;
    c.light_quiver = [&](std::uint32_t obj, std::int32_t rank) {
      return Raw(delegates.count("skill_lightQuiverComplement") || !complements.light_quiver
                     ? delegate("skill_lightQuiverComplement", {obj, std::uint32_t(rank)})
                     : complements.light_quiver(obj, rank));
    };
    c.axe_bandolier = [&](std::uint32_t obj, std::int32_t rank) {
      return Raw(delegates.count("skill_axeBandolierComplement") || !complements.axe_bandolier
                     ? delegate("skill_axeBandolierComplement", {obj, std::uint32_t(rank)})
                     : complements.axe_bandolier(obj, rank));
    };
    recalc_load(v, tables_.get(), c, sword(std::uint32_t(args.at(0))));
    return 0;
  }
  if (name == "player_recalcStats") {
    // The executed part: the load limits. The rest of player_recalcStats is
    // the combat world's, which the script host does not own.
    calls.push_back({name, args});
    const auto found = delegates.find(name);
    if (found != delegates.end())
      return found->second(args);
    recalc_load_limits(v);
    counts.deferred[name] += 1;
    return 0;
  }
  if (name == "vars_get" || name == "vars_set") {
    calls.push_back({name, args});
    const auto found = delegates.find(name);
    if (found != delegates.end())
      return found->second(args);
    const auto var = v.cstr(std::uint32_t(args.at(0)));
    if (name == "vars_get")
      return static_cast<std::uint32_t>(variables_.quest_get(var));
    variables_.quest_set(var, static_cast<amx::Cell>(sword(std::uint32_t(args.at(1)))));
    return 0;
  }
  if (name == "script_runByName")
    throw Refused("equipping a weapon runs a notify script inside inventory_equipItem; not composed here");
  return delegate(name, args);
}

template <class Body> auto Service::with_machine(Body body) {
  auto &binder = variables_.memory;
  msg::Memory overlay([&binder](const msg::Integer &address, std::int64_t size) {
    msg::Integer value;
    const auto base = std::uint32_t(address.low64());
    for (std::int64_t i = 0; i < size; ++i)
      value = value.or_shift(msg::Integer(binder.u8(base + static_cast<std::uint32_t>(i))), static_cast<std::size_t>(i));
    return value;
  });
  for (std::uint32_t k = 0; k < ITEM_SIZE; ++k)
    overlay.poke(SCRATCH + k, 1, scratch_[k]);
  msg::Callbacks callbacks;
  callbacks.service = [this, &overlay](const std::string &name, const std::vector<std::uint64_t> &args) {
    OverlayView v(overlay);
    return machine_service(v, name, args);
  };
  callbacks.local_player_slot = [this](std::uint32_t) {
    if (!variables_.local_player_slot)
      throw Refused("no local slot");
    return static_cast<std::uint32_t>(variables_.local_player_slot());
  };
  msg::Machine machine(overlay, callbacks, tables_.get());
  auto result = body(machine);
  for (const auto &write : overlay.writes) {
    const auto at = std::uint32_t(write.address.low64());
    const auto value = write.value.low64();
    for (std::int64_t i = 0; i < write.size; ++i) {
      const auto a = at + static_cast<std::uint32_t>(i);
      const auto b = std::uint8_t(value >> (8 * i));
      if (a >= SCRATCH && a - SCRATCH < ITEM_SIZE)
        scratch_[a - SCRATCH] = b;
      else
        binder.store(a, 1, b);
    }
  }
  return result;
}

Variables::InventoryResult Service::call(const std::string &name, const std::vector<Variables::InventoryValue> &args) {
  auto &memory = variables_.memory;
  if (name == "inventory_countItemByName") {
    BinderView v(memory);
    return static_cast<std::uint32_t>(count_item_by_name(v, text_arg(args, 0, "count"), sword(word_arg(args, 1, "count"))));
  }
  if (name == "inventory_findItemBySubstring") {
    BinderView v(memory);
    const auto found = find_item_by_substring(v, sword(word_arg(args, 0, "find")), text_arg(args, 1, "find"));
    // The handler's bag word starts at 0 and is used only after a match.
    return std::array<std::uint32_t, 2>{static_cast<std::uint32_t>(found.index),
                                        static_cast<std::uint32_t>(found.bag.value_or(0))};
  }
  if (name == "inventory_removeItem") {
    const auto slot = word_arg(args, 0, "remove"), index = word_arg(args, 1, "remove"),
               bag = word_arg(args, 2, "remove"), panel = word_arg(args, 3, "remove");
    with_machine([&](msg::Machine &m) {
      m.remove_item(sword(slot), sword(index), sword(bag), sword(panel));
      return 0;
    });
    counts.removed += 1;
    return 0u;
  }
  if (name == "item_parseDescriptor") {
    if (!tables_)
      throw Refused("item_parseDescriptor needs the item tables");
    const auto text = text_arg(args, 0, "parse");
    const auto variant = sword(word_arg(args, 2, "parse"));
    const auto use_presets = word_arg(args, 3, "parse");
    items::Rng rng(memory.word(G_RAND_SEED));
    items::Item item;
    try {
      item = items::parse_descriptor(*tables_, text, variant, use_presets != 0, &rng);
    } catch (const items::Error &error) {
      throw Refused(std::string("item_parseDescriptor: ") + error.what());
    }
    if (rng.draws)
      memory.word(G_RAND_SEED, rng.seed);
    scratch_ = item.pack(scratch_);
    counts.parsed += 1;
    return item.definition ? item.definition->addr : 0u;
  }
  if (name == "player_itemWeight") {
    ScratchView v(memory, scratch_);
    Complements c;
    c.light_quiver = [&](std::uint32_t obj, std::int32_t rank) {
      return Raw(delegates.count("skill_lightQuiverComplement") || !complements.light_quiver
                     ? delegate("skill_lightQuiverComplement", {obj, std::uint32_t(rank)})
                     : complements.light_quiver(obj, rank));
    };
    c.axe_bandolier = [&](std::uint32_t obj, std::int32_t rank) {
      return Raw(delegates.count("skill_axeBandolierComplement") || !complements.axe_bandolier
                     ? delegate("skill_axeBandolierComplement", {obj, std::uint32_t(rank)})
                     : complements.axe_bandolier(obj, rank));
    };
    return item_weight(v, tables_.get(), c, sword(word_arg(args, 0, "weight")), SCRATCH);
  }
  if (name == "inventory_giveItem") {
    const auto slot = word_arg(args, 0, "give"), announce = word_arg(args, 1, "give");
    // A record whose definition is 0 makes the original read the category byte
    // at address 0x1d (docs/spec/item-drops.md 2, error paths): unknown here.
    if (!scratch_definition())
      throw Refused("inventory_giveItem: the record holds no definition (stale record)");
    const auto index = with_machine([&](msg::Machine &m) { return m.give_item(sword(slot), SCRATCH, sword(announce)); });
    // A full bag or a second held-once item: -1. The handler does not test it,
    // so the item is lost.
    if (index == -1)
      counts.lost += 1;
    else
      counts.given += 1;
    return static_cast<std::uint32_t>(index);
  }
  if (name == "player_recalcLoad") {
    BinderView v(memory);
    machine_service(v, name, {word_arg(args, 0, "load")});
    return 0u;
  }
  if (name == "inventory_equipItem") {
    const auto slot = sword(word_arg(args, 0, "equip"));
    const auto entry = word_arg(args, 1, "equip"), quiet = word_arg(args, 2, "equip");
    bool inside = false;
    for (std::int32_t bag = 0; bag < BAGS; ++bag)
      if (entry - bag_entry(slot, bag, 0) < static_cast<std::uint32_t>(BAG_SLOTS) * ITEM_SIZE &&
          (entry - bag_entry(slot, bag, 0)) % ITEM_SIZE == 0)
        inside = true;
    // inventory_giveItem returned -1 and the handler equips the address 0x48
    // bytes before the bag. Not copied.
    if (!inside)
      throw Refused("inventory_equipItem: the entry is outside the bag (the give returned -1)");
    counts.equipped += 1;
    if (delegates.count(name)) {
      delegate(name, {std::uint32_t(slot), entry, quiet});
      return 0u;
    }
    if (!tables_)
      throw Refused("inventory_equipItem needs the item tables");
    with_machine([&](msg::Machine &m) {
      m.equip_item(slot, entry, sword(quiet));
      return 0;
    });
    return 0u;
  }
  if (name == "player_resetIdleAnim")
    return static_cast<std::uint32_t>(delegate(name, {word_arg(args, 0, "idle")}));
  if (name == "pool_new") {
    const auto size = word_arg(args, 0, "pool");
    if (delegates.count(name))
      return static_cast<std::uint32_t>(delegate(name, {size}));
    // Every native item caller requests 0x190 for a ground ItemProp. Its
    // resource and bounds services are unbound, so refuse before consuming a
    // pool block. The pool model itself is tested independently.
    if (size == 0x190)
      throw Refused("ground item: resource and model-bounds services are unbound");
    if (!variables_.world)
      throw Refused("pool_new: no object world");
    return variables_.world->pool.allocate(size); // full pool returns 0; corruption still throws
  }
  if (name == "ItemProp_ctor2") {
    const auto block = word_arg(args, 0, "ground item");
    const auto *pos = args.size() > 1 ? std::get_if<std::array<std::uint32_t, 3>>(&args[1]) : nullptr;
    if (!pos)
      throw Refused("ItemProp_ctor2: position");
    if (delegates.count(name)) {
      delegate(name, {block, (*pos)[0], (*pos)[1], (*pos)[2], SCRATCH, 0});
      return 0u;
    }
    ground_item(block, *pos);
    return 0u;
  }
  // givePlayerRandom's callees (docs/spec/item-drops.md section 2). The handler
  // blanks its stack record before its first draw; nothing reads the record
  // between that and this call, which is the first one after the draw.
  if (name == "level_monsterLevelTimesDifficulty") {
    // givePlayerRandom's stack record is unknown after ten failed generations
    // (item-drops.md section 6). No native caller may run the handler.
    throw Refused("givePlayerRandom: unmodelled stack record after generator failure");
  }
  if (name == "stack_record_store") {
    const auto offset = word_arg(args, 0, "record"), value = word_arg(args, 1, "record");
    if (offset >= ITEM_SIZE)
      throw Refused("stack_record_store: offset outside the record");
    scratch_[offset] = static_cast<std::uint8_t>(value);
    return 0u;
  }
  if (name == "item_generateWithAffixes") {
    if (!tables_)
      throw Refused("item_generateWithAffixes needs the item tables");
    const auto budget = sword(word_arg(args, 0, "generate")), category = sword(word_arg(args, 1, "generate")),
               suffix = sword(word_arg(args, 2, "generate")), level = sword(word_arg(args, 3, "generate"));
    items::Rng rng(memory.word(G_RAND_SEED));
    std::optional<items::Item> item;
    try {
      item = items::generate_with_affixes(*tables_, random_game(), rng, budget, category, suffix, level);
    } catch (const items::Error &error) {
      throw Refused(std::string("item_generateWithAffixes: ") + error.what());
    }
    memory.word(G_RAND_SEED, rng.seed);
    if (!item)
      return 0u; // the record is not written
    scratch_ = item->pack(scratch_);
    return 1u;
  }
  throw Refused(name + " is not part of the item contract");
}

std::uint32_t Service::scratch_definition() const {
  return std::uint32_t(scratch_[0]) | std::uint32_t(scratch_[1]) << 8 | std::uint32_t(scratch_[2]) << 16 |
         std::uint32_t(scratch_[3]) << 24;
}

// The handler's pre-set of its stack record (0x145f4c-0x145f98): halfwords +0x24,
// +0x2c, +0x34, +0x3c = 0xffff, bytes +0x21 and +0x20 = 0xff, halfwords
// +0x10..+0x1e = 0. The rest keeps what the stack held.
void Service::blank_record() {
  for (std::uint32_t k = 0; k < 4; ++k) {
    scratch_[0x24 + 8 * k] = scratch_[0x25 + 8 * k] = 0xff;
    for (std::uint32_t b : {0x10u + 2 * k, 0x11u + 2 * k, 0x18u + 2 * k, 0x19u + 2 * k})
      scratch_[b] = 0;
  }
  scratch_[0x21] = scratch_[0x20] = 0xff;
}

// The globals item_generateWithAffixes and level_monsterLevelTimesDifficulty read
// (ref_item_drops.game_from): g_numPlayers, the four class words, g_difficultyIdx,
// g_pvpModeA, g_pvpModeB and g_levelMonsterLevel.
items::Game Service::random_game() const {
  const auto &memory = variables_.memory;
  items::Game game;
  game.num_players = items::s32(memory.word(G_NUM_PLAYERS));
  game.classes.clear();
  for (std::int32_t k = 0; k < 4; ++k)
    game.classes.push_back(items::s32(memory.word(rec(k) + P_CLASS)));
  game.difficulty = items::s32(memory.word(G_DIFFICULTY));
  game.pvp_a = items::s32(memory.word(0x4f711c));
  game.pvp_b = items::s32(memory.word(0x4f7120));
  game.level = memory.word(0x4f714c);
  return game;
}

// The pinned constructor writes resource names, lookup pointers and model bounds
// before it can publish an ItemProp (item-drops.md sections 1.2-1.3). This host
// has no res_find/model_getBounds service, so it refuses before registering an
// incomplete object or drawing from the shared seed.
void Service::ground_item(std::uint32_t, const std::array<std::uint32_t, 3> &) {
  throw Refused("ItemProp_ctor2: resource and model-bounds services are unbound");
}

std::uint32_t Service::generator_spawn(std::uint32_t generator, std::uint32_t type_name_at, const std::string &type_name) {
  if (!create_object)
    throw Refused("ScriptGenerator_spawn: no object factory (world_createObject) is bound");
  auto &memory = variables_.memory;
  const std::array<std::uint32_t, 3> pos{memory.word(generator + 0x10), memory.word(generator + 0x14),
                                         memory.word(generator + 0x18)};
  const auto angle = items::s16((memory.u16(generator + 0x38) - 0x4000u) & 0xffffu);
  const auto props = memory.word(generator + 0x2c);
  const auto obj = create_object(type_name, pos, angle, props);
  if (obj == 0)
    return 0;
  // Only an exact Skeleton is put into the rise state 0x17. The Skeleton's
  // states stay refused (combat-ctors); refuse before anything is written.
  if (!dynamic_type)
    throw Refused("ScriptGenerator_spawn: the object's dynamic type is not bound");
  if (dynamic_type(obj) == "8Skeleton")
    throw Refused("ScriptGenerator_spawn: the Skeleton rise state 0x17 stays refused");
  memory.w64(obj + 0xa8, memory.u64(obj + 0xa8) | 0x1000000u);
  memory.word(obj + 0x34, type_name_at);
  return obj;
}

bool routes(const char *command) {
  static const char *names[] = {"playerHasItem", "onePlayerHasItem", "countQuestItems", "removePlayerItem",
                                "removeAllItems", "removeAllWeapons", "givePlayerItem", "giveOnePlayerItem",
                                "givePlayerRandom", "generateMonster"};
  for (const char *name : names)
    if (command && std::strcmp(command, name) == 0)
      return true;
  return false;
}
bool draws_seed(const char *command) {
  return command && (std::strcmp(command, "givePlayerItem") == 0 || std::strcmp(command, "giveOnePlayerItem") == 0 ||
                     std::strcmp(command, "givePlayerRandom") == 0);
}
} // namespace champions::simulation::rta::script_items
