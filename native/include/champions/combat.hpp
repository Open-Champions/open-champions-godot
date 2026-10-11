#pragma once
#include "champions/core_arithmetic.hpp"
#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace champions::simulation::rta {
struct SharedRandom;
}

// Authored port of the common contract in ref_combat.py, revision 7212ecf.
// Numeric values are raw patterns. This module uses the caller's selected core
// Fpu context; unlike items, it does not force the default arithmetic settings.
namespace champions::simulation::rta::combat {
class Error : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};
class Unsupported : public Error {
public:
  using Error::Error;
};
using Text = std::u32string;
using rta::Fpu;
using rta::s16;
using rta::s32;
using rta::set_fpu;
std::int32_t s8(std::uint32_t) noexcept;
std::int64_t c_rem(std::int32_t, std::int32_t) noexcept;
Raw fadd(Raw, Raw);
Raw fsub(Raw, Raw);
Raw fmul(Raw, Raw);
Raw fdiv(Raw, Raw);
Raw fsqrt(Raw);
Raw itof(std::int64_t);
std::int32_t ftoi(Raw);
bool flt(Raw, Raw);
bool fle(Raw, Raw);
bool feq(Raw, Raw);
using rta::fmax;
using rta::FMAX;
using rta::fmin;
using rta::HALF;
using rta::M32;
using rta::M64;
using rta::MINUS_ONE;
using rta::ONE;
using rta::round_half_away;
using rta::SIGN;
using rta::TWO;
using rta::ZERO;
inline constexpr Raw THREE = 0x40400000, FIVE = 0x40a00000, SIXTY = 0x42700000;
inline constexpr Raw K_001 = 0x3c23d70a, K_65536 = 0x47800000,
                     K_1_65536 = 0x37800000, K_1_30 = 0x3d088888;
inline constexpr DoubleRaw NAN_VALUE = rta::D_NAN;
// Fraction-based reference doubles are exposed as finite binary64 patterns plus
// NAN_VALUE. Exact zero is canonical positive zero. Operations outside the
// representable normal range throw Unsupported; no host float is used.
DoubleRaw dbl_from_int(std::int64_t); // the reference wraps this input to s32
DoubleRaw dbl_from_unsigned(
    std::uint32_t); // direct nonnegative integer, e.g. a supplied draw
DoubleRaw dbl_from_single(Raw);
DoubleRaw dbl_add(DoubleRaw, DoubleRaw);
DoubleRaw dbl_mul(DoubleRaw, DoubleRaw);
DoubleRaw dbl_fmod(DoubleRaw, DoubleRaw);
Raw dbl_to_single(DoubleRaw);
Raw sqrtf(Raw);
Raw scalbnf(Raw, std::int32_t);
Raw powf(Raw, Raw);
Raw cos16(std::uint32_t);
Raw sin16(std::uint32_t);

// Checked Python-style list access, including negative indices. Short lists
// fail only when the modeled operation reaches the absent entry.
std::size_t index(std::size_t size, std::int64_t position);
template <class T> T &at(std::vector<T> &v, std::int64_t p) {
  return v[index(v.size(), p)];
}
template <class T> const T &at(const std::vector<T> &v, std::int64_t p) {
  return v[index(v.size(), p)];
}
template <class T> T &at(std::deque<T> &v, std::int64_t p) {
  return v[index(v.size(), p)];
}
template <class T> const T &at(const std::deque<T> &v, std::int64_t p) {
  return v[index(v.size(), p)];
}
template <class T, std::size_t N> T &at(std::array<T, N> &v, std::int64_t p) {
  return v[index(N, p)];
}
template <class T, std::size_t N>
const T &at(const std::array<T, N> &v, std::int64_t p) {
  return v[index(N, p)];
}
template <class T> T &required(T *p) {
  if (!p)
    throw Error("missing referenced record");
  return *p;
}

struct Seed {
  std::uint32_t state = 0;
  std::vector<std::uint32_t> draws;
  explicit Seed(std::uint32_t state_ = 0) : state(state_) {}
  std::uint32_t draw();
  std::array<Raw, 3> in_unit_sphere();
};
struct Obj;
struct Spell {
  Text kind;
  std::int64_t skill = 0, level = 1;
  Obj *owner = nullptr;
  std::int64_t f18 = 0;
  Raw f1c = ZERO, f20 = ZERO;
  std::int64_t f2c = 0;
  Spell copy() const { return *this; }
};
// Identity links are non-owning. Callers must keep pointees alive and stable
// for a complete operation, including callbacks. Copies own their lists but
// retain the identity of pointed-to records, matching Python Record.copy().
struct Obj {
  Text tag = U"obj";
  std::uint64_t flags = 0;
  std::vector<Raw> pos = std::vector<Raw>(3, ZERO);
  std::int64_t facing = 0;
  bool owner_link = false;
  Obj *owner = nullptr;
  Text cls = U"Creature";
  std::int64_t innoruuk_immune = 0;
  std::vector<Raw> res = std::vector<Raw>(5, ZERO);
  std::vector<std::int64_t> flash = std::vector<std::int64_t>(8, 0);
  std::uint64_t cflags = 0;
  std::int64_t kind = 0;
  Raw dot = ZERO, field_dc = ZERO;
  std::int64_t armor = 0, stamp_f0 = 0;
  Raw hp = ZERO, max_hp = ZERO;
  std::vector<Raw> status = std::vector<Raw>(5, ZERO);
  std::int64_t poison_a = 0, poison_b = 0;
  Obj *status_source = nullptr;
  std::vector<Spell *> spells;
  std::int64_t counter2 = 0, counter4 = 0, counter11 = 0, counter12 = 0;
  std::int64_t slot = 0, state = 0, attack_start = 0, attack_chain = 0;
  std::vector<std::int64_t> bonus = std::vector<std::int64_t>(4, 0);
  std::int64_t model_points = 1, boss_rank = 0;
  Raw exp_value = ZERO;
  std::uint32_t own_seed = 0;
  Obj *last_attacker = nullptr;
  Spell *spell(const Text &) const;
  Obj copy() const { return *this; }
};
using Descriptor = std::vector<std::vector<std::int64_t>>;
inline const Descriptor EMPTY_DESC(4, std::vector<std::int64_t>{-1, 0, 0, 0});
std::int32_t desc_get(const Descriptor &, std::int64_t kind,
                      std::int64_t field);
struct ItemDef {
  Text name;
  std::int64_t dmg_min = 0, dmg_max = 0, armor = 0, attack = 0, category = 0,
               subtype = 0;
  std::uint64_t dmg_type = 0, flags = 0;
  ItemDef copy() const { return *this; }
};
struct Item {
  ItemDef *defn = nullptr;
  std::int64_t count = 1;
  std::uint64_t flags = 0;
  Descriptor mods = EMPTY_DESC;
  std::int32_t mod(std::int64_t code, std::int64_t field = 1) const;
  Item copy() const { return *this; }
};
struct PlayerRec {
  std::int64_t cls = 0, level58 = 1, level59 = 1, exp = 0;
  std::vector<std::int64_t> base = std::vector<std::int64_t>(4, 0),
                            eff = std::vector<std::int64_t>(4, 0);
  Raw hp = ZERO, max_hp = ZERO, mana = ZERO, max_mana = ZERO;
  std::int64_t attack = 0, dmg_min = 0, dmg_max = 0;
  Raw stagger = ZERO;
  std::int64_t skill_points = 0, stat_points = 0;
  std::vector<std::int64_t> passives = std::vector<std::int64_t>(48, 0);
  Raw load_limit = ZERO;
  std::vector<Item *> bag2;
  Obj *obj = nullptr;
  std::vector<Item *> equip = std::vector<Item *>(17, nullptr),
                      weapon_sets = std::vector<Item *>(2, nullptr);
  Item *second_weapon = nullptr;
  std::int64_t active_set = 0;
  std::vector<std::pair<std::int64_t, std::int64_t>> skills;
  std::int32_t rank(std::int64_t) const;
  PlayerRec copy() const { return *this; }
};

struct EventValue {
  using Value = std::variant<std::monostate, std::int64_t, std::uint64_t, bool,
                             Text, std::vector<Raw>>;
  Value value;
  EventValue() : value(std::monostate{}) {}
  EventValue(std::nullptr_t) : value(std::monostate{}) {}
  EventValue(bool v) : value(v) {}
  template <class I, std::enable_if_t<std::is_integral<I>::value &&
                                          !std::is_same<I, bool>::value,
                                      int> = 0>
  EventValue(I v) : value(number(v)) {}
  EventValue(Text v) : value(std::move(v)) {}
  EventValue(const char32_t *v) : value(Text(v)) {}
  EventValue(const char *ascii);
  EventValue(std::vector<Raw> v) : value(std::move(v)) {}

private:
  template <class I> static Value number(I v) {
    if constexpr (std::is_signed<I>::value)
      return static_cast<std::int64_t>(v);
    else
      return static_cast<std::uint64_t>(v);
  }
};
using Event = std::vector<EventValue>;
struct SharedDraw {
  std::uint32_t value = 0, state = 0;
};
struct SharedRandomSnapshot {
  std::string authority;
  std::uint32_t state = 0;
  std::vector<SharedDraw> draws;
};
struct World {
  // Optional synchronous borrowed authority. The caller must retain the source
  // until this World is destroyed. Copies of observations own values only.
  void bind_shared_random(rta::SharedRandom &, const std::string &label);
  std::uint32_t draw_shared();
  std::optional<SharedRandomSnapshot> shared_random_snapshot() const;
  std::size_t shared_random_draw_count() const noexcept;
  bool has_shared_random() const noexcept;

private:
  rta::SharedRandom *shared_random_ = nullptr;
  std::string shared_random_label_;
  std::vector<SharedDraw> shared_random_draws_;

public:
  Seed seed, cosmetic;
  std::int64_t frame = 0, pvp_a = 0, cutscene = 0, numbers_option = 1,
               network_mode = 0;
  Raw monster_level = ONE;
  std::vector<Raw> focus = std::vector<Raw>(2, ZERO);
  std::vector<std::int64_t> music = std::vector<std::int64_t>(32, 0);
  std::int64_t music_index = 0;
  // Appending players preserves active references. Callbacks may mutate fields,
  // but must not erase/reorder players or destroy any active referenced record.
  std::deque<PlayerRec> players;
  std::int64_t num_players = 0;
  std::vector<std::int64_t> alive;
  std::int64_t medal_mission = 0, difficulty = 1;
  Text level_name;
  std::int64_t load_cheat = 0;
  std::vector<Event> events;
  std::vector<std::unique_ptr<Spell>> owned_spells;
  Spell *add_owned_spell(Spell value);
  std::function<std::int64_t(std::int64_t, std::int64_t, std::int64_t)> stat_fn;
  std::function<std::int64_t(std::int64_t, std::int64_t, std::int64_t, Obj *,
                             std::int64_t)>
      stat_for_fn;
  std::function<std::int64_t(std::int64_t, std::int64_t, std::int64_t)>
      adjust_fn;
  std::function<bool(std::int64_t)> pool_fn;
  std::function<std::int64_t(Obj *, Obj *)> enemy_fn;
  std::function<void(Obj &, Raw, Obj *, std::uint32_t)> take_damage_fn;
  explicit World(std::uint32_t seed_ = 0, std::uint32_t cosmetic_ = 0);
  void log(std::initializer_list<EventValue> event);
  template <class... V> void log(V &&...values) {
    events.push_back(Event{EventValue(std::forward<V>(values))...});
  }
  std::int32_t stat(std::int64_t skill, std::int64_t statistic,
                    std::int64_t level);
  std::int32_t stat_for(std::int64_t skill, std::int64_t statistic,
                        std::int64_t level, Obj *caster, std::int64_t cap);
  std::int32_t adjust(std::int64_t skill, std::int64_t level, std::int64_t n);
  bool pool(std::int64_t size);
  void take_damage(Obj &victim, Raw amount, Obj *attacker, std::uint32_t flags);
};
struct Tables {
  std::vector<std::int64_t> exp_table = std::vector<std::int64_t>(81, 0);
  std::vector<std::array<Raw, 6>> class_formulas =
      std::vector<std::array<Raw, 6>>(7);
  std::array<Raw, 6> class_default{};
  std::vector<std::vector<std::uint64_t>> passive_masks =
      std::vector<std::vector<std::uint64_t>>(
          7, std::vector<std::uint64_t>(48, 0));
  std::array<Text, 4> figurine_stats{}, figurine_tiers{};
  std::vector<Text> boss_names, boss_last, fixed_levels;
  Text boss_gate, no_scale_name;
  std::vector<std::pair<Text, Text>> boss_pairs;
};
inline constexpr std::uint32_t FX_BLOCK = 0x53df18, FX_FIRE = 0x53df38,
                               FX_COLD_PROC = 0x53df50, FX_POISON = 0x53df70,
                               FX_LIGHTNING = 0x53df90, FX_COLD = 0x53dfb0,
                               FX_DISEASE = 0x53dfc8, FX_UNDEAD_KILL = 0x53dfe0,
                               FX_SPIRIT_SHIELD = 0x552da0,
                               FX_FROST_ARMOR = 0x553040, FX_STUN = 0x5520f8;
inline constexpr std::uint64_t CF_BLOCKING = 0x200, CF_INVULNERABLE = 0x800,
                               CF_ABSOLUTE = 0x100000,
                               CF_THORNS = UINT64_C(1) << 45;
inline constexpr std::int32_t SKILL_RIPOSTE = 9, SKILL_DISEASE = 0x1d,
                              SKILL_CRIT_A = 0x21, SKILL_CRIT_B = 0x35,
                              SKILL_STUN = 0x40, SKILL_CRIT_C = 0x51,
                              SKILL_RIPOSTE_B = 0x52;
} // namespace champions::simulation::rta::combat
