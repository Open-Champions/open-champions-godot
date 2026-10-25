#pragma once
#include "champions/archive.hpp"
#include "champions/ui_font_page.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace champions::ui_layout {
struct Rect { int x{}, y{}, width{}, height{}; };
struct Clip { int left{}, top{}, right{}, bottom{}; };
struct Draw { std::string resource; int x{}, y{}; bool clipped{}; Clip clip{}; };
struct HudLayer { Draw draw; std::uint32_t tint{}; int layer{6}; };

// Return to Arms US v2.00 logical 640x448 coordinates. Draws are in original
// submission order; a painter renderer reverses them within the same layer.
std::vector<Draw> menuFrame(int x, int y, int width, int height, int style = 2,
                            bool no_center = false, bool no_border = false);
Rect frameSourceRect(const std::string &resource);
Rect hudSourceRect(const std::string &resource);
int frameBorder(int style);
std::array<int, 4> hudOrigins(bool network = false,
                              const std::array<bool, 4> &local_slots = {false, false, false, false},
                              int player_count = 4);
int barClipRight(int origin, float current, float maximum);
std::vector<HudLayer> hudBaseLayers(int x, std::uint32_t tick, float health, float health_max,
                                    float mana, float mana_max, int experience = 0,
                                    int experience_base = 0, int experience_next = 1,
                                    int acknowledged_level = 1, bool local = true,
                                    int cooldown = 0, bool health_flash = false);

// Pending potion overlays (research-ui 3e39392 docs/hud-layout.md, "Known
// partial contracts", O/high, 0x1d9fb8-0x1da1d8): untextured rectangles that
// 0x1d9cd4 submits right after the health bar, health first, then mana, at
// layer 6. Observed, not function-executed. Each needs a local slot, the dead
// bit 0x8 clear and a positive pending value (+0xb4 health, +0xc8 mana); the
// caller decides those gates. x is the promoted origin X; slot picks the cap.
struct HudRect { int left{}, top{}, right{}, bottom{}; std::uint32_t colour{}; int layer{6}; };
int pendingRight(int origin, int cap, float current, float pending, float maximum);
std::vector<HudRect> hudPendingRects(int x, int slot, float health, float health_max,
                                     float pending_health, float mana, float mana_max,
                                     float pending_mana);

struct Glyph { std::uint16_t code{}; int u0{}, u1{}, v0{}, v1{}, y_offset{}, advance{}, kern_start{}; };
struct Kern { std::uint16_t previous{}, current{}; int adjustment{}; };
struct FontMetrics {
    std::array<int, 256> charmap{};
    std::vector<Glyph> glyphs;
    std::vector<Kern> kerns;
    std::pair<Rect, Rect> glyphRects(std::size_t index, int pen_x = 0, int text_y = 0) const;
    int widthAscii(const std::vector<std::uint8_t> &bytes, bool interlaced = true,
                   std::size_t count = static_cast<std::size_t>(-1)) const;
};
// Adds the documented layout fields to the existing ui-font-data reader.
FontMetrics fontMetrics(Bytes bytes);
// Validates the FNT tables, then decodes the embedded page. weave is the
// field interleave used by glyph rectangles. Header +2 is not read.
DecodedFontPage decodeFontFile(Bytes bytes, bool weave = false);

// The 34 original-art HUD identities. used is the TEX header and the source
// rectangle at (0, 0). upload is the GIF image rectangle. rows is one field.
// Bar entry pointers refer to stable storage owned by this function.
struct HudArt {
    const char *lump;
    const char *entry;
    int used_width, used_height, upload_width, upload_height, rows;
};
std::array<HudArt, 34> hudArtRound4();
Rect hudArtSource(const HudArt &art);
}
