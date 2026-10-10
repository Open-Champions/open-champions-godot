// Authored from the pinned RTA ref_objects.py contract; see
// objects_lifetime.cpp.
#include "objects_internal.hpp"
namespace champions::simulation::rta::objects {
using namespace detail;
std::vector<GameObject *> World::collect_in_box(const Vec3 &lo, const Vec3 &hi,
                                                std::int64_t limit) {
  auto rlo = std::max(asr(std::uint32_t(to_int(lo[1])) + 0x3ba0, 8), 0),
       rhi = std::min(asr(std::uint32_t(to_int(hi[1])) + 0x3c60, 8), 119);
  auto clo = std::max(asr(std::uint32_t(to_int(lo[0])) + 0x3ba0, 8), 0),
       chi = std::min(asr(std::uint32_t(to_int(hi[0])) + 0x3c60, 8), 119);
  std::vector<GameObject *> out;
  auto bound = std::int64_t(s32(std::uint32_t(limit))) - 1;
  std::size_t work = 0;
  for (auto row = rlo; row <= rhi; ++row)
    for (auto col = clo; col <= chi; ++col) {
      auto *n = grid[std::size_t(row) * 120 + std::size_t(col)].next;
      while (n) {
        budget(work);
        auto *raw = n->obj;
        n = n->next;
        if (!raw)
          continue;
        auto &o = checked_object(raw);
        if ((o.flags & exclude_mask) || (o.flags & FLAG_PREDICTION_COPY))
          continue;
        auto x = o.pos[0], y = o.pos[1], z = o.pos[2], r = o.radius;
        if (lt(add(x, r), lo[0]) || lt(hi[0], sub(x, r)))
          continue;
        if (lt(add(y, r), lo[1]) || lt(hi[1], sub(y, r)))
          continue;
        if (lt(hi[2], z) || lt(add(z, o.height), lo[2]))
          continue;
        if (std::int64_t(out.size()) >= bound)
          return out;
        out.push_back(&o);
      }
    }
  return out;
}
std::vector<GameObject *>
World::collect_in_sphere(Raw radius, const Vec3 &centre, Cell &head) {
  bool owned = &head == &off_grid || &head == &always_draw ||
               &head == &delete_list || &head == &run_active;
  for (auto &c : grid)
    owned = owned || &c == &head;
  for (auto &c : grid_b)
    owned = owned || &c == &head;
  for (auto &c : run15)
    owned = owned || &c == &head;
  for (auto &c : run60)
    owned = owned || &c == &head;
  for (auto &entry : extra_cells_)
    owned = owned || entry.second.get() == &head;
  for (auto &entry : storage_)
    for (auto &c : entry->heads)
      owned = owned || &c == &head;
  if (!owned)
    throw ObjectError("sphere result head must be owned by this World");
  Vec3 lo{}, hi{};
  for (unsigned i = 0; i < 3; ++i) {
    lo[i] = sub(centre[i], radius);
    hi[i] = add(centre[i], radius);
  }
  std::vector<GameObject *> out;
  for (auto *o : collect_in_box(lo, hi, 512)) {
    if (o->flags & 13)
      continue;
    auto dx = sub(centre[0], o->pos[0]), dy = sub(centre[1], o->pos[1]),
         dz = sub(centre[2], o->pos[2]);
    auto distance = sqrt(add(add(mul(dx, dx), mul(dy, dy)), mul(dz, dz)));
    if (lt(distance, add(radius, o->radius))) {
      handles.add(head, o);
      out.push_back(o);
    }
  }
  return out;
}
std::vector<GameObject *> World::find_by_class(const std::string &name,
                                               std::int64_t limit) {
  std::vector<GameObject *> out;
  bool done = false;
  auto scan = [&](Cell &head) {
    std::vector<GameObject *> walked;
    walk_list(head, exclude_mask, [&](GameObject &o) { walked.push_back(&o); });
    for (auto *o : walked)
      if (o->type_name == name) {
        out.push_back(o);
        if (limit > 0 && out.size() == std::uint64_t(limit - 1)) {
          done = true;
          return;
        }
      }
  };
  scan(run_active);
  if (done)
    return out;
  for (auto &c : run15) {
    scan(c);
    if (done)
      return out;
  }
  for (auto &c : run60) {
    scan(c);
    if (done)
      return out;
  }
  return out;
}
std::pair<Vec3, int> World::find_free_spot(const Vec3 &centre,
                                           std::int64_t spread,
                                           std::int64_t radius, bool any_height,
                                           const Terrain &blocked,
                                           const Terrain &height_fn) {
  auto fs = from_int(std::uint32_t(spread)),
       fr = from_int(std::uint32_t(radius));
  auto radius_sq = from_int(std::uint32_t(radius) * std::uint32_t(radius));
  auto result = centre;
  std::size_t draws = 0;
  unsigned tries = 0;
  while (tries < 50) {
    Vec3 ball{};
    for (;;) {
      budget(draws);
      ball = {shared.signed_unit(), shared.signed_unit(), shared.signed_unit()};
      if (!lt(ONE, add(add(mul(ball[0], ball[0]), mul(ball[1], ball[1])),
                       mul(ball[2], ball[2]))))
        break;
    }
    auto ox = mul(ball[0], fs), oy = mul(ball[1], fs);
    Vec3 cand{add(ox, centre[0]), add(oy, centre[1]), centre[2]};
    if (!blocked)
      throw ObjectError("missing terrain blocked handler");
    auto blocked_copy = blocked;
    if (!eq(blocked_copy(fr, cand), 0)) {
      ++tries;
      continue;
    }
    Vec3 probe{add(ox, centre[0]), add(oy, centre[1]), add(0, centre[2])};
    if (!height_fn)
      throw ObjectError("missing terrain height handler");
    auto height_copy = height_fn;
    auto h = height_copy(fr, probe);
    if (!lt(sub(h, centre[2]), 0x40c00000) && !any_height) {
      ++tries;
      continue;
    }
    result = {add(centre[0], ox), add(centre[1], oy), h};
    Vec3 lo{sub(result[0], fr), sub(result[1], fr), sub(h, 0x47c35000)},
        hi{add(result[0], fr), add(result[1], fr), add(h, 0x47c35000)};
    bool hit = false;
    for (auto *o : collect_in_box(lo, hi, 128)) {
      if (o->flags & 1)
        continue;
      auto dy = sub(o->pos[1], result[1]), dx = sub(o->pos[0], result[0]);
      auto reach = add(radius_sq, mul(o->radius, o->radius));
      if (le(add(mul(dx, dx), mul(dy, dy)), reach)) {
        hit = true;
        break;
      }
    }
    if (!hit)
      break;
    ++tries;
  }
  return {result, tries != 50 ? 1 : 0};
}
GameObject *World::player1_marker(std::uint32_t addr, const Vec3 &pos,
                                  std::int64_t angle, Props props,
                                  const std::string &prev,
                                  const std::string &entrance, bool gate,
                                  const Terrain &blocked,
                                  const Terrain &height) {
  auto *o = construct(addr, pos, {}, props);
  o->flags |= 3;
  o->vtable = VT_PLAYER1;
  o->type_name = "7Player1";
  o->angle = std::uint32_t(angle) & 65535;
  auto rank = start_rank;
  if (rank == 3)
    return o;
  auto source = props_find(props, "from"), name = props_find(props, "name");
  bool take = false;
  if (rank <= 0) {
    take = true;
    start_rank = 1;
  } else if (rank < 2 && props_int(props, "default")) {
    take = true;
    start_rank = 1;
  }
  if (source && *source == prev && !gate) {
    take = true;
    start_rank = 2;
  } else if (gate && source && *source == "gate") {
    take = true;
    start_rank = 3;
  }
  if (name && !gate && *name == entrance) {
    take = true;
    start_rank = 3;
  }
  if (take)
    for (unsigned i = 0; i < 4; ++i) {
      auto spot = find_free_spot(o->pos, 36, 24, false, blocked, height);
      auto draw = shared.draw();
      spawn_pos[i] = spot.first;
      spawn_facing[i] = (o->angle + draw % 4000 - 2000) & 65535;
    }
  return o;
}
GameObject *create_object(const std::optional<std::string> &name, Vec3 pos,
                          std::int64_t angle, Props props,
                          const Factory &factory) {
  const auto captured_name = name;
  if (!factory)
    return nullptr;
  auto fn = factory;
  auto *o = fn(captured_name, pos, s16(std::uint32_t(angle)), props);
  if (o)
    o->class_name = captured_name;
  return o;
}
Record::Record(std::optional<std::string> name, std::int64_t a, Vec3 p,
               Props prop, Bytes data)
    : class_name(std::move(name)), angle(s16(std::uint32_t(a))), pos(p),
      props(std::move(prop)), state(std::move(data)) {}
RecordBlob::RecordBlob(std::vector<Record> r, bool state)
    : records(std::move(r)), has_state(state) {}
std::size_t RecordBlob::size() {
  if (!cached_size)
    build();
  return *cached_size;
}
Bytes RecordBlob::build() {
  bounded(records.size(), 32767);
  Bytes strings{0}, body;
  std::map<std::string, std::uint32_t> offsets;
  auto byte = [&](Bytes &out, std::uint32_t value, unsigned n) {
    bounded(out.size() + n);
    for (unsigned i = 0; i < n; ++i)
      out.push_back(std::uint8_t(value >> (i * 8)));
  };
  auto append_string = [&](const std::string &text) {
    if (strings.size() >= MAX_BYTES)
      throw ResourceLimit("record string capacity exceeded");
    bounded(text.size(), MAX_BYTES - strings.size() - 1);
    strings.insert(strings.end(), text.begin(), text.end());
    strings.push_back(0);
  };
  for (const auto &r : records) {
    if (r.angle < -32768 || r.angle > 32767)
      throw ObjectError("record angle outside signed16 range");
    std::uint32_t name = 0xffffffff;
    if (r.class_name) {
      auto found = offsets.find(*r.class_name);
      if (found == offsets.end()) {
        name = std::uint32_t(strings.size());
        offsets.emplace(*r.class_name, name);
        append_string(*r.class_name);
      } else
        name = found->second;
    }
    Bytes rec;
    byte(rec, name, 4);
    byte(rec, 0, 2);
    byte(rec, std::uint32_t(r.angle), 2);
    for (auto p : r.pos)
      byte(rec, p, 4);
    check_props(r.props);
    if (r.props)
      for (const auto &text : *r.props) {
        byte(rec, std::uint32_t(strings.size()), 4);
        append_string(text);
      }
    byte(rec, 0, 4);
    if (has_state) {
      while (rec.size() % 8)
        rec.push_back(0);
      bounded(r.state.size(), MAX_BYTES - rec.size());
      rec.insert(rec.end(), r.state.begin(), r.state.end());
    }
    while (rec.size() % 8)
      rec.push_back(0);
    bounded(rec.size(), 32767);
    rec[4] = std::uint8_t(rec.size());
    rec[5] = std::uint8_t(rec.size() >> 8);
    bounded(rec.size(), MAX_BYTES - body.size());
    body.insert(body.end(), rec.begin(), rec.end());
  }
  cached_size = 8 + body.size();
  bounded(*cached_size);
  Bytes out;
  byte(out, std::uint32_t(records.size()), 2);
  byte(out, has_state ? 1 : 0, 2);
  byte(out, std::uint32_t(*cached_size), 4);
  out.insert(out.end(), body.begin(), body.end());
  bounded(strings.size(), MAX_BYTES - out.size());
  out.insert(out.end(), strings.begin(), strings.end());
  return out;
}
std::size_t World::load_records(
    RecordBlob &blob, const Factory &factory, bool filter_only,
    const std::function<bool(const std::optional<std::string> &)> &filter,
    const std::function<void(GameObject &, const Bytes &)> &load_state) {
  blob.relocated = true;
  bounded(blob.records.size(), MAX_RETAINED);
  std::size_t record_bytes = 0;
  for (const auto &r : blob.records) {
    bounded(r.state.size(), MAX_BYTES - record_bytes);
    record_bytes += r.state.size();
    check_props(r.props);
  }
  const auto *records_data = blob.records.data();
  const auto original_records = blob.records;
  auto stable_records = [&] {
    if (blob.records.data() != records_data ||
        blob.records.size() != original_records.size())
      throw ObjectError("callback changed record container structure");
    for (std::size_t n = 0; n < original_records.size(); ++n) {
      const auto &a = blob.records[n];
      const auto &b = original_records[n];
      if (a.class_name != b.class_name || a.angle != b.angle ||
          a.pos != b.pos || a.props != b.props || a.state != b.state)
        throw ObjectError(
            "callback changed supplied record identity or fields");
    }
  };
  std::size_t work = 0;
  for (std::size_t index = 0; index < blob.records.size(); ++index) {
    budget(work);
    auto record = blob.records[index];
    if (filter_only) {
      if (!filter)
        continue;
      auto f = filter;
      const bool accepted = f(record.class_name);
      stable_records();
      if (!accepted)
        continue;
    }
    if (!factory)
      throw ObjectError("missing object factory");
    auto fn = factory;
    auto *o = fn(record.class_name, record.pos, record.angle, record.props);
    stable_records();
    if (o)
      o->class_name = record.class_name;
    if (blob.has_state && o) {
      if (!load_state && callback_policy == CallbackPolicy::RequireHandlers)
        throw ObjectError("missing saved-state loader");
      if (load_state) {
        auto load = load_state;
        load(*o, record.state);
        stable_records();
      }
    }
  }
  flush_delete_list();
  return blob.size();
}
} // namespace champions::simulation::rta::objects
