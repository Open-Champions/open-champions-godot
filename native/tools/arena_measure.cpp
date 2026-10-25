// Arena runs on the owner's private disc. Reads the executable and an archive
// directory and writes neither. Standard output is counts only: no archive
// names, level names, positions, or property strings. With --captures, each of
// the three cold arena indices (69, 70, 71; docs/LEVEL_FLOW.md and the
// level-spawn-2 report) also gets a top-down picture, an overlay picture and a
// frame trace. Those go to the given directory, which must be outside the
// tracked tree (work/private/ is gitignored). Not a CTest.
//
// The recipe records and the two level-name tables are read from the
// executable at the addresses in research t3/research-arena docs/arena-modes.md
// section 1. They stay in memory. Every scenario input that the research does
// not supply is named "stand-in" in the trace and in docs/ARENA_MODES.md.
#include "champions/arena_executable.hpp"
#include "champions/arena_modes.hpp"
#include "champions/core_arithmetic.hpp"
#include "champions/core_math.hpp"
#include "champions/level_flow.hpp"
#include "champions/objects.hpp"
#include "champions/terrain_file.hpp"
#include "champions/world_queries.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {
using namespace champions::simulation::rta;
using namespace champions::simulation::rta::arena_modes;
using namespace champions::simulation::rta::level_flow;
namespace fs = std::filesystem;

// Research addresses (docs/arena-modes.md section 1).
constexpr std::uint32_t NAMES_A = NAMES_A_TABLE;
constexpr std::uint32_t NAMES_B = NAMES_B_TABLE;
constexpr std::uint32_t RECIPES = RECIPE_TABLE;
constexpr std::uint32_t RECIPE_COUNT = RECIPE_COUNT_HALF;
// The three cold arena indices in paired-archive order (level-spawn-2 report).
constexpr unsigned CAPTURE_INDICES[] = {69, 70, 71};

std::string ascii_fold(std::string s, bool upper) {
  for (auto &c : s) {
    if (upper && c >= 'a' && c <= 'z')
      c = char(c - 'a' + 'A');
    if (!upper && c >= 'A' && c <= 'Z')
      c = char(c - 'A' + 'a');
  }
  return s;
}
bool read_file(const fs::path &path, std::vector<std::uint8_t> &out) {
  std::ifstream in(path, std::ios::binary);
  if (!in)
    return false;
  in.seekg(0, std::ios::end);
  const auto n = in.tellg();
  if (n < 0)
    return false;
  in.seekg(0);
  out.resize(static_cast<std::size_t>(n));
  if (!out.empty())
    in.read(reinterpret_cast<char *>(out.data()), static_cast<std::streamsize>(out.size()));
  return bool(in);
}
std::uint32_t u32(const std::vector<std::uint8_t> &b, std::size_t at) {
  return std::uint32_t(b[at]) | std::uint32_t(b[at + 1]) << 8 | std::uint32_t(b[at + 2]) << 16 |
         std::uint32_t(b[at + 3]) << 24;
}
std::uint16_t u16(const std::vector<std::uint8_t> &b, std::size_t at) {
  return std::uint16_t(b[at] | (b[at + 1] << 8));
}
std::string cstr(const std::vector<std::uint8_t> &b, std::size_t at) {
  if (at >= b.size())
    return {};
  auto end = at;
  while (end < b.size() && b[end])
    ++end;
  return std::string(reinterpret_cast<const char *>(b.data() + at), end - at);
}

// The library's ELF reader (arena_executable.hpp), fed from a file.
struct Exe : Executable {
  bool load(const fs::path &path) {
    std::vector<std::uint8_t> bytes;
    return read_file(path, bytes) && Executable::load(std::move(bytes));
  }
};

struct Lump {
  std::string name;
  std::vector<std::uint8_t> bytes;
};
bool sequential_lumps(const std::vector<std::uint8_t> &data, std::vector<Lump> &out) {
  if (data.size() < 0x800)
    return false;
  std::size_t pos = 0x800;
  for (std::size_t i = 0; i + 0x28 <= 0x800; i += 0x28) {
    if (!data[i])
      return true;
    std::size_t end = i;
    while (end < i + 32 && data[end])
      ++end;
    std::string name(reinterpret_cast<const char *>(data.data() + i), end - i);
    const auto size = u32(data, i + 0x24);
    if (pos > data.size() || size > data.size() - pos)
      return false;
    out.push_back({std::move(name),
                   std::vector<std::uint8_t>(data.begin() + static_cast<std::ptrdiff_t>(pos),
                                             data.begin() + static_cast<std::ptrdiff_t>(pos + size))});
    const auto padded = std::uint64_t(pos) + size + 0x7ff;
    if (padded < pos)
      return false;
    pos = static_cast<std::size_t>(padded & ~std::uint64_t(0x7ff));
  }
  return false;
}
bool lmp_entries(const std::vector<std::uint8_t> &blob, std::vector<Lump> &out) {
  if (blob.size() < 4)
    return false;
  const unsigned count = u16(blob, 0);
  if (4ull + std::uint64_t(count) * 12 > blob.size())
    return false;
  for (unsigned i = 0; i < count; ++i) {
    const auto rec = 4 + i * 12;
    const auto name_at = u32(blob, rec);
    const auto data_at = u32(blob, rec + 4);
    const auto size = u32(blob, rec + 8);
    if (std::uint64_t(data_at) + size > blob.size())
      return false;
    out.push_back({cstr(blob, name_at),
                   std::vector<std::uint8_t>(blob.begin() + data_at, blob.begin() + data_at + size)});
  }
  return true;
}

// Display only: never feeds the simulation.
double as_double(Raw raw) {
  float f;
  std::memcpy(&f, &raw, sizeof f);
  return double(f);
}

struct Mark {
  Vec3 pos{};
  int facing = 0;
  int kind = 0; // 0 entry, 1 wave slot 0, 2 wave slot 1, 3 respawn
};
struct Run {
  bool ok = false;
  std::string refusal;
  int wave = 0, waves_left = 0, classes = 0, ring_steps = 0, monster_builds_refused = 0;
  int intro_frames = 0, latched = 0, placed = 0, placed_floor = 0, wave_retry = 0;
  int deaths = 0, respawns = 0, respawn_floor = 0, respawn_refused = 0, camera_fallback = 0;
  int purchases = 0, overlay_frames = 0, glyphs = 0, empty_wave_refused = 0, next_wave = 0;
  int end_prompt = 0, end_frame = 0, rank_known = 0, fade_frames = 0, confirm = 0;
  int level_change = 0, next_on_disc = 0, kills = 0, intermission_frames = 0;
  int frames = 0;
  std::vector<Mark> marks;
  std::vector<std::array<double, 3>> glyph_path; // x, y, k
  std::ostringstream trace;
};

struct LevelInput {
  unsigned index = 0;
  std::string stem;
  world_queries::Level *level = nullptr;
  std::vector<Record> records;
};

struct Tables {
  std::vector<Recipe> recipes;
  NameTable names_a, names_b;
  std::set<std::string> on_disc; // folded stems of paired archives
};

void plant(Match &match, const std::vector<Record> &records) {
  // arena_init plants the origin at entry 0 with count 1 (LEVEL_FLOW.md,
  // research-levels section 18). The level factories then pass 0 for a
  // start and -1 for an alternate (arena-modes.md section 5).
  match.register_spawn(-1, SpawnPoint{ZERO, ZERO, ZERO, 0});
  for (const auto &record : records) {
    const SpawnPoint point{record.position[0], record.position[1], record.position[2], record.angle};
    if (record.cls == "ArenaPlayerStart")
      match.register_spawn(0, point);
    else if (record.cls == "ArenaPlayerAlternate")
      match.register_spawn(-1, point);
  }
}

Run run_level(const LevelInput &in, const Tables &tables, bool mode_a) {
  Run run;
  auto &level = *in.level;
  auto blocked = [&](Raw radius, const Vec3 &pos) {
    return world_queries::terrain_test_flag1(level, radius, pos);
  };
  auto height = [&](Raw radius, const Vec3 &pos) {
    return world_queries::terrain_height(level, radius, pos);
  };
  // Empty object list: the class-constructor radii stay refused (LEVEL_FLOW.md).
  objects::World world(objects::CallbackPolicy::ReferenceBase);
  int retries = 0, fallbacks = 0;
  const Search search = [&](const Vec3 &centre, int spread, int radius, std::uint32_t &seed) {
    if (spread == 0xf0)
      ++retries;
    if (spread == 0x48)
      ++fallbacks;
    world.shared.state = seed;
    const auto found = world.find_free_spot(centre, spread, radius, false, blocked, height);
    seed = world.shared.state;
    return Spot{found.first, found.second};
  };
  Match match;
  match.mode_a = mode_a;
  match.mode_b = true;
  match.num_players = mode_a ? 2 : 1;
  match.active_count = match.num_players;
  match.recipes = tables.recipes;
  match.names_a = tables.names_a;
  match.names_b = tables.names_b;
  match.search = search;
  match.seed = 0; // stand-in: seed 0, one of the two seeds research-levels 18 executed
  for (int i = 0; i < match.num_players; ++i) {
    match.max_hp[std::size_t(i)] = from_int(100);  // stand-in record values
    match.max_mana[std::size_t(i)] = from_int(50); // stand-in record values
    match.hp[std::size_t(i)] = match.max_hp[std::size_t(i)];
  }
  if (mode_a) {
    match.kill_limit = kill_choice(0); // front-end menu index 0 stores 5
    const auto folded = ascii_fold(in.stem, false);
    auto it = std::find_if(tables.names_a.begin(), tables.names_a.end(),
                           [&](const std::string &n) { return ascii_fold(n, false) == folded; });
    if (it == tables.names_a.end()) {
      run.refusal = "level is not in the mode-A table";
      return run;
    }
    match.map_index = int(it - tables.names_a.begin());
  }
  auto &t = run.trace;
  t << "mode " << (mode_a ? "A" : "B") << " players " << match.num_players << '\n';
  try {
    match.init();
    plant(match, in.records);
    for (std::size_t i = 0; i < match.points.size() && i < match.spawn_count; ++i)
      run.marks.push_back({{match.points[i].x, match.points[i].y, match.points[i].z},
                           int(match.points[i].angle), 0});
    t << "spawn count " << int(match.spawn_count) << '\n';
    const auto started = match.begin();
    run.wave = started.wave;
    run.waves_left = started.waves_left;
    t << "begin wave " << run.wave << " waves_left " << run.waves_left << " reset " << started.reset
      << " seed " << match.seed << '\n';
    if (!mode_a) {
      run.classes = int(started.classes.size());
      run.ring_steps = int(started.steps.size());
      if (run.classes) {
        try {
          monster_builder();
        } catch (const arena_modes::Unsupported &) {
          run.monster_builds_refused = run.classes;
        }
      }
      t << "wave 0 classes " << run.classes << " ring steps " << run.ring_steps
        << " builds refused " << run.monster_builds_refused << '\n';
    }
    // Wave placement for each slot, a0 != 0 (arena-modes.md section 5).
    std::array<Vec3, SCORE_SLOTS> pos{};
    for (int slot = 0; slot < match.num_players; ++slot) {
      const auto before = retries;
      auto picked = pick_spawn(1, slot, match.points.data(), match.spawn_count, match.seed, mode_a,
                               false, false, Vec3{}, match.active_count, search, Vec3{});
      match.seed = picked.seed;
      const auto z = player_ctor_z(picked.pos, PLACEMENT_RADIUS, height);
      picked.pos[2] = z;
      pos[std::size_t(slot)] = picked.pos;
      ++run.placed;
      if (z != world_queries::NO_FLOOR)
        ++run.placed_floor;
      run.wave_retry += retries - before;
      run.marks.push_back({picked.pos, picked.facing, 1 + slot});
      t << "place slot " << slot << " x " << as_double(picked.pos[0]) << " y "
        << as_double(picked.pos[1]) << " z " << as_double(z) << " facing " << picked.facing << '\n';
    }
    // Stand-in camera focus: slot 0's wave position. g_cameraFocus is not
    // supplied by any module here.
    match.focus = pos[0];
    match.camera = pos[0];
    // Scenario frames (stand-ins): a purchase in mode B, then deaths.
    std::vector<int> deaths = mode_a ? std::vector<int>{300, 450, 600, 750, 900}
                                     : std::vector<int>{600, 800, 1000, 1200};
    const int purchase_frame = 300;
    int pending_respawn = -1;
    const int frame_cap = 3000;
    // Overlay stand-ins: the HUD anchor ints and the projected point are
    // owner values; fixed here so the curve can be drawn.
    const int sx[SCORE_SLOTS] = {320, 320, 320, 320}, sy[SCORE_SLOTS] = {224, 224, 224, 224};
    const int ax[SCORE_SLOTS] = {64, 576, 64, 576}, ay[SCORE_SLOTS] = {400, 400, 48, 48};
    for (int frame = 1; frame <= frame_cap; ++frame) {
      run.frames = frame;
      match.frame = std::uint32_t(frame); // stand-in for g_simFrame
      if (!mode_a && frame == purchase_frame) {
        match.gold[0] = 150; // stand-in pickup
        match.buyer_count = 1;
        match.buyers[0] = 0;
      }
      if (!deaths.empty() && frame == deaths.front()) {
        deaths.erase(deaths.begin());
        Killer killer{0x60, 1, 0};
        const auto result = on_killed(match.slots.data(), match.num_players, 0,
                                      mode_a ? &killer : nullptr, mode_a, match.num_players);
        ++run.deaths;
        match.hp[0] = ZERO;
        // Stand-in: the writer that runs the dead countdown down is not
        // documented. Zero it at death so the documented scan can decide.
        match.dead[0] = 0;
        pending_respawn = frame + 1;
        t << "frame " << frame << " death " << int(result) << " lives " << s16(match.slots[0].lives)
          << " kills1 " << s16(match.slots[1].kills) << '\n';
      }
      if (frame == pending_respawn) {
        pending_respawn = -1;
        const auto fb = fallbacks;
        const auto out = match.respawn(0, nullptr, 0);
        if (out.ran) {
          ++run.respawns;
          auto p = out.pos;
          p[2] = player_ctor_z(p, PLACEMENT_RADIUS, height);
          if (p[2] != world_queries::NO_FLOOR)
            ++run.respawn_floor;
          run.camera_fallback += fallbacks - fb;
          run.marks.push_back({p, out.facing, 3});
          t << "frame " << frame << " respawn x " << as_double(p[0]) << " y " << as_double(p[1])
            << " facing " << out.facing << " seed " << match.seed << '\n';
        } else {
          ++run.respawn_refused;
          t << "frame " << frame << " respawn refused lives " << s16(match.slots[0].lives) << '\n';
        }
      }
      const bool ready = match.end_flag && eq(match.fade, ONE);
      const auto before_intro = s16(match.intro);
      const auto out = match.tick(ready, false);
      if (before_intro > 0)
        ++run.intro_frames;
      if (out.intro_latch)
        run.latched = 1;
      if (out.granted) {
        ++run.purchases;
        t << "frame " << frame << " purchase lives " << s16(match.slots[0].lives) << '\n';
      }
      if (out.empty_wave_refused)
        ++run.empty_wave_refused;
      if (out.next_wave) {
        ++run.next_wave;
        t << "frame " << frame << " next_wave(" << out.next_wave_arg << ") recorded, body refused\n";
      }
      if (out.end_prompt) {
        ++run.end_prompt;
        run.end_frame = frame;
        run.rank_known = match.rank_known ? 1 : 0;
        t << "frame " << frame << " end prompt rank_known " << run.rank_known << '\n';
      }
      if (match.intermission)
        ++run.intermission_frames;
      if (out.level_change) {
        run.level_change = 1;
        run.next_on_disc = tables.on_disc.count(ascii_fold(match.pending_name, true)) ? 1 : 0;
        t << "frame " << frame << " level change, next on disc " << run.next_on_disc << '\n';
        break;
      }
      if (out.shutdown) {
        run.confirm = 1;
        run.next_on_disc = tables.on_disc.count(ascii_fold(match.pending_name, true)) ? 1 : 0;
        t << "frame " << frame << " confirm, shutdown, next on disc " << run.next_on_disc << '\n';
        break;
      }
      // Draw side. arena_drawHud, then (stand-in) one arena_drawPrompt fade
      // step per frame while the end flag is set: the draw-task route to the
      // prompt goes through helper 0x307bbc, which was not executed.
      const auto hud = draw_hud(match.mode_b, match.mode_a, false, false, match.slots.data(),
                                match.num_players, sx, sy, ax, ay);
      if (!hud.glyphs.empty()) {
        ++run.overlay_frames;
        run.glyphs += int(hud.glyphs.size());
        for (const auto &g : hud.glyphs)
          run.glyph_path.push_back({as_double(g.out[0]), as_double(g.out[1]), double(g.k)});
      }
      if (match.end_flag) {
        match.fade = fade_step(match.fade);
        ++run.fade_frames;
      }
    }
    run.kills = mode_a ? s16(match.slots[1].kills) : s16(match.slots[0].kills);
    run.ok = true;
  } catch (const std::exception &error) {
    run.refusal = error.what();
    t << "refused: " << error.what() << '\n';
  }
  return run;
}

void put_px(std::vector<std::uint8_t> &img, int w, int h, int x, int y, int r, int g, int b) {
  if (x < 0 || y < 0 || x >= w || y >= h)
    return;
  const auto at = (std::size_t(y) * std::size_t(w) + std::size_t(x)) * 3;
  img[at] = std::uint8_t(r);
  img[at + 1] = std::uint8_t(g);
  img[at + 2] = std::uint8_t(b);
}
void write_ppm(const fs::path &path, const std::vector<std::uint8_t> &img, int w, int h) {
  std::ofstream out(path, std::ios::binary);
  out << "P6\n" << w << ' ' << h << "\n255\n";
  out.write(reinterpret_cast<const char *>(img.data()), std::streamsize(img.size()));
}

void picture(const fs::path &path, world_queries::Level &level, const Run &run, bool mode_a) {
  if (run.marks.empty())
    return;
  double x0 = 1e30, y0 = 1e30, x1 = -1e30, y1 = -1e30;
  for (const auto &m : run.marks) {
    const double x = as_double(m.pos[0]), y = as_double(m.pos[1]);
    x0 = std::min(x0, x), x1 = std::max(x1, x), y0 = std::min(y0, y), y1 = std::max(y1, y);
  }
  const double cx = (x0 + x1) / 2, cy = (y0 + y1) / 2;
  const double span = std::max({x1 - x0, y1 - y0, 960.0}) + 640.0;
  const int W = 640, H = 640;
  std::vector<double> z(std::size_t(W) * H, 0);
  std::vector<int> valid(std::size_t(W) * H, 0);
  double zmin = 1e30, zmax = -1e30;
  for (int py = 0; py < H; ++py)
    for (int px = 0; px < W; ++px) {
      const double wx = cx - span / 2 + span * (px + 0.5) / W;
      const double wy = cy + span / 2 - span * (py + 0.5) / H;
      float fx = float(wx), fy = float(wy);
      Raw rx, ry;
      std::memcpy(&rx, &fx, 4);
      std::memcpy(&ry, &fy, 4);
      const auto hz = world_queries::terrain_height(level, PLACEMENT_RADIUS, Vec3{rx, ry, from_int(100000)});
      if (hz == world_queries::NO_FLOOR)
        continue;
      const auto d = as_double(hz);
      const auto at = std::size_t(py) * W + std::size_t(px);
      z[at] = d;
      valid[at] = 1;
      zmin = std::min(zmin, d), zmax = std::max(zmax, d);
    }
  std::vector<std::uint8_t> img(std::size_t(W) * H * 3, 0);
  for (int py = 0; py < H; ++py)
    for (int px = 0; px < W; ++px) {
      const auto at = std::size_t(py) * W + std::size_t(px);
      if (!valid[at]) {
        put_px(img, W, H, px, py, 10, 20, 60);
        continue;
      }
      const double n = zmax > zmin ? (z[at] - zmin) / (zmax - zmin) : 0.5;
      const int v = int(40 + 180 * n);
      put_px(img, W, H, px, py, v, v, v);
    }
  auto to_px = [&](double wx, double wy, int &px, int &py) {
    px = int((wx - (cx - span / 2)) / span * W);
    py = int(((cy + span / 2) - wy) / span * H);
  };
  auto circle = [&](double wx, double wy, double r, int cr, int cg, int cb) {
    for (int i = 0; i < 720; ++i) {
      const double a = i * 3.14159265358979 / 360;
      int px, py;
      to_px(wx + r * std::cos(a), wy + r * std::sin(a), px, py);
      put_px(img, W, H, px, py, cr, cg, cb);
    }
  };
  auto dot = [&](double wx, double wy, int s, int cr, int cg, int cb) {
    int px, py;
    to_px(wx, wy, px, py);
    for (int dy = -s; dy <= s; ++dy)
      for (int dx = -s; dx <= s; ++dx)
        put_px(img, W, H, px + dx, py + dy, cr, cg, cb);
  };
  auto facing_line = [&](const Mark &m, int cr, int cg, int cb) {
    const double c = as_double(cos16(std::uint32_t(m.facing) & 0xffff));
    const double s = as_double(sin16(std::uint32_t(m.facing) & 0xffff));
    for (int i = 0; i < 40; ++i) {
      int px, py;
      to_px(as_double(m.pos[0]) + c * i * span / W, as_double(m.pos[1]) + s * i * span / W, px, py);
      put_px(img, W, H, px, py, cr, cg, cb);
    }
  };
  const Mark *entry0 = nullptr;
  for (const auto &m : run.marks)
    if (m.kind == 0 && !entry0)
      entry0 = &m;
  if (entry0)
    circle(as_double(entry0->pos[0]), as_double(entry0->pos[1]), mode_a ? 180 : 36, 255, 140, 0);
  // The 240 candidate radius around the stand-in focus (slot 0's wave spot).
  for (const auto &m : run.marks)
    if (m.kind == 1) {
      circle(as_double(m.pos[0]), as_double(m.pos[1]), 240, 220, 0, 220);
      break;
    }
  for (const auto &m : run.marks) {
    const double x = as_double(m.pos[0]), y = as_double(m.pos[1]);
    switch (m.kind) {
    case 0:
      dot(x, y, 4, 255, 255, 255);
      break;
    case 1:
      dot(x, y, 5, 0, 220, 0);
      facing_line(m, 0, 220, 0);
      break;
    case 2:
      dot(x, y, 5, 0, 200, 255);
      facing_line(m, 0, 200, 255);
      break;
    default:
      dot(x, y, 3, 255, 230, 0);
      facing_line(m, 255, 230, 0);
      break;
    }
  }
  write_ppm(path, img, W, H);
}

void overlay_picture(const fs::path &path, const Run &run) {
  const int W = 640, H = 448;
  std::vector<std::uint8_t> img(std::size_t(W) * H * 3, 24);
  const int colours[5][3] = {{255, 255, 255}, {255, 200, 0}, {255, 120, 0}, {200, 0, 200}, {0, 160, 255}};
  for (const auto &g : run.glyph_path) {
    const int k = int(g[2]);
    const int px = int(g[0]), py = int(g[1]);
    for (int dy = -1; dy <= 1; ++dy)
      for (int dx = -1; dx <= 1; ++dx)
        put_px(img, W, H, px + dx, py + dy, colours[k][0], colours[k][1], colours[k][2]);
  }
  // Stand-in anchor (64, 400) and projected point (320, 224).
  for (int d = -4; d <= 4; ++d) {
    put_px(img, W, H, 64 + d, 400, 0, 255, 0);
    put_px(img, W, H, 64, 400 + d, 0, 255, 0);
    put_px(img, W, H, 320 + d, 224, 255, 0, 0);
    put_px(img, W, H, 320, 224 + d, 255, 0, 0);
  }
  write_ppm(path, img, W, H);
}

void print_row(unsigned index, char mode, const Run &r) {
  std::cout << "row " << index << " mode " << mode << " ok " << r.ok << " refused "
            << (r.refusal.empty() ? 0 : 1) << " wave " << r.wave << " waves_left " << r.waves_left
            << " classes " << r.classes << " ring_steps " << r.ring_steps << " builds_refused "
            << r.monster_builds_refused << " intro " << r.intro_frames << " latched " << r.latched
            << " placed " << r.placed << " placed_floor " << r.placed_floor << " wave_retry "
            << r.wave_retry << " deaths " << r.deaths << " respawns " << r.respawns
            << " respawn_floor " << r.respawn_floor << " respawn_refused " << r.respawn_refused
            << " camera_fallback " << r.camera_fallback << " purchases " << r.purchases
            << " overlay_frames " << r.overlay_frames << " glyphs " << r.glyphs << " empty_wave_refused "
            << r.empty_wave_refused << " next_wave " << r.next_wave << " end_prompt " << r.end_prompt
            << " end_frame " << r.end_frame << " rank_known " << r.rank_known << " fade_frames "
            << r.fade_frames << " confirm " << r.confirm << " kills " << r.kills
            << " intermission_frames " << r.intermission_frames << " level_change " << r.level_change
            << " next_on_disc " << r.next_on_disc << " frames " << r.frames << '\n';
}

// Refusal text is cut before any ':' so quoted names never reach stdout.
std::string reason(const std::string &text) { return text.substr(0, text.find(':')); }
} // namespace

int main(int argc, char **argv) {
  fs::path exe_path, root, captures;
  for (int i = 1; i + 1 < argc; i += 2) {
    const std::string flag = argv[i];
    if (flag == "--exe")
      exe_path = argv[i + 1];
    else if (flag == "--archives")
      root = argv[i + 1];
    else if (flag == "--captures")
      captures = argv[i + 1];
  }
  if (exe_path.empty() || root.empty() || argc % 2 == 0) {
    std::cerr << "usage: champions-arena-measure --exe FILE --archives DIR [--captures DIR]\n";
    return 2;
  }
  ScopedFpu ps2{Fpu{Fpu::Model::Ps2}};
  Exe exe;
  if (!exe.load(exe_path)) {
    std::cerr << "executable is not readable\n";
    return 1;
  }
  Tables tables;
  try {
    const unsigned count = exe.half(RECIPE_COUNT);
    for (unsigned i = 0; i < count; ++i) {
      Recipe record{};
      for (unsigned b = 0; b < 6; ++b)
        record[b] = exe.byte(RECIPES + 6 * i + b);
      tables.recipes.push_back(record);
    }
    tables.names_a = exe.names(NAMES_A);
    tables.names_b = exe.names(NAMES_B);
  } catch (const std::exception &error) {
    std::cerr << "executable tables: " << reason(error.what()) << '\n';
    return 1;
  }
  std::cout << "recipes " << tables.recipes.size() << "\nnames_a " << tables.names_a.size()
            << "\nnames_b " << tables.names_b.size() << '\n';
  std::error_code ec;
  if (!fs::is_directory(root, ec)) {
    std::cerr << "archive directory is not readable\n";
    return 1;
  }
  std::set<std::string> tex;
  std::vector<fs::path> gobs;
  for (fs::directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
    if (!it->is_regular_file())
      continue;
    const auto name = it->path().filename().string();
    const auto dot = name.rfind('.');
    if (dot == std::string::npos)
      continue;
    const auto ext = ascii_fold(name.substr(dot), true);
    const auto stem = name.substr(0, dot);
    if (ext == ".TEX")
      tex.insert(ascii_fold(stem, true));
    else if (ext == ".GOB")
      gobs.push_back(it->path());
  }
  std::sort(gobs.begin(), gobs.end());
  std::vector<std::pair<unsigned, fs::path>> paired;
  for (const auto &path : gobs)
    if (tex.count(ascii_fold(path.stem().string(), true))) {
      paired.push_back({unsigned(paired.size()), path});
      tables.on_disc.insert(ascii_fold(path.stem().string(), true));
    }
  std::set<std::string> table_names;
  for (const auto &n : tables.names_a)
    table_names.insert(ascii_fold(n, true));
  for (const auto &n : tables.names_b)
    table_names.insert(ascii_fold(n, true));
  unsigned table_levels = 0, failures = 0, captured = 0;
  std::map<std::string, unsigned> refusals;
  for (const auto &[index, path] : paired) {
    const auto stem = path.stem().string();
    const bool capture_this = std::find(std::begin(CAPTURE_INDICES), std::end(CAPTURE_INDICES), index) !=
                              std::end(CAPTURE_INDICES);
    if (!table_names.count(ascii_fold(stem, true)) && !capture_this)
      continue;
    ++table_levels;
    std::vector<std::uint8_t> file;
    std::vector<Lump> lumps, entries;
    const Lump *level_lump = nullptr;
    if (!read_file(path, file) || !sequential_lumps(file, lumps)) {
      ++failures;
      continue;
    }
    for (const auto &lump : lumps)
      if (ascii_fold(lump.name, false) == ascii_fold(stem, false) + ".lmp") {
        level_lump = &lump;
        break;
      }
    if (!level_lump || !lmp_entries(level_lump->bytes, entries)) {
      ++failures;
      continue;
    }
    std::vector<std::string> names;
    for (const auto &entry : entries)
      names.push_back(entry.name);
    const auto world_name = first_entry_with_extension(names, "world");
    const Lump *world_lump = nullptr, *objects_lump = nullptr;
    for (const auto &entry : entries) {
      if (world_name && !world_lump && entry.name == *world_name)
        world_lump = &entry;
      if (!objects_lump && entry.name == "objects.ob")
        objects_lump = &entry;
    }
    if (!world_lump) {
      ++failures;
      continue;
    }
    try {
      world_queries::Level level(terrain_file::decode(world_lump->bytes));
      LevelInput in;
      in.index = index;
      in.stem = stem;
      in.level = &level;
      if (objects_lump)
        in.records = parse_objects({objects_lump->bytes.data(), objects_lump->bytes.size()});
      for (const bool mode_a : {false, true}) {
        auto run = run_level(in, tables, mode_a);
        print_row(index, mode_a ? 'A' : 'B', run);
        if (!run.refusal.empty())
          ++refusals[reason(run.refusal)];
        if (capture_this && !captures.empty()) {
          fs::create_directories(captures, ec);
          const auto base = captures / ("arena-" + std::to_string(index) + (mode_a ? "-A" : "-B"));
          picture(base.string() + "-top.ppm", level, run, mode_a);
          if (!mode_a)
            overlay_picture(base.string() + "-overlay.ppm", run);
          std::ofstream(base.string() + "-trace.txt") << run.trace.str();
          ++captured;
        }
      }
    } catch (const std::exception &error) {
      ++failures;
      ++refusals[reason(error.what())];
    }
  }
  std::cout << "paired " << paired.size() << "\ntable_levels " << table_levels << "\nfailures " << failures
            << "\ncaptured_runs " << captured << '\n';
  for (const auto &[text, n] : refusals)
    std::cout << "refusal " << n << ' ' << text << '\n';
  return failures == 0 ? 0 : 1;
}
