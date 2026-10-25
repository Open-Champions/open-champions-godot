#include "champions/campaign_rewards.hpp"
#include <algorithm>
#include <cstring>

namespace champions::simulation::rta::campaign_rewards {
namespace {
constexpr Address PLAYERS = combat_runtime::PLAYERS, RECORD = combat_runtime::RECORD_SIZE,
                  NUM_PLAYERS = combat_runtime::NUM_PLAYERS, SHARED_SEED = combat_runtime::SHARED_SEED;
// Globals read from the shared bytes (research-pickups items.md section 8,
// research-campaign-combat kill-loot-xp.md "loot_generateDrops").
constexpr Address G_PVP_A = 0x4f711c, G_PVP_B = 0x4f7120;
std::int32_t s8(std::uint64_t v) { return std::int32_t(std::int8_t(std::uint8_t(v))); }
std::int32_t s32(std::uint64_t v) { return std::int32_t(std::uint32_t(v)); }
bool positive(Raw bits) { return combat::flt(0, bits); }
// The skill-point grant for the level being left (levelup.md): 1 for 1-5, 2 for
// 6-15, 3 for 16-25, 4 for 26-35, 5 from 36.
std::int32_t skill_points_for(std::int32_t level) {
  return level < 6 ? 1 : level < 16 ? 2 : level < 26 ? 3 : level < 36 ? 4 : 5;
}
} // namespace

ExpResult give_exp(const player_state::Tables &t, Memory &m, std::int64_t slot, std::int64_t amount) {
  if (slot < 0 || slot > 3) throw Unsupported("player_giveExp: slot outside 0..3");
  const Address r = PLAYERS + Address(slot) * RECORD;
  ExpResult out;
  out.level_before = std::int8_t(s8(m.read(r + 0x59, 1)));
  out.experience_before = s32(m.read(r + 0x5c, 4));
  // exp += amount (32-bit wrap), clamped to the ceiling table[80] - 1.
  auto exp = std::int32_t(std::uint32_t(out.experience_before) + std::uint32_t(amount));
  const auto ceiling = std::int32_t(std::uint32_t(t.experience[80]) - 1u);
  if (ceiling < exp) exp = ceiling;
  m.write(r + 0x5c, 4, std::uint32_t(exp));
  std::int32_t level = out.level_before;
  for (;;) {
    if (level < 0 || level >= 80) break; // table[80] - 1 is the ceiling: no level 81
    if (t.experience[std::size_t(level)] > exp) break;
    const auto points = skill_points_for(level);
    m.write(r + 0xe8, 4, std::uint32_t(s32(m.read(r + 0xe8, 4)) + points));
    m.write(r + 0xec, 1, std::uint8_t(m.read(r + 0xec, 1) + 3u));
    ++level;
    m.write(r + 0x59, 1, std::uint8_t(level));
    // player_recalcStats: the maxima (data-tables.md 4.3) from the earned
    // level and the effective statistics (+0x90..), then each current value
    // is trimmed to its maximum.
    std::array<std::int32_t, 4> stats{};
    for (unsigned n = 0; n < 4; ++n) stats[n] = s32(m.read(r + 0x90 + 4 * n, 4));
    const auto cls = s32(m.read(r + 0x50, 4));
    const auto max = player_state::maxima(t, cls, level, stats);
    m.write(r + 0xb0, 4, max.max_hp);
    m.write(r + 0xc4, 4, max.max_mana);
    if (combat::flt(max.max_mana, Raw(m.read(r + 0xc0, 4)))) m.write(r + 0xc0, 4, max.max_mana);
    if (combat::flt(max.max_hp, Raw(m.read(r + 0xac, 4)))) m.write(r + 0xac, 4, max.max_hp);
    // If current hit points were positive both currents are refilled.
    if (positive(Raw(m.read(r + 0xac, 4)))) {
      m.write(r + 0xac, 4, max.max_hp);
      m.write(r + 0xc0, 4, max.max_mana);
    }
    ++out.levels_gained;
  }
  out.experience_after = exp;
  out.level_after = std::int8_t(level);
  return out;
}

// ---- loot --------------------------------------------------------------------------
namespace {
struct RecordingWorld : items::World {
  Loot *owner = nullptr;
  LootOutcome *outcome = nullptr;
  void spawn_gold(std::int32_t, items::Rng &) override {
    // loot_spawnGold: three draws per scatter try, a terrain test, pool_new
    // and Gold_ctor2 (a further shared draw). Not bound here; the boar's
    // zero gold chance never reaches it, and any other caller is refused.
    outcome->gold_objects += 1;
    throw Unsupported("loot_spawnGold: Gold scatter, pool allocation and Gold_ctor2 are not bound");
  }
  bool spawn_item(const std::string &kind, items::Item &item, items::Rng &rng) override {
    // "make the item prop": pool_new(0x248) then PhProp_ctor, whose GameObject_ctor
    // takes one shared value (items.md section 8). The prop itself is not
    // built: the loot constructor's arguments are not in the research.
    (void)rng.next();
    GroundItem g;
    g.kind = kind;
    g.category = item.definition ? item.definition->category : -1;
    g.count = item.count;
    g.record = item.pack();
    outcome->items.push_back(g);
    return true; // pool_new succeeded: the equipment throw angle is drawn next
  }
  void throw_item(const items::Item &, std::int32_t) override {}
};
} // namespace

Loot::Loot(std::shared_ptr<const items::Tables> tables, Memory mem)
    : tables_(std::move(tables)), mem_(std::move(mem)) {
  if (!tables_) throw Unsupported("loot: item tables are not bound");
}

void Loot::bind_level_globals(Raw monster_level_bits, std::int32_t difficulty) {
  if (difficulty < 1 || difficulty > 5)
    throw Unsupported("loot: g_difficultyIdx outside 1..5 (the level-load difficulty is not bound)");
  level_ = monster_level_bits;
  difficulty_ = difficulty;
  level_bound_ = true;
}

LootOutcome Loot::roll(const combat_runtime::LootDrop &d, const std::string &reagent_filter) {
  // g_levelMonsterLevel and g_difficultyIdx are set by game_loadLevel
  // (research-pickups data-tables.md section 5); the World's bytes never hold
  // them in this session, so an unbound pair is refused, not defaulted.
  if (!level_bound_) throw Unsupported("loot: the level's monster level and the difficulty are not bound");
  items::Game game;
  const auto players = s32(mem_.read(NUM_PLAYERS, 4));
  if (players < 1 || players > 4) throw Unsupported("loot: player count outside 1..4");
  game.num_players = players;
  game.classes.assign(4, 0);
  for (std::int32_t k = 0; k < players; ++k)
    game.classes[std::size_t(k)] = s32(mem_.read(PLAYERS + Address(k) * RECORD + 0x50, 4));
  game.difficulty = difficulty_;
  game.pvp_a = s32(mem_.read(G_PVP_A, 4));
  game.pvp_b = s32(mem_.read(G_PVP_B, 4));
  game.level = level_;
  items::Rng rng(std::uint32_t(mem_.read(SHARED_SEED, 4)));
  LootOutcome out;
  RecordingWorld world;
  world.owner = this;
  world.outcome = &out;
  std::optional<std::string> filter;
  if (!reagent_filter.empty()) filter = reagent_filter;
  try {
    out.ending = items::loot_generate_drops(*tables_, game, rng, world, d.gold_scale, d.gold_chance,
                                            d.quality_row, d.item_chance, s32(d.angle), filter, d.force != 0);
  } catch (...) {
    mem_.write(SHARED_SEED, 4, rng.seed);
    throw;
  }
  mem_.write(SHARED_SEED, 4, rng.seed);
  out.shared_draws = rng.draws;
  return out;
}
} // namespace champions::simulation::rta::campaign_rewards
