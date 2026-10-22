// Contract test of the dialog gaps: replays native/tests/dialog_gaps_vectors.tsv
// (tools/generate_dialog_gaps_vectors.py on research t3/research-dialog-gaps) on the port,
// then the quirks the research documents (docs/spec/dialog-gaps.md).
#include "champions/dialog_gaps.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace champions::simulation::rta;
namespace g = champions::simulation::rta::dialog_gaps;

namespace {
int failures = 0;
std::size_t checks = 0;
void check(bool ok, const std::string &what) {
  ++checks;
  if (!ok && ++failures <= 40)
    std::cerr << "FAIL " << what << '\n';
}
std::vector<std::string> split(const std::string &s, char sep) {
  std::vector<std::string> out;
  std::string cur;
  for (char c : s) {
    if (c == sep) {
      out.push_back(cur);
      cur.clear();
    } else {
      cur += c;
    }
  }
  out.push_back(cur);
  return out;
}
std::vector<std::string> words(const std::string &s) {
  std::istringstream in(s);
  std::vector<std::string> out;
  std::string w;
  while (in >> w)
    out.push_back(w);
  return out;
}
std::optional<std::int64_t> num(const std::string &w) {
  if (w == "x" || w == "-")
    return std::nullopt;
  return std::strtoll(w.c_str(), nullptr, 10);
}
std::int64_t I(const std::string &w) { return num(w).value(); }
Raw R(const std::string &w) { return static_cast<Raw>(I(w)); }
std::string fmt(std::optional<std::int64_t> v) { return v ? std::to_string(*v) : "x"; }
std::string join(const std::vector<std::string> &v) {
  std::string out;
  for (std::size_t i = 0; i < v.size(); ++i)
    out += (i ? " " : "") + v[i];
  return out;
}
struct Out {
  std::vector<std::string> v;
  Out &operator<<(std::int64_t x) { v.push_back(std::to_string(x)); return *this; }
  Out &operator<<(std::optional<std::int64_t> x) { v.push_back(fmt(x)); return *this; }
  Out &operator<<(std::optional<std::int32_t> x) { v.push_back(x ? std::to_string(*x) : "x"); return *this; }
  std::string str() const { return join(v); }
};
std::optional<std::string> text_of(const std::string &f) {
  if (f == "-")
    return std::nullopt;
  return f.substr(2);
}
g::Stale stale_of(const std::string &f) {
  const auto w = words(f);
  g::Stale s;
  s.dist = num(w[0]) ? std::optional<std::int32_t>(std::int32_t(*num(w[0]))) : std::nullopt;
  s.height = num(w[1]) ? std::optional<std::int32_t>(std::int32_t(*num(w[1]))) : std::nullopt;
  s.lateral = num(w[2]) ? std::optional<std::int32_t>(std::int32_t(*num(w[2]))) : std::nullopt;
  s.pitch = num(w[3]) ? std::optional<std::int32_t>(std::int32_t(*num(w[3]))) : std::nullopt;
  return s;
}
g::View view_of(const std::string &f) {
  const auto w = words(f);
  g::View v;
  v.yaw = int(I(w[0]));
  v.pitch = int(I(w[1]));
  v.start = {R(w[2]), R(w[3]), R(w[4])};
  v.target = {R(w[5]), R(w[6]), R(w[7])};
  v.start_yaw = int(I(w[8]));
  v.target_yaw = int(I(w[9]));
  v.target_pitch = int(I(w[10]));
  v.start_pitch = int(I(w[11]));
  v.progress = R(w[12]);
  v.k = R(w[13]);
  return v;
}
Vec3 vec_of(const std::vector<std::string> &w, std::size_t at) { return {R(w[at]), R(w[at + 1]), R(w[at + 2])}; }

std::string run(const std::string &suite, const std::vector<std::string> &f) {
  // f[0] is the seed tag; the inputs follow.
  if (suite == "offset") {
    try {
      const auto o = g::parse_dialog_offset(text_of(f[1]), stale_of(f[2]));
      Out out;
      out << o.dist << o.height << o.lateral << o.pitch << std::int64_t(o.converted);
      return out.str();
    } catch (const g::Refused &) {
      return "refused";
    }
  }
  if (suite == "side") {
    const auto w = words(f[1]);
    int asked = 0;
    const auto idx = g::side_index(I(w[0]), I(w[1]), I(w[2]), [&] { ++asked; return I(w[3]) != 0; });
    return std::to_string(idx) + " " + std::to_string(asked);
  }
  if (suite == "view") {
    const auto yp = words(f[1]);
    const auto np = words(f[2]);
    const auto fs = words(f[3]);
    const auto pl = words(f[4]);
    const auto cp = words(f[5]);
    g::View v(int(I(yp[0])), int(I(yp[1])));
    std::vector<Vec3> players;
    for (std::size_t i = 0; i + 2 < pl.size(); i += 3)
      players.push_back(vec_of(pl, i));
    try {
      const auto info = g::setup_dialog_view(v, vec_of(np, 0), int(I(fs[0])), int(I(fs[1])), players, vec_of(cp, 0),
                                             text_of(f[6]), stale_of(f[7]));
      Out out;
      out << std::int64_t(v.target[0]) << std::int64_t(v.target[1]) << std::int64_t(v.target[2])
          << std::int64_t(v.target_yaw) << std::int64_t(v.target_pitch) << std::int64_t(v.start[0])
          << std::int64_t(v.start[1]) << std::int64_t(v.start[2]) << std::int64_t(v.start_yaw)
          << std::int64_t(v.start_pitch) << std::int64_t(v.progress) << std::int64_t(v.k)
          << std::int64_t(info.flags[0]) << std::int64_t(info.flags[1]) << std::int64_t(info.flags[2])
          << std::int64_t(info.angle) << info.fields.dist << info.fields.height << info.fields.lateral
          << info.fields.pitch << std::int64_t(info.fields.converted);
      return out.str();
    } catch (const g::Refused &) {
      return "refused";
    }
  }
  if (suite == "lookat") {
    auto v = view_of(f[1]);
    const auto l = g::look_at(v, int(I(f[2])));
    Out out;
    out << std::int64_t(l.result);
    out << (l.apply ? std::optional<std::int64_t>(l.h) : std::nullopt);
    for (int i = 0; i < 3; ++i)
      out << (l.apply ? std::optional<std::int64_t>((*l.apply)[std::size_t(i)]) : std::nullopt);
    out << std::int64_t(v.yaw) << std::int64_t(v.pitch) << std::int64_t(v.progress);
    return out.str();
  }
  if (suite == "chain") {
    auto v = view_of(f[1]);
    int up = 0, down = 0;
    do { ++up; } while (g::look_at(v, 45).result != 1 && up <= 400);
    do { ++down; } while (g::look_at(v, -45).result != 1 && down <= 400);
    return std::to_string(up) + " " + std::to_string(down);
  }
  if (suite == "runcam") {
    auto v = view_of(f[1]);
    const auto w = words(f[2]);
    const auto r = g::run_task_camera(v, int(I(w[0])), I(w[1]) != 0, I(w[2]) != 0, R(w[3]));
    Out out;
    out << (r.h ? std::optional<std::int64_t>(*r.h) : std::nullopt) << std::int64_t(v.yaw) << std::int64_t(v.pitch)
        << std::int64_t(v.progress);
    std::string toks;
    for (const auto &a : r.actions) {
      if (!toks.empty())
        toks += ' ';
      if (a.kind == "far_clip")
        toks += "F" + std::to_string(a.value);
      else if (a.kind == "apply")
        toks += "A" + std::to_string(a.pos[0]) + "," + std::to_string(a.pos[1]) + "," + std::to_string(a.pos[2]);
      else if (a.kind == "arrived")
        toks += "R";
      else
        toks += "S" + std::to_string(a.value);
    }
    return out.str() + " |" + toks;
  }
  if (suite == "apply") {
    const auto w = words(f[1]);
    const auto a = g::camera_apply(vec_of(w, 0), int(I(w[3])), int(I(w[4])), R(w[5]), std::uint32_t(I(w[6])),
                                   I(w[7]) != 0, I(w[8]) != 0);
    Out out;
    for (auto x : a.pos)
      out << std::int64_t(x);
    for (auto x : a.matrix)
      out << std::int64_t(x);
    out << std::int64_t(a.shake) << std::int64_t(a.seed) << std::int64_t(a.draws);
    return out.str();
  }
  if (suite == "frozen") {
    const auto w = words(f[1]);
    g::Frozen s{I(w[0]), I(w[1]), I(w[2]), I(w[3]), I(w[4]), I(w[5]), I(w[6]), I(w[7])};
    std::vector<std::string> log;
    const auto v = g::game_is_frozen(s, &log);
    std::string names;
    for (const auto &l : log)
      names += (names.empty() ? "" : ",") + l;
    return std::to_string(v) + " |" + names;
  }
  if (suite == "recall") {
    std::vector<g::RecallPlayer> players;
    if (f[1] != "-") {
      for (const auto &p : split(f[1], ';')) {
        const auto w = words(p);
        g::RecallPlayer r;
        r.slot = int(I(w[0]));
        r.lockout = I(w[1]); r.frozen = I(w[2]); r.dialog = I(w[3]); r.obj4b4 = I(w[4]); r.recall = I(w[5]);
        r.b4f66c8 = I(w[6]); r.camflag = I(w[7]); r.jumpgate = I(w[8]); r.pausemenu = I(w[9]);
        r.obj540 = I(w[10]); r.flag400000 = I(w[11]); r.use = I(w[12]); r.b100 = I(w[13]);
        players.push_back(r);
      }
    }
    const auto slots = g::recall_poll(players);
    if (slots.empty())
      return "-";
    std::vector<std::string> out;
    for (auto s : slots)
      out.push_back(std::to_string(s));
    return join(out);
  }
  if (suite == "camctl") {
    const auto w = words(f[1]);
    return std::to_string(g::camera_update_control(I(w[0]), I(w[1]), I(w[2]), I(w[3]), I(w[4]), I(w[5]), I(w[6]), I(w[7])));
  }
  if (suite == "site") {
    static const char *keys[] = {"dialog", "pvp_b", "active", "hp_le0", "network", "jumpgate", "cam_mode", "no_save",
                                 "paused", "lockout", "selector", "flag400000", "local", "pause_open", "frontend",
                                 "compass_on", "cutscene", "flag10000000", "model", "pending_level", "list_on",
                                 "countdown", "local_menu", "players"};
    const auto w = words(f[2]);
    g::Conditions c;
    for (std::size_t i = 0; i < w.size(); ++i)
      c[keys[i]] = I(w[i]);
    try {
      return g::site_outcome(f[1], c);
    } catch (const g::Refused &) {
      return "refused";
    }
  }
  if (suite == "idle") {
    const auto w = words(f[1]);
    const auto r = g::idle_voice(I(w[0]), I(w[1]), I(w[2]), I(w[3]), I(w[4]), I(w[5]), std::uint32_t(I(w[6])));
    Out out;
    out << r.countdown << std::int64_t(r.seed)
        << (r.voice ? std::optional<std::int64_t>(*r.voice) : std::nullopt);
    return out.str();
  }
  if (suite == "box") {
    const auto w = words(f[2]);
    return std::to_string(g::load_box(f[1] == "-" ? "" : f[1], I(w[0]) != 0, std::uint32_t(I(w[1])), std::uint32_t(I(w[2]))));
  }
  if (suite == "boxreads") {
    try {
      const auto r = g::box_sprite_reads(std::uint32_t(I(f[1])));
      return std::to_string(r.first) + " " + std::to_string(r.second);
    } catch (const g::Refused &) {
      return "refused";
    }
  }
  if (suite == "buttons") {
    std::string out;
    for (const auto &kv : g::init_camera_buttons(std::int32_t(I(f[1]))))
      out += (out.empty() ? "" : " ") + std::to_string(kv.first) + ":" + std::to_string(kv.second);
    return out;
  }
  if (suite == "factory") {
    g::Entries entries;
    if (f[1] != "-")
      entries = split(f[1], '|');
    const auto fb = g::factory_fallback(entries);
    return (fb ? *fb : "-") + " " + std::to_string(g::has_any_light(entries) ? 1 : 0) + " " +
           std::to_string(g::props_get_int(entries, "Autoprop")) + " " +
           std::to_string(g::props_get_int(entries, "Halo", 1));
  }
  return "unknown suite " + suite;
}

void quirks() {
  const auto F = [](double v) { return from_int(std::uint32_t(v)); };
  // No dialogOffset: dist, height and pitch are 0; lateral keeps the stale slot, and an unknown one is refused.
  {
    const auto o = g::parse_dialog_offset(std::nullopt, g::Stale{1, 2, 3, 4});
    check(o.dist == 0 && o.height == 0 && o.lateral == 3 && o.pitch == 0 && o.converted == 0, "absent offset");
    g::View v;
    bool refused = false;
    try {
      g::setup_dialog_view(v, {}, 0, 0, {}, {}, std::nullopt);
    } catch (const g::Refused &) { refused = true; }
    check(refused, "an unknown stack slot is refused");
    refused = false;
    try {
      g::setup_dialog_view(v, {}, 0, 0, {}, {}, std::string("5,6"));
    } catch (const g::Refused &) { refused = true; }
    check(refused, "fewer than four fields without stale slots is refused");
  }
  // A straight view in front of facing 0: 10 + 80 = 90 (research test).
  {
    g::View v(0, 100);
    g::setup_dialog_view(v, {F(10), F(20), F(5)}, 0, 0, {}, {F(10), 0, 0}, std::string("0,0,0,0"));
    check(v.target[0] == F(90) && v.target[2] == F(75), "straight view target");
    check(v.target_pitch == 4000 && v.target_yaw == s16(std::uint32_t(-0x4000)), "straight view angles");
    check(v.start_yaw == 0 && v.start_pitch == 100 && v.progress == ZERO, "straight view start");
  }
  // The side index table and the one place the camera side is asked.
  check(g::side_index(false, true, false, [] { return true; }) == 2 &&
            g::side_index(false, true, false, [] { return false; }) == 0, "side index B");
  // Arrival is one call after the progress reaches the end, in and out (k = 0: 47 / 47).
  {
    g::View v;
    v.k = ZERO;
    int up = 0, down = 0;
    do { ++up; } while (g::look_at(v, 45).result != 1);
    do { ++down; } while (g::look_at(v, -45).result != 1);
    check(up == 47 && down == 47, "k = 0 swoop is 47 / 47 calls");
    v = g::View();
    v.k = ONE;
    up = down = 0;
    do { ++up; } while (g::look_at(v, 45).result != 1);
    do { ++down; } while (g::look_at(v, -45).result != 1);
    check(up == 182 && down == 181, "k = 1 swoop is 182 / 181 calls");
    v = g::View();
    v.k = g::FAR_K;
    up = down = 0;
    do { ++up; } while (g::look_at(v, 45).result != 1);
    do { ++down; } while (g::look_at(v, -45).result != 1);
    check(up == 126 && down == 126, "far-start swoop is 126 / 126 calls");
    bool refused = false;
    try { g::look_at(v, 0); } catch (const g::Refused &) { refused = true; }
    check(refused, "zero steps divides by zero");
  }
  // Closing is held while the shop is open; message 3 only on arrival.
  {
    g::View v;
    v.progress = ZERO;
    const auto held = g::run_task_camera(v, -1, false, true, ZERO);
    check(held.actions.empty(), "closing is held while the shop is open");
    const auto arrived = g::run_task_camera(v, -1, false, false, ZERO);
    check(arrived.actions.size() >= 2 && arrived.actions.back().kind == "send" && arrived.actions.back().value == 3,
          "closing at progress 0 sends message 3");
  }
  // The box: intro and mode B write nothing; word 0 is refused by the draw.
  check(g::load_box("intro", false, 7, 9) == 7 && g::load_box("Intro", false, 7, 9) == 9 &&
            g::load_box("x", true, 7, 9) == 7 && g::load_box("x", false, 7, 0) == 0, "load_box");
  check(g::BOX_WORD_INITIAL == 0, "box word starts at 0");
  // The cursor glyph.
  check(g::cursor_glyph(g::LAYOUT_INITIAL) == 0x7e && g::cursor_glyph(0) == 0x7b && !g::cursor_glyph(2),
        "cursor glyph");
  // TestLocation-shaped records make no object.
  check(!g::factory_fallback({"showname=x", "dialogscript=y", "dialogoffset=1,2,3,4", "key=z"}), "talker record makes none");
  check(g::factory_fallback({"autochest=1", "halo1=1"}) == "AutoChestLP", "light chest");
  check(g::sites().size() == 24, "24 sites");
}
} // namespace

int main(int argc, char **argv) {
  if (argc < 3) {
    std::cerr << "usage: dialog_gaps_tests vectors.tsv contract.json\n";
    return 2;
  }
  std::ifstream in(argv[1]);
  std::string line;
  std::size_t lines = 0;
  std::map<std::string, std::size_t> per_suite;
  while (std::getline(in, line)) {
    const auto f = split(line, '\t');
    if (f[0] == "policy") {
      check(f.size() == 3 && f[1] == g::POLICY, "policy line");
      continue;
    }
    ++lines;
    const auto arrow = std::size_t(std::find(f.begin(), f.end(), "=>") - f.begin());
    if (arrow + 2 != f.size()) {
      check(false, "malformed line " + std::to_string(lines));
      continue;
    }
    const std::vector<std::string> inputs(f.begin() + 1, f.begin() + std::ptrdiff_t(arrow));
    const auto got = run(f[0], inputs);
    per_suite[f[0]] += 1;
    check(got == f.back(), f[0] + " " + join(inputs) + "\n  want " + f.back() + "\n  got  " + got);
  }
  std::ifstream contract(argv[2]);
  std::stringstream text;
  text << contract.rdbuf();
  check(text.str().find("\"lines\": " + std::to_string(lines)) != std::string::npos, "contract line count");
  check(text.str().find("79a2e4a") != std::string::npos || text.str().find("\"commit\"") != std::string::npos, "contract commit");
  quirks();
  std::cout << "dialog gaps: " << lines << " cases, " << checks << " checks, " << failures << " failed\n";
  for (const auto &kv : per_suite)
    std::cout << "  " << kv.first << ' ' << kv.second << '\n';
  return failures == 0 ? 0 : 1;
}
