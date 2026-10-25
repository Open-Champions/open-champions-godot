#include "champions/hud_extras.hpp"
#include "champions/core_math.hpp"
#include "champions/hud_dynamic.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace champions::hud_extras {
namespace F = champions::simulation::rta;
namespace {
using std::int32_t;
using std::uint32_t;
using std::uint64_t;

int32_t wrap32(long long v) { return int32_t(uint32_t(uint64_t(v))); }
int32_t s8(int32_t v) { return int32_t(std::int8_t(v & 0xff)); }
int32_t s16v(int32_t v) { return int32_t(std::int16_t(v & 0xffff)); }

constexpr Raw SPAN_F = 0x42c60000u; // float(99), 0x1d9eac
constexpr int BAR_INSET = 15, HEALTH_Y = 46, XP_Y = 56, MANA_Y = 63;
constexpr int CLIP_TOP = 24, CLIP_BOTTOM = 124, LAYER = 6;
constexpr uint32_t XP_TINT = 0x80808080u, FRAME_TINT = 0x80808080u;
constexpr int FRAME_DX = -4, FRAME_Y = 36;
constexpr uint32_t DEAD_FLAG = 8;
constexpr int PENDING_CAP_OTHER = 447;
constexpr std::array<int, 3> PENDING_CAPS{180, 605, 333};

void sprite(StatusResult &r, const std::string &name, int x, int y, uint32_t tint,
            int l, int t, int rt, int b) {
  StatusDraw d;
  d.resource = name;
  d.x0 = x;
  d.y0 = y;
  d.layer = LAYER;
  d.tint = tint;
  d.clip_left = l;
  d.clip_top = t;
  d.clip_right = rt;
  d.clip_bottom = b;
  r.draws.push_back(std::move(d));
}
// 0x1d9fd8-0x1da04c; left and uncapped right of a pending rectangle.
std::pair<int32_t, int32_t> pendingSpan(int32_t start, Raw cur, Raw max, Raw pending) {
  const int32_t a = F::to_int(F::mul(F::div(cur, max), SPAN_F));
  int32_t t = F::to_int(F::add(cur, pending));
  if (F::lt(max, F::from_int(uint32_t(t)))) t = F::to_int(max);
  const int32_t b = F::to_int(F::mul(F::div(F::from_int(uint32_t(t)), max), SPAN_F));
  const int32_t x7 = wrap32(start - (BAR_INSET - 7));
  return {wrap32(start + a), wrap32(x7 + wrap32(b - a) + wrap32(a + 8))};
}
void pendingRect(StatusResult &r, int slot, int32_t start, Raw cur, Raw max,
                 Raw pending, int top, int bottom, uint32_t colour) {
  const auto span = pendingSpan(start, cur, max, pending);
  StatusDraw d;
  d.rect = true;
  d.resource = "-";
  d.x0 = span.first;
  d.y0 = top;
  d.x1 = std::min(span.second, pendingCap(slot));
  d.y1 = bottom;
  d.layer = LAYER;
  d.tint = colour;
  r.draws.push_back(std::move(d));
}
int32_t pulseOf(int32_t tick) {
  return F::to_int(F::mul(F::sin16(uint32_t(tick) << 10), 0x41c80000u));
}
uint32_t pulseGrey(int32_t p, int32_t alpha) {
  const uint32_t v = uint32_t(p + 0x67);
  return (uint32_t(alpha & 0xff) << 24) | (v << 16) | (v << 8) | v;
}
} // namespace

int animFrame(int32_t tick) {
  const int32_t q = tick >> 2;
  const int32_t r = std::abs(q) % 11;
  return q < 0 ? -r : r;
}
std::string barName(const char *colour, int32_t tick) {
  char buf[64];
  std::snprintf(buf, sizeof buf, "bar_%s%02d.tex", colour, animFrame(tick));
  return buf;
}
int32_t fillRight(int32_t origin, Raw current, Raw maximum) {
  return F::to_int(F::add(F::from_int(uint32_t(origin)),
                          F::mul(F::div(current, maximum), SPAN_F)));
}
int32_t xpFillRight(int32_t origin, int32_t xp, int32_t lower, int32_t upper) {
  const Raw num = F::from_int(uint32_t(wrap32(int64_t(xp) - lower)));
  const Raw den = F::from_int(uint32_t(wrap32(int64_t(upper) - lower)));
  return F::to_int(F::add(F::from_int(uint32_t(origin)),
                          F::mul(F::div(num, den), SPAN_F)));
}
int pendingCap(int slot) {
  return (slot >= 0 && slot < 3) ? PENDING_CAPS[std::size_t(slot)] : PENDING_CAP_OTHER;
}
bool pendingRectsAllowed(bool local, uint32_t obj_flags) {
  return local && !(obj_flags & DEAD_FLAG);
}
bool statusVisible(bool menu_open, int hud_suppressed, bool shop_open,
                   int camera_mode, bool slot_active, bool has_object,
                   uint32_t vehicle, int32_t free_units) {
  return !menu_open && !s8(hud_suppressed) && !shop_open && camera_mode == 1 &&
         slot_active && has_object && !vehicle && free_units >= 3000;
}

StatusResult statusDraws(const StatusInput &in) {
  StatusResult r;
  const int32_t start = wrap32(int64_t(in.x) + BAR_INSET);
  uint32_t flash = in.flash;
  uint32_t grey = 0x80;
  if (flash & 1) {
    grey = 0xff;
    flash &= ~1u;
  }
  sprite(r, barName("red", in.tick), start, HEALTH_Y,
         0x80000000u | (grey << 16) | (grey << 8) | grey, start, CLIP_TOP,
         fillRight(start, in.health, in.health_max), CLIP_BOTTOM);
  const bool dead = in.obj_flags & DEAD_FLAG;
  if (pendingRectsAllowed(in.local, in.obj_flags)) {
    if (F::lt(0, in.health_pending))
      pendingRect(r, in.slot, start, in.health, in.health_max, in.health_pending, 47, 51, 0x80000060u);
    if (F::lt(0, in.mana_pending))
      pendingRect(r, in.slot, start, in.mana, in.mana_max, in.mana_pending, 66, 70, 0x805a4632u);
  }
  const int32_t ack = s8(in.ack_level);
  if (in.local) {
    if (ack > 0)
      sprite(r, barName("green", in.tick), start, XP_Y, XP_TINT, start, CLIP_TOP,
             xpFillRight(start, in.exp, in.exp_base, in.exp_next), CLIP_BOTTOM);
    const uint32_t alpha = s16v(in.cooldown) > 0 ? 0x2a : 0x80;
    sprite(r, barName("blue", in.tick), start, MANA_Y, (alpha << 24) | 0xacacac,
           start, CLIP_TOP, fillRight(start, in.mana, in.mana_max), CLIP_BOTTOM);
  }
  int32_t glow = in.glow_alpha;
  const int fx = wrap32(int64_t(in.x) + FRAME_DX);
  if (in.local) {
    if (!dead && F::lt(0, in.health) && ack < s8(in.level)) {
      glow = std::min(glow + 4, 128);
      sprite(r, "hud_statusbar00glow.tex", fx, FRAME_Y, pulseGrey(pulseOf(in.tick), glow), 0, 0, 1000, 1000);
    }
    sprite(r, "hud_statusbar00.tex", fx, FRAME_Y, FRAME_TINT, 0, 0, 1000, 1000);
  } else {
    sprite(r, "hud_multi.tex", fx, FRAME_Y, FRAME_TINT, 0, 0, 1000, 1000);
  }
  r.flash = flash;
  r.glow_alpha = glow;
  return r;
}

uint64_t scissor(int32_t left, int32_t top, int32_t right, int32_t bottom,
                 bool interlaced, bool both_fields, int parity, int32_t letterbox) {
  const auto sx = [](int64_t v) { return uint64_t(int64_t(wrap32(v))); };
  top = wrap32(int64_t(top) - letterbox);
  bottom = wrap32(int64_t(bottom) - letterbox);
  if (left < 0) left = 0;
  if (top < 0) top = 0;
  if (!(right < 0x281)) right = 640;
  if (!(bottom < 0x1c0)) bottom = 447;
  uint64_t x0, x1;
  int32_t b;
  if (interlaced) {
    const int32_t bit = both_fields ? (bottom & 1) : ((parity & 1) * (bottom & 1));
    b = wrap32(2LL * ((bottom >> 1) + bit) - 1);
    x0 = sx(int64_t(left) << 1);
    x1 = sx(2LL * right - 1) << 16;
  } else {
    b = bottom;
    x0 = sx(left);
    x1 = sx(right) << 16;
  }
  return x0 | x1 | (sx(top) << 32) | (sx(b) << 48);
}
int visibleColumns(int origin, int32_t right, int width) {
  right = std::min<int32_t>(right, 640);
  return std::max(0, std::min<int>(right, origin + width) - std::max(origin, 0));
}
BarGreenShape barGreenShape() { return {11, 100, 6, 128, 8, 104, 8, 8, 2, 3}; }

InitCurrent afterInit() { return {INIT_CURRENT, 0, INIT_CURRENT, INIT_MANA_MAX}; }
Raw clampCurrent(Raw current, Raw maximum) {
  return F::lt(maximum, current) ? maximum : current;
}
bool recalcClamps(int slot, int num_players, bool has_object) {
  return slot >= 0 && slot < num_players && has_object;
}
InitCurrent firstLevelCurrent(Raw hp_max, Raw mana_max, Raw current) {
  return {clampCurrent(current, hp_max), hp_max, clampCurrent(current, mana_max), mana_max};
}
Raw respawnHp(Raw hp_max, bool pvp_a) {
  return pvp_a ? hp_max : F::mul(hp_max, 0x3f000000u);
}

// ---- gold ----------------------------------------------------------------
uint32_t goldCredit(uint32_t word, uint32_t amount) {
  const uint32_t total = word + amount;
  return int32_t(total) > GOLD_CAP ? uint32_t(GOLD_CAP) : total;
}
int addGoldFloater(GoldState &st, int slot, uint32_t amount, const FloaterStart &fs) {
  int k = -1;
  for (int i = 0; i < FLOATER_COUNT; ++i)
    if (!st.floaters[std::size_t(i)].amount) { k = i; break; }
  if (slot < 0 || slot > 3) throw Refusal("gold floater slot outside 0..3: presentation tables are not documented beyond slot 3");
  if (k < 0) {
    st.gold[std::size_t(slot)] = goldCredit(st.gold[std::size_t(slot)], amount);
    return -1;
  }
  auto &f = st.floaters[std::size_t(k)];
  f.amount = amount;
  f.slot = slot & 0xff;
  f.start = st.frame;
  f.pos = fs.pos;
  f.vel = fs.vel;
  f.offscreen = fs.offscreen;
  ++st.cosmetic_draws;
  return k;
}
void tickGold(GoldState &st) {
  for (auto &f : st.floaters) {
    if (!f.amount) continue;
    if (wrap32(3LL * int64_t(uint32_t(st.frame - f.start))) < 128) continue;
    const int i = s8(f.slot);
    if (i < 0 || i > 3) throw Refusal("gold floater slot outside 0..3");
    st.pending[std::size_t(i)] = wrap32(int64_t(st.pending[std::size_t(i)]) + int64_t(f.amount));
    f.amount = 0;
  }
  for (std::size_t i = 0; i < 4; ++i) {
    const int32_t p = st.pending[i];
    if (p <= 0) continue;
    const int32_t step = wrap32(int64_t(p) + 7) >> 3;
    st.gold[i] = goldCredit(st.gold[i], uint32_t(step));
    st.pending[i] = wrap32(int64_t(p) - step);
  }
}
void flushGold(GoldState &st) {
  for (auto &f : st.floaters) {
    if (!f.amount) continue;
    const int i = s8(f.slot);
    if (i < 0 || i > 3) throw Refusal("gold floater slot outside 0..3");
    st.gold[std::size_t(i)] = goldCredit(st.gold[std::size_t(i)], f.amount);
    f.amount = 0;
  }
  for (std::size_t i = 0; i < 4; ++i)
    if (st.pending[i] > 0) {
      st.gold[i] = goldCredit(st.gold[i], uint32_t(st.pending[i]));
      st.pending[i] = 0;
    }
  st.hold = {};
  st.alpha = {};
}
void arenaBonusLife(GoldState &st, int slot) {
  if (slot < 0 || slot > 3) throw Refusal("bonus life slot outside 0..3");
  st.alpha[std::size_t(slot)] = 128;
}
int32_t goldFloaterAlpha(uint32_t frame, uint32_t start) {
  int32_t a = wrap32(128LL - int64_t(wrap32(3LL * int64_t(uint32_t(frame - start)))));
  a = a < 129 ? a : 128;
  return a < 0 ? 0 : a;
}
std::string insertSeparators(const std::string &digits, int language) {
  const int n = int(digits.size());
  const int g = n > 0 ? (n - 1) / 3 : 0;
  const char sep = s8(language) == 0 ? ',' : '.';
  const int end = n + g + 1;
  std::vector<unsigned char> buf(std::size_t(end) + 2, 0);
  std::memcpy(buf.data(), digits.data(), std::size_t(n));
  int cur = n;
  for (int i = 0; i < g; ++i) {
    const int rem = cur % 3;
    cur = rem ? rem : cur / 2;
    const int pos = cur + 4 * i;
    for (int k = end; k >= pos; --k) buf[std::size_t(k)] = buf[std::size_t(k) - 1];
    buf[std::size_t(pos)] = static_cast<unsigned char>(sep);
  }
  std::size_t len = 0;
  while (buf[len]) ++len;
  return std::string(reinterpret_cast<const char *>(buf.data()), len);
}
std::string formatGold(int32_t value, int language) {
  return insertSeparators(std::to_string(value), language);
}
GoldDraw drawGold(GoldState &st, const std::array<bool, 4> &local,
                  const std::function<int(const std::string &)> &width, int language,
                  bool hud_off) {
  GoldDraw out;
  if (hud_off) return out;
  constexpr uint32_t TEXT_COLOUR = 0x4080e4u, COIN_TINT = 0x808080u;
  static constexpr std::array<int, 4> TEXT_X{50, 590, 50, 590}, TEXT_Y{200, 200, 250, 250},
      ICON_X{16, 593, 16, 593};
  const auto isLocal = [&](int i) { return i >= 0 && i < 4 && local[std::size_t(i)]; };
  for (auto &f : st.floaters) {
    const int i = s8(f.slot);
    if (!isLocal(i) || !f.amount) continue;
    st.hold[std::size_t(i)] = 60;
    hud_dynamic::GoldFloater step;
    step.position = f.pos;
    step.velocity = f.vel;
    const auto next = hud_dynamic::goldFloaterStep(step, i);
    f.pos = next.position;
    f.vel = next.velocity;
    const int32_t a = goldFloaterAlpha(st.frame, f.start);
    out.floaters.push_back({F::to_int(f.pos[0]), F::to_int(f.pos[1]),
                            std::to_string(int32_t(f.amount)), (uint32_t(a) << 24) | TEXT_COLOUR});
  }
  for (int i = 0; i < 4; ++i) {
    if (!local[std::size_t(i)]) continue;
    const auto s = hud_dynamic::goldCounterStep(st.hold[std::size_t(i)], st.alpha[std::size_t(i)]);
    st.hold[std::size_t(i)] = s.first;
    st.alpha[std::size_t(i)] = s.second;
    if (st.alpha[std::size_t(i)] <= 0) continue;
    const std::string text = formatGold(int32_t(st.gold[std::size_t(i)]), language);
    int x = TEXT_X[std::size_t(i)];
    if (i == 1 || i == 3) x -= width(text);
    out.counters.push_back({i, x, TEXT_Y[std::size_t(i)], text,
                            (uint32_t(st.alpha[std::size_t(i)]) << 24) | TEXT_COLOUR});
  }
  for (int i = 0; i < 4; ++i)
    if (st.alpha[std::size_t(i)] > 0)
      out.coins.push_back({i, ICON_X[std::size_t(i)], TEXT_Y[std::size_t(i)] - 10,
                           (uint32_t(st.alpha[std::size_t(i)]) << 24) | COIN_TINT});
  return out;
}

// ---- quick slots -----------------------------------------------------------
int countPotions(const std::vector<Bag2Cell> &bag, int kind) {
  // count_potions also documents kind 2; Player_usePotion documents only 0/1.
  if (kind < 0 || kind > 2) throw Refusal("potion count kind outside 0..2");
  const int want = kind == 0 ? 0 : 1;
  int total = 0;
  for (const auto &c : bag)
    if (c.kind == want) total += s16v(c.quantity);
  return total;
}
int potionSize(Raw missing_plus_one, const std::array<int, 9> &found) {
  const int32_t whole = F::to_int(missing_plus_one);
  if (whole < 0) throw Refusal("potion size needs a positive missing amount (the original returns early)");
  int s = whole / 60;
  if (s >= 9) s = 8;
  if (found[std::size_t(s)] >= 0) return s;
  if (found[8] >= 0) return 8;
  for (int i = 7; i >= 0; --i)
    if (found[std::size_t(i)] >= 0) return i;
  return -1;
}
PotionUse usePotion(const PotionInput &in) {
  if (in.kind < 0 || in.kind > 1) throw Refusal("potion kind outside 0..1");
  PotionUse out;
  if (in.copy || F::le(in.hp, 0)) return out;
  out.regen = std::array<Raw, 2>{F::fmin(F::mul(in.max_hp, 0x3b088888u), 0x3effffffu),
                                 F::fmin(F::mul(in.max_mana, 0x3b088888u), 0x3effffffu)};
  Raw cur, mx, pend;
  int no_line, count_line;
  if (in.kind == 0) {
    cur = in.hp; mx = in.max_hp; pend = in.hp_pending; no_line = 1; count_line = 4;
  } else {
    cur = in.mana; mx = in.max_mana; pend = in.mana_pending; no_line = 2; count_line = 5;
  }
  const Raw have = F::add(cur, pend);
  if (F::le(mx, have)) return out;
  out.returned_early = false;
  const int want = in.kind == 0 ? 0 : 1;
  std::array<int, 9> found;
  found.fill(-1);
  for (std::size_t j = 0; j < in.bag.size(); ++j)
    if (in.bag[j].kind == want && in.bag[j].size >= 0 && in.bag[j].size < 9)
      found[std::size_t(in.bag[j].size)] = int(j);
  const int size = potionSize(F::add(F::sub(mx, have), F::ONE), found);
  out.size = size;
  out.bag = in.bag;
  if (size == -1) {
    out.notices.push_back({no_line, NOTICE_SHORT});
    out.pending = pend;
    return out;
  }
  pend = F::add(pend, F::mul(F::from_int(uint32_t(size + 1)), 0x42700000u));
  if (F::lt(mx, F::add(cur, pend))) pend = F::sub(mx, cur);
  out.pending = pend;
  if (in.menu_open && in.local && in.grid) {
    out.sent_drop = true;
  } else {
    const int cell = found[std::size_t(size)];
    auto &c = out.bag[std::size_t(cell)];
    c.quantity = int32_t(std::uint16_t(c.quantity - 1));
    if (c.quantity == 0) out.removed_cell = cell;
  }
  out.count = countPotions(out.bag, in.kind);
  if (out.count >= 1 && out.count <= 5) out.notices.push_back({count_line, NOTICE_LONG});
  return out;
}
bool padReadable(const std::array<bool, GateCount> &gates, bool local_menu_open) {
  if (local_menu_open) return true;
  return std::none_of(gates.begin(), gates.end(), [](bool g) { return g; });
}
PadAxes padAxes(uint32_t bits) {
  PadAxes a;
  if (bits & 0x2000) a.dx = 1;
  if (bits & 0x8000) a.dx = -1;
  if (bits & 0x1000) a.dy = -1;
  if (bits & 0x4000) a.dy = 1;
  a.any = bits & 0xf000;
  return a;
}
int buttonGlyph(uint32_t config_mask) {
  static constexpr std::array<uint32_t, 12> masks{0x20, 0x10, 0x80, 0x40, 0x8000, 0x2000,
                                                  0x1000, 0x4000, 0x4, 0x1, 0x8, 0x2};
  for (std::size_t i = 0; i < masks.size(); ++i)
    if (masks[i] == config_mask) return 0x7b + int(i);
  return 0;
}
SelectorMessage selectorMessage(int slot, int dir, bool menu_mode, uint32_t pressed,
                                const SelectorButtons &b, int kind, int value) {
  if (slot < 0 || slot > 3) throw Refusal("selector slot outside 0..3");
  if (dir < -1 || dir > 7) throw Refusal("selector direction outside -1..7");
  SelectorMessage m;
  if (dir == -1) return m;
  if (!menu_mode) {
    if (pressed & b.a) { m.id = 0; m.count = 3; m.args = {slot, 0, dir, 0}; }
    else if (pressed & b.b) { m.id = 0; m.count = 3; m.args = {slot, 1, dir, 0}; }
    else if (pressed & b.use) { m.id = 13; m.count = 2; m.args = {slot, dir, 0, 0}; }
  } else if (pressed & b.accept) {
    m.id = 1; m.count = 4; m.args = {slot, dir, kind, value};
  }
  return m;
}
MaskEntry maskLocalButtons(MaskEntry e, const SelectorButtons &b) {
  return {std::max<int32_t>(e.frames, 3), b.a | b.b | b.use, 0};
}
IconSource recordIcon(int id, bool newstyle) {
  if (id == -1) return IconSource::Blank;
  return newstyle ? IconSource::Skill : IconSource::Table;
}

// ---- minimap ----------------------------------------------------------------
namespace {
constexpr Raw K_ASPECT = 0x3faa9fbeu, K_QUARTER = 0x3e800000u, K_CELL = 0x3d088888u,
              K_STEP = 0x40f00000u, K_RADIUS = 0x3e088888u, K_BLIP_MIN_ZOOM = 0x3e000000u,
              K_TWO = 0x40000000u;
constexpr std::array<std::array<Raw, 2>, 3> BLIP_SHAPE{
    {{0, 0xbf800000u}, {0xbf400000u, 0x3f800000u}, {0x3f400000u, 0x3f800000u}}};
int32_t libmInt(Raw x, bool floor_mode) {
  const int e = int((x >> 23) & 0xff);
  if (e == 0) return 0;
  double v;
  if (e == 0xff) v = (x & 0x80000000u ? -1.0 : 1.0) * std::ldexp(1.0, 129);
  else {
    v = std::ldexp(double((x & 0x7fffffu) | 0x800000u), e - 150);
    if (x & 0x80000000u) v = -v;
  }
  const double n = floor_mode ? std::floor(v) : std::ceil(v);
  if (n >= 2147483647.0) return INT32_MAX;
  if (n <= -2147483648.0) return INT32_MIN;
  return int32_t(n);
}
} // namespace
bool minimapGate(bool world_texture, int32_t free_units, Raw half_w) {
  if (!world_texture) return false;
  if (free_units < 12000) return false;
  return F::to_int(half_w) >= 2;
}
std::array<Raw, 2> worldToCell(Raw x, Raw y, const std::array<Raw, 2> &origin) {
  return {F::add(origin[0], F::mul(F::mul(x, K_QUARTER), K_CELL)),
          F::add(origin[1], F::mul(F::mul(y, K_QUARTER), K_CELL))};
}
bool blipsEnabled(Raw zoom) { return F::lt(K_BLIP_MIN_ZOOM, zoom); }
MinimapView minimapView(const MinimapInput &in) {
  MinimapView v;
  v.gate = minimapGate(in.world_texture, in.free_units, in.half_w);
  if (!v.gate) return v;
  const int32_t hw = F::to_int(in.half_w), hh = F::to_int(in.half_h), oy = F::to_int(in.offy);
  const Raw fhw = F::from_int(uint32_t(hw));
  int32_t y1 = wrap32(int64_t(oy) + hh + 220), y0 = wrap32(int64_t(oy) - hh + 220);
  int32_t x0 = wrap32(int64_t(F::to_int(F::sub(in.offx, fhw))) + 320);
  int32_t x1 = wrap32(int64_t(F::to_int(F::add(in.offx, fhw))) + 320);
  v.sx0 = std::max(x0, 0);
  v.sy0 = std::max(y0, 0);
  v.sx1 = std::min(x1, 639);
  v.sy1 = std::min(y1, 447);
  const uint32_t a = uint32_t(-in.yaw);
  v.cos_t = F::cos16(a);
  v.sin_t = F::sin16(a);
  Raw r = F::div(F::mul(fhw, K_ASPECT), in.zoom);
  v.col_x = F::mul(F::mul(v.cos_t, K_STEP), in.zoom);
  v.col_y = F::mul(F::mul(v.sin_t, K_STEP), in.zoom);
  v.row_x = F::mul(F::mul(v.sin_t, K_STEP), in.zoom);
  v.row_y = F::mul(F::mul(F::neg(v.cos_t), K_STEP), in.zoom);
  const auto c = worldToCell(in.centre[0], in.centre[1], in.origin);
  r = F::mul(r, K_RADIUS);
  const Raw t1 = F::add(F::mul(c[0], v.col_x), F::mul(c[1], v.row_x));
  const Raw t0 = F::add(F::mul(c[0], v.col_y), F::mul(c[1], v.row_y));
  v.base_x = F::sub(in.offx, t1);
  v.base_y = F::sub(in.offy, t0);
  v.col_min = libmInt(F::fmax(F::sub(F::sub(c[0], r), F::ONE), 0), true);
  v.row_min = libmInt(F::fmax(F::sub(F::sub(c[1], r), F::ONE), 0), true);
  v.col_end = libmInt(F::fmin(F::add(c[0], r), F::from_int(uint32_t(in.width))), false);
  v.row_end = libmInt(F::fmin(F::add(c[1], r), F::from_int(uint32_t(in.height))), false);
  v.drawn = v.col_min < v.col_end && v.row_min < v.row_end;
  return v;
}
std::optional<BlipTriangles> blipTriangles(const MinimapInput &in, const MinimapView &v,
                                           const Blip &b, Raw blip_size) {
  if (!v.drawn || !blipsEnabled(in.zoom)) return std::nullopt;
  const auto uv = worldToCell(b.x, b.y, in.origin);
  const Raw sy = F::add(F::add(v.base_y, F::mul(uv[0], v.col_y)), F::mul(uv[1], v.row_y));
  const Raw sx = F::add(F::add(v.base_x, F::mul(uv[0], v.col_x)), F::mul(uv[1], v.row_x));
  const uint32_t ang = uint32_t(wrap32(int64_t(in.yaw) + (b.angle & 0xffff) - 0x3fff)) & 0xffff;
  const Raw c = F::cos16(ang), sn = F::sin16(ang);
  BlipTriangles out;
  for (int pass = 0; pass < 2; ++pass)
    for (std::size_t k = 0; k < 3; ++k) {
      const Raw fx = F::mul(F::mul(BLIP_SHAPE[k][0], blip_size), b.scale);
      const Raw fy = F::mul(F::mul(BLIP_SHAPE[k][1], blip_size), b.scale);
      Raw yy = F::add(sy, F::add(F::mul(fx, F::neg(sn)), F::mul(fy, c)));
      Raw xx = F::add(sx, F::add(F::mul(fx, c), F::mul(fy, sn)));
      if (pass == 0) {
        yy = F::add(yy, K_TWO);
        xx = F::add(xx, K_TWO);
        out.shadow[k] = {xx, yy};
      } else {
        out.colour[k] = {xx, yy};
      }
    }
  out.colour_word = b.r | 0x80000000u | (b.b << 16) | (b.g << 8);
  return out;
}
int iconAlpha(std::uint8_t high) {
  const int explored = high & 0xf0;
  return explored > 0x80 ? 0x80 : explored;
}
void minimapTiles() {
  throw Refusal("minimap tiles are drawn by VU1 program 0x403e20, which research read (O) but did not execute; refused until a VU1 interpreter or a frame capture confirms the arithmetic and the world +0x6c atlas layout");
}
void minimapIcons() {
  throw Refusal("minimap icons call vars_get and script_runByName from draw code on one machine; the icon table writer and field +0x10 are unread, so icons are refused");
}
} // namespace champions::hud_extras
