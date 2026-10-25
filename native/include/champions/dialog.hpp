#pragma once
// NPC dialog machine for Return to Arms (SLUS-20973 v2.00). Authored port of
// tools/ref_dialog.py on t3/research-dialog 334578d; the contract is that
// branch's docs/dialog.md. Hashes of the files are in
// native/tests/dialog_contract.json; docs/DIALOG.md describes this port.
//
// The machine owns only the dialog's own words. Everything it reads from other
// subsystems comes from a Host the caller fills in, and everything it does
// outside its own state is appended to `log` as an Event, in the original's
// call order. Where the original would read through a null pointer or run off
// a buffer, the port throws Refused instead of guessing.
#include "champions/core_arithmetic.hpp"
#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace champions::simulation::rta::dialog {
inline constexpr const char *POLICY = "rta-dialog-v1";

using Bytes = std::vector<std::uint8_t>;
using Wide = std::vector<std::uint16_t>;

// .dbin layout (docs/dialog.md section 3).
inline constexpr std::size_t NODE_SIZE = 0x448, CHOICE_SIZE = 0x54;
inline constexpr int MAX_CHOICES = 12;
inline constexpr std::size_t NODE_COND = 0x01, NODE_COND_SIZE = 0x20;
inline constexpr std::size_t NODE_TEXT = 0x21, NODE_TEXT_SIZE = 0x10;
inline constexpr std::size_t NODE_VOICE = 0x31, NODE_VOICE_SIZE = 0x10;
inline constexpr std::size_t NODE_SPEAKER = 0x41, NODE_SPEAKER_SIZE = 0x13;
inline constexpr std::size_t NODE_NCHOICES = 0x54, NODE_CHOICES = 0x58;
inline constexpr std::size_t CH_TEXT = 0x00, CH_TEXT_SIZE = 0x10;
inline constexpr std::size_t CH_COND = 0x10, CH_COND_SIZE = 0x20;
inline constexpr std::size_t CH_ACTION = 0x30, CH_ACTION_SIZE = 0x20;
inline constexpr std::size_t CH_NEXT = 0x50;

inline constexpr std::uint8_t MSG_OPEN = 0, MSG_CONFIRM = 1, MSG_MOVE = 2, MSG_END = 3, MSG_LEAVE = 4;
// script_runByName 0x1ed694 for an entry the level script does not export.
inline constexpr std::int32_t MISSING_ENTRY = -1;

// Box layout: 640 x 448 arguments of the text and sprite calls (section 7).
inline constexpr int LINE = 19, ROW0 = 0x127, BODY_X = 0x38, BODY_W = 0x20C, BODY_TOP = 0x127;
inline constexpr int BODY_BOTTOM_TEXT = 0x186, BODY_BOTTOM_NOTEXT = 0x127;
inline constexpr int CHOICE_X = 0x56, CHOICE_TOP = -100, CHOICE_BOTTOM = 0x244, CHOICE_BASE = 0x1A1;
inline constexpr int MORE_Y = 0x18E, SCROLL_PAGE = 0x4C, SCROLL_STEP = 5, HEIGHT_SLACK = 0x5F;
inline constexpr int BOX_X = 0x23, BOX_Y = 0x118, BOX_LAYER = 5;
// box.tex in dlgbox.lmp is 560 x 156 texels (header +0/+2, docs/dialog.md 7).
inline constexpr int BOX_TEXELS_W = 560, BOX_TEXELS_H = 156;
inline constexpr int NAME_X = 0xAE, NAME_Y = 0x11D, PLAYER_X = 0xB0, PLAYER_Y = 0x100, PLAYER_FIT = 0xDA;
inline constexpr int TEXT_LAYER = 6;
inline constexpr std::int64_t FORCED_BUDGET = 1000000, GLYPH_BUDGET = 100000, PLAYER_BUDGET = 10000;
inline constexpr std::uint16_t WIDE_NEWLINE = 10, CARET = 0x5E, SPACE = 0x20;
inline constexpr std::size_t TEXT_BUFFER_UNITS = 0x400; // 0x800 bytes at 0x4fdd38
inline constexpr std::size_t VISIBLE_ROOM = 10;         // 0x8e2d20 .. 0x8e2d47

inline constexpr Raw BLEND_STEP = 0x3DF5C28E;       // 0.12
inline constexpr Raw NAME_SHRINK = 0x3F199999;      // 0.6
inline constexpr Raw CLIP_FAR_OPEN = 0x44340000;    // 720.0
inline constexpr Raw CLIP_FAR_CLOSE = 0x44E10000;   // 1800.0
inline constexpr Raw OPEN_LOOK_LIMIT = 0xBEFB645A;  // -0.491
inline constexpr Raw CLOSE_LOOK_LIMIT = 0xBF251E85; // -0.645
inline constexpr Raw DUCK = 0x3E999999;             // music_setDuck(0.3)
inline constexpr Raw SECONDS_PER_BYTE = 0x3898EADB; // 28 / (16 * 24000)
inline constexpr Raw TICKS_PER_SECOND = 0x42700000; // 60.0
inline constexpr Raw ONE_SIXTIETH = 0x3C888888;
inline constexpr Raw LIP_LEAD = 0x3D8F5C28;         // 0.07 s
inline constexpr Raw SYNC_TOLERANCE = 0x3CA3D70A;   // 0.02 s
inline constexpr Raw NUDGE = 0x3DCCCCCC;            // 0.1 tick
inline constexpr Raw TALK_BLEND = 0x3E999999, IDLE_BLEND = 0x3ECCCCCC;
inline constexpr int FADE_WAIT_FRAMES = 0x14, CAMERA_LOOK_FRAMES = 0x2D;
inline constexpr std::uint32_t UP_BITS = 0x101000, DOWN_BITS = 0x204000;

inline constexpr std::uint32_t COLOUR_BODY = 0x3A5262, COLOUR_PICK = 0x36775A, COLOUR_OTHER = 0x404040,
                               COLOUR_GLYPH = 0x606060, COLOUR_NAME = 0x806464, COLOUR_BOX = 0x808080,
                               COLOUR_MEASURE = 0x80808080, COLOUR_LAYOUT = 0x80323C3C;

// The original's behaviour past this point is a null read or a buffer overrun.
class Refused : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

// ---- files --------------------------------------------------------------------------
struct Choice {
  std::string text, condition, action;
  std::int32_t next = -1;
};
struct Node {
  std::string condition, text, voice, speaker;
  std::vector<Choice> choices;
  // +0x54 as stored; dialog_showNode loops over it. Equal to choices.size()
  // for a parsed file.
  std::int32_t raw_choices = 0;
};
// Refuses a size that is not a multiple of 0x448, a count byte that does not
// match the size, a choice count outside 0..12 or a jump outside the file.
std::vector<Node> parse_dbin(const std::uint8_t *data, std::size_t size);
// A synthetic file: count in byte 0 of node 0, the other bytes 0.
Bytes build_dbin(const std::vector<Node> &nodes);
std::vector<int> start_candidates(const std::vector<Node> &nodes);
std::set<int> reachable_nodes(const std::vector<Node> &nodes);
using ScriptRunner = std::function<std::int32_t(const std::string &)>;
int pick_start(const std::vector<Node> &nodes, const ScriptRunner &run);

// ---- message 0 ----------------------------------------------------------------------
Bytes start_message(const std::string &name, int player, int a, int b, const std::array<Raw, 3> &cam,
                    int yaw, int pitch);
struct StartMessage {
  std::string name;
  int player = 0, a = 0, b = 0;
  std::array<Raw, 3> cam{};
  int yaw = 0, pitch = 0;
};
StartMessage parse_start_message(const Bytes &msg);

// ---- the log ------------------------------------------------------------------------
using Arg = std::variant<std::int64_t, std::string, Bytes, std::vector<std::int64_t>>;
struct Event {
  std::string kind;
  std::vector<Arg> args;
};
// Canonical text of one event, shared with tools/generate_dialog_vectors.py.
std::string format_event(const Event &);

// ---- host ---------------------------------------------------------------------------
struct Object {
  std::uint32_t addr = 0;
  std::array<Raw, 3> pos{};
  std::uint32_t facing = 0;
  std::uint64_t flags = 0;
  bool npc = false;
  std::uint32_t word24 = 0;               // *(obj + 0x24): the non-NPC focus word
  std::map<std::string, std::string> props; // key in lower case -> text
  int anim_count = 1;                     // s8 +0x118
  std::int64_t anim_cur = 0;              // +0x11c
  std::uint32_t display_name = 1;         // +0x368, 0 = none
  std::array<Raw, 8> tracks{};            // raw time of track i at +0x134 + 0x20 i
  std::int64_t flag20_track = -1;         // anim_findTrackByFlag(+0x114, 0x20)
  std::optional<std::pair<std::int64_t, Raw>> track_store;
};

// Inputs the original takes from other subsystems (ref_dialog.Host).
struct Host {
  bool network = false;
  std::set<int> local{0};
  int num_players = 1;
  std::map<int, Object *> player_obj{{0, nullptr}};
  std::vector<int> alive{0};
  std::map<int, std::uint32_t> raw_pad{{0, 0}};
  std::uint32_t confirm_mask = 0x40;
  int recall = 0;
  bool mission_menu = false;
  bool pause_menu = false;
  int shop_open = 0;
  std::map<std::string, std::vector<Object *>> objects;
  std::map<std::string, std::int32_t> scripts;  // used when runner is empty
  std::map<std::string, Wide> texts;
  std::map<std::pair<std::uint32_t, std::string>, std::int64_t> role_anims;
  std::map<std::string, std::int64_t> anm_length, vag_length;
  std::map<std::string, std::pair<std::int64_t, std::int64_t>> anm_entry;
  std::int64_t layout_height = 0;
  // text_drawWrapped(56, 0, text, 524, 0, 0, 19) of dialog_showNode; when set
  // it answers instead of layout_height.
  std::function<std::int64_t(const Wide &)> layout;
  std::vector<std::string> ran;
  // A level-script host; without one the scripts table answers.
  ScriptRunner runner;
  // A text table; without one the texts map answers.
  std::function<std::optional<Wide>(const std::string &)> text_source;
  // Camera_setupDialogView 0x203bb8, called by message 0 where the reference logs
  // "dialog_view" (docs/spec/dialog-gaps.md 2.2): the side byte, the focus object
  // and the message's camera position, yaw and pitch. The hook may throw Refused.
  // Without one only the log entry is made (the reference model).
  std::function<void(int side, const Object &focus, const std::array<Raw, 3> &cam, int yaw, int pitch)> view;

  std::int32_t run_script(const std::string &name);
  bool is_local(int slot) const { return local.count(slot) != 0; }
  Wide text(const std::string &key) const;
  std::int64_t role_anim(const Object *obj, const std::string &role) const;
};

// Per-call inputs of dialog_runTask (ref_dialog.TickIO). Each query returns
// the next value of its list; the last value repeats.
struct TickIO {
  std::vector<std::int64_t> camera{1};
  Raw look = ZERO;
  std::vector<std::int64_t> held{0}, queue{0}, position{0}, status{0};
  Raw fade = ZERO;
  int text_option = 1;
  std::int64_t shown = 0;
  // The measuring pass itself: the characters text_drawWrapped shows at y with
  // this budget (-1: unlimited) and clip bottom. When set it answers instead
  // of shown.
  std::function<std::int64_t(std::int64_t y, std::int64_t budget, int bottom)> measure;
  std::vector<std::pair<std::string, std::int64_t>> asked;
  // Camera_lookAt itself (docs/spec/dialog-gaps.md 2.3); when set it answers
  // instead of the camera list: 1 = arrived.
  std::function<std::int64_t(int frames)> look_at;

  std::int64_t camera_step(int frames);
  std::int64_t held_ready();
  std::int64_t queue_count();
  std::int64_t stream_position();
  std::int64_t stream_status();
};

struct Buffer {
  int ptr = 0, has = 0;
  std::int64_t size = 0, vag = 0;
};

// ---- the machine --------------------------------------------------------------------
class Dialog {
public:
  std::vector<Node> nodes;           // 0x8e2d5c
  bool has_nodes = false;
  std::optional<int> node;           // 0x8e2d60 g_dialogData: shown node, empty = closed
  int node_index = -1;               // 0x8e2e88
  int prompt_node = -1;              // 0x8e2e8c
  int prompt_shown = 0;              // 0x8e2e90
  int revealing = 0;                 // 0x8e2cb0
  int reveal_done = 0;               // 0x8e2cb4
  std::int64_t reveal = 0;           // 0x8e2d4c
  int at_end = 0;                    // 0x8e2ca8
  int more = 0;                      // 0x8e2cac
  std::int64_t cursor = 0;           // 0x8e2cb8
  std::int64_t cursor_target = 0;    // 0x8e2cbc
  Raw blend = ONE;                   // 0x8e2d54
  std::int64_t scroll = 0;           // 0x8e2ccc
  std::int64_t scroll_target = 0;    // 0x8e2cc8
  std::int64_t total = 0;            // 0x8e2cd0
  Raw span = ZERO;                   // 0x8e2d58, written only
  std::vector<int> visible;          // 0x8e2d20 / 0x8e2d18
  std::array<std::int64_t, MAX_CHOICES> choice_y{}; // 0x8e2ce8, not cleared between nodes
  Wide text;                         // 0x4fdd38, without the terminator
  std::int64_t text_len = 0;         // 0x8e2d48
  int cam_move = 0;                  // 0x8e2d50
  int closing = 0;                   // 0x8e2cd8
  int end_sent = 0;                  // 0x8e2ce0
  int drawn = 0;                     // 0x8e2cd4
  std::int64_t swoop = 0;            // 0x4fdce4
  int side = 0;                      // 0x8e2cc4, written only
  int remote = 0;                    // 0x8e2ca0
  int store_pending = 0;             // 0x4fdd30
  std::optional<int> store_index;    // 0x4f7150 (s16)
  std::string next_world;            // 0x4fdcf0
  Wide name;                         // 0x8e2d68
  std::int64_t name_len = 0;         // 0x8e2cdc
  int hold = 0;                      // 0x4fdcd4
  int anm_ready = 1;                 // 0x4fdcd5
  int buf_sel = 0;                   // 0x4fdcd6
  int cur_buf = 0;                   // 0x4fdcd7
  int talking = 0;                   // 0x4fdcd8
  Raw nudge = ZERO;                  // 0x4fdcdc
  std::array<Buffer, 2> buffers{};   // 0x8e2e68 + 16 i
  std::int64_t queue_mark = 0;       // 0x8e2ca4
  Object *focus = nullptr;           // 0x4fdcc0
  std::uint32_t focus_word = 0;      // 0x4fdcc4
  std::int64_t idle_anim = 0;        // 0x4fdccc
  int mutex = 0;                     // 0x4fe53c
  int hud_suppressed = 0;            // 0x4f66a8
  int swoop_done = 0;                // 0x4fdce8
  int active_player = 0;             // 0x4d8ae4 g_scriptActivePlayer
  std::optional<std::string> pending_world; // 0x41b6d0
  std::optional<int> orient;         // 0x41b720
  std::int64_t shown = 0;            // 0x44e028
  std::int64_t last_position = 0;    // 0x4fdce0
  std::vector<Event> log;
  // Offline, net_sendGameMsg calls the handler at once on the sender's buffer
  // (research docs/spec/game-messages.md 1.2). When set, a message the run
  // task sends is handed here as it is sent, so later steps of the same run
  // see its effect. The reference model leaves it unset and returns them.
  std::function<void(const Bytes &)> on_send;
  // script_runPostAction, at its place in the end handler (message 3). The
  // reference only logs it.
  std::function<void()> on_post_action;

  bool is_open() const { return node.has_value(); }
  void clear_text();
  int mutex_try();
  void mutex_release() { mutex = 0; }

  // dialog_start 0x22fa50: the message bytes when this machine sends one.
  std::optional<Bytes> start(const Host &, const std::string &name, int a, int b,
                             const std::array<Raw, 3> &cam, int yaw, int pitch);
  // Messages (dialog_onMsg 0x22ee2c).
  bool on_open(Host &, const Bytes &msg, const std::vector<Node> &file, const std::string &level = "level");
  void show_node(Host &, int index);
  void queue_line_audio(Host &, const Node &);
  void on_confirm(Host &);
  void on_move(Host &, int delta);
  void on_end(Host &);
  void on_leave() { cam_move = -1; }
  // Returns true for an open that was accepted (message 0); false otherwise.
  bool on_message(Host &, const Bytes &msg, const std::vector<Node> *file = nullptr,
                  const std::string &level = "level");
  // dialog_runTask 0x22ff48; returns the messages sent.
  std::vector<Bytes> tick(Host &, TickIO &);
  // dialog_startTalkAnim 0x22fc70 / dialog_syncMouth 0x22fdd8.
  std::optional<std::pair<std::int64_t, Raw>> talk_anim(Host &);
  void sync_mouth(Host &, const std::function<std::int64_t()> &position);
  // dialog_drawTask 0x230628; the calls in order (also appended to log).
  std::vector<Event> draw(Host &, Raw fade, int text_option, int glyph,
                          const std::function<std::int64_t()> &stream_status);
  void free(); // dialog_free 0x22f99c
  int prompt_is(int index) const { return prompt_shown != 0 && index == prompt_node ? 1 : 0; }

  // Script built-ins.
  void change_swoop(std::uint32_t mode) { swoop = s32(mode); }
  void start_with_swoop_done() { swoop_done = 1; }
  void new_world_post(const std::string &name) { next_world = name; }
  void activate_store(Host &, std::int64_t index);
  void activate_mission_menu(Host &, int first_active = 0);
  // Creature_interact 0x116bd8 / Creature_trigger 0x11a5f0. Returns what
  // happened: nothing, shop, behind, busy, no script, script.
  std::string interact(Host &, Object &obj, int slot, bool flag800000 = false);
  std::string trigger(Host &, Object &obj);

private:
  void reset_focus_anim();
  Object *find_speaker(Host &, const std::string &name, int player);
  bool audio_ready_branch(TickIO &);
};

// ---- text table (research docs/data-tables.md section 6) ---------------------------
// gtext.uni in GTEXT.LMP: a zero-terminated ASCII key, padding to
// (start + len + 2) & ~1, then UTF-16LE text ending in a zero unit; the list
// ends at a key that starts with a zero byte. text_lookup 0x173c84 drops
// double quotes from the key; of two equal keys the earlier is found; a miss
// returns "NF:" and the first 16 characters of the key.
class TextTable {
public:
  static TextTable parse(const std::uint8_t *data, std::size_t size);
  Wide lookup(const std::string &key) const;
  bool contains(const std::string &key) const;
  std::size_t entries() const { return entries_; }
  std::size_t keys() const { return table_.size(); }

private:
  std::map<std::string, Wide> table_;
  std::size_t entries_ = 0;
};

// obj_getPropInt: the leading decimal integer (atoi rules), 0 when none.
std::int64_t prop_int(const std::string &text);
} // namespace champions::simulation::rta::dialog
