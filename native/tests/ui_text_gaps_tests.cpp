// Replays tests/vectors/ui_text_gaps.tsv (tools/generate_ui_text_gaps_vectors.py:
// the research model tools/ref_ui_text_gaps.py at 523b3db on synthetic cases),
// then states the hand checks of docs/ui-text-gaps.md directly. No game file.
#include "champions/strings.hpp"
#include "champions/ui_layout.hpp"
#include "champions/ui_text_gaps.hpp"
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace G = champions::ui_text_gaps;
namespace S = champions::simulation::rta::strings;
namespace rta = champions::simulation::rta;

namespace {
int failures = 0;
long checks = 0;
long line_no = 0;
std::map<std::string, long> per_kind;

template <class A, class B> void eq(const A &got, const B &want, const std::string &what) {
  ++checks;
  if (!(got == want) && ++failures <= 25) {
    std::ostringstream o;
    o << "line " << line_no << ": " << what << "\n  got  " << got << "\n  want " << want;
    std::cerr << o.str() << "\n";
  }
}
void ok(bool cond, const std::string &what) { eq(cond ? 1 : 0, 1, what); }

std::vector<std::string> split(const std::string &s, char sep) {
  std::vector<std::string> out;
  std::string cur;
  for (const char c : s) {
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
std::uint32_t hex(const std::string &s) { return static_cast<std::uint32_t>(std::stoull(s, nullptr, 16)); }
long dec(const std::string &s) { return std::stol(s); }
std::vector<std::string> csv(const std::string &s) { return s == "-" ? std::vector<std::string>{} : split(s, ','); }
std::vector<rta::Raw> raws(const std::string &s) {
  std::vector<rta::Raw> out;
  for (const auto &t : csv(s))
    out.push_back(hex(t));
  return out;
}
std::string join(const std::vector<std::string> &v, char sep) {
  std::string out;
  for (std::size_t i = 0; i < v.size(); ++i)
    out += (i ? std::string(1, sep) : std::string()) + v[i];
  return out;
}
std::string joinCalls(const std::vector<G::Call> &calls) {
  std::vector<std::string> parts;
  for (const auto &c : calls) {
    auto t = G::canonical(c);
    for (auto &ch : t)
      if (ch == '\t')
        ch = '~';
    parts.push_back(t);
  }
  return parts.empty() ? "-" : join(parts, '|');
}
std::string h8(std::uint32_t v) {
  char b[16];
  std::snprintf(b, sizeof b, "%08x", v);
  return b;
}
std::string rawsOut(const std::vector<rta::Raw> &v) {
  std::vector<std::string> p;
  for (const auto r : v)
    p.push_back(h8(r));
  return p.empty() ? "-" : join(p, ',');
}

void ramps(const std::vector<std::string> &f) {
  G::Ramps in;
  const auto p = raws(f[1]), fa = raws(f[2]);
  for (int i = 0; i < 4; ++i) {
    in.pulse[std::size_t(i)] = p[std::size_t(i)];
    in.fade[std::size_t(i)] = fa[std::size_t(i)];
  }
  const auto out = G::rampsStep(in);
  eq(rawsOut({out.pulse.begin(), out.pulse.end()}), f[3], "ramps pulse");
  eq(rawsOut({out.fade.begin(), out.fade.end()}), f[4], "ramps fade");
}
void layout(const std::vector<std::string> &f) {
  std::vector<std::string> got;
  for (const auto &w : G::buttonLayout(static_cast<std::int8_t>(dec(f[1])))) {
    char b[40];
    std::snprintf(b, sizeof b, "%x=%x", w.address, w.value);
    got.push_back(b);
  }
  eq(got.empty() ? std::string("-") : join(got, ','), f[2], "layout words");
}
void help(const std::vector<std::string> &f) {
  const auto i = csv(f[1]);
  G::HelpState st;
  st.delay = int(dec(i[0]));
  st.open = static_cast<std::int8_t>(dec(i[1]));
  st.net = static_cast<std::int8_t>(dec(i[2]));
  st.paused = static_cast<std::int8_t>(dec(i[3]));
  st.pause_byte = static_cast<std::int8_t>(dec(i[4]));
  st.local_menu = static_cast<std::int8_t>(dec(i[5]));
  st.dl_free = int(dec(i[6]));
  st.pulse = hex(i[7]);
  st.title = hex(i[8]);
  st.body = hex(i[9]);
  st.font_med = hex(i[10]);
  st.glyph_word = hex(i[11]);
  st.num_players = int(dec(i[12]));
  st.confirm_mask = hex(i[13]);
  st.wrap_height = int(dec(i[14]));
  st.label_width = int(dec(i[15]));
  const auto pads = csv(f[2]);
  for (std::size_t k = 0; k < 4; ++k)
    st.pads[k] = hex(pads[k]);
  st.alive.clear();
  st.local.clear();
  for (const auto &a : csv(f[3])) {
    const int slot = int(dec(a.substr(0, a.size() - 1)));
    st.alive.push_back(slot);
    if (a.back() == 'L')
      st.local.insert(slot);
  }
  const auto r = G::helpTask(st);
  const auto res = csv(f[4]);
  eq(int(r.state.delay), int(dec(res[0])), "help delay");
  eq(int(static_cast<std::uint8_t>(r.state.open)), int(dec(res[1])), "help open");
  eq(int(r.state.paused), int(dec(res[2])), "help paused");
  eq(int(r.state.pause_byte), int(dec(res[3])), "help pause byte");
  eq(int(r.state.lockout), int(dec(res[4])), "help lockout");
  eq(joinCalls(r.calls), f[5], "help calls");
}
void helpInner(const std::vector<std::string> &f) {
  const auto fr = G::helpInnerFrame(int(dec(f[1])));
  std::vector<std::string> got;
  for (const auto &d : champions::ui_layout::menuFrame(fr.x, fr.y, fr.width, fr.height, fr.style)) {
    char b[200];
    std::snprintf(b, sizeof b, "%s,%d,%d,%s", d.resource.c_str(), d.x, d.y,
                  !d.clipped ? "-"
                             : (std::to_string(d.clip.left) + "/" + std::to_string(d.clip.top) + "/" +
                                std::to_string(d.clip.right) + "/" + std::to_string(d.clip.bottom)).c_str());
    got.push_back(b);
  }
  eq(join(got, ';'), f[2], "help inner frame draws");
}
void bust(const std::vector<std::string> &f) {
  const auto i = csv(f[1]);
  G::BustState st;
  st.shown = static_cast<std::int8_t>(dec(i[0]));
  st.latch = static_cast<std::int8_t>(dec(i[1]));
  st.fade = hex(i[2]);
  st.value = int(dec(i[3]));
  st.maximum = int(dec(i[4]));
  st.sim_frame = int(dec(i[5]));
  st.font_small = hex(i[6]);
  std::map<std::string, G::TexSize> sizes;
  for (const auto &e : split(f[2], ';')) {
    const auto kv = split(e, '=');
    const auto wh = split(kv[1], ',');
    sizes[kv[0]] = {int(dec(wh[0])), int(dec(wh[1]))};
  }
  st.tex = [&](const std::string &name) { return sizes.at(name); };
  const auto r = G::hudDrawBust(st);
  const auto res = csv(f[3]);
  eq(h8(r.fade), res[0], "bust fade");
  eq(int(r.latch), int(dec(res[1])), "bust latch");
  eq(int(r.value), int(dec(res[2])), "bust value");
  eq(joinCalls(r.calls), f[4], "bust calls");
}
void quad(const std::vector<std::string> &f) {
  const auto i = csv(f[1]);
  const auto q = G::spriteQuad(dec(i[0]) != 0, static_cast<std::int8_t>(dec(i[1])), int(dec(i[2])), int(dec(i[3])),
                               int(dec(i[4])), int(dec(i[5])), int(dec(i[6])), int(dec(i[7])), int(dec(i[8])),
                               int(dec(i[9])), {int(dec(i[10])), int(dec(i[11]))});
  std::vector<std::string> got;
  for (const auto &w : q) {
    char b[48];
    std::snprintf(b, sizeof b, "%llx:%x", static_cast<unsigned long long>(w.value), w.reg);
    got.push_back(b);
  }
  eq(join(got, ','), f[3], "quad qwords");
  eq(G::fieldCalls(dec(i[0]) != 0, static_cast<std::int8_t>(dec(i[1])), int(dec(i[5]))), int(dec(f[2])), "field calls");
}
void woven(const std::vector<std::string> &f) {
  const auto i = csv(f[1]);
  const auto w = G::wovenLine(int(dec(i[0])), int(dec(i[1])), int(dec(i[2])));
  eq(w ? std::to_string(w->stored_row) + "," + std::to_string(w->picture_line) : std::string("-"), f[2], "woven line");
}
void compass(const std::vector<std::string> &f) {
  const auto i = csv(f[1]);
  G::CompassState st;
  st.gate = int(dec(i[0]));
  st.player = int(dec(i[1]));
  st.object = static_cast<std::uint32_t>(dec(i[2]));
  st.dialog_open = int(dec(i[3]));
  st.shown = static_cast<std::int8_t>(dec(i[4]));
  st.flags = static_cast<std::uint32_t>(dec(i[5]));
  st.light_flag = static_cast<std::uint32_t>(dec(i[6]));
  st.light_depth = static_cast<std::uint32_t>(dec(i[7]));
  st.font_small = hex(i[8]);
  const auto view = raws(f[2]), cam = raws(f[3]), pxy = raws(f[4]), txy = raws(f[5]);
  for (std::size_t k = 0; k < 16; ++k)
    st.view[k] = view[k];
  for (std::size_t k = 0; k < 3; ++k)
    st.camera[k] = cam[k];
  for (std::size_t k = 0; k < 2; ++k) {
    st.player_xy[k] = pxy[k];
    st.target_xy[k] = txy[k];
  }
  const auto r = G::hudDrawCompass(st);
  const auto res = csv(f[6]);
  eq(h8(r.state.flags), res[0], "compass flags");
  eq(int(r.state.light_flag), int(dec(res[1])), "compass light flag");
  eq(long(r.state.light_depth), dec(res[2]), "compass light depth");
  eq(long(r.angle), dec(res[3]), "compass angle");
  eq(joinCalls(r.calls), f[7], "compass calls");
}
void seq(const std::vector<std::string> &f) {
  std::vector<G::SeqSlotView> slots;
  for (const auto &d : split(f[1], ';')) {
    const auto c = csv(d);
    G::SeqSlotView s;
    s.tracks = static_cast<std::int8_t>(dec(c[0]));
    s.released = dec(c[1]) != 0;
    s.has_event_table = dec(c[2]) != 0;
    s.sound_idx = static_cast<std::int16_t>(dec(c[3]));
    if (c[4] != "-")
      for (const auto &e : split(c[4], '/'))
        s.events.push_back(int(dec(e)));
    s.flags = static_cast<std::uint16_t>(dec(c[5]));
    s.duration = int(dec(c[6]));
    s.time = hex(c[7]);
    s.while_frozen = dec(c[8]) != 0;
    if (c[9] != "-") {
      const auto p = split(c[9], '/');
      s.linked = std::array<rta::Raw, 3>{hex(p[0]), hex(p[1]), hex(p[2])};
    }
    s.no_pos = dec(c[10]) != 0;
    const auto pos = split(c[11], '/');
    s.pos = {hex(pos[0]), hex(pos[1]), hex(pos[2])};
    s.serial = static_cast<std::uint16_t>(dec(c[12]));
    s.volume = hex(c[13]);
    slots.push_back(s);
  }
  std::vector<G::VoiceView> voices;
  if (f[2] != "-")
    for (const auto &v : split(f[2], ';')) {
      const auto p = split(v, ',');
      voices.push_back({static_cast<std::uint16_t>(hex(p[0])), int(dec(p[1])), int(dec(p[2]))});
    }
  const auto cam = raws(f[4]);
  std::vector<G::SeqEvent> events;
  bool fault = false;
  try {
    events = G::soundSeqUpdateAll(slots, voices, dec(f[3]) != 0, {cam[0], cam[1], cam[2]});
  } catch (const std::out_of_range &) {
    fault = true;
  }
  if (f[5] == "fault") {
    ok(fault, "sound cursor outside the event table faults");
    return;
  }
  ok(!fault, "seq did not fault");
  std::vector<std::string> ev;
  for (const auto &e : events)
    ev.push_back(e.kind + ":" + std::to_string(e.a) + ":" + (e.kind == "advance" ? h8(e.b) : std::string("0")));
  eq(ev.empty() ? std::string("-") : join(ev, ','), f[5], "seq events");
  std::vector<std::string> after;
  for (const auto &s : slots)
    after.push_back(std::to_string(int(static_cast<std::uint8_t>(s.tracks))) + "," + h8(s.tracks ? s.time : 0) + "," +
                    h8(s.pos[0]) + "/" + h8(s.pos[1]) + "/" + h8(s.pos[2]));
  eq(join(after, ';'), f[6], "seq state after");
}
void showname(const std::vector<std::string> &f) {
  std::vector<std::uint8_t> blob;
  for (std::size_t k = 0; k + 1 < f[1].size(); k += 2)
    blob.push_back(static_cast<std::uint8_t>(std::stoul(f[1].substr(k, 2), nullptr, 16)));
  S::TextTable table;
  table.register_table(blob, 0x600000);
  for (const auto &step : split(f[2], ';')) {
    const auto p = split(step, ',');
    std::string req;
    if (p[0] != "-")
      for (std::size_t k = 0; k + 1 < p[0].size(); k += 2)
        req += static_cast<char>(std::stoul(p[0].substr(k, 2), nullptr, 16));
    const auto r = table.lookup(req);
    std::string units;
    char b[8];
    for (const auto u : r.units) {
      std::snprintf(b, sizeof b, "%04x", u);
      units += b;
    }
    eq(int(r.hit), int(dec(p[1])), "showName hit");
    eq(units.empty() ? std::string("-") : units, p[2], "showName units");
    eq(table.nf_index(), int(dec(p[3])), "showName ring index");
  }
}

void replay(const std::string &path) {
  std::ifstream in(path);
  ok(bool(in), "vector file opens");
  std::string line;
  while (std::getline(in, line)) {
    ++line_no;
    if (line.empty() || line[0] == '#')
      continue;
    const auto f = split(line, '\t');
    per_kind[f[0]] += 1;
    if (f[0] == "ramps") ramps(f);
    else if (f[0] == "layout") layout(f);
    else if (f[0] == "padglyph") eq(h8(G::padMaskToGlyph(hex(f[1]))), f[2], "pad mask glyph");
    else if (f[0] == "glyphtext") {
      std::string hx;
      char b[4];
      for (const unsigned char c : G::glyphText(hex(f[1]))) {
        std::snprintf(b, sizeof b, "%02x", c);
        hx += b;
      }
      eq(hx.empty() ? std::string("-") : hx, f[2], "glyph text");
    } else if (f[0] == "help") help(f);
    else if (f[0] == "helpalpha") eq(G::helpButtonAlpha(int(dec(f[1]))), int(dec(f[2])), "help alpha");
    else if (f[0] == "helpinner") helpInner(f);
    else if (f[0] == "bust") bust(f);
    else if (f[0] == "quad") quad(f);
    else if (f[0] == "woven") woven(f);
    else if (f[0] == "compass") compass(f);
    else if (f[0] == "seq") seq(f);
    else if (f[0] == "showname") showname(f);
    else ok(false, "unknown vector kind " + f[0]);
  }
}

// docs/ui-text-gaps.md, stated directly.
void hand() {
  // 1. the ninth displayed frame from ui_restartFadeIn reaches exactly 1.0.
  const auto ramp = G::fadeRamp();
  const std::vector<rta::Raw> want{0x3df5c28f, 0x3e75c28f, 0x3eb851eb, 0x3ef5c28e, 0x3f199998,
                                   0x3f3851e9, 0x3f570a3a, 0x3f75c28b, 0x3f800000};
  ok(ramp == want, "the documented nine-value fade ramp");
  ok(rta::lt(ramp[7], G::ONE), "the eighth value is below 1.0");
  // Values at or above 1.0 are never touched; slots are independent.
  G::Ramps r;
  r.fade = {G::ONE, G::TWO, G::ZERO, 0x3f800001};
  r.pulse = {G::ONE, G::ONE, G::ONE, G::ONE};
  const auto n = G::rampsStep(r);
  ok(n.fade[0] == G::ONE && n.fade[1] == G::TWO && n.fade[2] == G::FADE_STEP && n.fade[3] == 0x3f800001,
     "fade at or above 1.0 stays; zero takes one step");
  ok(G::pulseRamp(G::TWO, 1)[0] == rta::add(rta::mul(G::ONE, G::PULSE_DECAY), G::ONE), "pulse 2.0 decays by 0.8 of the excess");
  ok(G::uiStartPulse() == G::TWO && G::uiRestartFadeIn() == 0, "ui_startPulse is 2.0, ui_restartFadeIn is 0.0");
  // 7. the glyph byte: 0x7e for the observed layout; layout 0 is inferred and refused; others write nothing.
  ok(G::confirmGlyph(1) == std::optional<std::uint32_t>(0x7e), "observed layout glyph is the cross 0x7e");
  ok(!G::confirmGlyph(0).has_value() && !G::confirmGlyph(2).has_value(), "inferred and unwritten layouts are refused");
  ok(G::buttonLayout(0).size() == 8 && G::buttonLayout(1).size() == 10 && G::buttonLayout(5).empty(), "layout word counts");
  for (const std::int8_t b : {std::int8_t(0), std::int8_t(1)})
    for (const auto &w : G::buttonLayout(b))
      if (w.address == G::G_CONFIRM_MASK)
        ok(G::padMaskToGlyph(w.value) == [&] {
          for (const auto &x : G::buttonLayout(b))
            if (x.address == G::G_GLYPH_CONFIRM)
              return x.value;
          return 0u;
        }(), "the confirm mask's glyph is the drawn glyph");
  ok(G::isButtonGlyph(0x7e) && G::isButtonGlyph(0x7b) && G::isButtonGlyph(0x88) && !G::isButtonGlyph(0x7a) && !G::isButtonGlyph(0x89),
     "0x7b..0x88 are the button glyphs");
  // 2. the help box: inner frame is as tall as the outer, style 0 (ctile), tint 0x3f000000.
  const auto inner = G::helpInnerFrame(40);
  ok(inner.x == 124 && inner.y == 104 && inner.width == 416 && inner.height == 116 && inner.style == 0 &&
         inner.tint == 0x3f000000,
     "help inner frame (124, 104, 416, h + 76), style 0");
  G::HelpState hs;
  hs.title = 0x1000;
  hs.body = 0x2000;
  hs.wrap_height = 42;
  hs.label_width = 60;
  const auto drawn = G::helpTask(hs);
  ok(drawn.calls.size() == 13, "help box draws 13 recorded calls (sprintf is not an outward call here)");
  hs.dl_free = 2999;
  hs.pads[0] = 0x40;
  const auto starved = G::helpTask(hs);
  ok(starved.calls.size() == 2 || (!starved.calls.empty() && starved.state.open == 0),
     "below 3,000 free units nothing is drawn but a confirm still closes the box");
  ok(starved.state.open == 0 && starved.state.delay == 60 && starved.state.lockout == 60, "the confirm closes with delay 60");
  // 3. bust: one wrap per branch, small font, alpha 110 caption and 128 number, neutral sprite RGB.
  G::BustState bs;
  bs.fade = G::ONE;
  bs.font_small = 0x4f7104;
  bs.tex = [](const std::string &) { return G::TexSize{32, 16}; };
  for (const int maximum : {-1, 50}) {
    bs.maximum = maximum;
    bs.value = 10;
    const auto b = G::hudDrawBust(bs);
    int wraps = 0;
    for (const auto &c : b.calls)
      wraps += c.name == "text_drawWrapped";
    ok(wraps == 1, "exactly one wrap per branch");
    ok(std::get<std::uint32_t>(b.calls[0].args[0]) == 0x4f7104, "caption font is gamefontsmall (word 0x4f7104)");
    ok(std::get<std::uint32_t>(b.calls[0].args[2]) >> 24 == 110 || b.fade != G::ONE, "caption alpha is fade * 110");
    const auto &last = b.calls.back();
    ok((std::get<std::uint32_t>(last.args[4]) & 0xffffff) == 0x808080, "sprite RGB is neutral 0x808080");
  }
  bs.maximum = 50;
  bs.bar_frame_available = false;
  bs.latch = 1;
  const auto without_bar = G::hudDrawBust(bs);
  int clipped = 0;
  for (const auto &c : without_bar.calls)
    clipped += c.name == "sprite_clipped";
  ok(clipped == 0 && without_bar.latch == 0 && without_bar.fade < bs.fade,
     "unknown simulation frame omits bar while the real latch fades the bust");
  // 4. sprite fields: an odd top asks gfx_displayField twice and the woven picture sits at y for both parities.
  ok(G::fieldCalls(false, 1, 0) == 1 && G::fieldCalls(false, 1, 1) == 2 && G::fieldCalls(true, 1, 1) == 0 &&
         G::fieldCalls(false, 0, 1) == 0,
     "field call counts by mode and parity");
  // 5. the compass draws two passes; pass 1 has flag 0x20 and pass 2 none, with light depth + 10.
  G::CompassState cs;
  cs.view = {0x3f800000, 0, 0, 0, 0, 0x3f800000, 0, 0, 0, 0, 0x3f800000, 0, 0, 0, 0, 0x3f800000};
  const auto c = G::hudDrawCompass(cs);
  int draws = 0;
  for (const auto &call : c.calls)
    draws += call.name == "model_draw";
  ok(draws == 2 && c.state.flags == 0 && c.state.light_flag == 1 && c.state.light_depth == 10, "compass two passes");
  cs.dialog_open = 1;
  ok(G::hudDrawCompass(cs).calls.size() == 1, "an open dialog hides the compass after dialog_isOpen");
  // 6. soundSeq_updateAll: a slot whose cursor reached the terminator is freed on the next update and
  // its looping in-use voices of that owner and serial are stopped; one-shot voices play on.
  G::SeqSlotView slot;
  slot.tracks = 1;
  slot.events = {1, 2, 3};
  slot.sound_idx = 3;
  slot.serial = 9;
  std::vector<G::SeqSlotView> slots{slot};
  const std::vector<G::VoiceView> voices{{0x14, 0, 9}, {0x04, 0, 9}, {0x14, 1, 9}};
  const auto ev = G::soundSeqUpdateAll(slots, voices, false, {0, 0, 0});
  ok(slots[0].tracks == 0 && ev.size() == 1 && ev[0].kind == "stop" && ev[0].a == 0, "terminator frees and stops only the looping voice");
  slots[0] = slot;
  slots[0].sound_idx = 1;
  slots[0].while_frozen = true;
  slots[0].no_pos = true;
  slots[0].linked = std::array<rta::Raw, 3>{1, 2, 3};
  const auto ev2 = G::soundSeqUpdateAll(slots, {}, true, {7, 8, 9});
  ok(ev2.size() == 1 && ev2[0].kind == "advance" && slots[0].pos == (std::array<rta::Raw, 3>{7, 8, 9}),
     "a frozen-safe slot advances and the camera wins over the object");
  // 8. showName: quotes are dropped before the lookup; a miss shows NF: and the bare key.
  const std::string key = "dialog_key";
  std::vector<std::uint8_t> blob(key.begin(), key.end());
  blob.push_back(0);
  while (blob.size() != ((key.size() + 2) & ~std::size_t(1)))
    blob.push_back(0);
  for (const std::uint16_t u : {std::uint16_t('H'), std::uint16_t('i')}) {
    blob.push_back(u & 0xff);
    blob.push_back(u >> 8);
  }
  blob.insert(blob.end(), {0, 0, 0, 0});
  S::TextTable t;
  t.register_table(blob, 0x600000);
  ok(t.lookup("\"dialog_key\"").hit, "a quoted key whose bare form is present is found");
  const auto miss = t.lookup("\"\"missing\"");
  ok(!miss.hit && S::nf_text("\"\"missing\"") == miss.units && miss.units.size() == 3 + 7, "a miss shows NF: and the bare key");
  ok(t.lookup("\"\"").units.size() == 3, "a key made only of quotes looks up the empty key and shows NF:");
}
} // namespace

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "usage: ui_text_gaps_tests vectors.tsv\n";
    return 2;
  }
  replay(argv[1]);
  hand();
  for (const auto &[k, n] : per_kind)
    std::cout << k << " " << n << "\n";
  std::cout << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
