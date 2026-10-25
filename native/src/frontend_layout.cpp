#include "champions/frontend.hpp"

#include <algorithm>
#include <cstring>
#include <utility>

// Draw calls of the front-end screens (docs/FRONTEND.md "Layout"). Sources, all
// from t3/research-menus-layout dc458d5:
//   docs/frontend-layout.md and tools/ref_frontend_layout.py (list geometry,
//   frames, fonts, sprites, sounds, theme gate),
//   tools/ref_frontend_screens.py (character, card and message-box tables).
// Anything those files list as Unknown is emitted as Evidence::Host.
namespace champions::frontend {

const char *evidence_name(Evidence e) {
  switch (e) {
  case Evidence::Executed: return "Executed";
  case Evidence::Observed: return "Observed";
  case Evidence::Inferred: return "Inferred";
  case Evidence::Host: return "Host";
  }
  return "Host";
}

const char *menu_sound_resource(menu_sound s) {
  // ref_frontend_layout.MENU_SOUND_IDS: FX.LMP resource names looked up before
  // sound_playOnObject 0x214230.
  switch (s) {
  case menu_sound::move: return "menu_main_toggle.anm";
  case menu_sound::select: return "menu_main_accept.anm";
  case menu_sound::back: return "menu_main_back.anm";
  }
  return "";
}

namespace layout {
namespace {
int bcd_value(std::uint8_t value) {
  const int high = value >> 4, low = value & 15;
  if (high > 9 || low > 9)
    throw Refusal("card directory date or clock has invalid BCD digits");
  return high * 10 + low;
}
// Row geometry of menuList_draw 0x1a07c4: rows are 22 apart, the first row sits
// at origin_y - (11 * rows + 6); a value row's text is shifted 23 left; a list
// with value rows is the wide 210 frame, others the 54-wide minimum. The research
// says only that text width "can enlarge" a narrow list; no width rule is traced,
// so only the executed minimum-width calls are emitted (REFUSED: text widening).
constexpr int ROW_STEP = 22;
constexpr int NARROW_W = 54, WIDE_W = 210;
constexpr int VALUE_SHIFT = 23;

Draw make_frame(int x, int y, int w, int h, int layer, std::uint32_t tint, int style, Evidence ev,
                const char *role) {
  Draw d;
  d.kind = Draw::Kind::Frame;
  d.evidence = ev;
  d.x = x;
  d.y = y;
  d.w = w;
  d.h = h;
  d.min_w = w;
  d.layer = layer;
  d.colour = tint;
  d.style = style;
  d.role = role;
  return d;
}
} // namespace

std::vector<CardRow> visible_card_rows(const std::vector<std::uint8_t> &metadata,
                                       const std::vector<std::uint8_t> &headers,
                                       int count, int scroll_base, bool saving) {
  if (count < 0 || scroll_base < 0 || scroll_base > count ||
      static_cast<std::size_t>(count) > metadata.size() / 16 ||
      static_cast<std::size_t>(count) > headers.size() / 0x70)
    throw Refusal("card cache count, scroll, metadata or header length is invalid");
  std::vector<CardRow> out;
  if ((count == 0 || scroll_base == count) && !saving)
    return out;
  for (int r = 0; r < 5; ++r) {
    const int index = scroll_base + r;
    CardRow row;
    row.index = index;
    row.y = 128 + 17 * r;
    row.number = std::to_string(static_cast<long long>(index) + 1) + ".";
    if (index < count) {
      row.populated = true;
      row.metadata_offset = static_cast<std::size_t>(index) * 16;
      row.header_offset = static_cast<std::size_t>(index) * 0x70;
      const auto meta = row.metadata_offset;
      const int month = bcd_value(metadata[meta + 6]);
      const int day = bcd_value(metadata[meta + 5]);
      const int year = bcd_value(metadata[meta + 7]);
      const int hour = bcd_value(metadata[meta + 3]);
      const int minute = bcd_value(metadata[meta + 2]);
      if (month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 || minute > 59)
        throw Refusal("card directory date or clock is outside supported fields");
      row.date = std::to_string(month) + "/" + std::to_string(day) + "/" +
                 std::to_string(2000 + year);
      const int shown_hour = hour % 12 == 0 ? 12 : hour % 12;
      row.clock = std::to_string(shown_hour) + ":" + (minute < 10 ? "0" : "") +
                  std::to_string(minute) + (hour < 12 ? "am" : "pm");
    } else if (saving && index == count) {
      row.new_save = true;
    }
    out.push_back(std::move(row));
  }
  return out;
}

CardPreview card_preview(const std::vector<std::uint8_t> &headers, int count, int header_index) {
  if (count <= 0 || header_index < 0 || header_index >= count ||
      static_cast<std::size_t>(count) > headers.size() / 0x70)
    throw Refusal("selected card header is unavailable");
  const auto at = static_cast<std::size_t>(header_index) * 0x70;
  const auto word = [&](std::size_t offset) {
    return std::uint32_t(headers[at + offset]) |
           (std::uint32_t(headers[at + offset + 1]) << 8) |
           (std::uint32_t(headers[at + offset + 2]) << 16) |
           (std::uint32_t(headers[at + offset + 3]) << 24);
  };
  return {header_index, at, word(0x64),
          std::uint16_t(headers[at + 0x6a] | (std::uint16_t(headers[at + 0x6b]) << 8)),
          headers[at + 0x6c]};
}

std::vector<Point> list_anchors(const ListSpec &s) {
  const int first = s.origin_y - (11 * s.rows + 6);
  std::vector<Point> out;
  for (int i = 0; i < s.rows; ++i)
    out.push_back({s.origin_x - ((s.value_rows >> i) & 1u ? VALUE_SHIFT : 0), first + ROW_STEP * i});
  return out;
}

std::array<Draw, 3> list_frames(const ListSpec &s, int selected) {
  const auto anchors = list_anchors(s);
  const int rows = s.rows;
  const int first = s.origin_y - (11 * rows + 6);
  const bool wide = s.value_rows != 0;
  // The wrapper (0x17fbb8): (x, y, w, h) = (origin - 27, first - 7, 54, 22 rows + 26).
  const int w = wide ? WIDE_W : NARROW_W;
  const int x = s.origin_x - w / 2;
  const int y = first - 7;
  const int h = ROW_STEP * rows + 26;
  const int row = std::clamp(selected, 0, rows - 1);
  const int row_y = anchors[static_cast<std::size_t>(row)].y;
  int cursor_x, cursor_w;
  if ((s.value_rows >> row) & 1u) {
    cursor_x = s.origin_x - 140; // row 0 of the options list (Executed); other value rows Inferred
    cursor_w = 280;
  } else if (wide) {
    cursor_x = s.origin_x - 21;
    cursor_w = 42;
  } else {
    cursor_w = 40;
    cursor_x = s.origin_x - cursor_w / 2;
  }
  std::array<Draw, 3> out;
  // Only row 0 was executed in the research. Other rows use a host highlight;
  // their cursor-frame geometry is not claimed as a native call.
  out[0] = make_frame(cursor_x, row_y, cursor_w, 32 + (row_y & 1), 7, LIST_CURSOR_TINT, 0,
                      row == 0 ? Evidence::Executed : Evidence::Host,
                      row == 0 ? "list cursor frame" : "host list cursor highlight");
  out[1] = make_frame(x - 1, y - 4, w + 2, h + 8, 7, LIST_BACKING_TINT, 2, Evidence::Executed,
                      "list backing frame");
  out[2] = make_frame(x + 8, y + 8, w, h, 6, LIST_OVERLAY_TINT, 0, Evidence::Executed,
                      "list front overlay");
  out[0].min_w = (s.value_rows >> row) & 1u ? 280 : wide ? 42 : 40;
  out[1].min_w = (wide ? WIDE_W : NARROW_W) + 2;
  out[2].min_w = wide ? WIDE_W : NARROW_W;
  return out;
}

ListSpec main_list() { return {4, 0, 320, 309}; }
ListSpec game_type_list() { return {2, 0, 320, 309}; }
ListSpec players_list() { return {4, 0, 320, 309}; }
ListSpec difficulty_list() { return {4, 0, 320, 309}; }
ListSpec mode_list() { return {2, 0, 320, 309}; }
ListSpec options_list() { return {13, 0x7u, 320, 238}; }
ListSpec pause_list() { return {5, 0, 320, 240}; }

std::array<Draw, 2> logo_sprites() {
  // ref_frontend_layout.publisher_logo_sprites: unclipped, layer 3, tint 0x80808080.
  std::array<Draw, 2> out;
  const struct {
    const char *name;
    int x, y;
  } rows[2] = {{"soe_splash_url.tex", 192, 330}, {"soe_splash.tex", 64, 50}};
  for (int i = 0; i < 2; ++i) {
    out[static_cast<std::size_t>(i)].kind = Draw::Kind::Sprite;
    out[static_cast<std::size_t>(i)].layer = 3;
    out[static_cast<std::size_t>(i)].x = rows[i].x;
    out[static_cast<std::size_t>(i)].y = rows[i].y;
    out[static_cast<std::size_t>(i)].colour = SPRITE_TINT;
    out[static_cast<std::size_t>(i)].lump = "SOELOGO.LMP";
    out[static_cast<std::size_t>(i)].resource = rows[i].name;
    out[static_cast<std::size_t>(i)].role = "publisher logo";
    out[static_cast<std::size_t>(i)].site = i == 0 ? "0x32165c" : "0x321688";
  }
  return out;
}

Draw title_sprite() {
  Draw d;
  d.kind = Draw::Kind::Sprite;
  d.layer = 5;
  d.x = 141;
  d.y = 15;
  d.colour = SPRITE_TINT;
  d.lump = "FRONTEND.LMP";
  d.resource = "title.tex";
  d.role = "title";
  d.site = "0x1e0ac8";
  return d;
}
} // namespace layout

namespace {
using layout::ListSpec;

// Host choices the research does not make. One place, so the report can list them.
constexpr std::uint32_t HOST_DISABLED = 0x80303030; // disabled rows: the treatment is Unknown
constexpr std::uint32_t HOST_MUTED = 0x80707070;    // host labels
// Front-end list text: the research gives the list batch (0x1a09e8) a fixture colour
// of 0 and the parent batch 0x1e0dcc 0x64464646; neither is a displayed colour, and
// 0x80464646 belongs to the pause batch only. The identity tint is a Host choice.
constexpr std::uint32_t HOST_LIST_TEXT = 0x80808080;
// Only the slot-list batch and credits colour points are tied to fade by the
// research. Other words retain their alpha; callers mark a known fade batch.

const char *const STAT_ROWS[4] = {"Strength", "Intelligence", "Dexterity", "Stamina"};

Draw text(Font font, Align align, int layer, int x, int y, std::uint32_t colour, std::string str,
          Evidence ev, const char *role, const char *site = "") {
  Draw d;
  d.kind = Draw::Kind::Text;
  d.evidence = ev;
  d.font = font;
  d.align = align;
  d.layer = layer;
  d.x = x;
  d.y = y;
  d.colour = colour;
  d.text = std::move(str);
  d.role = role;
  d.site = site;
  return d;
}

Draw rect(int left, int top, int right, int bottom, int layer, std::uint32_t colour, Evidence ev,
          const char *role, const char *site = "") {
  Draw d;
  d.kind = Draw::Kind::Rect;
  d.evidence = ev;
  d.layer = layer;
  d.x = left;
  d.y = top;
  d.w = right - left;
  d.h = bottom - top;
  d.colour = colour;
  d.role = role;
  d.site = site;
  return d;
}

// One list as menuList_draw submits it: the row text batch, then the three frames.
// The research does not order the text batch against the frames; text first puts
// it on top under the painter rule (Inferred).
void add_list(Scene &s, const ListSpec &spec, const std::vector<Entry> &entries, int cursor,
              std::uint32_t text_colour, Evidence colour_evidence, const char *role) {
  const auto anchors = layout::list_anchors(spec);
  for (std::size_t i = 0; i < anchors.size() && i < entries.size(); ++i) {
    const bool on = entries[i].enabled;
    s.draws.push_back(text(Font::Medium, Align::Centred, 7, anchors[i].x, anchors[i].y,
                           on ? text_colour : HOST_DISABLED, entries[i].label,
                           on ? colour_evidence : Evidence::Host, role, "0x1a09e8"));
  }
  const auto frames = layout::list_frames(spec, cursor);
  for (const auto &f : frames)
    s.draws.push_back(f);
}

// hud_drawMessageBox 0x1df248 (MESSAGE_BOX_CALLS): a small-font layer-6 batch with
// colour 0x323c3c, wrapped-centred lines at (320,212) width 470 line 17, then the
// frame wrapper (60,186,520,68). The wrapper's layer and tint are fixture values
// (layer 0, alpha not set); the inner call has a zero tint and draws nothing.
void add_message_box(Scene &s, const std::string &body, Evidence text_evidence) {
  Draw frame;
  frame.kind = Draw::Kind::Frame;
  frame.evidence = Evidence::Host;
  frame.x = 60 - 1;
  frame.y = 186 - 4;
  frame.w = 520 + 2;
  frame.h = 68 + 8;
  frame.min_w = 520;
  frame.layer = 0;
  frame.colour = 0x80808080; // Host opacity; wrapper gives only 0x00808080
  frame.style = 2; // host backing; the wrapper does not specify style or inflation
  frame.role = "message box frame";
  frame.site = "0x1df504";
  Draw line = text(Font::Small, Align::WrappedCentred, 6, 320, 212, 0x323c3c, body,
                   text_evidence, "message box text", "0x1df30c");
  line.wrap_width = 470;
  line.line = 17;
  s.draws.push_back(line);
  s.draws.push_back(frame);
}

// The character screens' common footer (0x2297b8 / 0x2298c4 / 0x229958): a frame
// (21,18,w,355) on layer 6, tint 0x08000000, style 1, and a prompt line centred at
// (320,404) in the small font, colour 0x0A316864. The frame width is a minimum-width
// fixture value of 0 in the research, so the width is Host. host_screen: the
// screen has no phase of its own in the research (class selection), so the whole
// footer is Host there.
void add_footer(Scene &s, const std::string &prompt, bool host_screen = false) {
  Draw f = Draw{};
  f.kind = Draw::Kind::Frame;
  f.evidence = Evidence::Host;
  f.x = 21;
  f.y = 18;
  f.w = 598;
  f.h = 355;
  f.layer = 6;
  f.colour = 0x08000000;
  f.style = 1;
  f.role = "footer frame (x, y, h, layer, tint, style Executed; width Host)";
  f.site = "0x2297b8";
  s.draws.push_back(f);
  s.draws.push_back(text(Font::Small, Align::Centred, 6, 320, 404, 0x0A316864, prompt,
                         host_screen ? Evidence::Host : Evidence::Inferred, "footer prompt (text Host)",
                         host_screen ? "" : "0x229cb4"));
}

void host_text(Scene &s, int x, int y, const std::string &str, Align align = Align::Left,
               Font font = Font::Small, int layer = 7) {
  s.draws.push_back(text(font, align, layer, x, y, HOST_MUTED, str, Evidence::Host, "host text"));
}

} // namespace

Scene Flow::layout() const {
  Scene s;
  s.screen = screen_name(screen_);
  const auto list = entries();
  const int cur = cursor_;
  auto host = [&](const char *label) {
    s.host_layout = true;
    s.host_label = label;
  };
  switch (screen_) {
  case Screen::BootCard:
    host("card prompt text and card screens are not captured");
    add_message_box(s, list.empty() ? std::string() : list.front().label, Evidence::Host);
    s.unknown = {"the card screens' text, spinner and prompts (memcard_tickSpinner draws nothing in a zeroed run)"};
    break;
  case Screen::SoeLogo: {
    for (const auto &d : layout::logo_sprites())
      s.draws.push_back(d);
    s.unknown = {"sprite sizes, any fade or animation across frames, the backdrop (drawn black)"};
    break;
  }
  case Screen::IntroMovie:
    host("the film's picture is not decoded; its size, timing and fade are Unknown");
    host_text(s, 320, 200, "Intro film (porko.lmp / porko_l.vag): picture not drawn", Align::Centred);
    host_text(s, 320, 222, "A button after frame 30 ends it.", Align::Centred);
    s.unknown = {"MPEG picture size, timing, fade and soundtrack playback"};
    break;
  case Screen::Main: case Screen::GameType: case Screen::Players: case Screen::Mode:
  case Screen::Difficulty: {
    const ListSpec spec = screen_ == Screen::Main       ? layout::main_list()
                          : screen_ == Screen::GameType ? layout::game_type_list()
                          : screen_ == Screen::Players  ? layout::players_list()
                          : screen_ == Screen::Mode     ? layout::mode_list()
                                                        : layout::difficulty_list();
    s.draws.push_back(layout::title_sprite()); // states 0-4 only
    add_list(s, spec, list, cur, HOST_LIST_TEXT, Evidence::Host, "list row (font, layer Executed; colour Host)");
    bool placeholder = false;
    for (const auto &e : list)
      placeholder = placeholder || !e.refusal.empty();
    if (placeholder)
      host("rows whose text the research does not record are drawn dim with a host label");
    s.unknown = {"the backdrop (movie or frame), title size/clip/fade, row text keys, row colours and fade, disabled-row treatment"};
    if (screen_ == Screen::Difficulty)
      s.unknown.push_back("the extra line centred at (320,366) (0x1e13e0): content and colour");
    break;
  }
  case Screen::Options: {
    const bool front = in_frontend();
    if (front)
      add_list(s, layout::options_list(), list, cur, HOST_LIST_TEXT, Evidence::Host,
               "list row (font, layer Executed; colour Host)");
    else {
      // The in-game table 0x44f218 has no captured count or geometry. Show only
      // the two host controls that this flow actually implements.
      for (std::size_t i = 0; i < list.size(); ++i)
        host_text(s, 320, 220 + 28 * static_cast<int>(i),
                  (static_cast<int>(i) == cur ? "> " : "  ") + list[i].label, Align::Centred);
    }
    // The value bar is host-drawn: knobs, glyphs and the live values are Unknown.
    if (list.size() > 1 && list[1].type == 4) {
      const auto anchors = front ? layout::list_anchors(layout::options_list()) : std::vector<layout::Point>{};
      float live = 0;
      std::uint32_t bits = list[1].value;
      static_assert(sizeof(float) == sizeof(std::uint32_t), "float is 32 bits");
      std::memcpy(&live, &bits, 4);
      if (!(live >= 0.0f)) // also rejects NaN
        live = 0.0f;
      live = std::min(live, 1.0f);
      const int bx = 336, by = front ? anchors[1].y + 4 : 252, bw = 72;
      s.draws.push_back(rect(bx, by, bx + bw, by + 6, 7, 0x80303030, Evidence::Host, "host slider track"));
      s.draws.push_back(rect(bx, by, bx + static_cast<int>(live * float(bw)), by + 6, 7,
                             0x80366876, Evidence::Host, "host slider fill"));
    }
    host(front ? "value bars, knobs and glyphs of the option rows are Unknown"
               : "the pause option list (table 0x44f218) was not captured; two host controls are shown");
    s.unknown = {"what rows 0-2 control, value bars and knobs, glyphs, fade"};
    break;
  }
  case Screen::OptionsLoad:
    host("the options-load message box text and choices are not captured");
    add_message_box(s, "Options: " + (list.size() > static_cast<std::size_t>(cur) ? list[static_cast<std::size_t>(cur)].label : std::string()), Evidence::Host);
    for (std::size_t i = 0; i < list.size(); ++i)
      host_text(s, 320, 300 + 22 * static_cast<int>(i), (static_cast<int>(i) == cur ? "> " : "  ") + list[i].label, Align::Centred);
    break;
  case Screen::Credits:
    host("credits.lmp's layout is not read; the lines are Unknown");
    host_text(s, 320, 200, "Credits: the data layout is not documented. Circle returns.", Align::Centred);
    s.unknown = {"credits data layout and line spacing, imagery, the exit input and sound"};
    break;
  case Screen::LoadGame: case Screen::SaveGame: {
    host("card directory and cached headers unavailable; host file actions below the original frame");
    // research-save-screen 0a3419b, memcard_slotList 0x21d814: the wrapper
    // adjusts (61,126,519,97) to (60,122,521,105), forces style 2, and
    // submits the nine tile*.tex pieces from INTERFAC.LMP. Its settled
    // fixture tint is not a constant across fades.
    Draw slot_frame;
    slot_frame.kind = Draw::Kind::Frame;
    slot_frame.evidence = Evidence::Executed;
    slot_frame.x = 60;
    slot_frame.y = 122;
    slot_frame.w = 521;
    slot_frame.h = 105;
    slot_frame.layer = 6;
    slot_frame.colour = 0x7f616369;
    slot_frame.style = 2;
    slot_frame.role = "slot-list tile frame (settled fixture)";
    slot_frame.site = "0x21d814";
    const char *title = screen_ == Screen::LoadGame ? "Load Game" : "Save Game";
    Draw slot_title = text(Font::Small, Align::Centred, 6, 320, 110, 0x001E6464, title,
                           Evidence::Host, "slot-list title (localized text unavailable)", "0x21d6e8");
    slot_title.settled = true;
    s.draws.push_back(slot_title);
    // A caller can bind actual 16-byte directory metadata and 0x70-byte
    // headers. The round-2 fix audit passed for indexed rows, date and clock
    // formatter calls, and header-backed preview fields. Localized level and
    // preview wording remains Host, and the real directory source is absent.
    if (card_cache_supplied_) {
      s.host_label = "caller card cache; level text and preview wording unverified";
      for (const auto &row : layout::visible_card_rows(card_metadata_, card_headers_,
                                                      card_count_, card_scroll_base_,
                                                      screen_ == Screen::SaveGame)) {
        s.draws.push_back(text(Font::Small, Align::Left, 6, 77, row.y, 0x7f323c3c,
                               row.number, Evidence::Executed, "supplied card number", "0x21d99c"));
        if (row.populated) {
          s.draws.push_back(text(Font::Small, Align::Left, 6, 365, row.y, 0x7f323c3c,
                                 row.date, Evidence::Executed, "supplied card date", "0x21db24"));
          s.draws.push_back(text(Font::Small, Align::Left, 6, 487, row.y, 0x7f323c3c,
                                 row.clock, Evidence::Executed, "supplied card clock", "0x21dbe8"));
        } else if (row.new_save) {
          s.draws.push_back(text(Font::Small, Align::Left, 6, 100, row.y, HOST_MUTED,
                                 "New Save", Evidence::Host,
                                 "save-only free row (localized wording unknown)", "0x21d9f4"));
        }
      }
      if (card_preview_index_ >= 0) {
        const auto preview = layout::card_preview(card_headers_, card_count_, card_preview_index_);
        s.draws.push_back(text(Font::Small, Align::Left, 6, 220, 247, HOST_MUTED,
                               "Players", Evidence::Host, "preview label (localized wording unknown)"));
        s.draws.push_back(text(Font::Small, Align::Left, 6, 320, 247, HOST_MUTED,
                               std::to_string(preview.player_count), Evidence::Host,
                               "preview player count (header field)", "0x21e208"));
        s.draws.push_back(text(Font::Small, Align::Left, 6, 220, 264, HOST_MUTED,
                               "Difficulty", Evidence::Host, "preview label (localized wording unknown)"));
        s.draws.push_back(text(Font::Small, Align::Left, 6, 320, 264, HOST_MUTED,
                               "index " + std::to_string(preview.difficulty_index), Evidence::Host,
                               "preview difficulty index (header field)", "0x21e2c0"));
        if (card_debug_time_) {
          s.draws.push_back(text(Font::Small, Align::Left, 6, 100, 281, HOST_MUTED,
                                 "Save time (raw)", Evidence::Host,
                                 "debug preview label (localized wording unknown)"));
          s.draws.push_back(text(Font::Small, Align::Left, 6, 320, 281, HOST_MUTED,
                                 std::to_string(preview.play_seconds) + "s", Evidence::Host,
                                 "debug play seconds (minute display unverified)", "0x21e0f0"));
        }
      }
    }
    s.draws.push_back(slot_frame);
    for (std::size_t i = 0; i < list.size(); ++i) {
      host_text(s, 320, 348 + 22 * static_cast<int>(i),
                (static_cast<int>(i) == cur ? "> " : "  ") + list[i].label,
                Align::Centred);
    }
    s.unknown = {"real card directory timestamp, card capacity and scroll reachability, localized level/preview/prompt text, minute display beyond completed fixture, retail pixels and backdrop"};
    break;
  }
  case Screen::Saved:
    host("the saved confirmation box art and localized text are not established");
    host_text(s, 320, 220, "Saved", Align::Centred);
    host_text(s, 320, 348, "Cross: close", Align::Centred);
    s.unknown = {"saved confirmation box frame, localized text, fade and sound"};
    break;
  case Screen::CharClass: {
    host("carousel order Observed; class art, the 3D model pose, cursor timing and per-race animation are Unknown");
    // Class selection has no phase of its own in the research (frontend-layout.md),
    // and phase 1 is "import selection": its (41,29) wrapped call and its sounds
    // are not mapped to this screen (REFUSED). The footer and the line are Host.
    add_footer(s, "Cross: choose   Circle: back   Triangle: import", true);
    host_text(s, 41, 29, "Choose a model (13-position carousel; preview Unknown)", Align::Left, Font::Small, 6);
    for (std::size_t i = 0; i < list.size(); ++i) {
      const int x = 150, y = 79 + static_cast<int>(i) * 24;
      if (static_cast<int>(i) == cur)
        s.draws.push_back(rect(x - 125, y - 3, x + 125, y + 19, 6, 0x80366876, Evidence::Host, "host row highlight"));
      s.draws.push_back(text(Font::Small, Align::Centred, 7, x, y, HOST_LIST_TEXT, list[i].label, Evidence::Host, "host row"));
    }
    s.unknown = {"carousel input timing, class art, camera preview, cursor and highlight"};
    break;
  }
  case Screen::CharAppearance:
    host("record appearance fields are Observed; reachable menu choices, values and art are Unknown");
    add_footer(s, "Cross: continue on default route   Circle: back");
    // Phase 2 (ref_frontend_screens PLAYERSEL_PHASES): batch 0x0A316864 precedes the
    // wide (41,38) call; the colour 0x08585E5D is set after it.
    s.draws.push_back(text(Font::Small, Align::Left, 6, 41, 38, 0x0A316864, "Appearance (selected values Unknown)", Evidence::Inferred,
                           "phase-2 wide text (text Host)", "0x227018"));
    for (std::size_t i = 0; i < list.size(); ++i) {
      const int y = 105 + static_cast<int>(i) * 32;
      if (static_cast<int>(i) == cur)
        s.draws.push_back(rect(60, y - 4, 365, y + 20, 6, 0x80366876, Evidence::Host, "host row highlight"));
      host_text(s, 70, y, list[i].label, Align::Left, Font::Small, 7);
    }
    s.unknown = {"look and skin art, option bounds and their selected values, menu-to-record handoff"};
    break;
  case Screen::CharAbility: {
    add_footer(s, "Right: add a point   Cross: continue   Circle: back");
    // Phase 3 colours follow the call order of PLAYERSEL_PHASES: batch 0x0A316864 ->
    // header (41,38); colour 0x08325D18 -> points row and value (72); 0x08585E5D ->
    // the unselected stat rows, 0x0068C7CC the selected one; 0x0A316864 again ->
    // the description (41,208).
    s.draws.push_back(text(Font::Small, Align::Wrapped, 6, 41, 38, 0x0A316864, "Spend your ability points", Evidence::Inferred,
                           "phase-3 header (text Host)", "0x227fb0"));
    s.draws.push_back(text(Font::Small, Align::Left, 6, 41, 72, 0x08325D18, "Points left", Evidence::Inferred,
                           "phase-3 points row (text Host)", "0x228038"));
    s.draws.push_back(text(Font::Small, Align::Centred, 6, 200, 72, 0x08325D18, std::to_string(ability_remaining()), Evidence::Host,
                           "points value (x Host: the centred x is not recorded)"));
    const auto stats = character_.stats();
    for (int i = 0; i < 4; ++i) {
      const int y = 106 + 17 * i; // four rows from y=106, 17 apart (Executed)
      const std::uint32_t colour = (i == cur ? 0x0068C7CC : 0x08585E5D); // selected row: Inferred
      s.draws.push_back(text(Font::Small, Align::Left, 6, 41, y, colour, STAT_ROWS[i], Evidence::Inferred,
                             "stat label (text Host)", "0x22812c"));
      s.draws.push_back(text(Font::Small, Align::Centred, 6, 200, y, colour,
                             character_.base_known ? std::to_string(stats[static_cast<std::size_t>(i)])
                                                   : "+" + std::to_string(character_.spent[static_cast<std::size_t>(i)]),
                             Evidence::Host, "stat value (x Host: the centred x is not recorded)"));
    }
    s.draws.push_back(text(Font::Small, Align::Wrapped, 6, 41, 208, 0x0A316864,
                           error_.empty() ? "Right adds a point to the selected row." : error_, Evidence::Inferred,
                           "stat description (text Host)", "0x228430"));
    host("the stat value column, the pad-button glyph and the description text are Unknown");
    s.unknown = {"the value column's x, the pad glyph, point removal, the stat descriptions"};
    break;
  }
  case Screen::CharName: {
    add_footer(s, "Cross: done   Circle: back   Type the name");
    // nameEntry_draw 0x22c920: batches medium 0x585e5d (0x22c9c0), small 0x316864
    // (0x22d0e4), medium 0x585e5d (0x22d198); which call belongs to which batch
    // follows the call-site order (Inferred). The grid glyphs, the cursor
    // rectangle and the accepted-name colour are Unknown, so they are not drawn.
    s.draws.push_back(text(Font::Small, Align::Wrapped, 6, 41, 38, 0x316864, "Name your character", Evidence::Inferred,
                           "name wrapped text (text Host)", "0x22d12c"));
    s.draws.push_back(text(Font::Medium, Align::Left, 6, 41, 72, 0x585e5d, character_.name.empty() ? "_" : character_.name,
                           Evidence::Inferred, "name ASCII line", "0x22d26c"));
    const int done_y = 300;
    for (std::size_t i = 1; i < list.size(); ++i) {
      if (static_cast<int>(i) == cur)
        s.draws.push_back(rect(220, done_y - 3, 420, done_y + 19, 6, 0x80366876, Evidence::Host, "host row highlight"));
      s.draws.push_back(text(Font::Medium, Align::Centred, 7, 320, done_y, HOST_LIST_TEXT, list[i].label, Evidence::Host, "host row"));
    }
    host("the name grid's glyphs and cursor are Unknown; the host keyboard types");
    s.unknown = {"grid glyphs, the cursor rectangle (zero width in the fixture), accepted-name colours, executed sounds"};
    break;
  }
  case Screen::Playing:
    break;
  case Screen::Pause:
    // The pause list's own font word is zero in the fixture (Unknown); the medium
    // font and 0x80464646 come from the later text_begin 0x18d5e8 (Observed).
    add_list(s, layout::pause_list(), list, cur, layout::PAUSE_TEXT, Evidence::Observed,
             "list row (geometry Executed; font and colour Observed)");
    s.unknown = {"glyph placement inside the frame, conditional net/debug rows, fade and prompts; the game scene behind it is the host's"};
    break;
  case Screen::ExitConfirm:
    host("the Exit Game? prompt was not captured");
    add_message_box(s, "Exit Game?", Evidence::Host);
    for (std::size_t i = 0; i < list.size(); ++i) {
      const int x = 250 + 140 * static_cast<int>(i);
      if (static_cast<int>(i) == cur)
        s.draws.push_back(rect(x - 50, 233, x + 50, 255, 6, 0x80366876, Evidence::Host, "host row highlight"));
      s.draws.push_back(text(Font::Medium, Align::Centred, 7, x, 236, HOST_LIST_TEXT, list[i].label, Evidence::Host, "host row"));
    }
    break;
  }
  return s;
}

} // namespace champions::frontend
