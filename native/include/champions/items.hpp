#pragma once
#include "champions/core_arithmetic.hpp"
#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// RTA item reference model, supplied data only. Names/descriptors are byte
// strings. Addresses are explicit 32-bit identities for modeled serialization,
// never pointers.
namespace champions::simulation::rta::items {
class Error : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};
class Unsupported : public Error {
public:
  using Error::Error;
};
inline constexpr std::uint32_t A_DEFS = 0x3f7650, A_PRESETS = 0x3ea550;
inline constexpr std::size_t ITEM_SIZE = 0x48, DESCRIPTOR_BUFFER = 208;
inline constexpr Raw F_ZERO = ZERO, F_HALF = HALF, F_ONE = ONE, F_TWO = TWO,
                     F_THREE = 0x40400000;
inline constexpr Raw F_HUNDREDTH = 0x3c23d70a, F_2_POW_16 = 0x47800000,
                     F_2_POW_M16 = 0x37800000, F_MAX = FMAX;
std::int32_t s8(std::uint32_t);
std::int32_t s16(std::uint32_t);
std::int32_t s32(std::uint32_t);
Raw fadd(Raw, Raw);
Raw fsub(Raw, Raw);
Raw fmul(Raw, Raw);
Raw fdiv(Raw, Raw);
Raw fint(std::uint32_t);
std::int32_t ftrunc(Raw);
bool flt(Raw, Raw);
bool fle(Raw, Raw);
Raw fmax(Raw, Raw);
Raw float_times_double(Raw, DoubleRaw finite_constant);
Raw powf(Raw, Raw);
struct Definition {
  std::uint32_t addr = 0;
  std::int32_t index = 0;
  bool preset = false;
  std::string name;
  std::int32_t level = 0;
  Raw weight = 0;
  std::int32_t count = 0, damage_min = 0, damage_max = 0, price = 0, armor = 0,
               attack = 0;
  std::int32_t slot = 0, category = 0, subtype = 0;
  std::uint32_t damage_bits = 0;
  std::uint64_t flags = 0;
  std::optional<std::string> picture;
  std::int32_t model_row = 0;
};
struct Prefix {
  std::uint32_t addr = 0;
  std::int32_t index = 0;
  std::string key;
  std::int32_t level = 0;
  std::uint64_t mask = 0;
  std::uint32_t damage_mask = 0;
  std::array<std::optional<std::string>, 3> texts{};
};
struct Suffix {
  std::uint32_t addr = 0;
  std::int32_t index = 0;
  std::string key;
  std::int32_t level = 0;
  std::array<std::optional<std::string>, 3> texts{};
};
struct TokenFormat {
  std::string text;
  std::optional<std::int32_t> code;
};
struct Tables {
  std::vector<std::shared_ptr<Definition>> defs, presets;
  std::vector<Prefix> prefixes;
  std::vector<Suffix> suffixes;
  std::vector<TokenFormat> formats;
  std::string potion_stem, thrown_stem, circlet_stem, circlet_stem_equip;
  std::string arrows, recall, name_key_format, name_space, of_key;
  DoubleRaw gold_scale_multiplayer = 0;
  std::vector<std::string> mana_potions, heal_potions;
  std::vector<Raw> weapon_multipliers;
  std::vector<std::array<Raw, 3>> quality_rows;
  std::shared_ptr<Definition> by_addr(std::uint32_t) const;
  std::optional<std::string> prefix_text(std::size_t,
                                         std::int32_t category) const;
  std::optional<std::string> suffix_text(std::size_t,
                                         std::int32_t category) const;
  Raw weapon_mult(std::int32_t subtype) const;
  std::array<Raw, 3> quality_row(std::int32_t row) const;
};
struct Rng {
  std::uint32_t seed = 0;
  std::uint64_t draws = 0;
  explicit Rng(std::uint32_t initial = 0) : seed(initial) {}
  std::uint32_t next();
};
struct Game {
  std::int32_t num_players = 1;
  std::vector<std::int32_t> classes{0, 0, 0, 0};
  std::int32_t difficulty = 1, pvp_a = 0, pvp_b = 0;
  Raw level = ONE;
  Raw level_times_0_4() const;
  Raw level_times_difficulty() const;
  Raw cloth_fraction() const;
};
struct Modifier {
  std::int32_t code = 0;
  std::array<std::int16_t, 3> values{};
};
struct Item {
  std::shared_ptr<Definition> definition;
  std::int32_t count = 0;
  std::uint64_t flags = 0;
  std::array<std::int16_t, 4> a{}, b{};
  std::uint8_t prefix = 0xff, suffix = 0xff;
  std::array<std::optional<Modifier>, 4> mods{};
  explicit Item(std::shared_ptr<Definition> d = nullptr)
      : definition(std::move(d)) {}
  Item copy() const { return *this; }
  std::uint32_t variant() const;
  std::int32_t get(std::int32_t code, std::size_t which = 0) const;
  std::int32_t set_modifier(std::int32_t code, std::int32_t v0 = 0,
                            std::int32_t v1 = 0, std::int32_t v2 = 0);
  std::vector<std::size_t> undefined_offsets() const;
  std::array<std::uint8_t, ITEM_SIZE>
  pack(const std::array<std::uint8_t, ITEM_SIZE> &stale = {}) const;
  static Item unpack(const Tables &, const std::vector<std::uint8_t> &);
};
std::int32_t encode_definition_index(const Tables &, const Item &);
std::shared_ptr<Definition> decode_definition_index(const Tables &,
                                                    std::int32_t index);
bool same_identity(const Item &, const Item &);
void copy_mutable(Item &dst, const Item &src);
std::int32_t resource_index(const Item &);
struct ScanResult {
  std::int32_t result = 0;
  std::vector<std::int32_t> values;
};
ScanResult scan(const std::string &format, const std::string &text);
void randomize_variant(Item &, Rng &);
std::shared_ptr<Definition> find_definition(const Tables &, const std::string &,
                                            bool presets = false);
Item parse_descriptor(const Tables &, std::string text,
                      std::int32_t variant = -1, bool use_presets = false,
                      Rng *rng = nullptr);
Raw item_weight(const Item &);
std::int32_t item_armor(const Item &);
std::int32_t item_attack_rating(const Item &);
std::pair<std::int32_t, std::int32_t>
item_damage_range(const Item &, bool with_elemental = true);
std::int32_t item_value(const Tables &, const Item &);
std::int32_t required_level_for_value(std::int32_t);
struct Player {
  std::int32_t class_id = 0, level = 1, strength = 0, intelligence = 0,
               dexterity = 0, stamina = 0;
  std::map<std::int32_t, std::int32_t> skills;
  std::int32_t skill_rank(std::int32_t id) const;
};
inline constexpr std::int32_t EQUIP_OK = 0, EQUIP_CLASS = 1, EQUIP_LEVEL = 2,
                              EQUIP_STRENGTH = 3, EQUIP_INTELLIGENCE = 4,
                              EQUIP_DEXTERITY = 5, EQUIP_STAMINA = 6,
                              EQUIP_BLUNT = 7, EQUIP_SLASHING = 8,
                              EQUIP_FLAG_41 = 9;
std::int32_t equip_restriction(const Tables &, const Player &, const Item &);
std::uint64_t load_resources(const Tables &, Rng &);
} // namespace champions::simulation::rta::items
