#pragma once
// Return to Arms (SLUS-20973 v2.00) string table, wide-string formatter and the
// HUD state of the five text script commands. Port of research
// t3/research-strings d4987fd, tools/ref_strings.py and docs/strings.md.
// Every address below is the one that document gives. Keys and texts come from
// the user's game folder at run time; this library holds none.
//
// Deliberate refusals (docs/STRINGS.md): a format that would read past its own
// terminator, and a HUD text that would overflow its fixed buffer. The original
// does both; the port throws Refused instead.
#include "champions/core_state.hpp"
#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace champions::simulation::rta::strings {
inline constexpr const char *POLICY = "rta-strings-v1";
using Units = std::vector<std::uint16_t>;

// The original has no defined result (an endless probe, a stack overrun, a
// null dereference), or the port refuses the case on purpose.
class Refused : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

// 1. The text table (text_hash 0x173a74, text_registerTable 0x173bac,
// text_unregisterTable 0x173ac0, text_lookup 0x173c84, text_find 0x173de0).
inline constexpr std::uint32_t TEXT_SLOTS = 0x2000;   // g_textHash 0x7c2ea0
inline constexpr std::uint32_t G_NF_INDEX = 0x41bfa8; // s8 ring position
inline constexpr std::uint32_t G_NF_RING = 0x7caea0;  // 16 entries x 40 bytes
inline constexpr std::size_t NF_ENTRIES = 16, NF_UNITS = 20;
inline constexpr std::size_t LOOKUP_KEY_BYTES = 0x200;

std::uint32_t text_hash(const std::string &key);
std::string strip_quotes(const std::string &key);
Units visible(const Units &units);
// What a miss shows: N, F, ':' and the first 16 bytes of the stripped key, each
// sign-extended to 16 bits.
Units nf_text(const std::string &request);

struct UniEntry {
  std::size_t key_offset = 0;
  std::string key;
  std::size_t text_offset = 0;
  Units units;
  std::size_t next = 0;
};
// The walk of text_registerTable: an ASCII key, 0, the UTF-16LE text from
// (key + strlen + 2) & ~1 to its 0 unit; it ends at a key whose first byte is 0.
// A stream that ends inside an entry is refused.
std::vector<UniEntry> uni_entries(const std::uint8_t *data, std::size_t size,
                                  std::size_t start = 0);
// Every `.uni` entry of one lump, in directory order, given (entry name, bytes).
bool is_uni_name(const std::string &entry_name);

struct Lookup {
  std::uint32_t address = 0;
  Units units;
  bool hit = false;
};

// g_textHash and the miss ring. Keys are identified by their address, as the
// original's table holds pointers. Blobs are registered at caller-chosen bases.
class TextTable {
public:
  TextTable();
  // Every entry goes to the first free slot from its hash; repeats are kept, so
  // the earlier copy is the one found. Returns (key address, slot) per entry.
  std::vector<std::pair<std::uint32_t, std::uint32_t>>
  register_table(const std::vector<std::uint8_t> &data, std::uint32_t base);
  // Zeroes the slot holding each key address, without a tombstone.
  void unregister_table(const std::vector<std::uint8_t> &data, std::uint32_t base);
  // text_find: the text address, or 0.
  std::uint32_t find(const std::string &request) const;
  // text_lookup. stale stands for the stack bytes after the key copy's 0.
  Lookup lookup(const std::string &request, const std::vector<std::uint8_t> &stale = {});
  // The visible units a lookup would return now, without advancing the ring.
  Units peek(const std::string &request) const;
  // The unit at an address in a registered stream or in the miss ring.
  std::uint16_t unit_at(std::uint32_t address) const;
  // Units from address up to its first 0 (a registered text or a ring entry).
  Units text_at(std::uint32_t address, std::size_t limit = 0x10000) const;
  bool holds(std::uint32_t address) const;
  const std::array<std::uint32_t, TEXT_SLOTS> &slots() const { return slots_; }
  int nf_index() const { return nf_index_; }
  const std::array<std::array<std::uint16_t, NF_UNITS>, NF_ENTRIES> &nf_ring() const {
    return nf_ring_;
  }
  std::size_t entries() const { return keys_.size(); }
  std::size_t probes() const { return probes_; }
  // The next 16-aligned base after every registered blob, for a caller that
  // registers several tables.
  std::uint32_t next_base() const;

private:
  std::pair<std::string, std::uint32_t> probe(const std::string &request) const;
  std::array<std::uint32_t, TEXT_SLOTS> slots_{};
  std::map<std::uint32_t, std::string> keys_;
  std::map<std::uint32_t, std::pair<std::uint32_t, Units>> texts_;
  std::map<std::uint32_t, std::vector<std::uint8_t>> blobs_;
  int nf_index_ = 0;
  std::array<std::array<std::uint16_t, NF_UNITS>, NF_ENTRIES> nf_ring_{};
  mutable std::size_t probes_ = 0;
};

// 2. Wide strings and wstr_format 0x19ea78.
Units wstr_copy(const Units &src);                        // 0x19e854
std::size_t wstr_len(const Units &src);                   // 0x19e994
Units wstr_from_ascii(const std::vector<std::uint8_t> &); // 0x19e884
std::vector<std::uint8_t> wstr_to_ascii(const Units &);   // 0x19e8b4
std::int32_t cvt_w_s(std::uint32_t raw);
std::uint32_t cvt_s_w(std::int32_t value);
// soft_extendsfdf2 0x3aef58 as the R5900 reads the pattern: exponent 0 is a
// signed zero, exponent 255 an ordinary number.
double widen(std::uint32_t raw);
// sprintf("%.2f", widen(raw)).
std::string float_text(std::uint32_t raw);

using UnitReader = std::function<std::uint16_t(std::size_t)>;
using CStr = std::function<std::vector<std::uint8_t>(std::uint32_t)>;
using WStr = std::function<Units(std::uint32_t)>;
struct Formatted {
  Units written; // every unit stored, the final 0 included
  int used = 0;  // arguments taken
};
// The eight float arguments f12..f19. A ninth conversion, an unresolvable %s/%u
// and a format that would copy its own terminator (it ends in a conversion or a
// lone '%', or is empty) are refused.
Formatted wstr_format(const UnitReader &fmt, const std::array<std::uint32_t, 8> &args,
                      const CStr &cstr = {}, const WStr &wstr = {});
Formatted wstr_format(const Units &fmt, const std::array<std::uint32_t, 8> &args,
                      const CStr &cstr = {}, const WStr &wstr = {});
// wstr_formatAscii 0x19e9c4: widen into the 0x200-byte stack buffer, then format.
Formatted wstr_format_ascii(const std::vector<std::uint8_t> &fmt8,
                            const std::array<std::uint32_t, 8> &args, const CStr &cstr = {},
                            const WStr &wstr = {});
// True when wstr_format would copy the format's terminator (docs/strings.md 2.3).
bool reads_past_terminator(const UnitReader &fmt);
// addHelpMessageF's call: f12 = cvt.s.w(value), f13..f19 = 0.0; %s and %u refuse.
Formatted format_one_int(const UnitReader &fmt, std::uint32_t value);

// 3. HUD state of the five commands (docs/strings.md 3.1).
inline constexpr std::uint32_t G_HUD_SUPPRESSED = 0x4f66a8;
inline constexpr std::uint32_t G_BUST_HIDE_LATCH = 0x4f66a9;
inline constexpr std::uint32_t G_HELP_CLOSE_DELAY = 0x4f66cc;
inline constexpr std::uint32_t G_BUST_SHOWN = 0x4f66d4;
inline constexpr std::uint32_t G_COMPASS_SHOWN = 0x4f66d5;
inline constexpr std::uint32_t G_MSG_FRAMES = 0x4f66e0;
inline constexpr std::uint32_t G_MSG_FORCE = 0x4f66e2;
inline constexpr std::uint32_t G_HELP_OPEN = 0x41b928;
inline constexpr std::uint32_t G_PAUSED = 0x41b4f4;
inline constexpr std::uint32_t G_HELP_PAUSE_BYTE = 0x44fa80;
inline constexpr std::uint32_t G_MENU_INPUT_LOCKOUT = 0x41b4fc;
inline constexpr std::uint32_t G_HELP_TITLE = 0x86aa64, G_HELP_BODY = 0x86aa68;
inline constexpr std::uint32_t HELP_F_TITLE = 0x5d3048, HELP_F_BODY = 0x5d3088,
                               HELP_F_END = 0x5d3188;
inline constexpr std::size_t BUST_TEXT_LIMIT = 31, COMPASS_LABEL_LIMIT = 31,
                             MESSAGE_LIMIT = 99, HELP_F_TITLE_LIMIT = 31,
                             HELP_F_BODY_LIMIT = 127, FORMAT_OUTPUT_LIMIT = 255,
                             SCRIPT_KEY_LIMIT = 159, BUST_TEXTURE_LIMIT = 39;
inline constexpr int MESSAGE_FRAMES = 180;        // setInfo
inline constexpr int HELP_CLOSE_DELAY_OPEN = 90;  // written when a box opens
inline constexpr int HELP_CLOSE_DELAY_CLOSE = 60; // and when it closes
inline constexpr int SHOWN_TICKS = 5;             // bust / compass lease
inline constexpr const char *HELP_CHIME_LUMP = "fx.lmp";
inline constexpr const char *HELP_CHIME_ENTRY = "butn_message.anm";

// One outward call: the chime, ui_startPulse, game message 2.
struct Event {
  std::string name;
  std::vector<std::string> args;
  std::string text() const;
};

struct HudInputs {
  bool suppressed = false;  // g_hudSuppressed 0x4f66a8
  bool local_menu = false;  // g_localMenuOpen 0x41b69d
  bool menu_open = false;   // g_menuOpenFlag 0x41b4f8
  // g_localCamera +0x34. nullopt: no camera object (the gate would fault).
  std::optional<int> camera_mode = 1;
  bool loadscreen = false;  // g_loadscreenActive 0x506094
  bool warmup = false;      // this machine's g_warmupFrames 0x506048
  int num_players = 1;      // g_numPlayers
  bool network = false;     // 0x402791
  std::vector<int> alive;   // alive list 0x4f8440, list order
  std::function<bool(int)> local = [](int) { return true; }; // net_isLocalSlot
};

// What help_task did on one displayed frame (docs/strings.md 3.3).
struct HelpFrame {
  bool closed = false;
  bool run_post_action = false; // offline: script_runPostAction
  bool paused = false;          // g_paused after this frame
};

class Hud {
public:
  HudInputs inputs;
  std::vector<Event> events;
  // Texts are stored as the units the copy wrote (visible units).
  Units message;
  std::uint16_t msg_frames = 0;
  std::uint8_t msg_force = 0;
  bool help_open = false;
  std::uint32_t help_title = 0, help_body = 0; // pointers, not copies
  std::int32_t help_close_delay = 0;
  Units help_f_title, help_f_body;
  std::uint8_t bust_shown = 0, compass_shown = 0;
  std::string bust_texture;
  Units bust_text, compass_label;
  std::uint32_t bust_value = 0, bust_max = 0;
  std::array<std::uint32_t, 3> compass_target{};
  std::uint8_t latch = 0;
  // help_task's outputs.
  bool paused = false, help_pause_byte = false;
  std::int32_t menu_input_lockout = 0;

  // hud_showMessage 0x1d92a0. True when it stored. Refuses a text over 99 units
  // when it would store.
  bool show_message(const Units &text, std::uint32_t frames, std::int32_t slot,
                    std::uint32_t force);
  // hud_showHelpMessage 0x1d9830. No queue: a call while open replaces both
  // pointers, chimes again and re-arms the delay.
  bool show_help(std::int32_t slot, std::uint32_t title, std::uint32_t body);
  // hud_setBust 0x1dfaa0, hud_setCompassTarget 0x1df668: the shared gate, then
  // the 5-tick lease. A null camera, a text over 31 units and a texture name
  // over 39 bytes are refused when the setter would store.
  bool set_bust(const std::string &texture, const Units &text, std::uint32_t value,
                std::uint32_t maximum);
  bool set_compass(const std::array<std::uint32_t, 3> &position, const Units &text);
  // The setters' gate: nullopt when it would fault (no camera).
  std::optional<bool> widget_gate() const;
  // Would the message setter store? (slot and force as show_message.)
  bool message_stores(std::int32_t slot, std::uint32_t force) const;
  // hud_tickMessage 0x1d934c, then the two countdowns of hud_run 0x1dbbec.
  void tick_message();
  void tick_widgets();
  // help_task 0x1dcc78, once per displayed frame. confirm is a confirm press on
  // one of the g_numPlayers raw pads (the button is the caller's).
  HelpFrame help_task(bool confirm);
  // Help F buffers as text_at would see them.
  bool holds(std::uint32_t address) const {
    return address >= HELP_F_TITLE && address < HELP_F_END;
  }
  Units text_at(std::uint32_t address) const;
};

// The five handlers on a TextTable and a Hud. Keys are the C strings the
// handlers copied out of the script. find_object returns the object's +0x10
// position words. Each check_* says, without changing anything, whether the
// command would be refused; the matching call then runs it.
class Commands {
public:
  Commands(TextTable &table, Hud &hud);
  TextTable &table;
  Hud &hud;
  std::function<std::optional<std::array<std::uint32_t, 3>>(const std::string &)> find_object;
  // Text pointer resolution for the help box: the table, the ring, the F buffers.
  Units resolve(std::uint32_t address) const;

  std::optional<std::string> check_set_info(const std::string &key) const;
  std::optional<std::string> check_help_f(const std::string &key1, std::uint32_t v1,
                                          const std::string &key2, std::uint32_t v2) const;
  std::optional<std::string> check_bust(const std::string &texture, const std::string &key) const;
  std::optional<std::string> check_compass(const std::string &name, const std::string &key) const;

  void set_info(const std::string &key);
  void add_help_message(const std::string &title, const std::string &body);
  void add_help_message_f(const std::string &key1, std::uint32_t v1, const std::string &key2,
                          std::uint32_t v2);
  void show_bust(const std::string &texture, const std::string &key, std::uint32_t value,
                 std::uint32_t maximum);
  void show_compass(const std::string &name, const std::string &key);
};

// 4. sound_playOnObject 0x214230 with anim_push 0x170838 (one track per slot).
inline constexpr std::uint32_t G_SOUND_SEQS = 0x871a68, SEQ_STRIDE = 0x4c;
inline constexpr std::size_t SEQ_SLOTS = 32;
inline constexpr std::uint32_t G_SOUND_GROUP_LIMIT = 0x4f8a28, G_SOUND_SERIAL = 0x4f8950;
inline constexpr std::uint32_t SOUND_NEAR = 0x43b40000, SOUND_FAR = 0x44520000;
// g_soundGroupLimit for the 0..98 groups on the NTSC disc: 2, except group 14 = 9
// and groups 50 and 51 = 8 (research-audio 6b08886 docs/audio-playback.md 4).
std::array<std::int8_t, 256> disc_group_limits();

struct SeqSlot {
  std::uint8_t tracks = 0, capacity = 1;
  std::uint16_t flags = 0;
  std::uint32_t seq = 0, track_link = 0, track_time = 0;
  std::uint16_t track_flags = 0;
  std::array<std::uint32_t, 4> track_words{};
  std::array<std::uint32_t, 3> pos{};
  std::uint16_t serial = 0;
  std::uint8_t no_pos = 0, frozen = 0, released = 0;
  std::uint32_t volume = 0, near = 0, far = 0;
  Cell handle_head;
};
// An object a sequence follows: its head 0 and position words.
struct SoundTarget {
  Cell *back = nullptr;
  Object *object = nullptr;
  std::array<std::uint32_t, 3> position{};
};
struct Played {
  std::uint32_t handle = 0xffffffffu;
  std::optional<std::size_t> slot;
  std::uint32_t cut_link = 0; // a track link whose +0x14 was zeroed
};
class SoundSeqs {
public:
  explicit SoundSeqs(HandlePool *pool = nullptr);
  std::array<SeqSlot, SEQ_SLOTS> slots;
  std::uint16_t serial = 0;
  std::array<std::int8_t, 256> limits{};
  std::map<std::uint32_t, std::uint8_t> groups; // sequence -> clip byte +0x3c
  std::uint8_t frozen = 0;                      // game_isFrozen()
  HandlePool *pool;
  // Keeps the LAST free slot; the category cap fails the call as soon as the
  // count of same-category busy slots reaches it. No seed is read.
  Played play_on_object(std::uint32_t seq, const SoundTarget *target, std::uint32_t volume);
};

// 5. Creature_setCharm 0x1189f0 with a live target handle at +0x234.
struct Creature {
  std::uint32_t flags = 0;
  std::array<Cell, 3> heads;
  Cell target;
  std::uint8_t team = 0;
  std::uint16_t charm = 0;
  Object object;
};
// True when it stored. The clear removes this creature's node(s) and each
// partner from the target's back list; the target is untouched.
bool set_charm(HandlePool &pool, Creature &creature, std::uint32_t frames, std::uint32_t team);
} // namespace champions::simulation::rta::strings
