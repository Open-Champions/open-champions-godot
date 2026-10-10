// Authored raw-readonly overlay invariants; no game assets or pool replacement.
#include "champions/creature_hp.hpp"
#include "champions/raw_handles.hpp"
#include <array>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <vector>
namespace R = champions::simulation::rta;
namespace H = R::creature_hp;
namespace {
unsigned checks = 0;
void check(bool value, const char *message) {
  ++checks;
  if (!value)
    throw std::runtime_error(message);
}
template <class E = H::Error, class F> void rejects(F f, const char *message) {
  bool caught = false;
  try {
    f();
  } catch (const E &) {
    caught = true;
  }
  check(caught, message);
}
struct Overlay final : H::ReadOverlay {
  std::uint32_t first = 0x100;
  std::array<std::uint8_t, 4> value{0x11, 0x22, 0x33, 0x44};
  bool active = true;
  bool covers(std::uint32_t a) const override {
    return a >= first && std::uint64_t(a) < std::uint64_t(first) + value.size();
  }
  std::uint8_t read_byte(std::uint32_t a) const override {
    if (!active)
      throw H::Error("expired authored owner");
    if (!covers(a))
      throw H::Error("uncovered byte");
    return value.at(a - first);
  }
};
void memory_overlay() {
  H::Memory m;
  auto backend = std::make_shared<Overlay>();
  m.poke(0xfe, 8, UINT64_C(0x8877665544332211));
  const auto backing = m.bytes();
  m.attach_overlay(backend);
  check(
      m.load(0xfe, 8) == UINT64_C(0x8877443322112211),
      "read merges unaligned backing and current overlay bytes little-endian");
  check(m.bytes() == backing && m.log().empty(),
        "read does not mutate backing or create logs");
  backend->value[0] = 0xab;
  check(m.load(0x100, 1) == 0xab,
        "external backend mutation is visible immediately");
  auto image = m.effective_bytes(0xfe, 8);
  backend->value[0] = 0xcd;
  check(image == std::vector<std::uint8_t>(
                     {0x11, 0x22, 0xab, 0x22, 0x33, 0x44, 0x77, 0x88}),
        "effective snapshot owns its read values");
  check(m.load(0x100, 4) == 0x443322cd,
        "prior snapshot does not pin live values");
  for (unsigned width = 1; width <= 8; ++width) {
    rejects([&] { m.poke(width == 1 ? 0x100 : 0xff, width, UINT64_MAX); },
            "mixed span poke rejects if it intersects graph");
    check(m.bytes() == backing && m.log().empty(),
          "poke failure writes no prefix and creates no log");
  }
  // A one-byte span immediately before coverage is still writable.
  m.poke(0xff, 1, 0x99);
  check(m.load(0xff, 1) == 0x99, "uncovered adjacent byte stays writable");
  const auto before = m.bytes();
  for (auto width : {2U, 4U, 8U}) {
    rejects([&] { m.store(0xff, width, UINT64_MAX); },
            "mixed store refuses entire span");
    check(m.bytes() == before && m.log().back().address == 0xff &&
              m.log().back().size == width,
          "rejected store records attempt without backing mutation");
    check(m.log().back().value ==
              (width == 8 ? UINT64_MAX : (UINT64_C(1) << (width * 8)) - 1),
          "attempted store width masks value");
  }
  const auto count = m.log().size();
  m.store(0x100, 0, 123);
  m.poke(0x100, 0, 123);
  check(m.load(0x100, 0) == 0 && m.log().size() == count + 1 &&
            m.log().back().value == 0 && m.bytes() == before,
        "zero-width operations retain legacy behavior inside coverage");
  auto other = std::make_shared<Overlay>();
  rejects([&] { m.attach_overlay(other); }, "second overlay refused");
  rejects([&] { m.attach_overlay(backend); },
          "duplicate attachment refused explicitly");
  rejects([&] { m.detach_overlay(other); }, "wrong detach token refused");
  std::shared_ptr<const H::ReadOverlay> counterfeit(
      backend.get(), [](const H::ReadOverlay *) {});
  rejects([&] { m.detach_overlay(counterfeit); },
          "same address with a different owner is not the detach token");
  H::Memory copied = m;
  check(copied.load(0x100, 4) == m.load(0x100, 4) &&
            copied.bytes() == m.bytes(),
        "copy owns backing while sharing safe overlay token");
  backend->active = false;
  rejects([&] { m.load(0x100, 1); },
          "expired backend produces an explicit read error");
  rejects([&] { copied.effective_bytes(0xff, 8); },
          "expired effective snapshot never returns a partial image");
  rejects([&] { m.poke(0x100, 1, 0); },
          "expired graph remains read-only until explicit detach");
  check(m.load(0xff, 1) == 0x99,
        "uncovered scalar memory remains usable after backend expiration");
  auto token = m.overlay();
  backend.reset();
  m.detach_overlay(token);
  check(!m.overlay() && m.load(0x100, 4) == 0x66554433,
        "detach restores actual old backing, not invented graph stores");
  rejects([&] { m.detach_overlay(token); },
          "repeated detach refuses missing attachment");
  copied.detach_overlay(copied.overlay());
  check(copied.load(0x100, 4) == 0x66554433,
        "independent copied attachment detaches safely");
  H::Memory empty;
  rejects([&] { empty.attach_overlay({}); }, "null attachment refused");
  auto missing = std::make_shared<Overlay>();
  empty.attach_overlay(missing);
  check(empty.effective_bytes(0xff, 6) ==
                std::vector<std::uint8_t>({0, 0x11, 0x22, 0x33, 0x44, 0}) &&
            empty.bytes().empty(),
        "effective reads leave absent padding absent");
  rejects<H::ResourceLimit>(
      [&] { empty.effective_bytes(0, H::MAX_MEMORY_BYTES + 1); },
      "effective output budget enforced");
  rejects<H::ResourceLimit>([&] { empty.effective_bytes(UINT32_MAX, 2); },
                            "effective raw32 span cannot wrap");
  rejects<H::ResourceLimit>(
      [&] { empty.effective_bytes(UINT64_C(0x100000000), 0); },
      "zero-width invalid address still refuses");
  check(empty.effective_bytes(UINT32_MAX, 0).empty(),
        "zero-byte observation at last valid address accepted");
  H::Limits limits;
  limits.memory_bytes = 0;
  H::Memory no_backing(limits);
  no_backing.attach_overlay(missing);
  check(no_backing.load(0x100, 4) == 0x44332211 && no_backing.bytes().empty(),
        "overlay does not consume sparse backing budget");
}
void model_write_prefix() {
  H::Model model(H::CallbackPolicy::ReferenceZero);
  constexpr std::uint32_t creature = 0x10000;
  auto overlay = std::make_shared<Overlay>();
  overlay->first = creature + 0x3b0;
  model.mem.attach_overlay(overlay);
  rejects([&] { model.take_damage(creature, 0, 0, R::ONE); },
          "Model write into managed bytes is refused");
  check(model.writes.size() == 1 && model.events.size() == 1 &&
            model.mem.log().size() == 1,
        "Model records write,event,and memory-store attempt before overlay "
        "refusal");
  check(model.writes[0].address == creature + 0x3b0 &&
            model.writes[0].size == 2 && model.writes[0].value == 0x7fff &&
            model.events[0].kind == H::EventKind::Write &&
            model.mem.log()[0].value == 0x7fff && model.mem.bytes().empty(),
        "error prefix keeps exact first arc write and no backing mutation");
  model.mem.detach_overlay(overlay);
  model.take_damage(creature, 0, 0, 0);
  check(model.writes.size() == 6,
        "overlay refusal unwinds Model depth and adds no failure latch");
}
void lifetime_tokens() {
  std::weak_ptr<Overlay> observed;
  {
    H::Memory m;
    auto o = std::make_shared<Overlay>();
    observed = o;
    m.attach_overlay(o);
    o.reset();
    check(!observed.expired(), "Memory owns attached backend token");
    m.detach_overlay(m.overlay());
  }
  check(observed.expired(),
        "detached overlay is released without dangling owner");
  auto owner = std::make_shared<Overlay>();
  {
    H::Memory temporary;
    temporary.attach_overlay(owner);
  }
  owner->active = false;
  check(owner.use_count() == 1,
        "Memory destruction releases token without invoking borrowed owner");
}

namespace O = R::objects;
namespace B = R::raw_handles;
constexpr std::uint32_t HEAD = 0x91000000, BACK = 0x91000010, FREE = 0x864ea8,
                        BASE = R::HandlePool::NODE_BASE;
struct Fixture {
  std::unique_ptr<O::World> world =
      std::make_unique<O::World>(O::CallbackPolicy::ReferenceBase);
  H::Model hp{H::CallbackPolicy::ReferenceZero};
  B::RawHandles bridge{*world, hp.mem};
  O::GameObject *a = world->new_object(0x400, {0, 0, 0});
  O::GameObject *b = world->new_object(0x400, {R::ONE, 0, 0});
  R::Cell *head = nullptr, *back = nullptr;
  Fixture() {
    bridge.bind_object(*a);
    bridge.bind_object(*b);
    head = &bridge.intern_raw_head(HEAD, "head");
    back = &bridge.intern_raw_head(BACK, "back");
  }
};
void shared_pool_and_identity() {
  Fixture f;
  auto &w = *f.world;
  auto &p = w.handles;
  auto &m = f.hp.mem;
  check(&f.bridge.pool() == &p && p.nodes().size() == 6096,
        "bridge injects actual World pool without replacement");
  check(m.load(FREE, 4) == BASE + 80 && m.load(BASE + 80, 4) == BASE + 100,
        "literal pool/free layout");
  m.poke(BASE + 80 + 18, 2, 0xbbaa);
  auto node = f.bridge.link_known(HEAD, f.b->id, *f.b);
  check(node == BASE + 80 && m.load(HEAD, 4) == BASE + 80 &&
            m.load(f.b->id, 4) == BASE + 100,
        "first pair consumes nodes zero and one in original order");
  check(m.load(node + 4, 4) == HEAD && m.load(node + 8, 4) == node + 20 &&
            m.load(node + 12, 4) == f.b->id && m.load(node + 20 + 8, 4) == node,
        "paired raw fields use physical heads and registered actor address");
  check(m.load(FREE, 4) == BASE + 120 && p.count_free() == 6090,
        "raw and typed views share one free-list state");
  const auto snap = f.bridge.snapshot();
  const auto ws = w.snapshot();
  w.destroy(*f.b, 3);
  check(m.load(HEAD, 4) == 0 && m.load(FREE, 4) == BASE + 100 &&
            p.count_free() == 6094,
        "external World destruction is immediately visible without refresh");
  check(m.load(node + 18, 2) == 0xbbaa && m.load(node + 12, 4) == f.b->id,
        "release retains raw payload and ordinary padding");
  check(snap.free == BASE + 120 && ws.objects[1].alive,
        "prior snapshots own values");
  const auto opaque = f.bridge.link(HEAD, f.a->id, 0xdeadbeef);
  auto *raw = p.nodes().at((opaque - BASE) / 20);
  const auto before = p.count_free();
  rejects<R::HandleError>([&] { p.remove(raw); },
                          "typed remove refuses opaque identity");
  check(p.count_free() == before && raw->object_raw == 0xdeadbeef,
        "typed refusal preserves graph before mutation");
  rejects<R::HandleError>([&] { R::HandlePool::items(*f.head); },
                          "typed list refuses opaque rather than null");
  auto ref = f.bridge.remove(opaque);
  check(ref.is_opaque() && ref.raw == 0xdeadbeef && !ref.object,
        "raw remove preserves opaque identity");
  auto rs = w.snapshot();
  bool found = false;
  for (const auto &l : rs.links)
    if (l.object_raw == 0xdeadbeef)
      found = true;
  check(found, "free-node opaque payload survives complete World snapshot");
  auto *replacement = w.new_object(0x400, {0, 0, 0});
  check(replacement->id == f.b->id && replacement->serial != f.b->serial,
        "reuse preserves separate allocation generation");
  rejects<B::Error>([&] { f.bridge.bind_object(*replacement); },
                    "implicit head generation rebinding refused");
  f.bridge.rebind_object(*replacement);
  check(m.load(replacement->id, 4) == 0,
        "explicit empty dead generation rebind accepted");
  auto rebound = f.bridge.link_known(HEAD, replacement->id, *replacement);
  check(m.load(rebound + 12, 4) == replacement->id,
        "new generation uses stable physical actor address");
  bool generation = false;
  for (const auto &x : f.bridge.snapshot().nodes)
    if (x.address == rebound)
      generation = x.object_serial == replacement->serial && !x.opaque;
  check(generation,
        "physical snapshot retains known generation independently of raw word");
}
void exhausted_pair() {
  Fixture f;
  auto &p = f.world->handles;
  for (std::size_t i = 0; i < 6091; ++i)
    p.add(*f.back, nullptr);
  auto *last = p.free;
  last->tag = 0xbeef;
  last->object_raw = 0xdeadcafe;
  rejects<R::HandleError>([&] { f.bridge.link_known(HEAD, f.b->id, *f.b); },
                          "second take failure uses full real pool");
  check(!p.free && last->tag == 0 && last->object_raw == 0xdeadcafe,
        "partial second take consumes first node, resets tag, retains payload");
  check(!f.head->next && !f.b->heads[0].next,
        "failed pair does not publish half linked nodes");
  check(f.hp.mem.load(FREE, 4) == 0 &&
            f.hp.mem.load(BASE + 6095 * 20 + 16, 2) == 0,
        "failed allocation is reflected immediately in raw reads");
  rejects<R::HandleError>([&] { f.bridge.link_known(HEAD, f.b->id, *f.b); },
                          "empty first take explicitly refuses");
}
void primary_free_node_clear() {
  Fixture f;
  auto &pool = f.world->handles;
  const auto initial_free = pool.count_free();
  for (std::size_t i = 1; i < initial_free; ++i)
    pool.add(*f.back, nullptr);
  auto *last = pool.free;
  // Keep the final real pool node on the free list while giving the target
  // list reciprocal links. Releasing it again would create a self-cycle.
  f.head->next = last;
  last->prev = f.head;
  rejects<B::Error>([&] { f.bridge.clear(HEAD); },
                    "clear refuses a primary node still on the free list");
  check(pool.free == last && f.head->next == last && last->prev == f.head &&
            last->next == nullptr && last->partner == nullptr,
        "primary-free-node refusal preserves the exact malformed graph");
}
void immediate_callback() {
  Fixture f;
  auto &m = f.hp.mem;
  f.bridge.intern_raw_head(f.a->id + 0x2e4, "last");
  f.bridge.link_known(HEAD, f.b->id, *f.b);
  m.poke(f.a->id + 0x228, 4, 0x42c80000);
  m.poke(f.b->id + 0xc, 4, 32);
  unsigned clears = 0, links = 0;
  f.hp.callbacks["handle_clear"] = [&](H::Model &, const auto &args) {
    ++clears;
    f.bridge.clear(args.at(0));
    f.world->destroy(*f.b, 3);
    check(m.load(HEAD, 4) == 0,
          "HP callback observes external destruction immediately");
  };
  f.hp.callbacks["handle_link"] = [&](H::Model &, const auto &args) {
    ++links;
    f.bridge.link(args.at(0), args.at(1), args.at(2));
  };
  const auto seed = f.world->shared.state;
  f.hp.take_damage(f.a->id, f.b->id, 0x40, 0x40800000);
  check(clears == 1 && links == 1 && m.load(f.a->id + 0x228, 4) == 0x42c00000,
        "real HP flow uses shared handle callbacks and exact 100 minus 4");
  check(f.hp.draws == 0 && f.world->shared.state == seed,
        "handle and HP adapter adds no random draws");
  f.hp.callbacks["handle_clear"] = [&](H::Model &, const auto &args) {
    f.bridge.clear(args.at(0));
    throw std::runtime_error("authored failure");
  };
  const auto old = m.load(f.a->id + 0x228, 4);
  rejects<std::runtime_error>(
      [&] { f.hp.take_damage(f.a->id, f.b->id, 0, R::ONE); },
      "callback failure escapes");
  check(m.load(f.a->id + 0x2e4, 4) == 0 && m.load(f.a->id + 0x228, 4) == old,
        "callback unlink retained while final HP write is not invented");
}
void scope_and_capacity() {
  Fixture f;
  f.bridge.bind_head(HEAD, *f.head, "head");
  rejects<B::Error>([&] { f.bridge.bind_head(HEAD, *f.head, "changed"); },
                    "idempotent binding cannot silently rename metadata");
  rejects<B::Error>([&] { f.bridge.bind_head(0x93000000, *f.head, "head"); },
                    "one Cell cannot acquire ambiguous physical addresses");
  rejects<B::Error>([&] { f.bridge.clear(0xdead0000); },
                    "unknown raw target back-head is not inferred");
  auto raw = f.bridge.link(HEAD, HEAD, 0xdeadbeef);
  const auto before = f.world->handles.count_free();
  rejects<R::HandleError>(
      [&] { f.bridge.clear(HEAD); },
      "same-head partner adjacency retains original clear refusal");
  check(f.world->handles.count_free() == before && f.head->next != nullptr,
        "original same-head failure does not fabricate successful cleanup");
  // remove_ref can remove the valid pair even though clear's adjacency rule
  // refuses.
  auto ref = f.bridge.remove(raw);
  check(ref.raw == 0xdeadbeef && !f.head->next,
        "raw remove preserves original same-head pair operation");
  auto world = std::make_unique<O::World>(O::CallbackPolicy::ReferenceBase);
  H::Memory memory;
  B::RawHandles limited(*world, memory, B::Limits{1});
  auto *obj = world->new_object(0x400, {0, 0, 0});
  rejects<B::Error>(
      [&] { limited.bind_object(*obj); },
      "object registration requires capacity for all three heads");
  check(limited.snapshot().heads.empty(),
        "ordinary registration capacity failure publishes no prefix");
  limited.intern_raw_head(0x94000000, "first");
  rejects<B::Error>([&] { limited.intern_raw_head(0x94000010, "second"); },
                    "head registry explicit capacity enforced");
  check(limited.snapshot().heads.size() == 1,
        "capacity refusal keeps earlier binding");
  rejects<B::Error>([&] { f.bridge.intern_raw_head(0xffffffff, "wrap"); },
                    "raw head cannot wrap its four-byte span");
  rejects<B::Error>([&] { f.bridge.intern_raw_head(0x94000000, ""); },
                    "empty diagnostic label refused");
}
void bridge_boundaries_and_lifetimes() {
  static_assert(!std::is_copy_constructible_v<B::RawHandles>);
  static_assert(!std::is_move_constructible_v<B::RawHandles>);
  Fixture f;
  rejects<B::Error>([&] { f.bridge.bind_head(HEAD, *f.back, "conflict"); },
                    "same physical address cannot select a different Cell");
  rejects<B::Error>([&] { f.bridge.bind_head(HEAD + 1, *f.back, "overlap"); },
                    "partial head address overlap refused");
  rejects<B::Error>([&] { f.bridge.bind_head(BASE, *f.back, "pool"); },
                    "head cannot overlap managed pool");
  R::Cell borrowed;
  rejects<B::Error>(
      [&] { f.bridge.bind_head(0x92000000, borrowed, "foreign"); },
      "foreign Cell refuses before lifetime can dangle");
  f.world->handles.nodes()[0]->tag = 65536;
  rejects<B::Error>([&] { f.hp.mem.load(BASE + 16, 2); },
                    "byte-backed graph refuses unrepresentable semantic tag");
  check(f.world->handles.nodes()[0]->tag == 65536,
        "tag-width refusal does not silently truncate semantic field");
  f.world->handles.nodes()[0]->tag = 0;
  auto *free = f.world->handles.free;
  auto *next = free->next;
  free->next = free;
  rejects<B::Error>([&] { f.bridge.link_known(HEAD, f.a->id, *f.a); },
                    "free-list cycle refuses boundedly before allocation");
  check(f.world->handles.free == free && free->next == free && !f.head->next,
        "cycle refusal preserves existing malformed state");
  free->next = next;
  auto *bad = reinterpret_cast<R::Link *>(std::uintptr_t(1));
  free->partner = bad;
  rejects<B::Error>([&] { f.bridge.snapshot(); },
                    "unknown Link is rejected before dereference");
  check(free->partner == bad, "unknown-link refusal preserves field");
  free->partner = nullptr;
  f.bridge.link_known(HEAD, f.a->id, *f.a);
  auto *front = f.head->next;
  auto *partner = front->partner;
  front->partner = front;
  rejects<B::Error>([&] { f.bridge.clear(HEAD); },
                    "self-partner refuses before double release creates cycle");
  check(f.head->next == front && front->partner == front,
        "self-partner refusal preserves prior graph");
  front->partner = f.back;
  rejects<B::Error>(
      [&] { f.bridge.clear(HEAD); },
      "partner Cell is refused before core release corrupts pool");
  check(f.head->next == front && front->partner == f.back,
        "partner-kind refusal preserves malformed input");
  auto *current_free = f.world->handles.free;
  front->partner = current_free;
  rejects<B::Error>([&] { f.bridge.clear(HEAD); },
                    "partner on free list is refused before unlink");
  check(f.world->handles.free == current_free && f.head->next == front,
        "free-partner refusal preserves allocation state");
  front->partner = partner;
  auto *first_node = f.world->handles.nodes()[0];
  const auto old_index = first_node->index;
  first_node->index = 1;
  rejects<B::Error>([&] { f.hp.mem.load(BASE, 4); },
                    "mutated physical node index refuses byte-backed read");
  rejects<B::Error>([&] { f.bridge.snapshot(); },
                    "mutated node index refuses physical snapshot");
  check(first_node->index == 1, "index refusal does not repair trusted state");
  first_node->index = old_index;
  auto *following = front->next;
  front->next = front;
  rejects<B::Error>([&] { f.bridge.clear(HEAD); },
                    "active-list next cycle refuses boundedly before unlink");
  check(f.head->next == front && front->next == front,
        "active-cycle refusal leaves prior graph");
  front->next = following;
  R::Iterator active(*f.head);
  rejects<B::Error>([&] { f.bridge.snapshot(); },
                    "unaddressed live iterator graph refuses");
  active.end();
  f.bridge.clear(HEAD);
  // This is deliberately not repaired/imported from a byte image.
  H::Memory memory;
  auto world = std::make_unique<O::World>(O::CallbackPolicy::ReferenceBase);
  auto bridge = std::make_unique<B::RawHandles>(*world, memory);
  auto token = memory.overlay();
  bridge.reset();
  rejects<B::Error>([&] { memory.load(BASE, 4); },
                    "bridge destruction invalidates retained raw reads");
  rejects<H::Error>([&] { memory.poke(BASE, 4, 0); },
                    "expired graph remains read-only");
  memory.detach_overlay(token);
  check(memory.load(BASE, 4) == 0,
        "explicit token detach restores sparse fallback");
  auto m = std::make_unique<H::Memory>();
  auto b = std::make_unique<B::RawHandles>(*world, *m);
  m.reset();
  b.reset();
  check(true, "Memory may die before bridge without destructor dereference");
  H::Memory late;
  auto dead = std::make_unique<B::RawHandles>(*world, late);
  auto retained_lifetime = world->handle_lifetime().lock();
  check(retained_lifetime && *retained_lifetime,
        "caller can retain lifetime token while World lives");
  world.reset();
  check(!*retained_lifetime,
        "World invalidates token despite an externally retained strong owner");
  rejects<B::Error>([&] { dead->snapshot(); },
                    "snapshot refuses dead World even with strong token");
  rejects<B::Error>([&] { dead->pool(); },
                    "pool access refuses dead World even with strong token");
  rejects<B::Error>(
      [&] { late.load(BASE, 4); },
      "World early destruction is detected before dangling access");
  dead.reset();
  late.detach_overlay(late.overlay());
}

} // namespace
int main() {
  try {
    memory_overlay();
    model_write_prefix();
    lifetime_tokens();
    shared_pool_and_identity();
    exhausted_pair();
    primary_free_node_clear();
    immediate_callback();
    bridge_boundaries_and_lifetimes();
    scope_and_capacity();
    std::cout << checks << " raw-handle checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "raw-handle check " << checks << ": " << e.what() << '\n';
    return 1;
  }
}
