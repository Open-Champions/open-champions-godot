#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Draw calls of the Return to Arms front-end screens, authored from
// t3/research-menus-layout dc458d5 docs/frontend-layout.md and the executed
// tables of tools/ref_frontend_layout.py and tools/ref_frontend_screens.py
// (SLUS-20973 v2.00, SHA-256 292f6f48...d7). Coordinates are the 640x448 UI draw
// API space. Every Draw carries the research's evidence label. Nothing that the
// research lists as Unknown is placed as if it were known: those elements are
// marked Evidence::Host, and the host labels them on screen.
//
// Texture, font and sample bytes are not held here. A Draw names the lump and
// entry (sprites) or the font word (text); the Godot host loads them from the
// user's disc at run time.
namespace champions::frontend {

// The two UI font words (research-ui docs/formats/ui-fonts.md): 0x4f7100 is the
// medium font (gamefontmed), 0x4f7104 the small one (gamefontsmall).
enum class Font { Medium, Small };
enum class Align { Left, Centred, Wrapped, WrappedCentred };
// Executed and Inferred are the research's labels. Host is anything the research
// lists as Unknown or leaves out.
enum class Evidence { Executed, Observed, Inferred, Host };
const char *evidence_name(Evidence);

struct Draw {
  enum class Kind { Sprite, Frame, Rect, Text };
  Kind kind = Kind::Text;
  Evidence evidence = Evidence::Executed;
  int layer = 0;
  int x = 0, y = 0, w = 0, h = 0; // Rect uses x,y,w,h = left,top,width,height
  std::uint32_t colour = 0;       // sprite or frame tint, rect colour, text colour (the research word)
  bool settled = false;           // documented fade batch; the host may paint it opaque
  int style = 0;                  // Frame: ctile 0, border style 1, tile 2
  int min_w = 0;                  // Frame: the documented minimum-width call
  std::string lump, resource;     // Sprite
  Font font = Font::Medium;       // Text
  Align align = Align::Left;
  int wrap_width = 0, line = 0;   // Wrapped
  std::string text;               // host-authored label (the original keys are not documented)
  std::string role;               // what the research or host calls this element
  const char *site = "";          // the research's call site, if it names one
};

struct Scene {
  std::string screen;
  std::vector<Draw> draws; // submission order; the host paints layers 0..7, reverse order inside a layer
  bool host_layout = false;
  std::string host_label;  // what the research leaves Unknown on this screen
  std::vector<std::string> unknown;
};

// The three menu sounds of FX.LMP (docs/frontend-layout.md, "Menu sound").
enum class menu_sound { move, select, back };
const char *menu_sound_resource(menu_sound);

namespace layout {
// Card cache supplied by a caller, never synthesized by the front end. These
// offsets and formatter submissions are the bounded Observed/Executed subset
// of research-save-screen 9c2b941 (r2 fix audit PASS).
struct CardRow {
  int index = 0, y = 0;
  std::size_t metadata_offset = 0, header_offset = 0;
  bool populated = false;
  bool new_save = false;
  std::string number, date, clock;
};
struct CardPreview {
  int header_index = 0;
  std::size_t header_offset = 0;
  std::uint32_t play_seconds = 0;
  std::uint16_t player_count = 0;
  std::uint8_t difficulty_index = 0;
};
std::vector<CardRow> visible_card_rows(const std::vector<std::uint8_t> &metadata,
                                       const std::vector<std::uint8_t> &headers,
                                       int count, int scroll_base, bool saving);
CardPreview card_preview(const std::vector<std::uint8_t> &headers, int count, int header_index);
// Colours the research documents (tint words of the executed calls).
inline constexpr std::uint32_t LIST_BACKING_TINT = 0x80808080; // style-2 backing frame
inline constexpr std::uint32_t LIST_CURSOR_TINT = 0x300000ff;  // style-0 selected-row frame
inline constexpr std::uint32_t LIST_OVERLAY_TINT = 0x3f000000; // style-0 front overlay
inline constexpr std::uint32_t PAUSE_TEXT = 0x80464646;        // pause batch 0x18d5e8 only (Observed)
inline constexpr std::uint32_t SPRITE_TINT = 0x80808080;       // title and logo sprites

struct Point {
  int x = 0, y = 0;
};
// One centred list drawn by menuList_draw 0x1a07c4.
struct ListSpec {
  int rows = 0;
  std::uint32_t value_rows = 0; // bit i set: row i is a value row (text shifted 23 left)
  int origin_x = 320, origin_y = 309;
};
std::vector<Point> list_anchors(const ListSpec &);
// The three ui_drawFrameImpl calls for a selected row: the style-0 cursor frame,
// the style-2 backing frame and the style-0 overlay: the documented minimum-width
// calls only (the research gives no text-width rule).
std::array<Draw, 3> list_frames(const ListSpec &, int selected);
// Row counts of the tables the offline path reaches (ref_frontend_layout.TABLES).
ListSpec main_list();       // 0x4f6b88, state 0
ListSpec game_type_list();  // 0x4f6bf8, state 1
ListSpec players_list();    // 0x4f6de8, state 2
ListSpec difficulty_list(); // 0x4f6e38, state 3
ListSpec mode_list();       // 0x4f6c68, state 4
ListSpec options_list();    // 0x4f6e88, state 11, origin y 238
ListSpec pause_list();      // 0x44f3b8 normal offline table, five enabled rows, origin y 240

std::array<Draw, 2> logo_sprites();    // splash_soeLogo 0x321640
Draw title_sprite();                   // frontend_run 0x1e0ac8, states 0-4
} // namespace layout

} // namespace champions::frontend
