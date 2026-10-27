#include "champions/loading_equipment.hpp"

namespace champions::simulation::rta::loading_equipment {
namespace {
[[noreturn]] void refuse() { throw amx::HostFault("equipment resource service or runtime publication unavailable"); }
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
  if (!base || model<base || !fixup_base || !fixup_model || !valid_resource) refuse();
  fixup_base(base); fixup_model(model);
  if (!valid_resource(base,model)) refuse();
  m.string(c+0xc,requested); m.store(c+0xa,2,1); m.word(0x4ef3ec,0);
  return {}; // completing call has zero outputs; only next matching lookup publishes
}
}
