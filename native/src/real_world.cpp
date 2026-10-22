// WORLD -> water services and level-flow moving pieces. Evidence: research
// t3/research-levels 36036da docs/formats/world.md (header, chunk, water and
// moving-piece records) and docs/level-flow.md sections 11-12 and 16.4;
// t3/research-anim 2502e2b docs/spec/water-services.md (fields the services
// read); docs/monsters-2.md AutoProp section 3 (AutoDoor states and
// setPassable). No original assets or executable instructions are included.
#include "champions/real_world.hpp"
#include "champions/player_wading.hpp"
#include "champions/core_math.hpp"
#include "champions/terrain_file.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>

namespace champions::real_world {
namespace {
std::string lower(std::string s) {
  for (auto &c : s)
    c = char(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
void put32(std::vector<std::uint8_t> &b, std::size_t at, std::uint32_t v) {
  if (at > b.size() || b.size() - at < 4)
    throw Error("WORLD relocation outside input");
  for (unsigned i = 0; i < 4; ++i)
    b[at + i] = std::uint8_t(v >> (8 * i));
}
std::uint32_t relocated(std::uint32_t offset) {
  if (offset > UINT32_MAX - WaterImage::BASE)
    throw Error("WORLD offset outside relocated image");
  return WaterImage::BASE + offset;
}
// Property value by key, case-insensitive. Records carry "key=value" strings.
std::optional<std::string> property(const lf::Record &r, const std::string &key) {
  const auto want = lower(key);
  for (const auto &p : r.props) {
    const auto eq = p.find('=');
    if (eq != std::string::npos && lower(p.substr(0, eq)) == want)
      return p.substr(eq + 1);
  }
  return std::nullopt;
}
std::optional<long> integer(const std::optional<std::string> &s) {
  if (!s || s->empty())
    return std::nullopt;
  char *end = nullptr;
  const long v = std::strtol(s->c_str(), &end, 10);
  if (end == s->c_str())
    return std::nullopt;
  return v;
}
} // namespace

WaterImage::WaterImage(const std::vector<std::uint8_t> &world,
                       const wg::Geometry &g)
    : image_(world),
      memory_({}, [this](cw::Address a, std::size_t n) { return load(a, n); }),
      service_(memory_) {
  if (image_.size() > UINT32_MAX - BASE)
    throw Error("WORLD too large for relocated image");
  const auto tree = g.header_words[0x04 / 4];
  // The tree offset points at the 0x4567 marker; the loader skips it and the
  // services read the root node through world +0x04.
  put32(image_, 0x04, tree ? relocated(tree + 2) : 0);
  put32(image_, 0x24, relocated(g.header_words[0x24 / 4]));
  for (const auto &w : g.water) {
    const auto &c = g.chunks.at(w.chunk);
    put32(image_, std::size_t(g.header_words[0x24 / 4]) + 0x3c * w.chunk,
          relocated(c.data_offset));
    put32(image_, std::size_t(c.data_offset) + 8, 0); // no ripple grid yet
  }
}
std::uint64_t WaterImage::load(cw::Address a, std::size_t n) const {
  std::uint64_t v = 0;
  for (std::size_t i = 0; i < n; ++i) {
    const std::uint64_t at = std::uint64_t(a) + i;
    if (at >= BASE && at - BASE < image_.size())
      v |= std::uint64_t(image_[std::size_t(at - BASE)]) << (8 * i);
  }
  return v; // outside the image (including the cut-skip byte): zero
}
WaterImage::Test WaterImage::test(Raw radius, const cw::Vec3 &p) {
  // Scratch result word just below the image; never part of the WORLD.
  constexpr cw::Address out = BASE - 0x10;
  service_.clear_writes();
  Test t;
  t.in_water = service_.water_test(radius, BASE, p, out) != 0;
  if (t.in_water)
    t.height = Raw(memory_.load(out, 4));
  return t;
}
std::uint32_t WaterImage::disturb(Raw strength, Raw radius, const cw::Vec3 &p) {
  service_.clear_writes();
  return service_.water_disturb(strength, radius, BASE, p);
}
std::vector<std::uint32_t> WaterImage::disturb_chunks(Raw radius,
                                                      const cw::Vec3 &p) {
  // The box and capacity of Service::water_disturb: x and y within the
  // radius, z from one below to 60 above, at most 15 chunks.
  constexpr Raw ONE = 0x3f800000, F60 = 0x42700000;
  constexpr cw::Address CUT_SKIP = 0x44fcec;
  if (memory_.load(CUT_SKIP, 1))
    return {};
  if (!simulation::rta::lt(0, radius))
    throw cw::Unsupported("water disturbance radius must be positive");
  namespace sr = simulation::rta;
  const cw::Vec3 lo{sr::sub(p[0], radius), sr::sub(p[1], radius), sr::sub(p[2], ONE)};
  const cw::Vec3 hi{sr::add(p[0], radius), sr::add(p[1], radius), sr::add(p[2], F60)};
  service_.clear_writes();
  const auto table = static_cast<std::uint32_t>(memory_.load(BASE + 0x24, 4));
  std::vector<std::uint32_t> out;
  for (const auto chunk : service_.query(BASE, lo, hi, 16)) {
    if (chunk < table || (chunk - table) % 0x3c != 0)
      throw Error("water query returned an address outside the chunk table");
    out.push_back((chunk - table) / 0x3c);
  }
  return out;
}

std::vector<lf::Piece> pieces(const wg::Geometry &g) {
  std::vector<lf::Piece> out;
  for (const auto &m : g.pieces) {
    lf::Piece p;
    p.tx = m.rest_translation[0];
    p.ty = m.rest_translation[1];
    p.tz = m.rest_translation[2];
    p.cosine = m.rest_cosine;
    p.sine = m.rest_sine;
    if (m.element < 0)
      throw Error("moving piece has a negative collision element");
    p.element = std::size_t(m.element);
    std::vector<bool> mirrored;
    for (std::int32_t i = 0; i < m.count; ++i)
      mirrored.push_back((g.chunks.at(std::size_t(m.first_chunk + i)).flags &
                          0x40) != 0);
    p.mirrored = !mirrored.empty() && mirrored.front();
    p.chunk_mirrored = mirrored;
    out.push_back(std::move(p));
  }
  return out;
}

bool claims_piece(const lf::Record &r) {
  if (lower(r.cls) == "doorswing")
    return true;
  const auto door = integer(property(r, "AutoDoor"));
  return door && *door != 0;
}
std::optional<bool> autodoor_start_open(const lf::Record &r) {
  const auto start = integer(property(r, "StartState"));
  if (!start || *start <= 0)
    return std::nullopt;
  const auto states = integer(property(r, "States"));
  if (!states || *states == 0) // AutoDoor default: 2 states, 1 passable
    return *start < 2 ? std::optional<bool>(*start == 1) : std::nullopt;
  if (*start >= *states)
    return std::nullopt; // setState ignores n >= States
  const auto passable = property(r, "Passable");
  if (!passable)
    return false;
  if (std::size_t(*start) >= passable->size() || *start >= 10)
    return false;
  return (*passable)[std::size_t(*start)] == '1';
}

PieceScene::PieceScene(const std::vector<std::uint8_t> &world,
                       const wg::Geometry &g,
                       const std::vector<lf::Record> &records)
    : geometry_(g) {
  terrain_ = std::make_unique<wq::Level>(
      simulation::rta::terrain_file::decode(world));
  pieces_ = std::make_unique<lf::Pieces>(pieces(g), *terrain_);
  claimer_.assign(pieces_->data().size(), -1);
  // Constructors claim in record order; the later of two equally near pieces
  // wins and an already claimed nearest piece is not replaced.
  for (std::size_t i = 0; i < records.size(); ++i) {
    if (!claims_piece(records[i]))
      continue;
    ++attempts_;
    const int piece = pieces_->claim(records[i].position);
    if (piece < 0)
      continue;
    claimer_[std::size_t(piece)] = int(i);
    if (lower(records[i].cls) != "doorswing") {
      const auto open = autodoor_start_open(records[i]);
      if (open && *open) {
        pieces_->autodoor(piece, true);
        ++started_open_;
      }
    }
  }
  pieces_->disable_unclaimed();
}
std::size_t PieceScene::claimed() const {
  return std::size_t(std::count_if(claimer_.begin(), claimer_.end(),
                                   [](int c) { return c >= 0; }));
}
bool PieceScene::collision(std::size_t piece) const {
  const auto element = terrain_->data().pieces.at(piece);
  return !(terrain_->data().elements.at(std::size_t(element)).flags & 2);
}
void PieceScene::set_door(std::size_t piece, bool open) {
  if (piece >= claimer_.size() || claimer_[piece] < 0)
    throw Error("only a claimed piece can open or close");
  pieces_->autodoor(int(piece), open);
}
std::vector<PieceDraw> PieceScene::draws() const {
  std::vector<PieceDraw> out;
  const auto &poses = pieces_->poses();
  for (std::size_t i = 0; i < geometry_.pieces.size(); ++i) {
    const auto &m = geometry_.pieces[i];
    for (std::int32_t k = 0; k < m.count; ++k) {
      PieceDraw d;
      d.piece = int(i);
      d.chunk = std::uint32_t(m.first_chunk + k);
      if (!poses[i].chunks.empty())
        d.affine = pose_affine(geometry_.chunks.at(d.chunk),
                               poses[i].chunks.at(std::size_t(k)));
      out.push_back(d);
    }
  }
  return out;
}

std::array<double, 12> pose_affine(const wg::Chunk &rest,
                                   const lf::ChunkTransform &pose) {
  const auto a = wg::chunk_matrix(rest.flags, rest.rotation);
  const auto b = wg::chunk_matrix(
      rest.flags, {pose.cosine, pose.sine, rest.rotation[2]});
  const double det = a[0] * a[3] - a[1] * a[2];
  if (!std::isfinite(det) || det == 0)
    throw Error("moving piece rest rotation is singular");
  // inverse of the rest matrix
  const std::array<double, 4> ai{a[3] / det, -a[1] / det, -a[2] / det,
                                 a[0] / det};
  const std::array<double, 4> m{b[0] * ai[0] + b[1] * ai[2],
                                b[0] * ai[1] + b[1] * ai[3],
                                b[2] * ai[0] + b[3] * ai[2],
                                b[2] * ai[1] + b[3] * ai[3]};
  const double dx = (double(pose.tx) - rest.translation[0]) * 0.25,
               dy = (double(pose.ty) - rest.translation[1]) * 0.25,
               dz = (double(pose.tz) - rest.translation[2]) * 0.25;
  return {m[0], m[1], 0, b[0] * dx + b[1] * dy,
          m[2], m[3], 0, b[2] * dx + b[3] * dy,
          0,    0,    1, dz};
}

bool Wader::step(Raw frame, std::uint32_t level_flags, const cw::Vec3 &position,
                 bool moving) {
  namespace pw = simulation::rta::player_wading;
  pw::Host host;
  host.water_test = [this](const pw::Vec3 &at, Raw radius) {
    // Scratch result word below the image, as WaterImage::test reads it.
    const auto t = image_.test(radius, {at[0], at[1], at[2]});
    pw::TestResult r;
    r.hit = t.in_water;
    r.has_height = t.in_water;
    r.height = t.height;
    return r;
  };
  host.water_disturb = [this](const pw::Vec3 &at, Raw strength, Raw radius) {
    ++totals_.disturbs;
    const auto n = disturb_(strength, radius, {at[0], at[1], at[2]});
    if (n)
      ++totals_.touched;
    return Raw(n);
  };
  host.hurt = [this] { ++totals_.hurts; };
  host.draw = [this] { ++totals_.draws; };
  host.splash = [this](Raw) { ++totals_.splashes; };
  host.burst = [](const pw::Vec3 &, const pw::TestResult &) {};
  pw::Input in;
  in.frame = frame;
  in.level_flags = level_flags;
  in.speed = moving ? simulation::rta::ONE : 0;
  in.position = {position[0], position[1], position[2]};
  in.in_water = in_water_;
  in.airborne = false;
  const bool after = pw::step(in, host);
  ++totals_.steps;
  if (after && !in_water_)
    ++totals_.entries;
  if (!after && in_water_)
    ++totals_.exits;
  if (after)
    ++totals_.ticks_in_water;
  in_water_ = after;
  return after;
}
} // namespace champions::real_world
