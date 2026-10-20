#include "champions/fx_world_save.hpp"
#include <cstring>
#include <limits>

namespace champions::rta::fx::sim {
namespace {
constexpr std::uint8_t MAGIC[8] = {'O', 'C', 'F', 'X', 'W', 'L', 'D', 0};
constexpr std::size_t MAX_COUNT = 1u << 24;

struct Out {
  SaveBytes b;
  void u(std::uint64_t v, unsigned n) {
    for (unsigned i = 0; i < n; ++i) b.push_back(std::uint8_t(v >> (8 * i)));
  }
  void u8(std::uint64_t v) { u(v, 1); }
  void u16(std::uint64_t v) { u(v, 2); }
  void u32(std::uint64_t v) { u(v, 4); }
  void u64(std::uint64_t v) { u(v, 8); }
  void i32(std::int64_t v) { u32(std::uint32_t(std::int32_t(v))); }
  void i64(std::int64_t v) { u64(std::uint64_t(v)); }
  void boolean(bool v) { u8(v ? 1 : 0); }
  void f64(double d) {
    std::uint64_t bits;
    std::memcpy(&bits, &d, 8);
    u64(bits);
  }
  void count(std::size_t n) {
    if (n > MAX_COUNT) throw WorldSaveError("effect world collection too large to save");
    u32(n);
  }
  void text(const std::string &s) {
    count(s.size());
    b.insert(b.end(), s.begin(), s.end());
  }
};
struct In {
  const SaveBytes &b;
  std::size_t at = 0;
  std::uint64_t u(unsigned n) {
    if (n > b.size() - at) throw WorldSaveError("truncated effect world snapshot");
    std::uint64_t v = 0;
    for (unsigned i = 0; i < n; ++i) v |= std::uint64_t(b[at++]) << (8 * i);
    return v;
  }
  std::uint8_t u8() { return std::uint8_t(u(1)); }
  std::uint16_t u16() { return std::uint16_t(u(2)); }
  std::uint32_t u32() { return std::uint32_t(u(4)); }
  std::uint64_t u64() { return u(8); }
  std::int32_t i32() { return std::int32_t(u32()); }
  std::int64_t i64() { return std::int64_t(u64()); }
  bool boolean() {
    const auto v = u8();
    if (v > 1) throw WorldSaveError("invalid boolean in effect world snapshot");
    return v != 0;
  }
  double f64() {
    const std::uint64_t bits = u64();
    double d;
    std::memcpy(&d, &bits, 8);
    return d;
  }
  std::size_t count(std::size_t stride = 1) {
    const std::uint32_t n = u32();
    if (n > MAX_COUNT || (stride && n > (b.size() - at) / stride))
      throw WorldSaveError("effect world collection exceeds the snapshot");
    return n;
  }
  std::string text() {
    const auto n = count(1);
    std::string s(b.begin() + std::ptrdiff_t(at), b.begin() + std::ptrdiff_t(at + n));
    at += n;
    return s;
  }
};

using namespace exact;
using namespace render;
using decals::DecalStore;

void put(Out &o, const Vec3 &v) {
  for (auto r : v) o.u32(r);
}
void get(In &in, Vec3 &v) {
  for (auto &r : v) r = in.u32();
}
void put(Out &o, const lighting::Vec3 &v) {
  o.f64(v.x);
  o.f64(v.y);
  o.f64(v.z);
}
void get(In &in, lighting::Vec3 &v) {
  v.x = in.f64();
  v.y = in.f64();
  v.z = in.f64();
}
template <std::size_t N> void put_ints(Out &o, const std::array<int, N> &a) {
  for (int v : a) o.i32(v);
}
template <std::size_t N> void get_ints(In &in, std::array<int, N> &a) {
  for (int &v : a) v = in.i32();
}
template <std::size_t N> void put_doubles(Out &o, const std::array<double, N> &a) {
  for (double v : a) o.f64(v);
}
template <std::size_t N> void get_doubles(In &in, std::array<double, N> &a) {
  for (double &v : a) v = in.f64();
}

void put(Out &o, const ParticleDef &d) {
  for (int v : {d.list_index, d.slot, d.live, d.callback}) o.i32(v);
  for (std::int32_t v : {d.u0, d.v0, d.u1, d.v1}) o.i32(v);
  for (double v : {d.gravity, d.size_rate, d.size0}) o.f64(v);
  put_doubles(o, d.c0);
  put_doubles(o, d.crate);
  for (int v : {d.kind, d.life_class, d.tile, d.life}) o.i32(v);
  o.f64(d.line_gravity);
  o.f64(d.line_lag);
}
void get(In &in, ParticleDef &d) {
  for (int *v : {&d.list_index, &d.slot, &d.live, &d.callback}) *v = in.i32();
  for (std::int32_t *v : {&d.u0, &d.v0, &d.u1, &d.v1}) *v = in.i32();
  for (double *v : {&d.gravity, &d.size_rate, &d.size0}) *v = in.f64();
  get_doubles(in, d.c0);
  get_doubles(in, d.crate);
  for (int *v : {&d.kind, &d.life_class, &d.tile, &d.life}) *v = in.i32();
  d.line_gravity = in.f64();
  d.line_lag = in.f64();
}
void put(Out &o, const ListState &l) {
  o.count(l.pages.size());
  for (int p : l.pages) o.i32(p);
  o.i32(l.head);
  o.i32(l.tail);
}
void get(In &in, ListState &l) {
  l.pages.resize(in.count(4));
  for (int &p : l.pages) p = in.i32();
  l.head = in.i32();
  l.tail = in.i32();
}
void put(Out &o, const ListSet &s) {
  for (const auto &l : s) put(o, l);
}
void get(In &in, ListSet &s) {
  for (auto &l : s) get(in, l);
}
void put(Out &o, const lighting::Node &n) {
  put(o, n.pos);
  put_ints(o, n.model_rgb);
  put_ints(o, n.world_rgb);
  for (double v : {n.model_radius, n.world_radius, n.model_falloff, n.world_falloff, n.floor}) o.f64(v);
  o.i32(n.layer);
  o.boolean(n.tint_decals);
}
lighting::Node get_node(In &in) {
  lighting::Node n({0, 0, 0}, {0, 0, 0}, {0, 0, 0}, 0);
  get(in, n.pos);
  get_ints(in, n.model_rgb);
  get_ints(in, n.world_rgb);
  for (double *v : {&n.model_radius, &n.world_radius, &n.model_falloff, &n.world_falloff, &n.floor}) *v = in.f64();
  n.layer = in.i32();
  n.tint_decals = in.boolean();
  return n;
}
} // namespace

// The single place with access to the private state (friend of EffectWorld,
// ParticleSystem and DecalStore).
struct WorldCodec {
  static void write(Out &o, const EffectWorld &w) {
    // Resident definitions first: slots refer to them by address.
    o.count(w.definitions_.size());
    for (const auto &kv : w.definitions_) {
      o.u32(kv.first);
      put(o, kv.second);
    }
    auto address_of = [&w](const ParticleDef *d) -> std::uint32_t {
      for (const auto &kv : w.definitions_)
        if (&kv.second == d) return kv.first;
      throw WorldSaveError("a resident particle slot outside the world's definitions");
    };
    // ParticleSystem.
    const ParticleSystem &p = w.particles;
    o.i32(p.clock_);
    o.i32(p.dropped_);
    o.i64(p.group_seq_);
    o.i32(p.current_);
    o.count(p.free_pages_.size());
    for (int f : p.free_pages_) o.i32(f);
    for (const ParticleDef *d : p.slots_) {
      o.boolean(d != nullptr);
      if (d) o.u32(address_of(d));
    }
    put(o, p.global_);
    for (const auto &s : p.group_lists_) put(o, s);
    o.count(p.groups_.size());
    for (const auto &g : p.groups_) {
      o.i64(g.handle);
      put_doubles(o, g.matrix);
      o.i32(g.idle);
    }
    o.count(p.memory_.size());
    for (const auto &c : p.memory_)
      for (auto v : c) o.u16(std::uint16_t(v));
    // Trails and beams.
    for (const Trail &t : w.trails.trails) {
      for (const auto &pt : t.points)
        for (auto v : pt) o.u16(std::uint16_t(v));
      o.u16(t.handle);
      o.u16(std::uint16_t(t.clock));
      o.u8(t.oldest);
      o.u8(t.newest);
      o.u8(std::uint8_t(t.row));
      o.u8(t.flags);
      o.u16(std::uint16_t(t.lifetime));
      for (auto v : t.rgba) o.u8(v);
      o.u8(t.width4);
    }
    o.u32(w.trails.generation);
    for (const Beam &b : w.beams.beams) {
      o.boolean(b.active);
      o.boolean(b.in_sim_tick);
      o.u32(b.handle);
      o.i32(b.depth);
      put(o, b.from);
      put(o, b.to);
      o.u32(b.jitter);
      o.u32(b.scale);
      o.u16(b.trail_a);
      o.u16(b.trail_b);
      o.i32(b.expiry);
      o.count(b.nodes.size());
      for (const auto &n : b.nodes) {
        put(o, n.off);
        put(o, n.drift);
        o.u32(n.count);
      }
    }
    // Glow queue.
    o.count(w.glows.records.size());
    for (const Glow &g : w.glows.records) {
      put(o, g.pos);
      o.u32(g.size);
      o.u32(g.bias);
      for (auto v : g.rgba) o.u8(v);
      o.u32(g.tex);
      o.u16(g.angle);
      o.u16(std::uint16_t(g.flags));
    }
    // Decals.
    const DecalStore &d = w.decals;
    o.i64(d.detail);
    o.count(d.free_.size());
    for (int f : d.free_) o.i32(f);
    for (int g : d.grid_) o.i32(g);
    for (const auto &blk : d.blocks_) {
      for (int v : {blk.index, blk.count}) o.i32(v);
      o.i32(blk.zmin);
      o.i32(blk.zmax);
      o.i32(blk.counted);
      for (const auto &dc : blk.decals) {
        put_ints(o, dc.rgb);
        for (std::int32_t v : {dc.x4, dc.y4, dc.z4, dc.counted, dc.c4, dc.s4, dc.tex}) o.i32(v);
        o.i64(dc.kind);
      }
    }
    for (const auto &g : d.growers_) {
      o.boolean(g.has_value());
      if (!g) continue;
      for (int v : {g->mode, g->block, g->index}) o.i32(v);
      o.u32(g->angle);
      o.u32(g->size);
      o.u32(g->final_size);
    }
    o.i32(d.total_);
    // Level light, generators, flags and counters.
    put(o, w.level.sun_direction);
    put_ints(o, w.level.sun);
    put_ints(o, w.level.ambient);
    o.i32(w.level.base);
    o.i32(w.level.layer);
    o.f64(w.level.scale);
    o.u32(w.cosmetic.state);
    o.u64(w.libc.state);
    for (bool m : w.made.made) o.boolean(m);
    o.u32(w.draw_frame);
    o.boolean(w.in_sim_tick);
    o.u8(w.waitq == WaitQ::WaitFirst ? 0 : 1);
    o.u32(w.stale_zw[0]);
    o.u32(w.stale_zw[1]);
    o.count(w.glow_dims.size());
    for (const auto &kv : w.glow_dims) {
      o.u32(kv.first);
      o.i32(kv.second.first);
      o.i32(kv.second.second);
    }
    o.i64(w.missing_definitions);
    o.count(w.missing_by_definition.size());
    for (const auto &kv : w.missing_by_definition) {
      o.u32(kv.first);
      o.i64(kv.second);
    }
    o.count(w.unplaced_groups.size());
    for (auto g : w.unplaced_groups) o.u32(g);
    o.i32(w.sim_frame_);
    o.count(w.effects_.size());
    for (const auto &e : w.effects_) {
      put_doubles(o, e.light.colour);
      put_doubles(o, e.light.rate);
      for (double v : {e.light.radius, e.light.radius_rate, e.light.floor}) o.f64(v);
      o.i32(e.light.life);
      o.i32(e.light.end);
      o.boolean(e.light.signed_colour);
      put(o, e.light.pos);
      o.u32(e.follow);
      put(o, e.node);
    }
    o.count(w.linked_.size());
    for (const auto &kv : w.linked_) {
      o.u64(kv.first);
      put(o, kv.second);
    }
  }

  static void read(In &in, EffectWorld &w) {
    w.definitions_.clear();
    for (std::size_t n = in.count(4 + 4 * 8 + 8 * 13), k = 0; k < n; ++k) {
      const auto address = in.u32();
      ParticleDef d;
      get(in, d);
      if (!w.definitions_.emplace(address, d).second) throw WorldSaveError("duplicate particle definition");
    }
    ParticleSystem &p = w.particles;
    p.clock_ = in.i32();
    p.dropped_ = in.i32();
    p.group_seq_ = in.i64();
    p.current_ = in.i32();
    p.free_pages_.clear();
    for (std::size_t n = in.count(4), k = 0; k < n; ++k) {
      const int f = in.i32();
      if (f < 0 || f >= PAGE_COUNT) throw WorldSaveError("particle page outside the pool");
      p.free_pages_.push_back(f);
    }
    for (auto &slot : p.slots_) {
      slot = nullptr;
      if (!in.boolean()) continue;
      const auto it = w.definitions_.find(in.u32());
      if (it == w.definitions_.end()) throw WorldSaveError("a resident slot names an absent definition");
      slot = &it->second;
    }
    get(in, p.global_);
    for (auto &s : p.group_lists_) get(in, s);
    p.groups_.resize(in.count(8 + 96 + 4));
    for (auto &g : p.groups_) {
      g.handle = in.i64();
      get_doubles(in, g.matrix);
      g.idle = in.i32();
    }
    if (in.count(16) != p.memory_.size()) throw WorldSaveError("particle memory size");
    for (auto &c : p.memory_)
      for (auto &v : c) v = std::int16_t(in.u16());
    if (p.current_ < -1 || p.current_ >= GROUP_COUNT) throw WorldSaveError("current particle set");
    for (Trail &t : w.trails.trails) {
      for (auto &pt : t.points)
        for (auto &v : pt) v = std::int16_t(in.u16());
      t.handle = in.u16();
      t.clock = std::int16_t(in.u16());
      t.oldest = in.u8();
      t.newest = in.u8();
      if (t.oldest > 0x7F || t.newest > 0x7F) throw WorldSaveError("trail ring index");
      t.row = std::int8_t(in.u8());
      t.flags = in.u8();
      t.lifetime = std::int16_t(in.u16());
      for (auto &v : t.rgba) v = in.u8();
      t.width4 = in.u8();
    }
    w.trails.generation = in.u32();
    for (Beam &b : w.beams.beams) {
      b.active = in.boolean();
      b.in_sim_tick = in.boolean();
      b.handle = in.u32();
      b.depth = in.i32();
      get(in, b.from);
      get(in, b.to);
      b.jitter = in.u32();
      b.scale = in.u32();
      b.trail_a = in.u16();
      b.trail_b = in.u16();
      b.expiry = in.i32();
      b.nodes.resize(in.count(28));
      for (auto &n : b.nodes) {
        get(in, n.off);
        get(in, n.drift);
        n.count = in.u32();
      }
    }
    w.glows.records.resize(in.count(32));
    if (w.glows.records.size() > GLOW_MAX) throw WorldSaveError("glow queue above 0x400");
    for (Glow &g : w.glows.records) {
      get(in, g.pos);
      g.size = in.u32();
      g.bias = in.u32();
      for (auto &v : g.rgba) v = in.u8();
      g.tex = in.u32();
      g.angle = in.u16();
      g.flags = std::int16_t(in.u16());
    }
    DecalStore &d = w.decals;
    d.detail = in.i64();
    d.free_.clear();
    for (std::size_t n = in.count(4), k = 0; k < n; ++k) {
      const int f = in.i32();
      if (f < 0 || f >= decals::BLOCKS) throw WorldSaveError("decal block outside the store");
      d.free_.push_back(f);
    }
    for (int &g : d.grid_) {
      g = in.i32();
      if (g < -1 || g >= decals::BLOCKS) throw WorldSaveError("decal grid cell");
    }
    for (auto &blk : d.blocks_) {
      blk.index = in.i32();
      blk.count = in.i32();
      if (blk.count < 0 || blk.count > decals::PER_BLOCK) throw WorldSaveError("decal block count");
      blk.zmin = in.i32();
      blk.zmax = in.i32();
      blk.counted = in.i32();
      for (auto &dc : blk.decals) {
        get_ints(in, dc.rgb);
        for (std::int32_t *v : {&dc.x4, &dc.y4, &dc.z4, &dc.counted, &dc.c4, &dc.s4, &dc.tex}) *v = in.i32();
        dc.kind = in.i64();
      }
    }
    for (auto &g : d.growers_) {
      g.reset();
      if (!in.boolean()) continue;
      decals::Grower v;
      v.mode = in.i32();
      v.block = in.i32();
      v.index = in.i32();
      if (v.block < 0 || v.block >= decals::BLOCKS || v.index < 0 || v.index >= decals::PER_BLOCK)
        throw WorldSaveError("decal grower");
      v.angle = in.u32();
      v.size = in.u32();
      v.final_size = in.u32();
      g = v;
    }
    d.total_ = in.i32();
    get(in, w.level.sun_direction);
    get_ints(in, w.level.sun);
    get_ints(in, w.level.ambient);
    w.level.base = in.i32();
    w.level.layer = in.i32();
    w.level.scale = in.f64();
    w.cosmetic.state = in.u32();
    w.libc.state = in.u64();
    for (bool &m : w.made.made) m = in.boolean();
    w.draw_frame = in.u32();
    w.in_sim_tick = in.boolean();
    const auto q = in.u8();
    if (q > 1) throw WorldSaveError("VU wait reading");
    w.waitq = q ? WaitQ::Vu1CppReading : WaitQ::WaitFirst;
    w.stale_zw[0] = in.u32();
    w.stale_zw[1] = in.u32();
    w.glow_dims.clear();
    for (std::size_t n = in.count(12), k = 0; k < n; ++k) {
      const auto word = in.u32();
      const auto width = in.i32(), height = in.i32();
      w.glow_dims[word] = {width, height};
    }
    w.missing_definitions = in.i64();
    w.missing_by_definition.clear();
    for (std::size_t n = in.count(12), k = 0; k < n; ++k) {
      const auto address = in.u32();
      w.missing_by_definition[address] = in.i64();
    }
    w.unplaced_groups.clear();
    for (std::size_t n = in.count(4), k = 0; k < n; ++k) w.unplaced_groups.insert(in.u32());
    w.sim_frame_ = in.i32();
    w.effects_.clear();
    for (std::size_t n = in.count(64), k = 0; k < n; ++k) {
      lighting::LightEffect light({0, 0, 0, 0}, {0, 0, 0, 0}, 0, 0, 1, 0);
      get_doubles(in, light.colour);
      get_doubles(in, light.rate);
      for (double *v : {&light.radius, &light.radius_rate, &light.floor}) *v = in.f64();
      light.life = in.i32();
      light.end = in.i32();
      light.signed_colour = in.boolean();
      get(in, light.pos);
      const auto follow = in.u32();
      w.effects_.push_back(EffectWorld::Effect{light, follow, get_node(in)});
    }
    w.linked_.clear();
    for (std::size_t n = in.count(16), k = 0; k < n; ++k) {
      const auto key = in.u64();
      w.linked_.emplace(key, get_node(in));
    }
  }
};

SaveBytes save_world(const EffectWorld &world) {
  Out o;
  for (auto c : MAGIC) o.u8(c);
  o.u32(WORLD_SAVE_VERSION);
  WorldCodec::write(o, world);
  return std::move(o.b);
}

void load_world(EffectWorld &target, const SaveBytes &bytes) {
  auto decode = [&bytes](EffectWorld &w) {
    In in{bytes};
    for (auto c : MAGIC)
      if (in.u8() != c) throw WorldSaveError("not an effect world snapshot");
    if (in.u32() != WORLD_SAVE_VERSION) throw WorldSaveError("unsupported effect world snapshot version");
    WorldCodec::read(in, w);
    if (in.at != bytes.size()) throw WorldSaveError("trailing bytes after the effect world snapshot");
  };
  {
    EffectWorld scratch;
    decode(scratch);
    if (save_world(scratch) != bytes) throw WorldSaveError("noncanonical effect world snapshot");
  }
  // The same bytes decoded the same way: this cannot be refused now.
  decode(target);
}
} // namespace champions::rta::fx::sim
