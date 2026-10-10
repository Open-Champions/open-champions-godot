#include "champions/combat_creature.hpp"
#include <algorithm>
#include <array>
#include <exception>
#include <map>

// Authored composition of ref_combat.py
// (7212ecfecbeae8dd2015f6c82897226f49a94695, SHA256
// b4523460760126de7e5b3ad0ebdee52f296f3665903eb45bd75e17a834bec781) and
// ref_creature.py (a0d89502ce1431375a6fc045b91a73039114e6b9, SHA256
// debec0ceb360d26e31ebeb777c8289ae35ebaab8e27c7453fbecbbf0495dc2b7).
// Mapping/authority/limits are explicit host policy; no handle algorithm is
// supplied here and no original virtual dispatch table is inferred.
namespace champions::simulation::rta::combat_creature {
namespace {
constexpr std::uint32_t extent_creature = 0x3b2;
constexpr std::size_t max_entries = 1024, max_regions = 1024, max_depth = 64;
constexpr std::uint64_t address_end = UINT64_C(0x100000000);
bool overlaps(std::uint32_t a, std::uint32_t n, std::uint32_t b,
              std::uint32_t m) {
  return std::uint64_t(a) < std::uint64_t(b) + m &&
         std::uint64_t(b) < std::uint64_t(a) + n;
}
void label_check(const std::string &label) {
  if (label.empty() || label.size() > 128)
    throw Error("callback/region label must contain 1..128 bytes");
}
struct Fields {
  std::uint32_t flags;
  std::array<Raw, 3> pos;
  std::uint64_t cflags;
  Raw hp;
  std::array<Raw, 5> status;
  bool operator==(const Fields &b) const {
    return flags == b.flags && pos == b.pos && cflags == b.cflags &&
           hp == b.hp && status == b.status;
  }
};
Fields fields(const combat::Obj &o) {
  if (o.flags > UINT32_MAX || o.pos.size() != 3 || o.status.size() != 5)
    throw Error("projected record requires flags32, exactly 3 positions and 5 "
                "status values");
  return {static_cast<std::uint32_t>(o.flags),
          {o.pos[0], o.pos[1], o.pos[2]},
          o.cflags,
          o.hp,
          {o.status[0], o.status[1], o.status[2], o.status[3], o.status[4]}};
}
} // namespace
struct Adapter::State {
  struct Entry {
    combat::Obj object;
    Binding binding;
    DamageCallback custom;
  };
  struct CallbackFrame {
    std::vector<Fields> expected;
    std::int64_t frame = 0;
  };
  Adapter &owner;
  Limits limits;
  combat::World world;
  creature_hp::Model model;
  std::vector<std::unique_ptr<Entry>> entries;
  std::vector<RawRegion> regions;
  std::map<std::string, std::pair<std::string, HandleCallback>> handles;
  DamageCallback before, after;
  std::string before_label, after_label;
  std::vector<CallbackFrame> callbacks;
  std::size_t depth = 0;
  State(Adapter &o, std::uint32_t seed, std::uint32_t cosmetic,
        creature_hp::CallbackPolicy policy, Limits l)
      : owner(o), limits(l), world(seed, cosmetic), model(policy, l.memory) {
    if (l.entries > max_entries || l.raw_regions > max_regions ||
        l.depth > max_depth || !l.depth)
      throw ResourceLimit("adapter limits exceed supported capacity");
    world.take_damage_fn = [this](combat::Obj &v, Raw a, combat::Obj *s,
                                  std::uint32_t f) { dispatch(v, a, s, f); };
  }
  Entry &entry(const combat::Obj &o) const {
    for (const auto &e : entries)
      if (&e->object == &o)
        return *e;
    throw Error("combat record is not registered in this adapter");
  }
  void setup_only() const {
    if (depth || !callbacks.empty())
      throw Error(
          "record/region registration is forbidden during an operation");
  }
  void range(std::uint32_t a, std::uint32_t n) const {
    if (!a || !n || std::uint64_t(a) + n > address_end)
      throw Error("binding/region needs a nonzero bounded raw32 span");
    if (overlaps(a, n, creature_hp::G_SIM_FRAME, 4))
      throw Error("binding/region overlaps the simulation frame word");
    for (const auto &e : entries)
      if (overlaps(a, n, e->binding.address, e->binding.extent))
        throw Error("binding/region overlaps an existing record");
    for (const auto &r : regions)
      if (overlaps(a, n, r.address, r.size))
        throw Error("binding/region overlaps declared raw backing");
  }
  std::vector<Fields> capture() const {
    std::vector<Fields> result;
    for (const auto &e : entries)
      if (e->binding.projection == Projection::CreatureFields)
        result.push_back(fields(e->object));
    return result;
  }
  void refresh_callbacks() {
    if (callbacks.empty())
      return;
    const auto current = capture();
    for (auto &c : callbacks) {
      c.expected = current;
      c.frame = world.frame;
    }
  }
  void check_callback() const {
    if (!callbacks.empty() && (capture() != callbacks.back().expected ||
                               world.frame != callbacks.back().frame))
      throw Error("projected typed mutation during a raw-memory callback is "
                  "unsupported");
  }
  void project_all() {
    const auto values = capture();
    if (world.frame < 0 || std::uint64_t(world.frame) > UINT32_MAX)
      throw Error("projected simulation frame must fit unsigned 32 bits");
    // Capacity preflight avoids half-publishing a set of projected fields.
    std::vector<creature_hp::Write> writes;
    writes.push_back(
        {creature_hp::G_SIM_FRAME, 4, static_cast<std::uint32_t>(world.frame)});
    std::size_t i = 0;
    for (const auto &e : entries) {
      if (e->binding.projection != Projection::CreatureFields)
        continue;
      const auto a = e->binding.address;
      const auto &v = values[i++];
      writes.push_back({a + 0xc, 4, v.flags});
      for (std::size_t k = 0; k < 3; ++k)
        writes.push_back({a + 0x10 + std::uint32_t(4 * k), 4, v.pos[k]});
      writes.push_back({a + 0xa8, 8, v.cflags});
      writes.push_back({a + 0x228, 4, v.hp});
      for (std::size_t k = 0; k < 5; ++k)
        writes.push_back({a + 0x340 + std::uint32_t(4 * k), 4, v.status[k]});
    }
    std::size_t missing = 0;
    for (const auto &w : writes)
      for (std::size_t k = 0; k < w.size; ++k)
        missing += !model.mem.bytes().count(w.address + std::uint32_t(k));
    if (model.mem.bytes().size() > limits.memory.memory_bytes ||
        missing > limits.memory.memory_bytes - model.mem.bytes().size())
      throw ResourceLimit("projected fields exceed sparse-memory capacity");
    for (const auto &w : writes)
      model.mem.poke(w.address, w.size, w.value);
    refresh_callbacks();
  }
  void publish_all() {
    for (const auto &e : entries) {
      if (e->binding.projection != Projection::CreatureFields)
        continue;
      auto &o = e->object;
      const auto a = e->binding.address;
      auto word = [&](std::uint32_t off) {
        return static_cast<std::uint32_t>(
            model.mem.load(std::uint64_t(a) + off, 4));
      };
      o.flags = word(0xc);
      o.pos = {word(0x10), word(0x14), word(0x18)};
      o.cflags = model.mem.load(std::uint64_t(a) + 0xa8, 8);
      o.hp = word(0x228);
      o.status = {word(0x340), word(0x344), word(0x348), word(0x34c),
                  word(0x350)};
    }
    world.frame =
        static_cast<std::uint32_t>(model.mem.load(creature_hp::G_SIM_FRAME, 4));
    refresh_callbacks();
  }
  template <class F> void raw_call(F fn) {
    if (callbacks.size() >= limits.depth)
      throw ResourceLimit("raw callback nesting limit exceeded");
    check_callback();
    publish_all();
    callbacks.push_back({capture(), world.frame});
    try {
      fn();
      check_callback();
    } catch (...) {
      const auto error = std::current_exception();
      callbacks.pop_back();
      publish_all();
      std::rethrow_exception(error);
    }
    callbacks.pop_back();
    publish_all();
  }
  void dispatch(combat::Obj &victim, Raw amount, combat::Obj *attacker,
                std::uint32_t flags) {
    auto &e = entry(victim);
    const auto source = attacker ? entry(*attacker).binding.address : 0;
    if (e.binding.dispatch == Dispatch::Custom) {
      const auto fn = e.custom;
      if (!fn)
        throw Error("custom damage dispatch has no supplied handler");
      fn(owner, victim, amount, attacker, flags);
      return;
    }
    project_all();
    try {
      if (before) {
        const auto fn = before;
        raw_call([&] { fn(owner, victim, amount, attacker, flags); });
      }
      model.take_damage(e.binding.address, source, flags, amount);
      publish_all();
      if (after) {
        const auto fn = after;
        raw_call([&] { fn(owner, victim, amount, attacker, flags); });
      }
    } catch (...) {
      const auto error = std::current_exception();
      publish_all();
      std::rethrow_exception(error);
    }
  }
  template <class F> auto operation(F fn) -> decltype(fn()) {
    if (depth >= limits.depth)
      throw ResourceLimit("adapter operation nesting limit exceeded");
    const bool nested_raw = !callbacks.empty();
    if (nested_raw) {
      check_callback();
      publish_all();
    }
    ++depth;
    try {
      auto result = fn();
      if (nested_raw)
        project_all();
      --depth;
      return result;
    } catch (...) {
      const auto error = std::current_exception();
      --depth;
      if (nested_raw)
        project_all();
      std::rethrow_exception(error);
    }
  }
};
Adapter::Adapter(std::uint32_t seed, std::uint32_t cosmetic,
                 creature_hp::CallbackPolicy policy, Limits limits)
    : state_(std::make_unique<State>(*this, seed, cosmetic, policy, limits)) {}
Adapter::~Adapter() = default;
combat::Obj &Adapter::create(std::uint32_t address, combat::Obj value,
                             Projection projection, Dispatch dispatch,
                             std::uint32_t extent) {
  auto &s = *state_;
  s.setup_only();
  if (s.entries.size() >= s.limits.entries)
    throw ResourceLimit("adapter record capacity exceeded");
  if ((projection != Projection::CreatureFields &&
       projection != Projection::None) ||
      (dispatch != Dispatch::BaseCreature && dispatch != Dispatch::Custom) ||
      (dispatch == Dispatch::BaseCreature &&
       projection != Projection::CreatureFields))
    throw Error(
        "base Creature dispatch requires its explicit field projection");
  if (projection == Projection::CreatureFields) {
    extent = extent_creature;
    (void)fields(value);
  }
  s.range(address, extent);
  auto e = std::make_unique<State::Entry>();
  e->object = std::move(value);
  e->binding = {address, s.entries.size() + 1, extent, projection, dispatch,
                {}};
  auto &out = e->object;
  s.entries.push_back(std::move(e));
  return out;
}
combat::Obj *Adapter::lookup(std::uint32_t address) noexcept {
  for (const auto &e : state_->entries)
    if (e->binding.address == address)
      return &e->object;
  return nullptr;
}
const combat::Obj *Adapter::lookup(std::uint32_t address) const noexcept {
  for (const auto &e : state_->entries)
    if (e->binding.address == address)
      return &e->object;
  return nullptr;
}
Binding Adapter::binding(const combat::Obj &o) const {
  return state_->entry(o).binding;
}
std::vector<Binding> Adapter::bindings() const {
  std::vector<Binding> out;
  for (const auto &e : state_->entries)
    out.push_back(e->binding);
  return out;
}
void Adapter::reserve_raw(std::uint32_t a, std::uint32_t n,
                          const std::string &label) {
  auto &s = *state_;
  s.setup_only();
  if (s.regions.size() >= s.limits.raw_regions)
    throw ResourceLimit("raw backing region capacity exceeded");
  label_check(label);
  s.range(a, n);
  s.regions.push_back({a, n, label});
}
std::vector<RawRegion> Adapter::raw_regions() const { return state_->regions; }
void Adapter::bind_custom(combat::Obj &o, const std::string &label,
                          DamageCallback fn) {
  label_check(label);
  if (!fn)
    throw Error("custom dispatch requires a callable");
  auto &e = state_->entry(o);
  e.custom = std::move(fn);
  e.binding.dispatch = Dispatch::Custom;
  e.binding.label = label;
}
void Adapter::set_handle_callback(const std::string &name,
                                  const std::string &label, HandleCallback fn) {
  if (name != "handle_clear" && name != "handle_link")
    throw Error("unsupported Creature HP handle callback name");
  label_check(label);
  if (!fn)
    throw Error("handle callback requires a callable");
  auto &s = *state_;
  s.handles[name] = {label, std::move(fn)};
  s.model.callbacks[name] = [&s, name](creature_hp::Model &,
                                       const std::vector<std::uint32_t> &args) {
    const auto handler = s.handles.at(name).second;
    s.raw_call([&] { handler(s.owner, args); });
  };
}
void Adapter::set_before_base(const std::string &label, DamageCallback fn) {
  label_check(label);
  if (!fn)
    throw Error("before-base hook requires a callable");
  state_->before_label = label;
  state_->before = std::move(fn);
}
void Adapter::set_after_base(const std::string &label, DamageCallback fn) {
  label_check(label);
  if (!fn)
    throw Error("after-base hook requires a callable");
  state_->after_label = label;
  state_->after = std::move(fn);
}
std::vector<CallbackBinding> Adapter::callback_bindings() const {
  std::map<std::string, std::string> labels;
  for (const auto &h : state_->handles)
    labels[h.first] = h.second.first;
  if (state_->before)
    labels["before_base"] = state_->before_label;
  if (state_->after)
    labels["after_base"] = state_->after_label;
  std::vector<CallbackBinding> out;
  for (const auto &e : labels)
    out.push_back({e.first, e.second});
  return out;
}
combat::World &Adapter::world() noexcept { return state_->world; }
const combat::World &Adapter::world() const noexcept { return state_->world; }
creature_hp::Model &Adapter::model() noexcept { return state_->model; }
const creature_hp::Model &Adapter::model() const noexcept {
  return state_->model;
}
void Adapter::project() {
  state_->setup_only();
  state_->project_all();
}
void Adapter::publish() {
  state_->setup_only();
  state_->publish_all();
}
void Adapter::take_damage(combat::Obj &v, Raw amount, combat::Obj *a,
                          std::uint32_t f) {
  state_->operation([&] {
    (void)state_->entry(v);
    if (a)
      (void)state_->entry(*a);
    state_->world.take_damage(v, amount, a, f);
    return 0;
  });
}
int Adapter::damage(combat::Obj &v, combat::Obj *a, Raw lo, Raw hi,
                    std::int64_t attack, std::uint32_t flags,
                    const combat::Descriptor *desc, std::int64_t no_crit) {
  return state_->operation([&] {
    (void)state_->entry(v);
    if (a)
      (void)state_->entry(*a);
    return combat::damage_apply(state_->world, v, a, lo, hi, attack, flags,
                                desc, no_crit);
  });
}
std::size_t Adapter::operation_depth() const noexcept { return state_->depth; }
std::size_t Adapter::raw_callback_depth() const noexcept {
  return state_->callbacks.size();
}
} // namespace champions::simulation::rta::combat_creature
