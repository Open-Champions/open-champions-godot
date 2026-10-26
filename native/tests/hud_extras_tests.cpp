// Contract tests for hud_extras. They replay the research reference models
// (t3/research-hud-gaps f7c3964, tools/ref_hud_*.py) through the TSV vectors in
// native/tests/vectors/hud_extras_*.tsv, plus authored invariants and refusals.
#include "champions/hud_dynamic.hpp"
#include "champions/hud_extras.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace H = champions::hud_extras;
namespace F = champions::simulation::rta;
namespace {
long checks = 0;
std::string where;
void expect(bool ok, const std::string &what) {
  ++checks;
  if (!ok) throw std::runtime_error(where + ": " + what);
}
template <class A, class B> void same(const A &got, const B &want, const std::string &what) {
  ++checks;
  if (!(got == want)) {
    std::ostringstream o;
    o << where << ": " << what << " got " << got << " want " << want;
    throw std::runtime_error(o.str());
  }
}
using Row = std::vector<std::string>;
std::vector<Row> readRows(const std::string &path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("cannot open " + path);
  std::vector<Row> rows;
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') continue;
    Row r;
    std::size_t at = 0;
    for (;;) {
      const auto tab = line.find('\t', at);
      r.push_back(line.substr(at, tab == std::string::npos ? tab : tab - at));
      if (tab == std::string::npos) break;
      at = tab + 1;
    }
    rows.push_back(std::move(r));
  }
  return rows;
}
std::int64_t num(const std::string &s) { return std::strtoll(s.c_str(), nullptr, 0); }
std::uint32_t hex(const std::string &s) { return std::uint32_t(std::strtoull(s.c_str(), nullptr, 0)); }
std::int32_t i32(const std::string &s) { return std::int32_t(num(s)); }
std::vector<std::string> split(const std::string &s, char sep) {
  std::vector<std::string> out;
  if (s == "-" || s.empty()) return out;
  std::size_t at = 0;
  for (;;) {
    const auto p = s.find(sep, at);
    out.push_back(s.substr(at, p == std::string::npos ? p : p - at));
    if (p == std::string::npos) break;
    at = p + 1;
  }
  return out;
}
std::string hx(std::uint32_t v) {
  char b[16];
  std::snprintf(b, sizeof b, "0x%08x", v);
  return b;
}

// ---- status ---------------------------------------------------------------
void statusVectors(const std::string &dir) {
  const auto rows = readRows(dir + "/hud_extras_status.tsv");
  int n = 0, rects = 0, xp = 0, glow = 0;
  for (const auto &r : rows) {
    where = "status row " + std::to_string(n++);
    H::StatusInput in;
    in.slot = int(num(r[0])); in.x = int(num(r[1])); in.local = num(r[2]) != 0;
    in.obj_flags = hex(r[3]); in.health = hex(r[4]); in.health_max = hex(r[5]);
    in.health_pending = hex(r[6]); in.mana = hex(r[7]); in.mana_max = hex(r[8]);
    in.mana_pending = hex(r[9]); in.tick = i32(r[10]); in.flash = hex(r[11]);
    in.cooldown = i32(r[12]); in.ack_level = i32(r[13]); in.level = i32(r[14]);
    in.exp = i32(r[15]); in.exp_base = i32(r[16]); in.exp_next = i32(r[17]); in.glow_alpha = i32(r[18]);
    const auto got = H::statusDraws(in);
    same(got.flash, hex(r[19]), "new flash");
    same(got.glow_alpha, i32(r[20]), "new glow alpha");
    same(got.draws.size(), std::size_t(num(r[21])), "draw count");
    for (std::size_t k = 0; k < got.draws.size(); ++k) {
      const auto *g = &r[22 + 12 * k];
      const auto &d = got.draws[k];
      same(d.rect ? "R" : "S", g[0], "kind");
      if (d.rect) {
        ++rects;
        same(d.x0, int(num(g[2])), "rect x0"); same(d.y0, int(num(g[3])), "rect y0");
        same(d.x1, int(num(g[4])), "rect x1"); same(d.y1, int(num(g[5])), "rect y1");
      } else {
        same(d.resource, g[1], "resource");
        if (d.resource.rfind("bar_green", 0) == 0) ++xp;
        if (d.resource == "hud_statusbar00glow.tex") ++glow;
        same(d.x0, int(num(g[2])), "x"); same(d.y0, int(num(g[3])), "y");
        same(d.clip_left, int(num(g[8])), "clip left"); same(d.clip_top, int(num(g[9])), "clip top");
        same(d.clip_right, int(num(g[10])), "clip right"); same(d.clip_bottom, int(num(g[11])), "clip bottom");
      }
      same(d.layer, int(num(g[6])), "layer");
      same(hx(d.tint), hx(hex(g[7])), "tint");
    }
  }
  expect(rects > 100 && xp > 100 && glow > 5, "vectors must cover rectangles, the XP bar and the glow");
  std::printf("status: %d rows, %d rectangles, %d xp bars, %d glows\n", n, rects, xp, glow);
}

void statusInvariants() {
  where = "status invariants";
  // Pending rectangles have no health gate: dead bit clear, health 0, both drawn.
  H::StatusInput in;
  in.x = 7; in.health = 0; in.health_max = 0x42c80000u /*100*/; in.health_pending = 0x41200000u;
  in.mana = 0; in.mana_max = 0x42480000u; in.mana_pending = 0x41200000u; in.ack_level = 1; in.level = 1;
  int rects = 0;
  for (const auto &d : H::statusDraws(in).draws) rects += d.rect;
  same(rects, 2, "rectangles at health 0");
  in.obj_flags = 8;
  rects = 0;
  for (const auto &d : H::statusDraws(in).draws) rects += d.rect;
  same(rects, 0, "rectangles with the dead bit");
  expect(H::pendingRectsAllowed(true, 0) && !H::pendingRectsAllowed(true, 8) && !H::pendingRectsAllowed(false, 0),
         "pending gate is local and dead bit only");
  // Section 1 gates, each alone.
  expect(H::statusVisible(false, 0, false, 1, true, true, 0, 3000), "all clear draws");
  expect(!H::statusVisible(true, 0, false, 1, true, true, 0, 3000), "menu open");
  expect(!H::statusVisible(false, 0x80, false, 1, true, true, 0, 3000), "suppressed 0x80 counts");
  expect(!H::statusVisible(false, 0, true, 1, true, true, 0, 3000), "shop");
  expect(!H::statusVisible(false, 0, false, 0, true, true, 0, 3000), "camera 0");
  expect(!H::statusVisible(false, 0, false, 2, true, true, 0, 3000), "camera 2");
  expect(!H::statusVisible(false, 0, false, 1, false, true, 0, 3000), "inactive slot");
  expect(!H::statusVisible(false, 0, false, 1, true, false, 0, 3000), "no object");
  expect(!H::statusVisible(false, 0, false, 1, true, true, 1, 3000), "vehicle");
  expect(!H::statusVisible(false, 0, false, 1, true, true, 0, 2999), "display list 2999");
  const auto shape = H::barGreenShape();
  same(shape.entries, 11, "bar_green entries");
  same(shape.used_width * 1000 + shape.used_height, 100006, "used size 100x6");
  same(shape.upload_width * 1000 + shape.upload_height, 104008, "upload 104x8");
  same(shape.rows, 3, "field rows");
}

// ---- xp / init -------------------------------------------------------------
void xpVectors(const std::string &dir) {
  int n = 0;
  for (const auto &r : readRows(dir + "/hud_extras_xp.tsv")) {
    where = "xp row " + std::to_string(n++) + " " + r[0];
    if (r[0] == "frame") {
      same(H::animFrame(i32(r[1])), int(num(r[2])), "frame");
      char b[48]; std::snprintf(b, sizeof b, "bar_green%02d.tex", H::animFrame(i32(r[1])));
      same(std::string(b), r[3], "name");
    } else if (r[0] == "fill") {
      same(H::xpFillRight(i32(r[1]), i32(r[2]), i32(r[3]), i32(r[4])), i32(r[5]), "fill right");
    } else if (r[0] == "scissor") {
      char b[32];
      std::snprintf(b, sizeof b, "0x%016llx", (unsigned long long)H::scissor(i32(r[1]), i32(r[2]), i32(r[3]), i32(r[4]),
                    num(r[5]) != 0, num(r[6]) != 0, int(num(r[7])), i32(r[8])));
      same(std::string(b), r[9], "scissor");
    } else if (r[0] == "columns") {
      same(H::visibleColumns(int(num(r[1])), i32(r[2])), int(num(r[3])), "columns");
    } else if (r[0] == "init") {
      const auto i = H::afterInit();
      same(hx(i.hp), r[1], "hp"); same(i.hp_max, std::uint32_t(num(r[2])), "hp max");
      same(hx(i.mana), r[3], "mana"); same(hx(i.mana_max), r[4], "mana max");
    } else if (r[0] == "first") {
      const auto f = H::firstLevelCurrent(hex(r[1]), hex(r[2]), hex(r[3]));
      same(hx(f.hp), r[4], "first hp"); same(hx(f.mana), r[5], "first mana");
    } else if (r[0] == "respawn") {
      same(hx(H::respawnHp(hex(r[1]), num(r[2]) != 0)), r[3], "respawn hp");
    } else if (r[0] == "clamps") {
      same(H::recalcClamps(int(num(r[1])), int(num(r[2])), num(r[3]) != 0) ? 1 : 0, int(num(r[4])), "clamps");
    } else throw std::runtime_error("unknown xp row " + r[0]);
  }
  // Authored: a created character's first frame has current = max (hud-xp.md part B).
  where = "xp invariants";
  const auto f = H::firstLevelCurrent(0x42040000u /*33.0*/, 0x41840000u /*16.5*/);
  same(hx(f.hp), hx(0x42040000u), "hp = max"); same(hx(f.mana), hx(0x41840000u), "mana = max");
  same(hx(H::INIT_CURRENT), "0x4e6e6b27", "999999936.0 pattern");
  std::printf("xp: %d rows\n", n);
}

// ---- gold -------------------------------------------------------------------
std::string goldDigest(const H::GoldState &st) {
  std::ostringstream o;
  const auto join = [&](auto &arr) {
    for (std::size_t i = 0; i < arr.size(); ++i) o << (i ? "," : "") << arr[i];
  };
  join(st.gold); o << '|'; join(st.pending); o << '|'; join(st.hold); o << '|'; join(st.alpha);
  o << '|' << st.frame << '|' << st.cosmetic_draws << '|';
  for (std::size_t i = 0; i < st.floaters.size(); ++i) {
    const auto &f = st.floaters[i];
    o << (i ? "," : "") << f.amount << ':' << std::int32_t(std::int8_t(f.slot)) << ':' << f.start << ':'
      << hx(f.pos[0]) << '/' << hx(f.pos[1]) << '/' << hx(f.pos[2]) << ':' << hx(f.vel[0]) << '/'
      << hx(f.vel[1]) << '/' << hx(f.vel[2]);
  }
  std::uint64_t v = 0xcbf29ce484222325ull;
  for (unsigned char c : o.str()) v = (v ^ c) * 0x100000001b3ull;
  char b[24];
  std::snprintf(b, sizeof b, "%016llx", (unsigned long long)v);
  return b;
}
void goldVectors(const std::string &dir) {
  const auto width = [](const std::string &t) { return int(t.size()); };
  H::GoldState st;
  std::map<std::string, std::pair<H::GoldState, int>> life; // unused placeholder for clarity
  int n = 0, ops = 0, draws = 0, floatersDrawn = 0, countersDrawn = 0, bonus = 0;
  std::int64_t lifeKey = -1;
  H::GoldState ls;
  for (const auto &r : readRows(dir + "/hud_extras_gold.tsv")) {
    where = "gold row " + std::to_string(n++) + " " + r[0] + (r.size() > 1 ? " " + r[1] : "");
    if (r[0] == "sep") {
      same(H::formatGold(i32(r[1]), int(num(r[2]))), r[3], "formatGold");
    } else if (r[0] == "rawsep") {
      same(H::insertSeparators(r[1] == "-" ? "" : r[1], int(num(r[2]))), r[3], "insertSeparators");
    } else if (r[0] == "falpha") {
      same(H::goldFloaterAlpha(std::uint32_t(num(r[1])), std::uint32_t(num(r[2]))), i32(r[3]), "floater alpha");
    } else if (r[0] == "credit") {
      same(H::goldCredit(std::uint32_t(num(r[1])), std::uint32_t(num(r[2]))), std::uint32_t(num(r[3])), "credit");
    } else if (r[0] == "seq") {
      st = H::GoldState{};
      const auto g = split(r[2], ',');
      for (std::size_t i = 0; i < 4; ++i) st.gold[i] = std::uint32_t(num(g[i]));
    } else if (r[0] == "op") {
      ++ops;
      if (r[1] == "add") {
        H::FloaterStart fs;
        const int slot = int(num(r[2]));
        const auto amount = std::uint32_t(num(r[3]));
        if (r[5] != "-") {
          const auto p = split(r[5], '/'), v = split(r[6], '/');
          for (std::size_t i = 0; i < 3; ++i) { fs.pos[i] = hex(p[i]); fs.vel[i] = hex(v[i]); }
          fs.offscreen = std::uint8_t(num(r[7]));
        }
        same(H::addGoldFloater(st, slot, amount, fs), int(num(r[4])), "floater index");
        same(goldDigest(st), r[8], "state after add");
      } else if (r[1] == "tick") {
        H::tickGold(st); same(goldDigest(st), r[2], "state after tick");
      } else if (r[1] == "adv") {
        st.frame = std::uint32_t(num(r[2])); same(goldDigest(st), r[3], "state after advance");
      } else if (r[1] == "flush") {
        H::flushGold(st); same(goldDigest(st), r[2], "state after flush");
      } else if (r[1] == "bonus") {
        ++bonus; H::arenaBonusLife(st, int(num(r[2]))); same(goldDigest(st), r[3], "state after bonus");
      } else if (r[1] == "draw") {
        ++draws;
        const int mask = int(num(r[2]));
        const auto d = H::drawGold(st, {bool(mask & 1), bool(mask & 2), bool(mask & 4), bool(mask & 8)}, width,
                                   int(num(r[3])), num(r[4]) != 0);
        std::string fl, ct, co;
        for (const auto &f : d.floaters) {
          ++floatersDrawn;
          fl += (fl.empty() ? "" : ";") + std::to_string(f.x) + ":" + std::to_string(f.y) + ":" + f.text + ":" + std::to_string(f.colour);
        }
        for (const auto &c : d.counters) {
          ++countersDrawn;
          ct += (ct.empty() ? "" : ";") + std::to_string(c.slot) + ":" + std::to_string(c.x) + ":" + std::to_string(c.y) + ":" + c.text + ":" + std::to_string(c.colour);
        }
        for (const auto &c : d.coins)
          co += (co.empty() ? "" : ";") + std::to_string(c.x) + ":" + std::to_string(c.y) + ":" + std::to_string(c.tint);
        same(fl.empty() ? "-" : fl, r[5], "floater texts");
        same(ct.empty() ? "-" : ct, r[6], "counter texts");
        same(co.empty() ? "-" : co, r[7], "coins");
        same(goldDigest(st), r[8], "state after draw");
      } else throw std::runtime_error("unknown gold op " + r[1]);
    } else if (r[0] == "life") {
      const std::int64_t key = num(r[1]) * 10 + num(r[2]);
      const std::int64_t t = num(r[3]);
      if (key != lifeKey) {
        lifeKey = key;
        ls = H::GoldState{};
        ls.floaters[0].amount = std::uint32_t(num(r[1]));
        ls.floaters[0].slot = 0;
        ls.floaters[0].start = 0;
      }
      ls.frame = std::uint32_t(t);
      H::tickGold(ls);
      for (int i = 0; i < num(r[2]); ++i) H::drawGold(ls, {true, false, false, false}, width, 0);
      same(std::int64_t(std::int32_t(ls.gold[0])), num(r[4]), "gold");
      same(std::int64_t(ls.pending[0]), num(r[5]), "pending");
      same(std::int64_t(ls.hold[0]), num(r[6]), "hold");
      same(std::int64_t(ls.alpha[0]), num(r[7]), "alpha");
    } else throw std::runtime_error("unknown gold row " + r[0]);
  }
  expect(ops > 1000 && draws > 100 && floatersDrawn > 50 && countersDrawn > 50 && bonus > 10,
         "gold vectors must exercise every operation");
  // Authored: the documented single-pickup timeline (hud-gold.md section 5).
  where = "gold timeline";
  H::GoldState t;
  t.floaters[0].amount = 800;
  int visible = 0, first_credit = -1;
  for (int tick = 0; tick < 200; ++tick) {
    t.frame = std::uint32_t(tick);
    const auto before = t.gold[0];
    H::tickGold(t);
    if (first_credit < 0 && t.gold[0] != before) first_credit = tick;
    H::drawGold(t, {true, false, false, false}, width, 0);
    if (t.alpha[0] > 0) ++visible;
  }
  same(first_credit, 43, "gold starts arriving at tick 43");
  same(visible, 133, "counter visible 133 frames");
  same(std::int64_t(t.gold[0]), 800, "full amount credited");
  // Non-local slots are never stepped; the arena bonus life shows the counter at once.
  H::GoldState a;
  H::arenaBonusLife(a, 2);
  const auto d = H::drawGold(a, {true, true, false, true}, width, 0);
  same(a.alpha[2], 128, "bonus alpha"); same(d.coins.size(), std::size_t(1), "coin for non-local bonus slot");
  same(d.counters.size(), std::size_t(0), "no counter text for a non-local slot");
  bool refused = false;
  try { H::addGoldFloater(a, 4, 1, {}); } catch (const H::Refusal &) { refused = true; }
  expect(refused, "slot above 3 is refused");
  std::printf("gold: %d rows, %d ops, %d draws\n", n, ops, draws);
}

// ---- quick slots ---------------------------------------------------------------
std::vector<H::Bag2Cell> parseBag(const std::string &s) {
  std::vector<H::Bag2Cell> bag;
  for (const auto &c : split(s, ',')) {
    const auto p = split(c, ':');
    H::Bag2Cell cell;
    cell.kind = p[0] == "h" ? 0 : p[0] == "m" ? 1 : -1;
    cell.size = cell.kind < 0 ? -1 : int(num(p[1]));
    cell.quantity = std::int32_t(num(p[2]));
    bag.push_back(cell);
  }
  return bag;
}
std::string bagText(const std::vector<H::Bag2Cell> &bag) {
  std::string out;
  for (std::size_t i = 0; i < bag.size(); ++i) {
    const auto &c = bag[i];
    out += (i ? "," : "") + std::string(c.kind == 0 ? "h" : c.kind == 1 ? "m" : "x") + ":" +
           std::to_string(c.kind < 0 ? 0 : c.size) + ":" + std::to_string(c.quantity);
  }
  return out.empty() ? "-" : out;
}
void quickVectors(const std::string &dir) {
  int n = 0, used = 0, notices = 0, msgs = 0, removed = 0;
  for (const auto &r : readRows(dir + "/hud_extras_quick.tsv")) {
    where = "quick row " + std::to_string(n++) + " " + r[0];
    if (r[0] == "count") {
      same(H::countPotions(parseBag(r[2]), int(num(r[1]))), int(num(r[3])), "count");
    } else if (r[0] == "size") {
      std::array<int, 9> found{};
      const auto f = split(r[2], ',');
      for (std::size_t i = 0; i < 9; ++i) found[i] = int(num(f[i]));
      same(H::potionSize(hex(r[1]), found), int(num(r[3])), "size");
    } else if (r[0] == "use") {
      H::PotionInput in;
      in.copy = num(r[1]) != 0; in.hp = hex(r[2]); in.max_hp = hex(r[3]); in.hp_pending = hex(r[4]);
      in.mana = hex(r[5]); in.max_mana = hex(r[6]); in.mana_pending = hex(r[7]); in.bag = parseBag(r[8]);
      in.local = num(r[9]) != 0; in.menu_open = num(r[10]) != 0; in.grid = num(r[11]) != 0; in.kind = int(num(r[12]));
      const auto o = H::usePotion(in);
      ++used;
      same(o.returned_early ? 1 : 0, int(num(r[13])), "returned early");
      same(o.size, int(num(r[14])), "size");
      if (r[15] != "-") same(hx(o.pending), r[15], "pending");
      same(o.count, int(num(r[16])), "count");
      same(o.removed_cell ? *o.removed_cell : -1, int(num(r[17])), "removed cell");
      if (o.removed_cell) ++removed;
      same(o.sent_drop ? 1 : 0, int(num(r[18])), "sent drop");
      std::string ns;
      for (const auto &nt : o.notices) ns += (ns.empty() ? "" : ";") + std::to_string(nt.first) + ":" + std::to_string(nt.second);
      notices += int(o.notices.size());
      same(ns.empty() ? "-" : ns, r[19], "notices");
      std::string rg = "-";
      if (o.regen) rg = hx((*o.regen)[0]) + "/" + hx((*o.regen)[1]);
      same(rg, r[20], "regen");
      if (r[21] != "-") same(bagText(o.bag), r[21], "bag after");
    } else if (r[0] == "axes") {
      const auto a = H::padAxes(hex(r[1]));
      same(a.dx, int(num(r[2])), "dx"); same(a.dy, int(num(r[3])), "dy"); same(a.any ? 1 : 0, int(num(r[4])), "any");
    } else if (r[0] == "gates") {
      std::array<bool, H::GateCount> g{};
      for (int i = 0; i < H::GateCount; ++i) g[std::size_t(i)] = (num(r[1]) >> i) & 1;
      same(H::padReadable(g, num(r[2]) != 0) ? 1 : 0, int(num(r[3])), "pad readable");
    } else if (r[0] == "glyph") {
      same(H::buttonGlyph(hex(r[1])), int(num(r[2])), "glyph");
    } else if (r[0] == "msg") {
      const int slot = int(num(r[1])), dx = int(num(r[2])), dy = int(num(r[3]));
      const bool menu = num(r[4]) != 0;
      const std::uint32_t pressed = hex(r[5]);
      const H::SelectorButtons b{hex(r[6]), hex(r[7]), hex(r[8]), hex(r[9])};
      const int d = champions::hud_dynamic::selectorDirection(dx, dy);
      same(d, int(num(r[13])), "direction");
      const auto m = H::selectorMessage(slot, d, menu, pressed, b, int(num(r[10])), int(num(r[11])));
      same(m.id, int(num(r[14])), "message id");
      std::string args;
      for (int i = 0; i < m.count; ++i) args += (i ? "," : "") + std::to_string(m.args[std::size_t(i)]);
      same(args.empty() ? "-" : args, r[15], "message args");
      if (m.id >= 0) ++msgs;
      std::uint32_t bits = (dx > 0 ? 0x2000u : dx < 0 ? 0x8000u : 0) | (dy > 0 ? 0x4000u : dy < 0 ? 0x1000u : 0);
      H::MaskEntry e{i32(r[12]), 0x55, 9};
      if (H::padAxes(bits).any) e = H::maskLocalButtons(e, b);
      same(e.frames, i32(r[16]), "mask frames"); same(e.mask, hex(r[17]), "mask"); same(e.third, hex(r[18]), "mask third");
    } else if (r[0] == "icon") {
      const auto s = H::recordIcon(int(num(r[1])), num(r[2]) != 0);
      const std::string want = r[3];
      same(s == H::IconSource::Blank ? "blank" : s == H::IconSource::Skill ? "skill" : "table", want, "icon source");
    } else throw std::runtime_error("unknown quick row " + r[0]);
  }
  expect(used >= 1600 && notices > 100 && msgs > 20 && removed > 10, "quick vectors must reach starters and removal");
  // Authored: the potion buttons are not D-pad: pad axes ignore face bits.
  where = "quick invariants";
  expect(!H::padAxes(0x00ff).any, "face buttons are not direction bits");
  auto refuses = [](const auto &f) {
    try { f(); } catch (const H::Refusal &) { return true; }
    return false;
  };
  expect(refuses([] { H::countPotions({}, 3); }), "potion count refuses kind above its documented range");
  expect(refuses([] { H::PotionInput in; in.kind = 2; H::usePotion(in); }), "use potion refuses kind 2 even on early return");
  const H::SelectorButtons buttons{};
  expect(refuses([&] { H::selectorMessage(4, 0, false, 0, buttons, 0, 0); }), "selector refuses slot 4");
  expect(refuses([&] { H::selectorMessage(0, 8, false, 0, buttons, 0, 0); }), "selector refuses direction 8");
  std::printf("quick: %d rows, %d potion uses, %d notices, %d messages\n", n, used, notices, msgs);
}

// ---- minimap -----------------------------------------------------------------------
void minimapVectors(const std::string &dir) {
  int n = 0, views = 0, drawn = 0, blips = 0;
  H::MinimapInput in;
  H::MinimapView view;
  for (const auto &r : readRows(dir + "/hud_extras_minimap.tsv")) {
    where = "minimap row " + std::to_string(n++) + " " + r[0];
    if (r[0] == "cell") {
      const auto c = H::worldToCell(hex(r[1]), hex(r[2]), {hex(r[3]), hex(r[4])});
      same(hx(c[0]), r[5], "cell x"); same(hx(c[1]), r[6], "cell y");
    } else if (r[0] == "gate") {
      same(H::minimapGate(num(r[1]) != 0, i32(r[2]), hex(r[3])) ? 1 : 0, int(num(r[4])), "gate");
    } else if (r[0] == "view") {
      ++views;
      in = {};
      in.world_texture = num(r[1]) != 0; in.free_units = i32(r[2]); in.yaw = i32(r[3]);
      in.centre = {hex(r[4]), hex(r[5])}; in.origin = {hex(r[6]), hex(r[7])};
      in.offx = hex(r[8]); in.offy = hex(r[9]); in.zoom = hex(r[10]); in.half_w = hex(r[11]); in.half_h = hex(r[12]);
      in.width = int(num(r[13])); in.height = int(num(r[14]));
      view = H::minimapView(in);
      same(view.gate ? 1 : 0, int(num(r[15])), "gate");
      same(view.drawn ? 1 : 0, int(num(r[16])), "drawn");
      if (view.drawn) {
        ++drawn;
        same(view.sx0, int(num(r[17])), "scissor x0"); same(view.sx1, int(num(r[18])), "scissor x1");
        same(view.sy0, int(num(r[19])), "scissor y0"); same(view.sy1, int(num(r[20])), "scissor y1");
      }
      if (r[21] != "-") {
        same(view.col_min, int(num(r[21])), "col min"); same(view.row_min, int(num(r[22])), "row min");
        same(view.col_end, int(num(r[23])), "col end"); same(view.row_end, int(num(r[24])), "row end");
      }
    } else if (r[0] == "blip") {
      ++blips;
      H::Blip b;
      b.x = hex(r[2]); b.y = hex(r[3]); b.angle = i32(r[4]); b.r = std::uint32_t(num(r[5]));
      b.g = std::uint32_t(num(r[6])); b.b = std::uint32_t(num(r[7])); b.scale = hex(r[8]);
      const auto t = H::blipTriangles(in, view, b, hex(r[9]));
      expect(t.has_value(), "blip triangles for a drawn view");
      const auto v = split(r[10], ';');
      for (std::size_t k = 0; k < 6; ++k) {
        const auto p = split(v[k], '/');
        const auto &got = k < 3 ? t->shadow[k] : t->colour[k - 3];
        same(hx(got[0]), p[0], "vertex x"); same(hx(got[1]), p[1], "vertex y");
      }
      same(hx(t->colour_word), r[11], "colour word");
    } else if (r[0] == "icon_alpha") {
      same(H::iconAlpha(std::uint8_t(num(r[1]))), int(num(r[2])), "icon alpha");
    } else throw std::runtime_error("unknown minimap row " + r[0]);
  }
  expect(drawn >= 30 && blips >= 30, "minimap vectors must draw views with blips");
  where = "minimap refusals";
  bool tiles = false, icons = false;
  try { H::minimapTiles(); } catch (const H::Refusal &e) { tiles = std::string(e.what()).find("VU1") != std::string::npos; }
  try { H::minimapIcons(); } catch (const H::Refusal &e) { icons = std::string(e.what()).find("script_runByName") != std::string::npos; }
  expect(tiles && icons, "tiles and icons are refused with their reason");
  std::printf("minimap: %d rows, %d views (%d drawn), %d blips\n", n, views, drawn, blips);
}
} // namespace

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "usage: hud_extras_tests VECTORS_DIR\n";
    return 2;
  }
  try {
    statusVectors(argv[1]);
    statusInvariants();
    xpVectors(argv[1]);
    goldVectors(argv[1]);
    quickVectors(argv[1]);
    minimapVectors(argv[1]);
  } catch (const std::exception &e) {
    std::cerr << "FAIL " << e.what() << "\n";
    return 1;
  }
  std::printf("hud_extras: %ld checks passed\n", checks);
  return 0;
}
