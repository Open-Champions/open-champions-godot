#include "champions/loading_equipment.hpp"
#include <limits>
#include <tuple>
#include <variant>

namespace champions::simulation::rta::loading_equipment {
namespace {
[[noreturn]] void refuse() { throw amx::HostFault("equipment resource service or runtime publication unavailable"); }
// All original fields/branches below: research-loading-readiness@2550d34,
// docs/loading-arrival-r3.md. Publication/overflow checks are client restrictions.
using Writes = std::vector<std::tuple<std::uint32_t, unsigned, std::uint32_t>>;
std::uint32_t address(std::uint32_t base, std::uint32_t offset) {
  if (offset > std::numeric_limits<std::uint32_t>::max()-base) refuse();
  return base+offset;
}
std::uint32_t field(builtins::Memory &m, std::uint32_t base, unsigned off, unsigned size) {
  auto a=address(base,off);
  if (size-1 > std::numeric_limits<std::uint32_t>::max()-a || !m.published(a,size)) refuse();
  return a;
}
Writes texture(builtins::Memory &m, std::uint32_t b) {
  Writes out;
  // 2550d34:docs/loading-arrival-r3.md:11-16: guard, relative table, marker/header.
  if (m.word(field(m,b,0x1c,4))) return out;
  auto relative=m.word(field(m,b,0x10,4));
  auto table=address(b,relative);
  field(m,b,0xa,2);
  auto flags=m.u16(field(m,b,8,2)); // 2550d34:docs/loading-arrival-r3.md:18-20.
  out={{b+0x10,4,table},{b+0xa,2,0xa7fb},{b+0x1c,4,0xffffffff}};
  if ((flags&1) || !(flags&0x100)) return out;
  // 2550d34:docs/loading-arrival-r3.md:22-29: table word, rectangles, terminator,
  // per-cell relocation, accumulator and low-halfword store.
  out.emplace_back(field(m,table,0,4),4,address(b,m.word(table)));
  auto cursor=address(table,4);
  std::uint32_t total=0x48;
  while (m.s8(field(m,cursor,0,1))!=-1) {
    field(m,cursor,0,4);
    auto columns=m.s8(cursor+2)-m.s8(cursor)+1;
    auto rows=m.s8(cursor+3)-m.s8(cursor+1)+1;
    total+=6+(rows*0x800*columns)/128;
    cursor=address(cursor,4);
    for (int row=0;row<rows;++row) for (int col=0;col<columns;++col) {
      field(m,cursor,0,4);
      out.emplace_back(cursor,4,address(b,m.word(cursor)));
      cursor=address(cursor,4);
    }
  }
  out.emplace_back(field(m,b,6,2),2,static_cast<std::uint32_t>(total)&0xffff);
  return out;
}
Writes model(builtins::Memory &m, std::uint32_t b) {
  Writes out;
  // 2550d34:docs/loading-arrival-r3.md:31-40: count fallback, width + 1, words;
  // no guard: deliberate second application adds the resource base again.
  int n=m.s8(field(m,b,0x26,1));
  if (n<=0) { n=1; out.emplace_back(b+0x26,1,1); }
  auto width=m.s8(field(m,b,0x12,1))+1;
  for(int i=0;i<n*width;++i) {
    auto a=field(m,b,0x28+4*i,4);
    out.emplace_back(a,4,address(b,m.word(a)));
  }
  return out;
}
void apply_writes(builtins::Memory &m, const Writes &out) {
  for (auto [a,n,v]:out) m.store(a,n,v);
}

std::uint32_t record(unsigned index) {
  // d6f035fa:docs/loading-arrival-r2.md:58-73. Client bounds to executed indices.
  if (index > 1) refuse();
  return 0x4d8af0u + index * 0x4c30u;
}
}
bool Runtime::published(std::uint32_t address, std::size_t size) const {
  return m.published(address,size);
}
void Runtime::require(std::uint32_t a, std::size_t n) const { if (!published(a,n)) refuse(); }
void Runtime::publish_player(std::uint32_t player, unsigned index) {
  // d6f035fa:docs/loading-arrival-r2.md:58-66: display-only suppresses record store.
  const auto r = record(index);
  if (!player) refuse();
  require(player + 0x462, 1);
  m.word(player + 0x3c8, index);
  if (!m.read(player + 0x462)) m.word(r + 0x43d8, player);
}
void Runtime::publish_slot(unsigned index, unsigned slot, std::uint32_t item) {
  // d6f035fa:docs/loading-arrival-r2.md:68-73; slot selection is caller-owned.
  // 17-slot client bound: 9dd49046:docs/loading-readiness.md:41-53.
  if (slot >= 17) refuse();
  m.word(record(index) + 0x43f0 + 4 * slot, item);
}
std::optional<std::uint32_t> Runtime::cache(unsigned slot, unsigned index) {
  // d6f035fa:docs/loading-arrival-r2.md:75-97.
  if (index > 1) return {};
  std::uint32_t base;
  switch (slot) {
    case 3: base=0x866648; break; case 4: base=0x8659e8; break;
    case 9: base=0x865ca8; break; case 10: base=0x865b48; break;
    case 11: base=0x865e08; break; case 12: base=0x865f68; break;
    case 13: base=0x8660c8; break; default: return {};
  }
  return base + index * 0x58;
}
std::uint32_t Runtime::header() const {
  // d6f035fa:docs/loading-arrival-r2.md:140-144.
  require(0x4d8ae0,4); require(0x506040,1);
  return m.word(0x4d8ae0)<2 && !m.read(0x506040) ? 0x535c08 : 0x535bf8;
}
void Runtime::submit(std::uint32_t c, std::uint32_t metadata,
                     std::uint32_t file_sector, bool elapsed, bool busy) {
  // d6f035fa:docs/loading-arrival-r2.md:101-118. Elapsed/busy are service
  // observations; no undocumented cache timestamp is guessed here.
  require(c+8,2);
  if (!elapsed || busy || m.u16(c+8)) return;
  if (!transfer) refuse();
  require(c,4); require(c+0x54,1); require(c+0x56,1);
  require(metadata+4,4); require(metadata+10,2); require(metadata+12,4);
  auto begin=m.word(metadata+4), split=m.word(metadata+12);
  auto size=(split-begin+std::uint32_t(m.s16(metadata+10)))*0x800u;
  Read r{m.read(c+0x54) ? 0x535d10u : m.read(c+0x56) ? 0x535d20u : 0x535d30u,
         m.word(c),0x4ef3e8,begin*0x800u,size};
  m.store(c+8,2,1); m.word(c+0x4c,(split-begin)*0x800u);
  // Original bookkeeping is not media completion. A throwing transfer retains
  // this prefix; no completion/publication is inferred from submission.
  m.word(0x7c25c0,r.completion); m.byte(r.completion,0);
  m.word(0x7c25b0,1); m.word(0x7c25b4,file_sector+(r.offset>>11));
  m.word(0x7c25b8,(r.size+0x7ffu)>>11); m.word(0x7c25bc,r.destination);
  m.byte(0x41bf90,1);
  transfer(r);
}
void Runtime::poll(bool busy, bool error, std::optional<std::uint32_t> remaining) {
  // d6f035fa:docs/loading-arrival-r2.md:120-128.
  require(0x41bf90,1);
  if (!m.read(0x41bf90)) return;
  require(0x41bf91,1);
  if (m.read(0x41bf91)) {
    if (!remaining) refuse();
    if (*remaining) return;
  } else {
    if (busy) return;
    if (error) { if (!retry) refuse(); retry(); return; }
  }
  require(0x7c25c0,4);
  const auto completion=m.word(0x7c25c0);
  if (!completion) refuse();
  m.byte(completion,1); m.byte(0x41bf90,0);
}
Runtime::Pair Runtime::lookup(std::uint32_t c, const std::string &identity) {
  // d6f035fa:docs/loading-arrival-r2.md:130-138. No fake pointer publication.
  require(c+0xa,2);
  require(c,4); require(c+0x4c,4);
  const auto base=m.word(c), model=base+m.word(c+0x4c);
  if (m.u16(c+0xa)) {
    auto saved=m.cstr(c+0xc); require(c+0xc,saved.size()+1);
    if (saved==identity) {
      if (!base || model<base || !valid_resource || !valid_resource(base,model)) refuse();
      return {true,model,base};
    }
  }
  // d6f035fa:docs/loading-readiness.md:63-72 and its difftest:85-101:
  // service only after four frames since the published request. The pin does
  // not establish wrap/signed rollover semantics; refuse that domain.
  require(0x44fdd0,4); require(0x4ef3f0,4);
  const auto current=m.word(0x44fdd0), request=m.word(0x4ef3f0);
  if (current<request) refuse();
  if (current-request<4) return {};
  require(c+8,2);
  if (!m.u16(c+8)) refuse(); // metadata/formatter/submission not guessed
  require(0x4ef3e8,1);
  if (!m.read(0x4ef3e8)) return {};
  require(0x4ef3ec,4);
  if (m.word(0x4ef3ec)!=c) refuse(); // client ownership guard, not original rule
  auto requested=m.cstr(0x8667a8); require(0x8667a8,requested.size()+1);
  if (!base || model<base || !valid_resource) refuse();
  if (fixup_base || fixup_model) {
    if (!fixup_base || !fixup_model) refuse();
    fixup_base(base); fixup_model(model);
    if (!valid_resource(base,model)) refuse();
  } else {
    // 2550d34:docs/loading-arrival-r3.md:49-55: ordered pair, once before
    // publication. Preflight both resources so a refusal cannot poison the guard.
    auto first=texture(m,base), second=loading_equipment::model(m,model);
    // Client safety: validation may reject or throw after seeing transformed
    // bytes. Restore our writes on refusal, so a later scan cannot apply the
    // unguarded model relocation twice (2550d34:docs/loading-arrival-r3.md:31-37).
    auto previous=first;
    previous.insert(previous.end(),second.begin(),second.end());
    for (auto &[a,n,v]:previous) {
      v=0;
      for(unsigned i=0;i<n;++i) v|=std::uint32_t(m.read(a+i))<<(8*i);
    }
    try {
      apply_writes(m,first); apply_writes(m,second);
      if (!valid_resource(base,model)) refuse();
    } catch (...) {
      apply_writes(m,previous);
      throw;
    }
  }
  m.string(c+0xc,requested); m.store(c+0xa,2,1); m.word(0x4ef3ec,0);
  return {}; // completing call has zero outputs; only next matching lookup publishes
}

namespace {
constexpr int kClasses[12] = {0, 0, 3, 3, 1, 1, 4, 4, 2, 2, 5, 6};
constexpr std::uint32_t kWeapons[12] = {
    0x3f7f50, 0x3f7f50, 0x3f7650, 0x3f7650, 0x3f8910, 0x3f8910,
    0x3f9250, 0x3f9250, 0x3f7a10, 0x3f7a10, 0x3f7f50, 0x3f87d0};
constexpr std::uint32_t kArmor[12][3] = {
    {0x3fb110, 0x3fb150, 0x3fb210}, {0x3fb110, 0x3fb150, 0x3fb210},
    {0x3fb110, 0x3fb150, 0x3fb210}, {0x3fb110, 0x3fb150, 0x3fb210},
    {0x3fb110, 0x3fb150, 0x3fb210}, {0x3fb110, 0x3fb150, 0x3fb210},
    {0x3fa210, 0x3fa250, 0x3fa310}, {0x3fa210, 0x3fa250, 0x3fa310},
    {0x3fb110, 0x3fb150, 0x3fb210}, {0x3fb110, 0x3fb150, 0x3fb210},
    {0x3fb110, 0x3fb150, 0x3fb210}, {0x3fb110, 0x3fb150, 0x3fb210}};
constexpr std::uint32_t kDeltas[12][3] = {
    {0x4878, 0x4a08, 0x4ff8}, {0x28d8, 0x2a68, 0x3058},
    {0x177f8, 0x17938, 0x17e38}, {0x159e8, 0x15b28, 0x16028},
    {0xff68, 0x100a8, 0x105a8}, {0xe168, 0xe2a8, 0xe7a8},
    {0xb448, 0xb588, 0xba88}, {0x9648, 0x9788, 0x9c88},
    {0x86a8, 0x87f8, 0x8d28}, {0x6808, 0x6958, 0x6e88},
    {0x13be8, 0x13d28, 0x14228}, {0x11de8, 0x11f28, 0x12428}};
constexpr unsigned kArmorSlots[3] = {4, 10, 12};
constexpr std::uint32_t kDeferCount = 0x86da78, kDeferQueued = 0x86da7c, kParity = 0x4f8690;
constexpr std::uint32_t kSaveInhibit = 0x50605c, kPostAction = 0x86fa80, kInPost = 0x4f8698;
constexpr std::uint32_t kPlayers = 0x4d8ae0, kDifficulty = 0x4f719c, kQuestCount = 0x43d0;

int armor_index(unsigned slot) {
  for (int i = 0; i < 3; ++i)
    if (kArmorSlots[i] == slot) return i;
  return -1;
}
[[noreturn]] void reject(const char *why) { throw amx::HostFault(why); }
void require_clear(builtins::Memory &m) {
  if (m.word(kDeferCount) || m.word(kDeferQueued) || m.word(kParity))
    reject("arrival prefix disturbed the deferred queue");
}
[[noreturn]] void stop_at(ArrivalPrefix &out, std::uint32_t boundary, const char *reason,
                          builtins::Memory &m) {
  require_clear(m);
  out.boundary = boundary;
  out.refusal = reason;
  throw amx::HostFault(reason);
}
}

StartingSelection starting_selection(int body, std::uint32_t seed, int kind) {
  // 366b65a:docs/starting-equipment-r4.md:27-56.
  if (body < 0 || body > 11 || (seed != 7 && seed != 20973) || (kind != -1 && kind != 1))
    reject("starting selection outside executed controls");
  StartingSelection out;
  out.body = body;
  out.class_index = kClasses[body];
  out.weapon_slot = (body == 2 || body == 3 || body == 6 || body == 7) ? 2u : 0u;
  out.weapon_definition = kWeapons[body];
  for (int i = 0; i < 3; ++i) {
    out.armor_definitions[i] = kArmor[body][i];
    out.armor_header_deltas[i] = kDeltas[body][i];
    auto cache = Runtime::cache(kArmorSlots[i], 0);
    if (!cache) reject("starting selection outside executed controls");
    out.armor_caches[i] = *cache;
  }
  return out;
}
HeaderChoice header_choice(std::uint32_t player_count, int override_byte) {
  // 366b65a:docs/starting-equipment-r4.md:76-83. Combinations are (1|2, 0|1).
  if ((player_count != 1 && player_count != 2) || (override_byte != 0 && override_byte != 1))
    reject("header selection outside executed controls");
  HeaderChoice out;
  out.primary = player_count < 2 && override_byte == 0 ? 0x535c08u : 0x535bf8u;
  out.primary_groups = header_group_count(out.primary);
  out.secondary = 0x535c28;
  out.secondary_groups = header_group_count(out.secondary);
  return out;
}
std::uint32_t header_group_count(std::uint32_t identifier) {
  if (identifier == 0x535c08) return 243;
  if (identifier == 0x535bf8) return 245;
  if (identifier == 0x535c28) return 155;
  reject("header group count unavailable");
}
std::uint32_t armor_header_delta(int body, unsigned slot) {
  if (body < 0 || body > 11) reject("armor metadata outside executed controls");
  const int index = armor_index(slot);
  if (index < 0) reject("armor metadata outside executed controls");
  return kDeltas[body][index];
}
CacheSpan cache_span(std::uint32_t begin, std::uint32_t split, std::int16_t tail) {
  // 366b65a:docs/starting-equipment-r4.md:134-135.
  CacheSpan out;
  out.offset = begin * 0x800u;
  out.size = (split - begin + std::uint32_t(tail)) * 0x800u;
  return out;
}
std::int32_t generic_weapon_index(std::int16_t definition_short, bool flag_bit_36,
                                  std::int32_t variant) {
  // 366b65a:docs/starting-equipment-r4.md:156-160.
  const auto sum = std::int64_t(definition_short) + (flag_bit_36 ? std::int64_t(variant) : 0);
  if (sum < std::numeric_limits<std::int32_t>::min() || sum > std::numeric_limits<std::int32_t>::max())
    reject("generic weapon index overflow");
  return static_cast<std::int32_t>(sum);
}
void reset_arrival_queue(builtins::Memory &m) {
  // 366b65a:docs/loading-arrival-r4.md:78-91.
  m.word(kDeferCount, 0);
  m.word(kDeferQueued, 0);
  m.word(kParity, 0);
  m.word(kSaveInhibit, 0);
  m.byte(kPostAction, 0);
}
std::uint64_t arrival_hide_mask(const builtins::Memory &m, std::uint32_t model,
                                std::uint32_t bits) {
  // 56a6760:docs/loading-arrival-r6.md:82-84: the halfword at +0x10 is loaded
  // first; a null model faults there (load address 0x10).
  if (!model) reject("hide mask null model (0x179550 faults at load address 0x10)");
  const auto flag = m.u16(model + 0x10);
  std::uint64_t mask = ~m.u64(model + 8);
  // r6:92-98: flag bit 0x2 walks 16-byte {show, hide} records from the byte at
  // +0x21 shifted up by 4; flags 0 and 1 keep the complement.
  if (flag & 2) {
    const std::uint32_t base = model + (std::uint32_t(m.read(model + 0x21)) << 4);
    for (std::uint32_t bit = 0; bit < 32; ++bit)
      if (bits & (1u << bit)) {
        mask |= m.u64(base + 16 * bit);
        mask &= ~m.u64(base + 16 * bit + 8);
      }
  }
  return mask;
}

namespace {
// Round-6 constants. Addresses: 56a6760:docs/loading-arrival-r5.md and
// docs/loading-arrival-r6.md; layouts the notes leave to their difftests are
// cited at 56a6760:tools/difftest_arrival_stage_r5.py / _r6.py.
constexpr std::uint32_t kNet = 0x402791, kPlayerObject = 0x4dcec8;
constexpr std::uint32_t kLevel = 0x41b721, kCamera = 0x4f8450;
constexpr std::uint32_t kLumps = 0x7b9480, kLumpCount = 0x41bf7c;
constexpr std::uint32_t kAutoPropTable = 0x5133c0, kGameObjectTable = 0x5122a8;
constexpr std::uint32_t kLight = 0x4039ac, kDrawFrame = 0x44fdd0;

struct Queue { std::uint32_t front, back, parity; };
[[noreturn]] void finish(ArrivalPrefix &out, std::uint32_t boundary, const char *reason,
                         builtins::Memory &m, Queue want) {
  if (m.word(kDeferCount) != want.front || m.word(kDeferQueued) != want.back ||
      m.word(kParity) != want.parity)
    reject("arrival prefix disturbed the deferred queue");
  out.boundary = boundary;
  out.refusal = reason;
  throw amx::HostFault(reason);
}
// 0x17306c (r5:97-99): the data pointer when the 16-byte record name matches
// and flag bit 0 is clear. Only the one-record table was executed
// (r5:101-108; record layout difftest_arrival_stage_r5.py:144-160).
std::optional<std::uint32_t> resident_lump(builtins::Memory &m, const std::string &name) {
  const auto count = m.word(kLumpCount);
  if (count > 1) reject("lump table beyond one record was not executed");
  if (count == 0 || name.empty() || name.size() >= 16) return std::nullopt;
  if (m.cstr(kLumps, 16) != name || (m.word(kLumps + 0x14) & 1)) return std::nullopt;
  return m.word(kLumps + 0x10);
}
objects::GameObject *one_object(builtins::Variables &v, const std::string &name,
                                std::int64_t limit, const char *why) {
  const auto found = v.find(name, limit);
  if (found.size() > 1) reject(why);
  return found.empty() ? nullptr : found.front();
}
// showMonster 0x144db8 / hideMonster 0x144c00 with one creature object
// (56a6760:docs/loading-arrival-r6.md:115-125). The grid bodies are the
// host's obj_updateGridCell / obj_moveOffGrid services, as in the builtins.
void show_hide(builtins::Variables &v, ArrivalPrefix &out, const std::string &name, bool hide) {
  auto &m = v.memory;
  auto *obj = one_object(v, name, 0x200, "show or hide with more than one object was not executed");
  if (obj) {
    if (!(m.word(obj->addr + 0xc) & 0x20))
      reject("show or hide of a non-creature object was not executed");
    if (!v.scene_service) reject("grid service unbound");
    v.scene_service(hide ? "obj_moveOffGrid" : "obj_updateGridCell", {obj->addr});
    out.callees.push_back(hide ? 0x14ef48 : 0x14f028);
    const auto flags = m.word(obj->addr + 0xc);
    m.word(obj->addr + 0xc, hide ? flags | 0x8000u : flags & ~0x8000u);
    const auto bits = m.read(obj->addr + 0x231);
    m.byte(obj->addr + 0x231, hide ? std::uint8_t(bits | 8) : std::uint8_t(bits & 0xf7));
  }
  out.executed.push_back(hide ? 0x144c00 : 0x144db8);
}

void run_arrival_r6(builtins::Variables &v, const ArrivalControl &c, ArrivalPrefix &out) {
  auto &m = v.memory;
  // r5:18-21: player count 1, difficulty 1, network byte 0, player object 0.
  if (m.read(kNet) || m.word(kPlayerObject))
    reject("arrival fixture inputs differ (network byte or player object)");
  m.word(kPlayers, 1);
  m.word(kDifficulty, 1);
  reset_arrival_queue(m);
  if (c.route == 2) {
    if (c.quest_title.empty() || c.quest_text.empty() || c.var_name.empty() ||
        c.post_name.empty() || c.cut_name.empty() || c.replacement_name.empty() ||
        c.delete_name.empty())
      reject("arrival prefix arguments were not supplied");
    if (c.first_frame < -1 || c.first_frame > 1)
      reject("cut_runFrame argument was not executed");
    for (int slot = 0; slot < 4; ++slot) {
      m.byte(v.player_record(slot) + 0x4c1a, c.choice_bit ? 2 : 0);
      if (c.completion_set) {
        const auto where = v.mission_byte(slot, 1, c.mission);
        m.byte(where, m.read(where) | 2);
      }
    }
    m.word(v.player_record(0) + kQuestCount, c.quest_list_full ? 8u : 0u);
  }
  out = {};
  if (c.route != 1) out.missing_entry = -1;

  if (c.route == 1) {
    if (c.lump_name.empty() || c.show_name.empty() || c.hide_names.size() != 3 ||
        c.var_name.empty())
      reject("arrival prefix arguments were not supplied");
    // r5:97-108: lump_load 0x173558, lookup 0x17306c; a miss or flag bit 0
    // reaches cd_findFile 0x1724fc, which is not entered.
    out.callees.push_back(0x173558);
    out.callees.push_back(0x17306c);
    if (!resident_lump(m, c.lump_name))
      finish(out, 0x1724fc, "cd_findFile", m, {0, 0, 0});
    out.executed.push_back(0x14222c);
    // Order: 56a6760:tools/difftest_arrival_stage_r5.py:35-37.
    for (int k = 0; k < 2; ++k) { (void)v.quest_get(c.var_name); out.executed.push_back(0x143500); }
    show_hide(v, out, c.show_name, false);
    for (const auto &name : c.hide_names) show_hide(v, out, name, true);
    for (int k = 0; k < 6; ++k) { (void)v.quest_get(c.var_name); out.executed.push_back(0x143500); }
    // r5:34 and 106-108: the entry returns, no post action, queue 0. The
    // loader call after this route was not executed.
    if (m.read(kPostAction)) reject("route 1 published a post action");
    finish(out, 0x322e6c, "loader call after route 1", m, {0, 0, 0});
  }

  if (c.route == 0) {
    if (c.object_name.empty() || c.deferred_names.size() != 2 || c.remove_items.size() != 17 ||
        c.var_name.empty())
      reject("arrival prefix arguments were not supplied");
    m.word(kSaveInhibit, static_cast<std::uint32_t>(c.save_inhibit));
    out.executed.push_back(0x1473b4);
    v.mission_set_available(1, c.mission, c.available_on);
    out.executed.push_back(0x1420d0);
    v.mission_set_complete(1, c.mission, -1);
    out.executed.push_back(0x1420a4);
    // Object load 0x142320 (r5:45-49): world_findObjectsByName with limit
    // 0x64; slot 18 only for object flag 0x10000000.
    auto *obj = one_object(v, c.object_name, 0x64, "object load with more than one object was not executed");
    if (obj && (m.word(obj->addr + 0xc) & 0x10000000)) {
      // r5:47-49: table 0x5133c0 slot 18 is 0x23d680.
      if (m.word(obj->addr + 0x48) != kAutoPropTable)
        reject("object load slot 18 outside AutoProp was not executed");
      out.callees.push_back(0x23d680);
      // r5:67-74: signed argument 0 < States enters; otherwise it returns.
      if (0 < m.s32(obj->addr + 0x120)) {
        // r5:61-65: +0x178 = argument, +0x17c = 0, 8-byte +0x180 = 0.
        m.w32(obj->addr + 0x178, 0);
        m.w32(obj->addr + 0x17c, 0);
        m.w64(obj->addr + 0x180, 0);
        out.callees.push_back(0x179550);
        const auto model = m.word(obj->addr + 0x90);
        // r6:101-103: null model faults; the names are not queued.
        if (!model)
          finish(out, 0x179550, "hide mask null model (fault at load address 0x10)", m, {0, 0, 0});
        // The setState variant argument is not given; only flag-clear
        // headers were executed on this path (r6:103-105).
        if (m.u16(model + 0x10) & 2)
          finish(out, 0x179550, "prop hide-mask record walk was not executed", m, {0, 0, 0});
        m.w64(obj->addr + 0xc8, arrival_hide_mask(m, model, 0));
        // r6:105-107: a zero clip word is not stored and anim_push is skipped.
        if (m.word(obj->addr + 0x128))
          finish(out, 0x170838, "anim_push with a nonzero clip", m, {0, 0, 0});
        out.callees.push_back(0x23d7ac);
        if (c.slot19_value)
          finish(out, 0x23d7ac, "slot 19 with a nonzero value", m, {0, 0, 0});
        // r6:107: slot 19 "is called once and returns"; its stores are not
        // recorded, so none are applied.
        out.unmodeled.push_back(0x23d7ac);
      }
    }
    out.executed.push_back(0x142320);
    // r5:51-56: append, player count, append, quest read, quest write.
    v.queue_deferred(c.deferred_names[0]);
    out.executed.push_back(0x144160);
    (void)m.word(kPlayers);
    out.executed.push_back(0x143534);
    v.queue_deferred(c.deferred_names[1]);
    out.executed.push_back(0x144160);
    (void)v.quest_get(c.var_name);
    out.executed.push_back(0x143500);
    v.quest_set(c.var_name, c.var_first);
    out.executed.push_back(0x143540);
    // r5:58-61 and r6:107-108: with an object the entry was executed only to
    // this stop.
    if (obj) finish(out, 0x14598c, "removePlayerItem after object load", m, {0, 2, 0});
    // r6:37-44: 17 calls, player -1, one search per current player, no hit.
    if (!v.inventory_service) reject("removePlayerItem inventory search unbound");
    for (const auto &item : c.remove_items) {
      for (std::uint32_t slot = 0; slot < m.word(kPlayers); ++slot) {
        const auto result = v.inventory_service("inventory_findItemBySubstring", {slot, item});
        const auto *pair = std::get_if<std::array<std::uint32_t, 2>>(&result);
        if (!pair) reject("findItemBySubstring requires index and bag");
        if ((*pair)[0] != 0xffffffffu)
          finish(out, 0x1c1cb0, "removePlayerItem with an inventory hit", m, {0, 2, 0});
      }
      out.executed.push_back(0x14598c);
    }
    // r6:50-59: three reads, setPlayerFlag(0..3, 4, 0), two reads, return.
    for (int k = 0; k < 3; ++k) { (void)v.quest_get(c.var_name); out.executed.push_back(0x143500); }
    for (int slot = 0; slot < 4; ++slot) { v.player_set_flag(slot, 4, 0); out.executed.push_back(0x1416d4); }
    for (int k = 0; k < 2; ++k) { (void)v.quest_get(c.var_name); out.executed.push_back(0x143500); }
    if (m.read(kPostAction)) reject("route 0 published a post action");
    // r6:63-69: the loader's 0x322e6c is one 0x1eda24 pass. The front is 0,
    // so it promotes the two names and runs none of them.
    if (m.word(kDeferCount) != 0 || m.word(kDeferQueued) != 2 || m.word(kParity) != 0)
      reject("arrival prefix disturbed the deferred queue");
    v.run_deferred();
    out.callees.push_back(0x1eda24);
    out.loader_promoted = true;
    // r6:76-78 and 181-185: dbg_printf 0x355490 and the warm-up loop are not
    // entered; the 48 0x145690 tests belong to a later pass.
    finish(out, 0x355490, "dbg_printf/warm-up", m, {2, 0, 1});
  }

  // Route 2: the round-4 prefix (366b65a:docs/loading-arrival-r4.md:154-179).
  (void)v.mission_complete(1, c.mission);
  out.executed.push_back(0x142084);
  if (!c.completion_set) {
    (void)v.choice_get(0x4c1a);
    out.executed.push_back(0x142044);
    v.quest_add(c.quest_title, c.quest_text);
    out.executed.push_back(0x1433c8);
  }
  v.mission_set_available(1, c.mission, c.available_on);
  out.executed.push_back(0x1420d0);
  v.quest_set(c.var_name, c.var_first);
  out.executed.push_back(0x143540);
  v.quest_set(c.var_name, c.var_second);
  out.executed.push_back(0x143540);
  v.set_post_action(c.post_name);
  out.executed.push_back(0x143ee4);
  (void)v.choice_get(0x4c1b);
  out.executed.push_back(0x142010);
  m.byte(kPostAction, 0);
  m.byte(kInPost, 1);
  out.post_action = true;
  (void)v.quest_get(c.var_name);
  out.executed.push_back(0x143500);
  // cut_start (r5:115-151). Offline and no player object were checked above.
  const auto level = m.cstr(kLevel, 16);
  if (level.empty()) finish(out, 0x143f1c, "cutscene runtime: level name absent", m, {0, 0, 0});
  const auto lump = resident_lump(m, level + ".lmp");
  if (!lump) finish(out, 0x17365c, "cutscene runtime: resource not resident", m, {0, 0, 0});
  // One-entry lump image {count, label, data, size}:
  // difftest_arrival_stage_r5.py:144-160 and r6 arm_cut.
  if (!*lump || m.u16(*lump) != 1 || m.cstr(m.word(*lump + 4), 64) != c.cut_name + ".cut")
    finish(out, 0x17365c, "cutscene runtime: resource not resident", m, {0, 0, 0});
  const auto header = m.word(*lump + 8);
  // r5:120-121 and 158-161: the header word is read with no null test; only
  // header word 0 was executed.
  if (!header || m.word(header) != 0)
    finish(out, 0x19b4dc, "cutscene runtime: header word not 0", m, {0, 0, 0});
  const auto camera = m.word(kCamera);
  if (!camera) finish(out, 0x19b4dc, "cutscene runtime: no camera object", m, {0, 0, 0});
  for (const std::uint32_t call : {0x3b7ef0u, 0x3b7ef0u, 0x17365cu, 0x19b4dcu, 0x322934u,
                                   0x1eb000u, 0x19a62cu, 0x18617cu, 0x2072e8u, 0x18619cu,
                                   0x3a59a8u, 0x3a59a8u, 0x3a59a8u, 0x19e4a0u, 0x19e4a0u})
    out.callees.push_back(call);
  // r5:136-151, the finals on both seeds.
  m.w32(0x506048, 10);
  m.w32(0x506080, m.word(kDrawFrame));
  for (const std::uint32_t a : {0x44fd2cu, 0x44fd28u, 0x44fcfcu, 0x44fd00u, 0x44fce8u}) m.w32(a, 0);
  m.w8(0x44fcd2, 0);
  m.w8(0x44fcec, 0);
  m.w32(0x44fce0, 1);
  m.w8(0x44fcd0, 1);
  m.w8(0x44fcd1, 1);
  m.w32(0x7d8a04, 0x44e10000);
  m.w32(0x41b4fc, 0x3c);
  m.w8(0x4f66a8, 1);
  m.w32(0x83d198, 0);
  m.w32(0x83d18c, 0);
  m.w32(camera + 0x34, 0); // 0x2072e8(0); +0x38, +0x3c and +0x98 untouched
  m.w32(0x44fdb8, 2);
  m.w32(0x44fce4, header);
  out.executed.push_back(0x143f1c);
  // r5:153-156: one quest write, then the replacement is published.
  v.quest_set(c.var_name, c.var_after_cut);
  out.executed.push_back(0x143540);
  v.set_post_action(c.replacement_name);
  out.executed.push_back(0x143ee4);
  // The post action returns: 0x1edac0 clears the in-post byte after it
  // (56a6760:tools/ref_natives.py:973-981, compared in r4:144-147).
  m.byte(kInPost, 0);
  out.quest_count = static_cast<int>(m.word(v.player_record(0) + kQuestCount));
  if (c.first_frame >= 0) {
    // r6:144-149: cut_runFrame 0x199944, light_setScale 0x14a480 stores 0
    // at 0x4039ac, cut_setup 0x19a6cc; setup byte 0x44fcd2 is not stored.
    out.callees.push_back(0x199944);
    out.callees.push_back(0x14a480);
    m.w32(kLight, 0);
    out.callees.push_back(0x19a6cc);
    finish(out, 0x14f498, "cut_setup", m, {0, 0, 0});
  }
  // r5:155-156 and r6:127-136: the replacement's only native is
  // deleteMonster 0x144b44.
  m.byte(kPostAction, 0);
  m.byte(kInPost, 1);
  auto *obj = one_object(v, c.delete_name, 0x200, "delete with more than one object was not executed");
  if (obj) {
    if (!(m.word(obj->addr + 0xc) & 0x20))
      reject("delete of a non-creature object was not executed");
    // 0x144b44 dispatches slot 1 through the object's own table (the
    // builtin's virtual_call(obj, 1, 3)). 0x14cf90 is slot 1 only of the
    // GameObject table: the research's object is GameObject_ctor 0x14cb10
    // plus the creature flag (r6:127-136; 56a6760:tools/
    // difftest_arrival_stage_r6.py:114-124). Any other table's slot 1 was
    // not executed.
    if (m.word(obj->addr + 0x48) != kGameObjectTable)
      reject("delete of an object outside the GameObject table was not executed");
    // r6:138-140: a property list takes the walk this fixture skips.
    if (m.word(obj->addr + 0x2c) || obj->props)
      finish(out, 0x3b91d8, "delete with a property list", m, {0, 0, 0});
    // GameObject_dtor 0x14cf90 through the three handle clears; no free and
    // no name-count store (r6:129-136, 175-177). The no-free part relies on
    // round 6's "does not call pool_free 0x150470". Round 7 (4487a88, still
    // under audit) disputes that; it is not ported here. See the round-5
    // fix report.
    out.callees.push_back(0x14cf90);
    m.w32(obj->addr + 0x48, kGameObjectTable);
    for (int k = 0; k < 3; ++k) out.callees.push_back(0x1a1698);
    // r6:132: handle_clear's stores are not recorded; none are applied.
    out.unmodeled.push_back(0x1a1698);
  }
  out.executed.push_back(0x144b44);
  m.byte(kInPost, 0);
  // r5:156: the two registered cut tasks are not run.
  finish(out, 0x199944, "cut_runFrame tasks", m, {0, 0, 0});
}
}

void run_arrival_prefix(builtins::Variables &v, const ArrivalControl &c, ArrivalPrefix &out) {
  // 366b65a:docs/loading-arrival-r4.md:154-191. Difficulty one and one player
  // are the initialized-record controls. An unlisted route is "route state".
  if (c.route < 0 || c.route > 2) throw amx::HostFault("route state");
  if (c.through_round != 4 && c.through_round != 6) reject("arrival round was not audited");
  if (c.through_round == 6) {
    if (c.mission < 0 || c.mission > 12 || (c.available_on != 0 && c.available_on != 1))
      reject("arrival prefix arguments were not supplied");
    run_arrival_r6(v, c, out);
  }
  if (c.mission < 0 || c.mission > 12 || (c.available_on != 0 && c.available_on != 1))
    reject("arrival prefix arguments were not supplied");
  auto &m = v.memory;
  m.word(kPlayers, 1);
  m.word(kDifficulty, 1);
  reset_arrival_queue(m);
  if (c.route == 2) {
    if (c.quest_title.empty() || c.quest_text.empty() || c.var_name.empty() || c.post_name.empty())
      reject("arrival prefix arguments were not supplied");
    for (int slot = 0; slot < 4; ++slot) {
      m.byte(v.player_record(slot) + 0x4c1a, c.choice_bit ? 2 : 0);
      if (c.completion_set) {
        const auto where = v.mission_byte(slot, 1, c.mission);
        m.byte(where, m.read(where) | 2);
      }
    }
    m.word(v.player_record(0) + kQuestCount, c.quest_list_full ? 8u : 0u);
  }
  out = {};
  if (c.route != 1) out.missing_entry = -1;
  if (c.route == 1) stop_at(out, 0x14222c, "resource bootstrap", m);
  if (c.route == 0) {
    m.word(kSaveInhibit, static_cast<std::uint32_t>(c.save_inhibit));
    out.executed.push_back(0x1473b4);
    v.mission_set_available(1, c.mission, c.available_on);
    out.executed.push_back(0x1420d0);
    v.mission_set_complete(1, c.mission, -1);
    out.executed.push_back(0x1420a4);
    stop_at(out, 0x142320, "object load", m);
  }
  (void)v.mission_complete(1, c.mission);
  out.executed.push_back(0x142084);
  if (!c.completion_set) {
    (void)v.choice_get(0x4c1a);
    out.executed.push_back(0x142044);
    v.quest_add(c.quest_title, c.quest_text);
    out.executed.push_back(0x1433c8);
  }
  v.mission_set_available(1, c.mission, c.available_on);
  out.executed.push_back(0x1420d0);
  v.quest_set(c.var_name, c.var_first);
  out.executed.push_back(0x143540);
  v.quest_set(c.var_name, c.var_second);
  out.executed.push_back(0x143540);
  v.set_post_action(c.post_name);
  out.executed.push_back(0x143ee4);
  (void)v.choice_get(0x4c1b);
  out.executed.push_back(0x142010);
  // Explicit post-action phase. The private action body is not run.
  // 366b65a:docs/loading-arrival-r4.md:163-179: 0x1edac0 clears the name and
  // sets 0x4f8698, then 0x143500 runs, then stop before 0x143f1c.
  m.byte(kPostAction, 0);
  m.byte(kInPost, 1);
  out.post_action = true;
  (void)v.quest_get(c.var_name);
  out.executed.push_back(0x143500);
  out.quest_count = static_cast<int>(m.word(v.player_record(0) + kQuestCount));
  stop_at(out, 0x143f1c, "cutscene runtime", m);
}
}
