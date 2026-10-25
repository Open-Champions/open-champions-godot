#pragma once
// UI text gaps for Return to Arms (SLUS-20973 v2.00). Authored port of
// tools/ref_ui_text_gaps.py on research t3/research-ui-text-gaps (523b3db); the
// contract is that branch's docs/ui-text-gaps.md and docs/spec/ui-text-gaps.md.
// Hashes of the pinned files are in native/tests/ui_text_gaps_contract.json and
// docs/UI_TEXT_GAPS.md says what is ported, wired and still refused.
//
// Floats are raw single-precision patterns (champions::simulation::rta::Raw).
// Nothing here loads art or fonts: the functions return the calls the original
// makes, in order, and the caller draws them.
#include "champions/core_arithmetic.hpp"
#include "champions/core_math.hpp"
#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace champions::ui_text_gaps {
using Raw = simulation::rta::Raw;

inline constexpr const char *POLICY = "rta-ui-text-gaps-v1";

// One outward call. Arguments are 32-bit words (a signed value is its two's
// complement) or a name for something the caller owns (a label, a resource).
using Arg = std::variant<std::uint32_t, std::string>;
struct Call {
  std::string name;
  std::vector<Arg> args;
};
// "name<TAB>arg<TAB>arg": words as 8 hex digits, names as s:<text>. The same
// form is written by tools/generate_ui_text_gaps_vectors.py.
std::string canonical(const Call &);

// 1. The UI ramps (session step 13, 0x323448-0x3234d0; docs 1) ----------------
inline constexpr Raw PULSE_DECAY = 0x3f4ccccc;  // 0.8
inline constexpr Raw FADE_STEP = 0x3df5c28f;    // 0.12
inline constexpr Raw ONE = 0x3f800000, TWO = 0x40000000, ZERO = 0;
struct Ramps {
  std::array<Raw, 4> pulse{ONE, ONE, ONE, ONE};
  std::array<Raw, 4> fade{ONE, ONE, ONE, ONE};
};
// Once per displayed frame, for each of the four slots.
Ramps rampsStep(const Ramps &);
inline Raw uiStartPulse() { return TWO; }   // ui_startPulse 0x320ea4
inline Raw uiRestartFadeIn() { return ZERO; } // ui_restartFadeIn 0x320ec4
std::vector<Raw> fadeRamp(Raw start = ZERO, int limit = 64);
std::vector<Raw> pulseRamp(Raw start = TWO, int frames = 32);

// 2. Button layout words and the glyph byte 0x41b978 (docs 7) ----------------
// The layout byte 0x41b69c holds 1 in the executable's data (O). Layout 0 is
// inferred from the code only (nothing observed stores it).
struct LayoutWord {
  std::uint32_t address = 0, value = 0;
};
inline constexpr std::uint32_t G_GLYPH_CONFIRM = 0x41b978, G_GLYPH_OTHER = 0x41b97c,
                               G_CONFIRM_MASK = 0x41bfbc, G_CANCEL_MASK = 0x41bfb8;
inline constexpr std::int8_t LAYOUT_OBSERVED = 1;
// The words game_initCamera stores for a layout byte, sorted by address. Empty
// for any value other than 0 and 1 (the words keep their data values).
std::vector<LayoutWord> buttonLayout(std::int8_t layout_byte);
// 0x7e (cross) for the observed layout, from the word table. Refuses other
// layouts: the word keeps its data value, which is not documented.
std::optional<std::uint32_t> confirmGlyph(std::int8_t layout_byte);
// pad_buttonMaskToGlyph 0x1eacb8: the glyph code, or 0xffffffff.
std::uint32_t padMaskToGlyph(std::uint32_t mask);
// sprintf("%c", word): the low byte, or nothing for 0.
std::string glyphText(std::uint32_t word);
// Glyph codes 0x7b-0x88 are the font's button glyphs (tint 0x606060).
bool isButtonGlyph(std::uint8_t code);

// 3. help_task 0x1dcc78 (docs 2) ----------------------------------------------
inline constexpr std::uint32_t HELP_TITLE_COLOUR = 0x8028444e, HELP_BODY_COLOUR = 0x80464646,
                               HELP_BUTTON_RGB = 0x464646, HELP_FRAME_COLOUR = 0x80808080,
                               HELP_INNER_TINT = 0x3f000000;
inline constexpr std::int32_t HELP_CLOSE_DELAY = 60, DL_MIN_FREE = 3000;
inline constexpr const char *HELP_LABEL_KEY = "Affirmation"; // 0x53c770
int helpButtonAlpha(std::int32_t delay);
struct HelpState {
  std::int32_t delay = 0;
  std::int8_t open = 1, net = 0, paused = 0, pause_byte = 0, local_menu = 0;
  std::int32_t lockout = 0, dl_free = 5000;
  Raw pulse = ONE;
  std::uint32_t title = 0, body = 0, font_med = 0, glyph_word = 0x7e;
  std::int32_t num_players = 1;
  std::array<std::uint32_t, 4> pads{};
  std::uint32_t confirm_mask = 0x40;
  std::vector<int> alive{0};
  std::set<int> local{0};
  std::int32_t wrap_height = 0, label_width = 0;
};
struct HelpResult {
  std::vector<Call> calls;
  HelpState state;
};
// One call with the notice boxes closed. The caller supplies what the text
// calls return (wrap height, label width).
HelpResult helpTask(const HelpState &);
struct Frame {
  int x = 0, y = 0, width = 0, height = 0;
  std::uint32_t tint = 0;
  int style = 0;
};
// The inner ctile frame (style 0) of the help box: (124, 104, 416, h + 76),
// tint 0x3f000000. Its height is the outer frame's argument, not h.
Frame helpInnerFrame(int wrap_height);

// 4. hud_drawBust 0x1dfb6c (docs 3) -------------------------------------------
inline constexpr Raw BUST_FADE_STEP = 0x3dacf3df, BUST_TEXT_ALPHA = 0x42dc0000,
                     BUST_SPRITE_ALPHA = 0x43000000;
inline constexpr std::uint32_t BUST_TEXT_RGB = 0x395f6b, BUST_SPRITE_RGB = 0x808080;
std::uint32_t bustAlpha(Raw fade, Raw factor);
struct TexSize {
  std::int32_t w = 0, h = 0; // the texture's s16 header words
};
struct BustState {
  std::int8_t shown = 1, latch = 0;
  Raw fade = ZERO;
  std::int32_t value = 0, maximum = -1, sim_frame = 0;
  // Host refusal: omit the animated bar if g_simFrame is unavailable.
  // Exact research/vector calls use the default and always draw it.
  bool bar_frame_available = true;
  std::uint32_t font_small = 0;
  // Sizes of every res_find result the draw reads, by resource name:
  // "frame.tex", "<portrait>", "fieldnumber.tex", the bars.
  std::function<TexSize(const std::string &)> tex;
};
struct BustResult {
  std::vector<Call> calls;
  Raw fade = ZERO;
  std::int8_t latch = 0;
  std::int32_t value = 0;
};
BustResult hudDrawBust(const BustState &);

// 5. ui_emitSpriteQuad 0x17ab50 (docs 4) --------------------------------------
struct Qword {
  std::uint64_t value = 0;
  std::uint32_t reg = 0;
};
// fields are bit 0 of the gfx_displayField results, in call order.
std::vector<Qword> spriteQuad(bool frame_mode, std::int8_t field_mode, std::int32_t tex_w,
                              std::int32_t tex_h, std::int32_t x, std::int32_t y,
                              std::int32_t u0, std::int32_t v0, std::int32_t u1,
                              std::int32_t v1, std::array<int, 2> fields = {0, 0});
int fieldCalls(bool frame_mode, std::int8_t field_mode, std::int32_t y);
// INFERRED progressive equivalent (docs 4, "Rule for a progressive client").
struct WovenLine {
  std::int32_t stored_row = 0, picture_line = 0;
};
std::optional<WovenLine> wovenLine(std::int32_t screen_line, std::int32_t y, std::int32_t h);

// 6. hud_drawCompass 0x1df720 (docs 5) ----------------------------------------
inline constexpr std::array<Raw, 3> COMPASS_OFFSET{0xbde977c8, 0x3e20c38f, 0xc0b8273d};
inline constexpr std::uint32_t COMPASS_LABEL_COLOUR = 0x632f6159;
struct CompassWorld {
  simulation::rta::Mat34 world;
  std::int32_t angle = 0;
};
CompassWorld compassWorld(const simulation::rta::Mat4 &view, const std::array<Raw, 3> &camera,
                          const std::array<Raw, 2> &player_xy, const std::array<Raw, 2> &target_xy);
struct CompassState {
  std::int32_t gate = 1, player = 0;
  std::uint32_t object = 1;
  std::int32_t dialog_open = 0;
  std::int8_t shown = 1;
  simulation::rta::Mat4 view{};
  std::array<Raw, 3> camera{};
  std::array<Raw, 2> player_xy{}, target_xy{};
  std::uint32_t flags = 0;
  std::uint32_t light_flag = 0, light_depth = 0, font_small = 0;
};
struct CompassResult {
  std::vector<Call> calls;
  CompassState state;
  std::int32_t angle = 0;
};
// The two model_draw calls carry the world matrix; the mesh itself is the
// caller's (compass.vif / compass.tex of the player's disc). What the second
// pass looks like on screen is Unknown (docs 13) and is not modelled.
CompassResult hudDrawCompass(const CompassState &);

// 7. soundSeq_updateAll 0x2147c0 (docs 6) -------------------------------------
inline constexpr std::int32_t EVENT_END = 0x7fffffff;
inline constexpr std::uint16_t STATE_ENDED = 0x80;
struct SeqSlotView {
  std::int8_t tracks = 0;       // s8 +0x04
  bool released = false;        // +0x3c
  bool has_event_table = true;  // clip +0x38 != 0
  std::int32_t sound_idx = 0;   // track cursor, s16
  std::vector<std::int32_t> events; // the table up to the terminator (excluded)
  std::uint16_t flags = 0;      // state flags +0x06
  std::int32_t duration = 0;    // clip +0x40
  Raw time = ZERO;              // spec: track +0x18; research prose says +0x20 (unresolved)
  bool while_frozen = false;    // +0x3b
  std::optional<std::array<Raw, 3>> linked; // the object +0x28 links
  bool no_pos = false;          // +0x3a
  std::array<Raw, 3> pos{};
  std::uint16_t serial = 0;
  Raw volume = ONE;
};
struct VoiceView {
  std::uint16_t flags = 0;
  std::int32_t owner = 0, generation = 0;
};
struct SeqEvent {
  std::string kind; // "stop" (voice) or "advance" (slot, volume)
  std::int32_t a = 0;
  Raw b = 0;
};
// One call. advance(slot_index, view) runs the original's anim_advance
// (anim_advanceUnitSpeed) and is the caller's; Native Runtime refuses sound
// execution, so the loop and its freeing rules are what is ported here.
std::vector<SeqEvent> soundSeqUpdateAll(
    std::vector<SeqSlotView> &slots, const std::vector<VoiceView> &voices, bool frozen,
    const std::array<Raw, 3> &camera,
    const std::function<void(std::size_t, SeqSlotView &)> &advance = {});

// 8. The dialog's showName text (docs 8) is champions::strings::TextTable::lookup
// on the raw value after "=", quotes included: it drops every quote from the
// request, and a miss shows "NF:" and the first 16 bytes of the bare key.
} // namespace champions::ui_text_gaps
