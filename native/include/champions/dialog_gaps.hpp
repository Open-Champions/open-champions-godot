#pragma once
// The parts of the NPC dialog that research docs/dialog.md left open: the
// conversation camera, the 24 further dialog_isOpen sites, the box without
// box.tex, the cursor glyph and the property fallback of the object factory
// (the TestLocation records). Authored port of tools/ref_dialog_gaps.py on
// research t3/research-dialog-gaps 79a2e4a; the contract is that branch's
// docs/spec/dialog-gaps.md. Hashes of the files are in
// native/tests/dialog_gaps_contract.json; docs/DIALOG.md describes the port.
//
// Floats are raw R5900 patterns (core_arithmetic.hpp). Where the original
// reads memory nobody initialised (a stack slot, address 0), the port takes
// that value as an explicit argument or throws dialog::Refused; it never
// makes one up.
#include "champions/core_math.hpp"
#include "champions/dialog.hpp"
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace champions::simulation::rta::dialog_gaps {
using dialog::Refused;
inline constexpr const char *POLICY = "rta-dialog-gaps-v1";

// ---- constants (docs/spec/dialog-gaps.md 2) -----------------------------------------
inline constexpr Raw NEAR = 0x42A00000;          // 80.0, players nearer choose the side
inline constexpr Raw SIDE_DOT = 0x3E4CCCCC;      // 0.2
inline constexpr Raw SIDE_DOT_NEG = 0xBE4CCCCC;  // -0.2
inline constexpr std::int32_t SIDE_ANGLE = 6000;
inline constexpr std::uint32_t FACING_X_ADD = 0x4000, FACING_Y_ADD = 0x41C2;
inline constexpr std::int32_t DIST_ADD = 0x50, HEIGHT_ADD = 0x46, PITCH_ADD = 0xFA0;
inline constexpr Raw FAR_START = 0x43B10000;     // 354.0
inline constexpr Raw FAR_K = 0x3F599999;         // 0.85
inline constexpr Raw LOOK_K_SCALE = 0x3F400000;  // 0.75
inline constexpr std::uint32_t APPLY_PITCH_ADD = 0x3FD3;
inline constexpr Raw SHAKE_SCALE = 0x37800000;   // 2^-16
inline constexpr std::uint32_t LCG_MUL = 0x19660D, LCG_ADD = 0x3C6EF35F;
inline constexpr int SWOOP_STEPS = 45;
inline constexpr Raw FAR_CLIP_OPEN = 0x44340000, FAR_CLIP_CLOSE = 0x44E10000;
inline constexpr Raw FWD_Z_OPEN = 0xBEFB645A, FWD_Z_CLOSE = 0xBF251E85;

// ---- Camera_setupDialogView 0x203bb8 ------------------------------------------------
// The four stack slots sscanf may leave untouched; nullopt = the original reads
// an uninitialised word there (section 10), which the port refuses to invent.
struct Stale {
  std::optional<std::int32_t> dist, height, lateral, pitch;
};
struct Offset {
  std::optional<std::int32_t> dist, height, lateral, pitch;
  int converted = 0;
};
// libc_sscanf(text, "%d,%d,%d,%d") as 0x203c34 calls it. text empty (no
// dialogOffset property): dist, height and pitch are 0 and lateral keeps the
// stale slot. Values outside 32 bits are refused.
Offset parse_dialog_offset(const std::optional<std::string> &text, const Stale &stale = {});
// Index 0, 1 or 2 (angle +6000, 0, -6000); behind() is asked only for A B C = 0 1 0.
int side_index(bool a, bool b, bool c, const std::function<bool()> &behind);

// The camera words (offsets in section 2.1).
struct View {
  Vec3 start{}, target{};                          // +0x00 / +0x0c
  std::int32_t yaw = 0, pitch = 0;                 // +0x68 / +0x6a, s16
  std::int32_t start_yaw = 0, target_yaw = 0;      // +0x76 / +0x78
  std::int32_t target_pitch = 0, start_pitch = 0;  // +0x7a / +0x7c
  Raw progress = ZERO, k = ZERO;                   // +0xa8 / +0xac
  View() = default;
  View(std::int32_t yaw_, std::int32_t pitch_) : yaw(s16(std::uint32_t(yaw_))), pitch(s16(std::uint32_t(pitch_))) {}
};
struct ViewInfo {
  Offset fields;
  std::array<int, 3> flags{};
  std::int32_t angle = 0;
};
// side is message 0's byte b; players are the positions of g_alivePlayers in order.
ViewInfo setup_dialog_view(View &view, const Vec3 &npc_pos, std::int32_t npc_facing, int side,
                           const std::vector<Vec3> &players, const Vec3 &camera_pos,
                           const std::optional<std::string> &offset_text, const Stale &stale = {});

// ---- Camera_lookAt 0x204250 ---------------------------------------------------------
struct Look {
  int result = 0; // 1 = arrived, nothing written
  Raw h = ZERO;
  std::optional<Vec3> apply; // the position passed to Camera_apply
};
Look look_at(View &view, int steps);

// dialog_runTask's camera step 0x22ff74-0x2300bc on top of look_at.
struct CameraAction {
  std::string kind; // "far_clip", "apply", "arrived", "send"
  Raw value = ZERO;
  Vec3 pos{};
};
struct RunCamera {
  std::vector<CameraAction> actions;
  std::optional<Raw> h;
};
RunCamera run_task_camera(View &view, int direction, bool swoop_done, bool shop_open, Raw forward_z);

// ---- Camera_apply 0x2035c8 with no forward vector -----------------------------------
std::uint32_t lcg(std::uint32_t seed);
struct Applied {
  Vec3 pos{};
  Mat34 matrix{};
  Raw shake = ZERO;
  std::uint32_t seed = 0;
  int draws = 0;
};
Applied camera_apply(const Vec3 &pos, std::int32_t yaw, std::int32_t pitch, Raw shake, std::uint32_t seed,
                     bool frozen, bool dialog_open);

// ---- the gates ----------------------------------------------------------------------
struct Frozen {
  std::int64_t network = 0, paused = 0, cutscene = 0, warmup = 0, player_select = 0, sim_frame = 0,
               controller_out = 0, dialog = 0;
};
// game_isFrozen 0x1ebdf8; log collects "task" and "dialog" for the calls it makes.
std::int64_t game_is_frozen(const Frozen &, std::vector<std::string> *log = nullptr);

struct RecallPlayer {
  int slot = 0;
  bool lockout = false, frozen = false, dialog = false, obj4b4 = false, recall = false, b4f66c8 = false,
       camflag = false, jumpgate = false, pausemenu = false, obj540 = false, flag400000 = false, use = false,
       b100 = false;
};
// player_pollRecallButton 0x1bce88: the slots sent to hudmsg_sendRecall.
std::vector<int> recall_poll(const std::vector<RecallPlayer> &);

// Camera_update 0x203400: the user-control word +0x60.
int camera_update_control(bool frozen, bool world, bool menu_open, bool warmup, bool b4f66c8, bool dialog,
                          bool lockout, bool flag400000);

enum class SiteKind { Sim, Input, Draw, Seed };
struct Site {
  const char *name;
  std::uint32_t address, function;
  SiteKind kind;
  const char *effect;
};
// The 24 sites, in the research table's order.
const std::vector<Site> &sites();
using Conditions = std::map<std::string, std::int64_t>;
// The label each stretch ends on, from the conditions at the site (missing keys 0).
std::string site_outcome(const std::string &name, const Conditions &);

struct IdleVoice {
  std::int64_t countdown = 0;
  std::uint32_t seed = 0;
  std::optional<std::uint32_t> voice; // player_voice slot
};
// mainloop_runSession's idle voice 0x322b50-0x322bfc.
IdleVoice idle_voice(std::int64_t countdown, bool cutscene, bool paused, bool local_menu, bool dialog,
                     std::int64_t players, std::uint32_t seed);

// ---- dialog_loadBox 0x22f910 ----------------------------------------------------------
// The new word 0x4fe538: in "intro" (case-sensitive) or with g_pvpModeB nothing is
// written. res_find_result 0 = no lump or no entry.
std::uint32_t load_box(const std::string &level, bool pvp_b, std::uint32_t old, std::uint32_t res_find_result);
inline constexpr std::uint32_t BOX_WORD_INITIAL = 0;
// ui_drawSpriteImpl reads the texture's s16 width and height first: with word 0
// those are addresses 0 and 2, which are not modelled, so the port refuses.
std::pair<std::uint32_t, std::uint32_t> box_sprite_reads(std::uint32_t word);

// ---- game_initCamera 0x1e6b68 -----------------------------------------------------------
// The words stored by the s8 at 0x41b69c; 0x41b978 is the cursor / "more" glyph byte,
// 0x41bfbc the confirm mask, 0x41bfb8 the cancel mask. Layouts other than 0 and 1 store
// none of the button words (only 0x4f85b4 and 0x4f7134, both 0).
std::map<std::uint32_t, std::uint32_t> init_camera_buttons(std::int32_t layout);
inline constexpr std::uint32_t G_GLYPH = 0x41B978, G_CONFIRM_MASK = 0x41BFBC, G_CANCEL_MASK = 0x41BFB8;
// The executable's initial layout byte is 1 (section 5): cursor character 0x7e.
inline constexpr std::int32_t LAYOUT_INITIAL = 1;
// The cursor glyph for a layout, nullopt for a layout that writes none.
std::optional<std::uint32_t> cursor_glyph(std::int32_t layout);

// ---- world_objectFactory 0x159c94: the property fallback --------------------------------
using Entries = std::vector<std::string>; // "key=value" entries of the record
std::optional<std::string> props_get_str(const Entries &, const std::string &key, int nth = 0);
std::int32_t props_get_int(const Entries &, const std::string &key, int nth = 0);
bool has_any_light(const Entries &);
struct Ctor {
  const char *key;
  std::uint32_t address, pool;
};
const std::vector<Ctor> &fallback_ctors();
// The key of the constructor the fallback picks, or nullopt: the factory
// returns 0 and the record makes no object (TestLocation).
std::optional<std::string> factory_fallback(const Entries &);
} // namespace champions::simulation::rta::dialog_gaps
