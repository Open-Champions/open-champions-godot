#include "champions/loading_equipment.hpp"
#include "champions/core_state.hpp"
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

// ---- Round 7. Every line below cites research-loading-readiness@f8c780b
// (audit research-loading-readiness-r7-fix2 PASS): docs/loading-arrival-r7.md
// as r7:<line>, and its difftests tools/difftest_arrival_stage_r7.py,
// _r8.py and _r9.py where the note defers a layout to them.
constexpr std::uint32_t kWarmup = 0x506048, kLoadscreenByte = 0x5060a0, kCutByte = 0x44fcd0;
constexpr std::uint32_t kFreeTable = 0x403a1c;
// The client's pool layout (core_state.hpp Pool, ref_core.Pool): class 1 is
// the 0x500-byte class, where a 0x250-byte GameObject is allocated.
constexpr std::uint32_t kPoolClass1 = Pool::BASE + Pool::CLASSES[0][0] * Pool::CLASSES[0][1];
constexpr std::uint32_t kPoolClass1End = kPoolClass1 + Pool::CLASSES[1][0] * Pool::CLASSES[1][1];

// setState 0x23d680 at state 0 with a nonzero clip (r7:272-350). The entry is
// state 0, so the clip is +0x128 and its flag word +0x12c (r7:272-276).
void set_state_clip(builtins::Memory &m, ArrivalPrefix &out, std::uint32_t obj, std::uint32_t clip) {
  // r7:303-310: only capacity bytes 0 and 4 were executed. r7:309, r8
  // prop_case: every push ran with clip +4 = 5, and 0x170bc8 stores 5.0 at
  // entry +0x18; the source of that value is not recorded, so another clip
  // length is refused rather than given a constant.
  const auto capacity = m.read(obj + 0x9d);
  if (capacity != 0 && capacity != 4)
    finish(out, 0x170838, "anim_push capacity other than 0 or 4 was not executed", m, {0, 0, 0});
  const auto length = m.word(clip + 4);
  if (capacity == 4 && length != 5)
    finish(out, 0x170bc8, "anim entry +0x18 for a clip length other than 5 was not executed", m, {0, 0, 0});
  const std::uint32_t mode = (m.word(obj + 0x12c) & 2) ? 3 : 6; // r7:273-275, 310
  // r7:340-341 (R10-2): the depth clear is `sb zero` at 0x23d6f8, Observed
  // static by field only. Its effect is inferred from the executed second
  // push (r7:342-344): depth 1 and the entry rewritten at +0xa0.
  m.w8(obj + 0x9c, 0);
  out.callees.push_back(0x170838);
  const std::uint32_t entry = obj + 0xa0;
  if (capacity == 4) {
    // r7:307-326 and r8 props: each field and the store that writes it.
    m.w32(entry, clip);                                                    // 0x170878
    m.w16(obj + 0x9e, std::uint32_t(m.u16(obj + 0x9e)) | mode);            // 0x17087c, OR (r7:326-339)
    m.w32(entry + 0x18, 0);                                                // 0x170880
    m.w16(entry + 0x1e, 0);                                                // 0x170888
    if (const auto link = m.word(entry + 4)) {
      m.w32(link + 0x14, 0);                                               // 0x17088c
      m.w32(entry + 4, 0);                                                 // 0x170890
    }
    m.w16(entry + 0x1c, mode);                                             // 0x1708a0
    m.w32(entry + 0xc, 0);                                                 // 0x1708a4
    m.w32(entry + 8, 0x3f800000);                                          // 0x1708a8
    m.w32(entry + 0x10, 0x3f800000);                                       // 0x1708ac
    m.w32(entry + 0x14, 0);                                                // 0x1708b0
    m.w8(obj + 0x9c, 1);                                                   // r7:307-308
  } else {
    // r7:303-306: capacity 0 pushes nothing and stores nothing in the entry.
    // Its +0x9e stores were not checked (audit R10-2), so none are applied.
    out.unmodeled.push_back(0x170838);
  }
  out.callees.push_back(0x170bc8);
  if (capacity == 4) m.w32(entry + 0x18, 0x40a00000);                      // 0x170c1c, 5.0
  // r7:279-299: +0x17c was zeroed at 0x23d6ac; 0x23d764 stores it + clip +4
  // + 1, so it is set to clip +4 + 1, never added to an older value.
  m.w32(obj + 0x17c, length + 1);
}

// The executed property list (r7:375-384): one other entry, then one
// "name=" entry whose value names this object. Anything else is refused.
// The memory layout is +0x2c -> null-terminated array of string pointers
// (difftest_arrival_stage_r7.py delete); a host list is the same entries.
std::string delete_name_entry(builtins::Variables &v, ArrivalPrefix &out, objects::GameObject *obj) {
  auto &m = v.memory;
  std::vector<std::string> entries;
  if (const auto list = m.word(obj->addr + 0x2c)) {
    if (obj->props) finish(out, 0x3b91d8, "delete with two property lists was not executed", m, {0, 0, 0});
    for (std::uint32_t k = 0; k < 3; ++k) {
      const auto at = m.word(list + 4 * k);
      if (!at) break;
      entries.push_back(m.cstr(at, 64));
    }
  } else {
    entries = *obj->props;
  }
  const auto named = [](const std::string &s) { return s.rfind("name=", 0) == 0; };
  if (entries.size() != 2 || named(entries[0]) || !named(entries[1]))
    finish(out, 0x3b91d8, "delete with this property list was not executed", m, {0, 0, 0});
  const auto value = entries[1].substr(5);
  const auto found = v.find(value, 0x200);
  if (value.empty() || found.size() != 1 || found.front() != obj)
    finish(out, 0x3b91d8, "delete whose name= value names another object was not executed", m, {0, 0, 0});
  return value;
}
}

// r7:141-151, 153-158: game_getHubName at the current difficulty.
std::string new_game_destination(builtins::Variables &v) {
  auto &m = v.memory;
  // r7:137-139: the select flag 0x4f70a0 and PvP mode B 0x4f7120 take other
  // paths; only both clear reaches the destination below.
  if (m.word(0x4f70a0) || m.word(0x4f7120)) reject("player select with the select flag or PvP mode B was not executed");
  const auto difficulty = m.word(kDifficulty);
  if (difficulty < 1 || difficulty > 5) reject("hub name outside difficulties 1-5 was not executed");
  // r7:134-135: frontend_startPlayerSelect 0x1e3ba0 stores the destination at
  // 0x4fdc30. r7:153-155: playerSel_finish(0) copies it to the request
  // 0x41b6d0 and stores orientation 0xff at 0x41b720.
  m.string(0x4fdc30, "hub");
  m.string(0x41b6d0, "hub");
  m.w8(0x41b720, 0xff);
  // r7:144-151 and difftest_arrival_stage_r7.py producer A2: the decision
  // bit (+0x4c1b) and choice bit (+0x4c1a) are bit `difficulty`; the
  // completion bit is 0x2 of the mission-1 byte at that difficulty.
  const auto record = v.player_record(0);
  const bool decision = m.read(record + 0x4c1b) & (1u << difficulty);
  const bool choice = m.read(record + 0x4c1a) & (1u << difficulty);
  const bool complete = m.read(v.mission_byte(0, difficulty, 1)) & 2;
  if (decision && choice && complete) m.string(0x41b6d0, "hub2");
  return m.cstr(0x41b6d0, 0x50);
}
// r7:166, 181-184: row 0 +8 of g_missionTable 0x4fe540. Row 0's evil
// alternative +0x0c is null; a non-null one was not executed. The menu state
// that copies it into the request is not executed (r7:193-197).
std::string mission_one_start(const builtins::Memory &m) {
  if (m.word(0x4fe540 + 0x0c)) reject("mission row 0 evil alternative was not executed");
  const auto name = m.word(0x4fe540 + 8);
  if (!name) reject("mission row 0 start level absent");
  return m.cstr(name, 0x50);
}
// r7:167-172, 187-188: main copies the boot request; offline, frontend_start
// replaces it with the front end's request before a session runs. The
// online path was not executed.
std::string boot_request(builtins::Memory &m, bool offline) {
  if (!offline) reject("online boot request was not executed");
  m.string(0x41b6d0, "intro");
  return "intro";
}
// r7:160-163, 177-185: mainloop_loadRequestedWorld 0x320edc copies the
// request into g_levelName 0x41b721 and 0x44fe18 and clears its first byte.
// Only the three executed names split; each names a route ordinal.
ArrivalRequest split_request(builtins::Memory &m) {
  const auto name = m.cstr(0x41b6d0, 0x50);
  ArrivalRequest out;
  if (name == "hub") out.route = 0;
  else if (name == "strife") out.route = 1;
  else if (name == "blood1") out.route = 2;
  else reject("request split for this level was not executed");
  m.string(kLevel, name);
  m.string(0x44fe18, name);
  m.w8(0x41b6d0, 0);
  out.level = name;
  return out;
}

namespace {
void run_arrival_staged(builtins::Variables &v, const ArrivalControl &c, ArrivalPrefix &out, int round) {
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
        const auto clip = m.word(obj->addr + 0x128);
        // Round 7: slot 19's value is bit 0 of +0x12c (r7:275-276).
        const auto value = round < 7 ? c.slot19_value : (m.word(obj->addr + 0x12c) & 1);
        if (clip && round < 7)
          finish(out, 0x170838, "anim_push with a nonzero clip", m, {0, 0, 0});
        // r7:301-349: a clip and slot-19 value 1 ran only separately.
        if (clip && value)
          finish(out, 0x170838, "a clip with slot-19 value 1 was not executed", m, {0, 0, 0});
        if (clip) set_state_clip(m, out, obj->addr, clip);
        out.callees.push_back(0x23d7ac);
        if (value && round < 7)
          finish(out, 0x23d7ac, "slot 19 with a nonzero value", m, {0, 0, 0});
        if (value) {
          // r7:347-349: with +0x11c bit 0, 0x182b00 is called with the world
          // word 0x44fdc8 and faults at 0x182b08 when it is 0. A real
          // collision world is Unknown (r7:351). Both stay refused.
          if (m.word(obj->addr + 0x11c) & 1) {
            if (!m.word(0x44fdc8))
              finish(out, 0x182b08, "slot 19 collision: 0x182b00 faults on a null world 0x44fdc8", m, {0, 0, 0});
            finish(out, 0x182b00, "slot 19 collision with a collision world was not executed", m, {0, 0, 0});
          }
          // r7:345-346: +0xc bit 0 is set (store at 0x23d878).
          m.w32(obj->addr + 0xc, m.word(obj->addr + 0xc) | 1u);
        }
        // r6:107: slot 19 "is called once and returns"; its other stores are
        // not recorded, so none are applied.
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
    if (round < 7) {
      v.run_deferred();
      out.callees.push_back(0x1eda24);
      out.loader_promoted = true;
      // r6:76-78 and 181-185: dbg_printf 0x355490 and the warm-up loop are not
      // entered; the 48 0x145690 tests belong to a later pass.
      finish(out, 0x355490, "dbg_printf/warm-up", m, {2, 0, 1});
    }
    // Round 7, the loader from 0x322e58 (r7:199-252). The reads the passes
    // test are the fixture's: camera word 0, cut byte 0, one player whose
    // pointer is 0 (checked above), net byte 0 (r7:213, 232-238, 245).
    if (m.word(kCamera)) finish(out, 0x203400, "Camera_update in the warm-up loop was not executed", m, {0, 2, 0});
    if (m.read(kCutByte)) finish(out, 0x199944, "cut_runFrame in the warm-up loop was not executed", m, {0, 2, 0});
    // r7:207-213: startWarmup stores 10, dbg_printf, the one promote (front
    // 2, back 0, parity 1, no native run), dbg_printf, the stack check.
    m.w32(kWarmup, 10);
    out.callees.push_back(0x322934);
    out.callees.push_back(0x355490);
    v.run_deferred();
    out.callees.push_back(0x1eda24);
    out.loader_promoted = true;
    out.callees.push_back(0x355490);
    out.callees.push_back(0x19d9f8);
    // r7:215-229: ten passes of six calls while 0x17fe88 returns 0; the
    // counter falls by one per pass through stores at 0x322f30. The timer-0
    // store at 0x322f04 is a hardware register in the research's fixture map
    // (r7:44-46) and is not applied to client memory.
    constexpr std::uint32_t pass[6] = {0x1e72f8, 0x1ebc18, 0x172ca8, 0x17d830, 0x17fce0, 0x17fe88};
    for (std::uint32_t left = 10; left-- > 0;) {
      for (const auto call : pass) out.callees.push_back(call);
      m.w32(kWarmup, left);
      ++out.warmup_passes;
    }
    // None of the six records its stores (r7:265-268).
    for (const auto call : pass) out.unmodeled.push_back(call);
    // r7:243-248: the second stack check, dbg_printf, the net byte, then the
    // shop test by level name.
    out.callees.push_back(0x19d9f8);
    out.callees.push_back(0x355490);
    const auto level = m.cstr(kLevel, 16);
    if (level == "hub") {
      // r7:254-263: shop_openOrStock 0x1c7af8 (a1 = 1) -> lump_load 0x173558,
      // stopped before the disc lookup.
      out.callees.push_back(0x1c7af8);
      out.callees.push_back(0x173558);
      finish(out, 0x1724fc, "cd_findFile", m, {2, 0, 1});
    }
    // r7:246-248: only the empty name was executed past the four compares.
    if (!level.empty()) finish(out, 0x323074, "shop test for this level name was not executed", m, {2, 0, 1});
    // r7:248-252: dbg_printf, then loadscreen_stop 0x3224d8 stores 1 at
    // 0x5060a0 and waits in ReferThreadStatus on a thread this path has not
    // started (r7:265-267, 504-505).
    out.callees.push_back(0x355490);
    out.callees.push_back(0x3224d8);
    m.w8(kLoadscreenByte, 1);
    finish(out, 0x3528e4, "loadscreen_stop: ReferThreadStatus on an unstarted thread", m, {2, 0, 1});
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
    const bool listed = m.word(obj->addr + 0x2c) || obj->props;
    if (listed && round < 7)
      finish(out, 0x3b91d8, "delete with a property list", m, {0, 0, 0});
    // Round 7 refusals come before any dtor store.
    std::string removed;
    if (listed) removed = delete_name_entry(v, out, obj);
    std::uint32_t head = 0, previous = 0;
    if (round >= 7) {
      // r7:362-371 and difftest_arrival_stage_r8.py free_head: pool_free
      // 0x150470 links the block at the head word 0x403a1c + 16 x bucket.
      // Only bucket 1 with a non-zero head was executed (a 0x250-byte
      // GameObject); the client accepts only its pool's class-1 blocks.
      if (obj->addr < kPoolClass1 || obj->addr >= kPoolClass1End ||
          (obj->addr - kPoolClass1) % Pool::CLASSES[1][0])
        finish(out, 0x150470, "pool_free of a block outside pool class 1 was not executed", m, {0, 0, 0});
      head = kFreeTable + 16 * 1;
      previous = m.word(head);
      if (!previous) finish(out, 0x150470, "pool_free onto an empty free list was not executed", m, {0, 0, 0});
    }
    // GameObject_dtor 0x14cf90. Round 6 (r6:129-136, 175-177) recorded no
    // free and no name-count store and is kept as its pin. Round 7 withdraws
    // the no-free claim (r7:355-360, 512-513): 0x144b44 calls slot 1 with
    // a1 = 3 and 0x14cf90 tail-jumps to pool_free at 0x14d1fc.
    out.callees.push_back(0x14cf90);
    m.w32(obj->addr + 0x48, kGameObjectTable);
    if (listed) {
      // r7:375-383: six strncmp calls, then objNames_remove 0x14be58 once
      // with the value after "name=" and the object (the client's name table
      // model). Afterwards the name finds no object.
      for (int k = 0; k < 6; ++k) out.callees.push_back(0x3b91d8);
      out.callees.push_back(0x14be58);
      const auto names = v.object_names.count;
      v.object_names.remove(removed, obj);
      if (v.object_names.count != names - 1 || !v.find(removed, 0x200).empty())
        reject("objNames_remove left the name in the table");
    }
    for (int k = 0; k < 3; ++k) out.callees.push_back(0x1a1698);
    // r6:132: handle_clear's stores are not recorded; none are applied.
    out.unmodeled.push_back(0x1a1698);
    if (round >= 7) {
      // r7:364-371: exactly two stores. 0x1504bc puts the block in the head
      // word; 0x1504c4 puts the previous head in the block's first word.
      out.callees.push_back(0x150470);
      m.w32(head, obj->addr);
      m.w32(obj->addr, previous);
    }
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
  if (c.through_round != 4 && c.through_round != 6 && c.through_round != 7)
    reject("arrival round was not audited");
  if (c.through_round >= 6) {
    if (c.mission < 0 || c.mission > 12 || (c.available_on != 0 && c.available_on != 1))
      reject("arrival prefix arguments were not supplied");
    // Round 7 reads slot 19's value from +0x12c bit 0 (r7:275-276).
    if (c.through_round == 7 && c.slot19_value)
      reject("slot19_value is a round-6 control; round 7 reads +0x12c bit 0");
    run_arrival_staged(v, c, out, c.through_round);
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
