#pragma once

#include "champions/frontend_layout.hpp"
#include "champions/music_volume.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

// Boot screens, front end, character creation, options, load/save and the pause
// menu of Champions: Return to Arms (SLUS-20973 v2.00), as a navigation and
// state contract. Authored from the research docs only:
//   t3/research-ui 3e39392   docs/mainloop.md 8, docs/ui.md "Front end", docs/player.md,
//                            docs/data-tables.md 4-5, docs/issues/live-rig.md 4a and 21.7,
//                            docs/issues/native-online.md, symbols/mainloop.tsv
//   t3/research-menus 099900e docs/menus.md 6-7
//   t3/research-saves 8d43286 docs/save-format.md (options image, card states)
//   t3/research-combat-ctors e9777eb docs/spec/music-volume.md (slider, commit, boot)
//   t3/research-menus-layout dc458d5 docs/frontend-layout.md and its tools
//                            (list/frame geometry, fonts, sprites, sounds, theme gate)
// layout() returns the draw calls the research documents (positions, frames,
// sprite and font names, layers, colours) and marks every element it does not
// document as Host; notes() names each remaining gap. Texture and sample bytes
// are never held here: the host loads them from the user's disc at run time.
// Nothing here is copied from the game.
// Local presentation state only: nothing here feeds shared gameplay state.
namespace champions::frontend {

struct Refusal : std::runtime_error {
  using std::runtime_error::runtime_error;
};

// Pad bits (docs/netcode.md; 11 of them Ran, docs/issues/live-rig.md).
inline constexpr std::uint32_t TRIANGLE = 0x10, CIRCLE = 0x20, CROSS = 0x40, SQUARE = 0x80,
                               SELECT = 0x100, START = 0x800, UP = 0x1000, RIGHT = 0x2000,
                               DOWN = 0x4000, LEFT = 0x8000;
// Confirm and cancel of the byte-1 scheme game_initCamera stores (docs/menus.md 1).
// That the US build takes byte 1 is Inferred there.
inline constexpr std::uint32_t CONFIRM = CROSS, CANCEL = CIRCLE;

// symbols/mainloop.tsv notes: splash_soeLogo done after 240 frames or a press of
// pad 0 or 1 (which sets g_skipSplash 0x506084); splash_introMovie done when the
// movie finishes or on a press after 30 frames.
inline constexpr int SOE_LOGO_FRAMES = 240;
inline constexpr int INTRO_PRESS_AFTER = 30;
// recall_execute sets g_menuInputLockout 0x41b4fc to 60 (docs/engine.md P17).
inline constexpr int RECALL_LOCKOUT = 60;
// The lockout clears when no pad holds a button under this mask (docs/mainloop.md).
inline constexpr std::uint32_t LOCKOUT_HELD_MASK = 0xff0fffffu;
// Character creation (docs/player.md, docs/data-tables.md 4; rig route Ran).
inline constexpr int MODELS = 12;
inline constexpr int CAROUSEL_POSITIONS = 13;
inline constexpr int CLASSES = 7;
inline constexpr int NAME_BYTES = 0x20;     // record +0x00 char[0x20]
inline constexpr int START_GOLD = 500;      // record +0xe4, init
inline constexpr int START_LEVEL = 1;       // record +0x59 and +0x58 start at 1
inline constexpr int ABILITY_POINTS = 20;   // must be spent before leaving (Ran)
inline constexpr std::uint32_t CLASS_START_STATS = 0x4ef400; // 7 rows x 4 s32
// Options image g_savedConfig 0x4f8f38 (docs/save-format.md).
inline constexpr std::size_t OPTIONS_BYTES = 0x2f8;
inline constexpr std::size_t OPTIONS_MUSIC = 0x08;

// player_initNewCharacter 0x1b1d70: class from model. Throws outside 0..11.
int class_of_model(int model);
// Observed +8 model field of the 13 rows at 0x4fbc00. Position 0 repeats 3.
int carousel_model(int position);
const char *body_code_of_model(int model);
// g_skillTreePictures class words in class order (Observed strings; that they
// are class names is Inferred).
const char *class_word(int class_index);

enum class Screen {
  BootCard,       // mainloop_runBootCardCheck 0x32396c, card state 0
  SoeLogo,        // splash_soeLogo 0x321640
  IntroMovie,     // splash_introMovie 0x321788
  Main,           // frontend_run state 0
  GameType,       // state 1
  Players,        // state 2
  Difficulty,     // state 3
  Mode,           // state 4
  Options,        // state 11 (front end) or the pause option list
  OptionsLoad,    // memcard_optionsScreen, card state 10
  Credits,        // state 15
  LoadGame,       // memcard_requestLoad 0x2176e0, card state 4
  SaveGame,       // memcard_requestSave 0x2175b4, card state 3
  Saved,          // the "saved" box of memcard_saveScreen
  CharClass,      // playerSel_task page 1
  CharAppearance, // page 2
  CharAbility,    // page 3
  CharName,       // page 4
  Playing,
  Pause,          // pauseMenu_task 0x18cc08, table 0x44f3b8
  ExitConfirm,    // "Exit Game? Yes / No"
};
const char *screen_name(Screen);
// g_frontendState for the front-end screens, the playerSel page byte 0x8e2c02 for
// the character pages, -1 otherwise.
int screen_state(Screen);

struct Entry {
  std::string id, label, evidence;
  bool enabled = true;
  std::string refusal; // why a disabled entry is refused
  int type = 0;        // menuList type: 4 float slider; 0 action
  std::uint32_t value = 0;
};

// What the host must do for the flow. Drained by take_requests().
struct Request {
  std::string kind;
  std::string arg;
  std::uint32_t value = 0;
};

struct Character {
  std::string name;
  int model = 0;
  int class_index = 0;
  std::array<std::int32_t, 4> base{{0, 0, 0, 0}}; // str, int, dex, sta
  bool base_known = false;
  std::array<std::int32_t, 4> spent{{0, 0, 0, 0}};
  std::int32_t gold = START_GOLD;
  std::int32_t level = START_LEVEL;
  std::int32_t acknowledged = START_LEVEL;
  std::int32_t stat_points = 0;  // +0xec init 0
  std::int32_t skill_points = 0; // +0xe8: init value not documented
  bool skill_points_known = false;
  bool imported = false;
  // The menu's reachable appearance choices and handoff bytes are Unknown.
  // Do not represent an unobserved default as four zeroes.
  bool appearance_known = false;
  std::string level_name; // the imported slot's level, empty for a new character
  std::array<std::int32_t, 4> stats() const;
};

class Flow {
public:
  // options_file: the host's stored 760-byte options image, or nullopt for a
  // card without the file. A file the strict reader refuses is a Refusal.
  explicit Flow(std::optional<std::vector<std::uint8_t>> options_file = std::nullopt);

  // One displayed frame of pad 0. buttons is the 16-bit pad word; stick bytes
  // are 0x80 at rest. The pad goes through netwire::pad_apply (executed) to get
  // the held, pressed and auto-repeat words.
  void frame(std::uint16_t buttons, std::uint8_t stick_x = 0x80, std::uint8_t stick_y = 0x80);

  Screen screen() const { return screen_; }
  int cursor() const { return cursor_; }
  std::vector<Entry> entries() const;
  std::vector<std::string> notes() const;
  // The documented draw calls of the current screen, in submission order. The
  // frames are the documented minimum-width calls (no text-width rule is traced).
  Scene layout() const;
  // Optional caller-supplied card cache. The host must obtain real directory
  // metadata and headers; this flow never invents them from a file chooser.
  void set_card_cache(std::vector<std::uint8_t> metadata, std::vector<std::uint8_t> headers,
                      int count, int scroll_base);
  void set_card_preview(int header_index, bool debug_time);
  // Music requests the volume gate refused (music_playGlobal 0x2110e0).
  int gated_music() const { return gated_music_; }
  std::vector<Request> take_requests();

  // Host results.
  void set_class_start_stats(const std::array<std::array<std::int32_t, 4>, CLASSES> &rows);
  void set_name(const std::string &name);
  void import_result(bool ok, const Character &character, const std::string &error = {});
  void load_result(bool ok, const std::string &error = {});
  void options_loaded(std::optional<std::vector<std::uint8_t>> file);
  void save_result(bool ok, const std::string &error = {});
  void recall_executed(); // recall_execute: closes the pause menu, lockout 60
  // After exit_session: the exit code the pause menu's countdown leads to is not
  // documented, so the host restarts the front end with frontend_start(0).
  void return_to_frontend();
  void session_exit(int code); // g_sessionExitCode 1, 0x65, 0xc9
  void request_save();         // memcard_requestSave from a savepoint or script
  // A play session the host started outside the front end (its host menu):
  // the flow joins at Playing so start opens the pause menu. Host behaviour.
  void attach_play();
  void set_stream2_playing(bool playing) { stream2_playing_ = playing; }
  void set_stream2_status(std::uint32_t status) { stream2_status_ = status; }
  void set_net_game(bool net);

  // State.
  const audio::MusicVolumeWords &music() const { return music_; }
  const std::vector<std::uint8_t> &options_image() const { return image_; }
  bool options_file_present() const { return file_present_; }
  bool paused() const { return paused_; }
  // dialog_isOpen: a conversation is open (docs/spec/dialog-gaps.md 3). Offline the
  // start button cannot open the pause menu (warpMenu 0x18e0e4), and the entries
  // menu_recall (0x18d400) and menu_exitmission (0x18cd08) are not selectable.
  void set_dialog_open(bool open) { dialog_open_ = open; }
  bool dialog_open() const { return dialog_open_; }
  bool skip_splash() const { return skip_splash_; }
  int lockout() const { return lockout_; }
  int num_players() const { return num_players_; }
  int difficulty_index() const { return difficulty_; }
  int frames_on_screen() const { return frames_; }
  int save_latch() const { return save_latch_; }
  int load_from_frontend() const { return load_from_frontend_; }
  int last_exit_code() const { return exit_code_; }
  bool in_frontend() const;
  const Character &character() const { return character_; }
  bool character_ready() const { return character_ready_; }
  int ability_remaining() const;
  std::uint32_t slider_timer() const { return slider_timer_; }
  std::string last_error() const { return error_; }

private:
  Screen screen_ = Screen::BootCard;
  Screen options_return_ = Screen::Main;
  Screen load_return_ = Screen::Main;
  int cursor_ = 0;
  int frames_ = 0;
  std::array<std::uint8_t, 0x58> pad_{};
  std::array<std::int64_t, 2> pad_prev_{{0, 0}};
  audio::MusicVolumeWords music_;
  std::vector<std::uint8_t> image_;
  bool file_present_ = false;
  bool paused_ = false;
  bool skip_splash_ = false;
  bool net_ = false;
  bool dialog_open_ = false;
  bool stream2_playing_ = false;
  std::uint32_t stream2_status_ = 0;
  bool hold_flag_ = false; // 0x41b500, set only by script pauseGameControl
  int lockout_ = 0;
  int num_players_ = 1;
  int difficulty_ = 1; // the executable image holds 1 (docs/script-items.md)
  int save_latch_ = 0; // 0x4f8f28
  int load_from_frontend_ = 0; // 0x8e1ab8
  int exit_code_ = 0;
  int gated_music_ = 0;
  std::uint32_t slider_timer_ = 0; // 0x4d8a10
  Character character_;
  bool character_ready_ = false;
  bool start_known_ = false;
  int selected_carousel_position_ = 0;
  std::array<std::array<std::int32_t, 4>, CLASSES> start_stats_{};
  std::vector<Request> requests_;
  std::string error_;
  std::string import_context_; // "char" (page 1 triangle) or "load"
  bool card_paused_ = false;   // memcard_pushPause state to restore
  bool card_cache_supplied_ = false;
  std::vector<std::uint8_t> card_metadata_, card_headers_;
  int card_count_ = 0, card_scroll_base_ = 0;
  int card_preview_index_ = -1;
  bool card_debug_time_ = false;

  void go(Screen next, int cursor = 0);
  void music(const char *kind, const char *name, std::uint32_t flags);
  void sound(menu_sound which);
  void request(const std::string &kind, const std::string &arg = {}, std::uint32_t value = 0);
  void enter_frontend(int code);
  void finish_character();
  void move_cursor(std::uint32_t repeat);
  void options_frame(std::uint32_t held, std::uint32_t pressed, std::uint32_t repeat);
  void leave_options();
  void commit(bool query_only);
  std::uint32_t image_word(std::size_t offset) const;
  void set_image_word(std::size_t offset, std::uint32_t value);
};

} // namespace champions::frontend
