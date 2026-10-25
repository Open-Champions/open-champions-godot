#include "champions/hud_dynamic.hpp"
#include <algorithm>
#include <climits>
#include <stdexcept>

namespace champions::hud_dynamic {
namespace F = simulation::rta;
namespace {
int checked(long long n) {
  if (n < INT_MIN || n > INT_MAX)
    throw std::out_of_range("HUD coordinate overflow");
  return int(n);
}
void slot(int n) {
  if (n < 0 || n > 3)
    throw std::out_of_range("HUD slot outside 0..3");
}
int truncDiv(int a, int b) { return a / b; }
const std::array<int, 4> NOTICE_X{0, 477, 162, 321}, GOLD_X{50, 590, 50, 590},
    GOLD_Y{200, 200, 250, 250}, GOLD_ICON_X{16, 593, 16, 593},
    SELECTOR_X{0, 471, 156, 316};
struct NoticeLine {
  const char *resource;
  int dx, row;
  bool counted;
};
const std::array<NoticeLine, 8> NOTICE{{{"nocamera.tex", 51, 1, false},
                                        {"nohealth.tex", -17, 0, false},
                                        {"nomana.tex", -17, 1, false},
                                        {"noarrows.tex", 51, 0, false},
                                        {"counthealth.tex", -17, 0, true},
                                        {"countmana.tex", -17, 1, true},
                                        {"countarrows.tex", 51, 0, true},
                                        {"countthrown.tex", 51, 0, true}}};
} // namespace
const std::array<Stage, 10> &drawSequence() {
  static const std::array<Stage, 10> s{
      Stage::Gold,         Stage::Status,        Stage::Arena,
      Stage::MessageBoss,  Stage::Bust,          Stage::Compass,
      Stage::NetCamPrompt, Stage::ControllerOut, Stage::MedalPrompt,
      Stage::NetStatus};
  return s;
}
const char *stageName(Stage stage) {
  switch (stage) {
  case Stage::Gold:
    return "gold";
  case Stage::Status:
    return "status";
  case Stage::Arena:
    return "arena";
  case Stage::MessageBoss:
    return "message_boss";
  case Stage::Bust:
    return "bust";
  case Stage::Compass:
    return "compass";
  case Stage::NetCamPrompt:
    return "net_cam_prompt";
  case Stage::ControllerOut:
    return "controller_out";
  case Stage::MedalPrompt:
    return "medal_prompt";
  case Stage::NetStatus:
    return "net_status";
  }
  throw std::out_of_range("unknown HUD draw stage");
}
const std::array<const char *, 11> &statusSequence() {
  static const std::array<const char *, 11> s{
      "health bar",        "pending health rect",
      "pending mana rect", "experience bar",
      "mana bar",          "name text",
      "level text",        "level-up glow",
      "status frame",      "notice lines 0..7",
      "spell selector"};
  return s;
}
Records composeHud(const StageRecords &stages) {
  Records out;
  for (const auto &part : stages)
    out.insert(out.end(), part.begin(), part.end());
  return out;
}
Records composeStatus(const std::array<Records, 11> &parts) {
  Records out;
  for (const auto &part : parts)
    out.insert(out.end(), part.begin(), part.end());
  return out;
}
std::vector<std::size_t> painterIndices(const Records &records) {
  struct Packet {
    int layer = 0;
    std::vector<std::size_t> indices;
  };
  std::vector<Packet> packets;
  for (std::size_t i = 0; i < records.size();) {
    const int layer =
        std::visit([](const auto &r) { return r.layer; }, records[i]);
    if (layer < 0 || layer > 7)
      throw std::out_of_range("HUD sort layer");
    const Text *text = std::get_if<Text>(&records[i]);
    Packet packet;
    packet.layer = layer;
    if (text && text->batch != 0) {
      const int batch = text->batch;
      while (i < records.size()) {
        const Text *next = std::get_if<Text>(&records[i]);
        if (!next || next->batch != batch || next->layer != layer)
          break;
        packet.indices.push_back(i);
        ++i;
      }
    } else {
      packet.indices.push_back(i);
      ++i;
    }
    packets.push_back(std::move(packet));
  }
  std::vector<std::size_t> out;
  for (int layer = 0; layer < 8; ++layer)
    for (std::size_t p = packets.size(); p > 0; --p)
      if (packets[p - 1].layer == layer)
        out.insert(out.end(), packets[p - 1].indices.begin(),
                   packets[p - 1].indices.end());
  return out;
}
int pulse(std::uint32_t tick) {
  return F::to_int(F::mul(F::sin16(tick << 10), 0x41c80000));
}
std::uint32_t pendingLevelColour(int p) {
  const int h = truncDiv(p, 2);
  return 0x80000000u | (std::uint32_t(h + 50) << 16) |
         (std::uint32_t(h + 60) << 8) | std::uint32_t(h + 60);
}
std::uint32_t nameColour(int slot_no, int tick, std::uint32_t arena,
                         bool has_player, std::uint32_t player,
                         std::uint32_t mask) {
  slot(slot_no);
  std::uint32_t c = has_player ? player : arena ? arena : 0x80323c3c;
  if ((mask >> slot_no & 1) && (truncDiv(tick, 32) & 1))
    c = (c & 0xff000000u) | (((c >> 16 & 255) >> 1) << 16) |
        ((c >> 1) & 0x7f00) | ((c & 255) >> 1);
  return c;
}
int nameWidthBudget(bool a, bool b, int width) {
  return b && !a ? checked(131LL - width) : 131;
}
std::uint32_t pulseGrey(int p, int alpha) {
  const auto v = std::uint32_t(p + 103);
  return (std::uint32_t(alpha & 255) << 24) | (v << 16) | (v << 8) | v;
}
int glowStep(int alpha) { return std::min(alpha + 4, 128); }
Sprite glowSprite(int x, int alpha, int p) {
  return {"hud_statusbar00glow.tex", checked(static_cast<long long>(x) - 4), 36,
          6, pulseGrey(p, alpha)};
}
int noticeBaseX(int n, bool network) {
  slot(n);
  return NOTICE_X[network ? 0 : n] + 37;
}
int noticeAlphaStep(int timer, int alpha) {
  if (timer > 81)
    alpha += 4;
  if (timer < 38)
    alpha -= 4;
  return std::clamp(alpha, 0, 128);
}
NoticeFrame noticeFrame(int n, std::array<int, 8> timers,
                        std::array<int, 8> alphas,
                        const std::array<int, 8> &counts, std::uint32_t tick,
                        bool network, bool hide_counts) {
  NoticeFrame result;
  result.timers = timers;
  result.alphas = alphas;
  const int p = pulse(tick), base = noticeBaseX(n, network);
  for (int i = 0; i < 8; ++i) {
    if (!timers[i])
      continue;
    const auto &line = NOTICE[i];
    const int x = base + line.dx, y = 27 * line.row + 82;
    if (line.counted) {
      result.alphas[i] = noticeAlphaStep(timers[i], alphas[i]);
      if (counts[i] >= 0 && !hide_counts) {
        result.submissions.emplace_back(
            Text{"ascii", x + 31, 27 * line.row + 81, 4,
                 (std::uint32_t(result.alphas[i] & 255) << 24) | 0x4080e4,
                 "x " + std::to_string(counts[i])});
        result.submissions.emplace_back(
            Sprite{line.resource, x, y, 4, pulseGrey(p, result.alphas[i])});
      }
    } else
      result.submissions.emplace_back(
          Sprite{line.resource, x, y, 4, pulseGrey(p, 128)});
    --result.timers[i];
    if (result.timers[i] <= 0)
      result.cleared.push_back(i);
  }
  return result;
}
int countActiveNotices(const std::array<int, 8> &timers, int except_index) {
  const int hidden[8] = {-1, -1, -1, -1, 1, 2, 3, 3};
  int n = 0;
  for (int i = 0; i < 8; ++i)
    if (timers[i] && i != except_index && (hidden[i] < 0 || !timers[hidden[i]]))
      ++n;
  return n;
}
int messageAlphaStep(int timer, int alpha) {
  return std::min(timer < 32 ? timer * 4 : alpha + 3, 128);
}
MessageFrame messageFrame(int timer, int alpha, bool force, bool suppressed,
                          int width) {
  MessageFrame r;
  r.alpha = alpha;
  if (timer <= 0 || (suppressed && !force))
    return r;
  r.alpha = messageAlphaStep(timer, alpha);
  const int l = force ? 7 : 6, w = std::max(300, width);
  r.submissions.emplace_back(
      Text{"wide-centered",
           320,
           78,
           l,
           (std::uint32_t(r.alpha & 255) << 24) | 0x1e6464,
           {}});
  r.submissions.emplace_back(
      Frame{300 - truncDiv(w, 2), 76, checked(static_cast<long long>(w) + 40),
            32, l, std::uint32_t(truncDiv(r.alpha, 2) & 255) << 24, 0});
  return r;
}
BossFrame bossMeterFrame(BossState s, int language) {
  s.maximum = std::max(s.maximum, s.hp);
  BossFrame r{{}, s};
  if (!(s.hp > 0 && s.maximum > 0) && s.alpha <= 0) {
    r.state.visible = 0;
    return r;
  }
  if (s.hp <= 0) {
    s.hp = 0;
    s.alpha -= 3;
  }
  if (s.maximum <= 0)
    throw std::invalid_argument("boss maximum <= 0");
  r.submissions.emplace_back(
      Text{"wide-centered",
           320,
           408,
           5,
           (std::uint32_t(s.alpha & 255) << 24) | 0x464646,
           {}});
  const bool wide = language == 3 || language == 4;
  const int left = wide ? 162 : 218;
  const int fill =
      F::to_int(F::mul(F::div(F::from_int(std::uint32_t(s.hp)),
                              F::from_int(std::uint32_t(s.maximum))),
                       wide ? 0x439d8000u : 0x43420000u));
  std::uint32_t colour;
  if (s.visible) {
    const int prev = std::int16_t(s.previous);
    colour = (std::uint32_t(s.alpha & 255) << 24) |
             (s.hp < prev ? 0x808080 : 0x181850);
  } else {
    s.alpha = 78;
    colour = 0x4e181850;
  }
  r.submissions.emplace_back(Rect{left, 414, left + fill, 430, 5, colour});
  r.submissions.emplace_back(
      Sprite{wide ? "bossmeter_pal.tex" : "bossmeter.tex", wide ? 158 : 209,
             409, 6, 0x80808080});
  r.state = {s.hp, s.maximum, s.alpha, 1, s.hp & 65535};
  return r;
}
GoldFloater goldFloaterStep(const GoldFloater &state, int n) {
  slot(n);
  GoldFloater r = state;
  for (int i = 0; i < 3; ++i)
    r.position[i] = F::add(state.position[i], state.velocity[i]);
  const std::array<Raw, 3> target{F::from_int(std::uint32_t(GOLD_X[n])),
                                  F::from_int(std::uint32_t(GOLD_Y[n])), 0};
  for (int i = 0; i < 3; ++i) {
    const Raw d = F::sub(target[i], r.position[i]),
              w = F::add(state.velocity[i], F::mul(d, 0x3bd099e0));
    r.velocity[i] = F::sub(w, F::mul(w, 0x3db3be9b));
  }
  return r;
}
int goldFloaterAlpha(int tick, int start) {
  return int(std::clamp(128LL - 3LL * (static_cast<long long>(tick) - start),
                        0LL, 128LL));
}
std::pair<int, int> goldCounterStep(int hold, int alpha) {
  if (hold)
    return {hold - 1, alpha < 128 ? alpha + 4 : alpha};
  return {0, alpha > 0 ? alpha - 4 : alpha};
}
Records goldCounter(int n, int alpha, int text_width) {
  slot(n);
  Records out;
  if (alpha <= 0)
    return out;
  const int x = (n == 1 || n == 3) ? GOLD_X[n] - text_width : GOLD_X[n];
  out.emplace_back(Text{"ascii",
                        x,
                        GOLD_Y[n],
                        6,
                        (std::uint32_t(alpha & 255) << 24) | 0x4080e4,
                        {}});
  out.emplace_back(Sprite{"gold03.tex", GOLD_ICON_X[n], GOLD_Y[n] - 10, 6,
                          (std::uint32_t(alpha & 255) << 24) | 0x808080});
  return out;
}
std::pair<int, int> selectorCentre(int n, bool network, bool menu_mode) {
  slot(n);
  return menu_mode ? std::pair<int, int>{141, 249}
                   : std::pair<int, int>{SELECTOR_X[network ? 0 : n] + 67, 145};
}
int selectorDirection(int dx, int dy) {
  static const int table[3][3] = {{7, 0, 1}, {6, -1, 2}, {5, 4, 3}};
  return dx >= -1 && dx <= 1 && dy >= -1 && dy <= 1 ? table[dy + 1][dx + 1]
                                                    : -1;
}
SelectorFade selectorFadeStep(SelectorFade s, bool active) {
  if (active) {
    s.timer = 0x41f00000;
    s.speed = 4;
  }
  if (F::eq(s.timer, F::ZERO))
    s.speed = -1;
  s.timer = F::sub(s.timer, F::ONE);
  s.alpha = std::clamp(s.alpha + s.speed * 4, 0, 128);
  return s;
}
std::pair<int, int> selectorRadii(int alpha) {
  const Raw t = F::mul(F::from_int(std::uint32_t(alpha)), 0x3c000000),
            smooth = F::hermite(t, F::ZERO, F::ONE, F::ZERO, F::ZERO);
  return {F::to_int(F::mul(smooth, 0x42700000)),
          F::to_int(F::mul(smooth, 0x42000000))};
}
std::array<SelectorIcon, 8> selectorRing(int cx, int cy, int alpha, int page) {
  std::array<SelectorIcon, 8> out{};
  const Raw radius = F::from_int(std::uint32_t(selectorRadii(alpha).first));
  for (int i = 0; i < 8; ++i) {
    const std::uint32_t angle = std::uint32_t(-0x4000 + 0x2000 * i);
    out[i] = {i,
              checked(static_cast<long long>(cx) +
                      F::to_int(F::mul(radius, F::cos16(angle))) +
                      320LL * page),
              checked(static_cast<long long>(cy) +
                      F::to_int(F::mul(radius, F::sin16(angle))))};
  }
  return out;
}
std::pair<std::uint32_t, std::uint32_t> selectorHighlight(std::uint32_t tick,
                                                          int alpha) {
  const Raw s = F::mul(F::sin16(tick << 10), 0x41800000);
  const int corona = F::to_int(F::add(s, 0x43000000)),
            grey = F::to_int(F::add(s, 0x436f0000));
  return {(std::uint32_t(corona & 255) << 24) | 0x808080,
          (std::uint32_t(alpha & 255) << 24) | (std::uint32_t(grey) << 16) |
              (std::uint32_t(grey) << 8) | std::uint32_t(grey)};
}
} // namespace champions::hud_dynamic
