#include "champions/player_state.hpp"
#include "champions/combat.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace champions::simulation::rta::player_state {
namespace {
constexpr std::uint32_t XP_TABLE = 0x41b558; // research-combat data-tables.md 4.4
constexpr std::uint32_t CHAIN_BEGIN = 0x1b6740, CHAIN_END = 0x1b694c;
constexpr std::uint8_t MAGIC[8] = {'O','C','P','L','Y','R','0','2'};
// player_initNewCharacter's model-to-class mapping (research-menus
// docs/player.md +0x50 and research-combat docs/data-tables.md 4.1).
constexpr std::int32_t MODEL_CLASS[12] = {0, 0, 3, 3, 1, 1, 4, 4, 2, 2, 5, 6};

std::array<Raw, 6> class_formula(const arena_modes::Executable &exe, int cls) {
  // The bounded instruction subset and branch-delay walk are documented in
  // research-combat tools/ref_combat.py read_class_formula. Only the six float
  // immediates left by this chain are retained; no code is executed from it.
  std::array<std::uint32_t, 32> regs{}, fp{};
  std::uint32_t pc = CHAIN_BEGIN, next = pc + 4;
  for (int step = 0; step < 400; ++step) {
    if (pc == CHAIN_END)
      return {fp[23], fp[24], fp[25], fp[22], fp[20], fp[21]};
    if (pc < CHAIN_BEGIN || pc >= CHAIN_END || (pc & 3))
      throw Refusal("class formula branch left the documented chain");
    const auto ins = exe.word(pc);
    const auto op = ins >> 26, rs = (ins >> 21) & 31u, rt = (ins >> 16) & 31u;
    const auto imm = ins & 0xffffu;
    const auto signed_imm = static_cast<std::int32_t>(static_cast<std::int16_t>(imm));
    std::uint32_t target = next + 4;
    if (ins == 0) {
    } else if (op == 0x23 && imm == 0x50) {
      regs[rt] = static_cast<std::uint32_t>(cls);
    } else if (op == 0x0f) {
      regs[rt] = imm << 16;
    } else if (op == 0x0d) {
      regs[rt] = regs[rs] | imm;
    } else if (op == 0x09) {
      regs[rt] = regs[rs] + static_cast<std::uint32_t>(signed_imm);
    } else if (op == 0x11 && rs == 4) {
      fp[(ins >> 11) & 31u] = regs[rt];
    } else if (op == 4 || op == 5) {
      if ((regs[rs] == regs[rt]) == (op == 4))
        target = pc + 4 + static_cast<std::uint32_t>(signed_imm * 4);
    } else {
      throw Refusal("undocumented instruction in class formula chain");
    }
    regs[0] = 0;
    pc = next;
    next = target;
  }
  throw Refusal("class formula chain did not end");
}
bool finite(Raw bits) {
  float value;
  std::memcpy(&value, &bits, 4);
  return std::isfinite(value);
}
void put32(std::vector<std::uint8_t> &out, std::uint32_t v) {
  for (int n = 0; n < 4; ++n) out.push_back(std::uint8_t(v >> (8 * n)));
}
std::uint32_t take32(const std::vector<std::uint8_t> &in, std::size_t &at) {
  if (at + 4 > in.size()) throw Refusal("native player companion is truncated");
  const auto v = std::uint32_t(in[at]) | std::uint32_t(in[at+1]) << 8 |
                 std::uint32_t(in[at+2]) << 16 | std::uint32_t(in[at+3]) << 24;
  at += 4;
  return v;
}
} // namespace

Tables read_tables(const arena_modes::Executable &exe) {
  Tables t;
  for (std::size_t level = 0; level < t.experience.size(); ++level)
    t.experience[level] = static_cast<std::int32_t>(exe.word(XP_TABLE + std::uint32_t(level * 4)));
  for (std::size_t cls = 0; cls < t.formulas.size(); ++cls) {
    t.formulas[cls] = class_formula(exe, int(cls));
    for (auto raw : t.formulas[cls])
      if (!finite(raw)) throw Refusal("class formula is not finite");
  }
  // Sanity check only: research-combat data-tables.md 4.4 documents table[80]
  // as a far-above sentinel, not that every step increases. It holds on the
  // retail table; a table that fails it is refused rather than indexed.
  for (std::size_t n = 1; n < t.experience.size(); ++n)
    if (t.experience[n] <= t.experience[n-1])
      throw Refusal("experience thresholds are not strictly increasing");
  return t;
}

void validate(const Record &r, const Tables &t) {
  if (r.name.empty() || r.name.size() >= 0x20 || r.name.find('\0') != std::string::npos)
    throw Refusal("player name does not fit record +0x00 char[0x20]");
  if (r.player_class < 0 || r.player_class >= 7 || r.model < 0 || r.model >= 12)
    throw Refusal("player class or model is outside the documented range");
  if (r.player_class != MODEL_CLASS[r.model])
    throw Refusal("player class disagrees with the documented model mapping");
  if (r.level < 1 || r.level > 80 || r.acknowledged < 1 || r.acknowledged > r.level)
    throw Refusal("player level bytes are outside the documented range");
  if (r.experience < 0 || r.experience >= t.experience[80])
    throw Refusal("player experience exceeds the documented cap");
  if (r.gold < 0 || r.gold > 99999999)
    throw Refusal("player gold is outside the documented counter range");
  for (auto raw : {r.hp, r.max_hp, r.mana, r.max_mana})
    if (!finite(raw)) throw Refusal("player health or mana is not finite");
}

Maxima maxima(const Tables &tables, std::int32_t cls, std::int32_t level,
              const std::array<std::int32_t, 4> &stats) {
  if (cls < 0 || cls >= 7) throw Refusal("class is outside 0..6");
  const auto &f = tables.formulas[std::size_t(cls)];
  // player_recalcStats 0x1b6458, exact order from research-combat 4.3.
  Maxima m;
  m.max_mana = combat::fadd(
      combat::fmul(combat::fsub(combat::itof(stats[1]), f[0]), f[1]),
      combat::fmul(combat::itof(level), f[2]));
  m.max_hp = combat::fsub(
      combat::fadd(combat::fmul(combat::itof(level), f[3]),
                   combat::fmul(combat::itof(stats[3]), f[4])), f[5]);
  return m;
}
// Provenance of a created character's values (research-menus docs/player.md,
// research-combat docs/data-tables.md 4.1-4.4, plus an executed difftest of
// player_initNewCharacter 0x1b1d70 with tools/ee_emu.py, summarised in
// work/loop-player-state-r3-report.md):
//  * init zeroes the whole 0x4c30-byte record first (memset 0x3b60c0), so
//    experience is 0; it stores level 1, acknowledged level 1 and gold 500, and
//    copies base statistics to the effective block (+0x90).
//  * init itself leaves maximum HP 0.0 and maximum mana 10.0 and sets both
//    current values to the float 0x4e6e6b27. It calls player_recalcStats
//    0x1b6458 at 0x1b23f8 *before* the character has a Player object (+0x43d8
//    is null), and that call skips the maxima for such a record.
//  * The first player_recalcStats with a Player object present computes the
//    maxima (starting gear adds no statistic modifier 7..10 for any of the
//    12 models; effective = base) and trims each current to its maximum.
// This function computes the documented maxima but does not execute that live
// recalc. Current HP/mana remain unestablished (current_known false).
Record create(const std::string &name, std::int32_t cls, std::int32_t model,
              const std::array<std::int32_t, 4> &stats, const Tables &tables) {
  if (cls < 0 || cls >= 7) throw Refusal("class is outside 0..6");
  Record r;
  r.name = name; r.player_class = cls; r.model = model;
  r.base_stats = r.effective_stats = stats;
  r.level = r.acknowledged = 1; r.experience = 0; r.gold = 500;
  const auto m = maxima(tables, cls, r.level, stats);
  r.max_mana = m.max_mana;
  r.max_hp = m.max_hp;
  // Current values are not established by the cited new-character contract.
  // Their raw storage is not a claim of zero health or mana: current_known
  // stays false, and the session refuses current-dependent script commands.
  validate(r, tables);
  return r;
}

Record import(Record r, const Tables &tables) {
  // player_importCharacter 0x1bd458 clamps the level to 50 and the experience
  // to table[50] (research-combat docs/data-tables.md 4.4; docs/player.md
  // +0x58 and +0x5c). This function is the front-end character import path
  // only (the session controller's character-import route, memcard_importScreen
  // via player_importCharacter, research-menus docs/issues/draw-sim-leaks.md:196).
  // A whole saved record is read by load(), which has no cap of 50.
  if (r.level > 50 || r.acknowledged > 50)
    throw Refusal("imported level exceeds player_importCharacter's cap of 50");
  if (r.experience > tables.experience[50])
    throw Refusal("imported experience exceeds player_importCharacter's cap of table[50]");
  r.imported = true;
  r.current_known = true;
  validate(r, tables);
  return r;
}

std::vector<std::uint8_t> save(const Record &r, const Tables &tables) {
  validate(r, tables);
  std::vector<std::uint8_t> out(std::begin(MAGIC), std::end(MAGIC));
  out.insert(out.end(), r.name.begin(), r.name.end());
  out.resize(8 + 32, 0);
  for (auto v : {r.player_class, r.model, r.level, r.acknowledged, r.experience, r.gold})
    put32(out, static_cast<std::uint32_t>(v));
  for (auto v : r.base_stats) put32(out, static_cast<std::uint32_t>(v));
  for (auto v : r.effective_stats) put32(out, static_cast<std::uint32_t>(v));
  for (auto v : {r.hp, r.max_hp, r.mana, r.max_mana}) put32(out, v);
  put32(out, r.imported ? 1u : 0u);
  put32(out, r.current_known ? 1u : 0u);
  return out;
}
Record load(const std::vector<std::uint8_t> &in, const Tables &tables) {
  if (in.size() != 8 + 32 + (6 + 4 + 4 + 4 + 2) * 4 ||
      !std::equal(std::begin(MAGIC), std::end(MAGIC), in.begin()))
    throw Refusal("native player companion has an unsupported size or version");
  Record r;
  const auto end = std::find(in.begin() + 8, in.begin() + 40, 0);
  r.name.assign(in.begin() + 8, end);
  std::size_t at = 40;
  r.player_class = static_cast<std::int32_t>(take32(in, at));
  r.model = static_cast<std::int32_t>(take32(in, at));
  r.level = static_cast<std::int32_t>(take32(in, at));
  r.acknowledged = static_cast<std::int32_t>(take32(in, at));
  r.experience = static_cast<std::int32_t>(take32(in, at));
  r.gold = static_cast<std::int32_t>(take32(in, at));
  for (auto &v : r.base_stats) v = static_cast<std::int32_t>(take32(in, at));
  for (auto &v : r.effective_stats) v = static_cast<std::int32_t>(take32(in, at));
  r.hp = take32(in, at); r.max_hp = take32(in, at);
  r.mana = take32(in, at); r.max_mana = take32(in, at);
  const auto imported = take32(in, at);
  const auto current_known = take32(in, at);
  if (imported > 1 || current_known > 1) throw Refusal("native player source flag is invalid");
  r.imported = imported != 0;
  r.current_known = current_known != 0;
  validate(r, tables);
  return r;
}
} // namespace champions::simulation::rta::player_state
