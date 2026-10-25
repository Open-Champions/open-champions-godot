#include "champions/frontend.hpp"
#include "champions/dialog_gaps.hpp"

#include "champions/native_audio.hpp"
#include "champions/netwire.hpp"
#include "champions/ps2_save.hpp"

#include <algorithm>
#include <tuple>
#include <utility>

namespace champions::frontend {
namespace ps2 = champions::simulation::rta::ps2_save;
namespace netwire = champions::simulation::rta::netwire;

namespace {
constexpr const char *SOUND_GAP =
    "Menu sounds: the FX.LMP resource names are documented (move menu_main_toggle, select menu_main_accept, back menu_main_back) and requested at the documented triggers. They are not voiced: the clips' variation draws and the group-67 volume word 0x41bac8 are not in the research.";
constexpr const char *BACK_GAP =
    "Circle as back is host behaviour: the cancel mask 0x20 is documented, the front end's back transitions are not.";
constexpr const char *CURSOR_GAP =
    "Cursor wrap at the list ends is not documented; the host clamps. Moves read the auto-repeat word pad +0x10 (Inferred), confirm and cancel the press word +0xc.";
constexpr const char *FADE_GAP =
    "No screen-to-screen fade of the front end is documented; changes are immediate. The alpha byte of the colour words is fade state; the host uses the settled 0x80 (Inferred).";

std::uint32_t word_at(const std::array<std::uint8_t, 0x58> &pad, std::size_t at) {
  return std::uint32_t(pad[at]) | std::uint32_t(pad[at + 1]) << 8 | std::uint32_t(pad[at + 2]) << 16 |
         std::uint32_t(pad[at + 3]) << 24;
}

Entry entry(const char *id, std::string label, const char *evidence, bool enabled = true,
            const char *refusal = "") {
  Entry e;
  e.id = id;
  e.label = std::move(label);
  e.evidence = evidence;
  e.enabled = enabled;
  e.refusal = refusal;
  return e;
}

const char *const STAT_ROWS[4] = {"Strength", "Intelligence", "Dexterity", "Stamina"};
} // namespace

int class_of_model(int model) {
  // player_initNewCharacter 0x1b1d70: models 0,1 -> 0; 4,5 -> 1; 8,9 -> 2;
  // 2,3 -> 3; 6,7 -> 4; 10 -> 5; 11 -> 6 (docs/data-tables.md 4.1, Observed).
  static constexpr int classes[MODELS] = {0, 0, 3, 3, 1, 1, 4, 4, 2, 2, 5, 6};
  if (model < 0 || model >= MODELS)
    throw Refusal("model index is outside 0..11");
  return classes[model];
}

int carousel_model(int position) {
  // t3/research-char-create b6dda4e, table +8, both pinned seeds.
  static constexpr int order[CAROUSEL_POSITIONS] = {0, 11, 10, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  if (position < 0 || position >= CAROUSEL_POSITIONS)
    throw Refusal("carousel position is outside 0..12");
  return order[position];
}

const char *body_code_of_model(int model) {
  // t3/research-player-model 7f13899, body table 0x4ebbb8.
  static const char *const codes[MODELS] = {"bm", "bf", "wem", "wef", "hem", "hef",
                                             "em", "ef", "dem", "def", "vm", "im"};
  if (model < 0 || model >= MODELS)
    throw Refusal("model index is outside 0..11");
  return codes[model];
}

const char *class_word(int class_index) {
  static const char *const words[CLASSES] = {"barbarian", "cleric", "shadowknight", "ranger",
                                             "wizard", "berserker", "shaman"};
  if (class_index < 0 || class_index >= CLASSES)
    throw Refusal("class index is outside 0..6");
  return words[class_index];
}

const char *screen_name(Screen s) {
  switch (s) {
  case Screen::BootCard: return "boot-card";
  case Screen::SoeLogo: return "soe-logo";
  case Screen::IntroMovie: return "intro-movie";
  case Screen::Main: return "main";
  case Screen::GameType: return "game-type";
  case Screen::Players: return "players";
  case Screen::Difficulty: return "difficulty";
  case Screen::Mode: return "mode";
  case Screen::Options: return "options";
  case Screen::OptionsLoad: return "options-load";
  case Screen::Credits: return "credits";
  case Screen::LoadGame: return "load-game";
  case Screen::SaveGame: return "save-game";
  case Screen::Saved: return "saved";
  case Screen::CharClass: return "char-class";
  case Screen::CharAppearance: return "char-appearance";
  case Screen::CharAbility: return "char-ability";
  case Screen::CharName: return "char-name";
  case Screen::Playing: return "playing";
  case Screen::Pause: return "pause";
  case Screen::ExitConfirm: return "exit-confirm";
  }
  return "unknown";
}

int screen_state(Screen s) {
  switch (s) {
  case Screen::Main: return 0;
  case Screen::GameType: return 1;
  case Screen::Players: return 2;
  case Screen::Difficulty: return 3;
  case Screen::Mode: return 4;
  case Screen::Credits: return 15;
  case Screen::CharClass: return 1;
  case Screen::CharAppearance: return 2;
  case Screen::CharAbility: return 3;
  case Screen::CharName: return 4;
  default: return -1;
  }
}

std::array<std::int32_t, 4> Character::stats() const {
  std::array<std::int32_t, 4> out{};
  for (int i = 0; i < 4; ++i)
    out[static_cast<std::size_t>(i)] = (base_known ? base[static_cast<std::size_t>(i)] : 0) +
                                       spent[static_cast<std::size_t>(i)];
  return out;
}

Flow::Flow(std::optional<std::vector<std::uint8_t>> options_file) {
  // save_init 0x216fcc -> config_setDefaults(-1): the image and 0x4f8944 are 0.5.
  image_ = ps2::config_set_defaults(-1);
  music_.set_defaults(-1);
  if (options_file) {
    try {
      (void)ps2::parse_config(*options_file, true);
    } catch (const std::exception &e) {
      throw Refusal(std::string("options file refused: ") + e.what());
    }
    // memcard_browser step 29, mode 1 at boot: the file is copied over the image.
    // The OR of the word at 0x88d3cc into image +0x00 is not modelled (its boot
    // value is not documented), so +0x00 stays as the file holds it.
    image_ = *options_file;
    music_.browser_copy(1, image_word(OPTIONS_MUSIC));
    file_present_ = true;
  }
}

std::uint32_t Flow::image_word(std::size_t at) const {
  return std::uint32_t(image_[at]) | std::uint32_t(image_[at + 1]) << 8 |
         std::uint32_t(image_[at + 2]) << 16 | std::uint32_t(image_[at + 3]) << 24;
}

void Flow::set_image_word(std::size_t at, std::uint32_t v) {
  for (int i = 0; i < 4; ++i)
    image_[at + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(v >> (8 * i));
}

void Flow::request(const std::string &kind, const std::string &arg, std::uint32_t value) {
  requests_.push_back(Request{kind, arg, value});
}

std::vector<Request> Flow::take_requests() {
  std::vector<Request> out;
  out.swap(requests_);
  return out;
}

// music_playGlobal 0x2110e0 queues only at or above the volume gate (docs/frontend-layout.md,
// "Menu sound and first theme"); the request keeps its original flags.
void Flow::music(const char *kind, const char *name, std::uint32_t flags) {
  if (!audio::music_gate_open(music_.live)) { // the one gate: native_audio.cpp
    ++gated_music_;
    return;
  }
  request(kind, name, flags);
}

void Flow::sound(menu_sound which) {
  request("menu_sound", menu_sound_resource(which));
}

void Flow::go(Screen next, int cursor) {
  screen_ = next;
  card_cache_supplied_ = false;
  card_metadata_.clear();
  card_headers_.clear();
  card_count_ = 0;
  card_scroll_base_ = 0;
  card_preview_index_ = -1;
  card_debug_time_ = false;
  cursor_ = cursor;
  frames_ = 0;
  slider_timer_ = 0;
  error_.clear();
}

void Flow::set_card_cache(std::vector<std::uint8_t> metadata,
                          std::vector<std::uint8_t> headers, int count, int scroll_base) {
  if (screen_ != Screen::LoadGame && screen_ != Screen::SaveGame)
    throw Refusal("card cache requires the save or load screen");
  (void)layout::visible_card_rows(metadata, headers, count, scroll_base,
                                  screen_ == Screen::SaveGame);
  card_metadata_ = std::move(metadata);
  card_headers_ = std::move(headers);
  card_count_ = count;
  card_scroll_base_ = scroll_base;
  card_cache_supplied_ = true;
  card_preview_index_ = -1;
  card_debug_time_ = false;
}

void Flow::set_card_preview(int header_index, bool debug_time) {
  if (!card_cache_supplied_)
    throw Refusal("selected card preview requires a supplied card cache");
  (void)layout::card_preview(card_headers_, card_count_, header_index);
  card_preview_index_ = header_index;
  card_debug_time_ = debug_time;
}

bool Flow::in_frontend() const {
  switch (screen_) {
  case Screen::Main: case Screen::GameType: case Screen::Players: case Screen::Difficulty:
  case Screen::Mode: case Screen::Credits: case Screen::CharClass: case Screen::CharAppearance:
  case Screen::CharAbility: case Screen::CharName:
    return true;
  case Screen::Options: case Screen::OptionsLoad:
    return options_return_ == Screen::Main;
  case Screen::LoadGame:
    return load_return_ == Screen::Main;
  default:
    return false;
  }
}

void Flow::set_net_game(bool net) {
  if (net)
    throw Refusal("net games are not run by this front end: the lobby path (states 6-10) is not part of this client");
  net_ = false;
}

void Flow::set_class_start_stats(const std::array<std::array<std::int32_t, 4>, CLASSES> &rows) {
  // t3/research-char-create b6dda4e, table 0x4ef400, both pinned seeds.
  static constexpr std::array<std::array<std::int32_t, 4>, CLASSES> observed{{
      {{35, 20, 25, 35}}, {{26, 36, 27, 26}}, {{28, 30, 27, 30}},
      {{28, 25, 35, 27}}, {{23, 39, 25, 28}}, {{32, 22, 31, 30}},
      {{26, 37, 24, 28}}}};
  if (rows != observed)
    throw Refusal("class start stats do not match the audited RTA US v2.00 table");
  start_stats_ = rows;
  start_known_ = true;
}

void Flow::set_name(const std::string &name) {
  if (name.size() >= static_cast<std::size_t>(NAME_BYTES))
    throw Refusal("name does not fit record +0x00 char[0x20] with its terminator");
  character_.name = name;
}

int Flow::ability_remaining() const {
  int spent = 0;
  for (auto v : character_.spent)
    spent += v;
  return ABILITY_POINTS - spent;
}

void Flow::enter_frontend(int code) {
  // frontend_start(code) clears g_paused; mainloop_runSession then runs
  // game_addCoreTasks, which copies image +0x08 into 0x4f8944 and applies it.
  exit_code_ = code;
  paused_ = false;
  load_from_frontend_ = 1;
  music_.add_core_tasks();
  request("apply_music_volume", {}, music_.live);
  // frontend_run 0x1e09b0/0x1e09c4 requests flags 1 or 5; the condition that
  // selects 5 is not documented, so this host always asks for 1 (see notes()).
  music("front_end_music", "mx_maintheme_l.vag", 1);
  options_return_ = Screen::Main;
  load_return_ = Screen::Main;
  go(Screen::Main);
}

void Flow::session_exit(int code) {
  int start = 0;
  if (code == 1)
    start = 0;
  else if (code == 0x65)
    start = 1;
  else if (code == 0xc9)
    start = 2;
  else
    throw Refusal("session exit code is not 1, 0x65 or 0xc9");
  character_ready_ = false;
  enter_frontend(start);
}

void Flow::move_cursor(std::uint32_t repeat) {
  const auto list = entries();
  if (list.empty())
    return;
  const int last = static_cast<int>(list.size()) - 1;
  int next = cursor_;
  if (repeat & UP)
    next = std::max(0, next - 1);
  if (repeat & DOWN)
    next = std::min(last, next + 1);
  if (next != cursor_) {
    cursor_ = next;
    slider_timer_ = 0; // the repeat timer resets only when the cursor moves
  }
}

void Flow::commit(bool query_only) {
  // config_commitOptions(force, queryOnly, noCardSave). force and noCardSave at
  // the pause and front-end call sites are not in the research; 0 is passed.
  const auto r = audio::config_commit_music(music_.live, music_.image, query_only, false, false);
  if (query_only)
    return;
  music_.image = r.image;
  set_image_word(OPTIONS_MUSIC, r.image);
  if (r.save_requested)
    request("write_options"); // memcard_requestConfigSave 0x217bdc, card state 5
}

void Flow::leave_options() {
  if (options_return_ == Screen::Pause) {
    commit(true);  // 0x18d180
    commit(false); // 0x18d2c8 / 0x18d2e0
    go(Screen::Pause, 3);
  } else {
    commit(false); // frontend_run's call; the state-14 prompt is not documented
    go(Screen::Main, 2);
  }
}

void Flow::options_frame(std::uint32_t held, std::uint32_t pressed, std::uint32_t repeat) {
  if (pressed & CANCEL) {
    leave_options();
    return;
  }
  if (pressed & TRIANGLE) {
    // memcard_requestOptions 0x222b10 after bit 0x10 of pad +0xc.
    go(Screen::OptionsLoad, 0);
    return;
  }
  move_cursor(repeat);
  if (cursor_ != 1)
    return;
  const auto before = music_.live;
  const auto call = audio::menu_slider_call(music_.live, held, slider_timer_);
  slider_timer_ = call.timer;
  music_.live = call.value;
  if (music_.live != before && !stream2_playing_)
    request("preview_music", options_return_ == Screen::Pause ? "mx_main_l.vag" : "mx_maintheme_l.vag", 0x41);
  request("apply_music_volume", {}, music_.live);
}

void Flow::finish_character() {
  // playerSel_finish 0x22a360: sets needPlayers and queues the hub level.
  character_ready_ = true;
  request("start_play", character_.level_name.empty() ? "hub" : character_.level_name);
  paused_ = false;
  go(Screen::Playing);
}

void Flow::frame(std::uint16_t buttons, std::uint8_t stick_x, std::uint8_t stick_y) {
  const netwire::ByteVector record{static_cast<std::uint8_t>(buttons & 0xff),
                                   static_cast<std::uint8_t>(buttons >> 8), stick_x, stick_y,
                                   0x80, 0x80, 0, 0};
  netwire::pad_apply(pad_, record, pad_prev_);
  const auto real_held = word_at(pad_, 8);
  auto held = real_held, pressed = word_at(pad_, 0xc), repeat = word_at(pad_, 0x10);
  if (lockout_ > 0)
    held = pressed = repeat = 0; // menus read a dummy pad while the lockout runs
  ++frames_;
  const auto list = entries();
  const bool on_enabled = cursor_ >= 0 && cursor_ < static_cast<int>(list.size()) &&
                          list[static_cast<std::size_t>(cursor_)].enabled;
  const std::string id = cursor_ >= 0 && cursor_ < static_cast<int>(list.size())
                             ? list[static_cast<std::size_t>(cursor_)].id
                             : std::string();
  const bool confirm = (pressed & CONFIRM) && on_enabled;

  switch (screen_) {
  case Screen::BootCard:
    if (confirm) {
      if (!file_present_) {
        // Blank card (Ran): the create question; yes writes the defaults image.
        file_present_ = true;
        request("write_options");
      }
      go(Screen::SoeLogo);
    }
    break;
  case Screen::SoeLogo:
    if (pressed) {
      skip_splash_ = true; // the press that ends the logo also skips the film
      enter_frontend(0);
    } else if (frames_ >= SOE_LOGO_FRAMES) {
      go(Screen::IntroMovie);
    }
    break;
  case Screen::IntroMovie:
    if (pressed && frames_ > INTRO_PRESS_AFTER)
      enter_frontend(0);
    break;
  case Screen::Main: {
    const int before = cursor_;
    move_cursor(repeat);
    if (cursor_ != before)
      sound(menu_sound::move); // an actual move looks up menu_main_toggle.anm (Executed, 0x1a03fc)
    if (confirm) {
      // main_accept: every accept first plays menu_main_accept.anm (0x1e15b8), then
      // row 0 -> state 1, row 1 -> memcard_requestLoad, row 2 -> options setup and
      // state 11, row 3 -> credits music and credits_load (state 15). Dispatch is by key.
      sound(menu_sound::select);
      if (id == "new_game") go(Screen::GameType);
      else if (id == "load_game") { load_return_ = Screen::Main; load_from_frontend_ = 1; go(Screen::LoadGame); }
      else if (id == "options") { options_return_ = Screen::Main; go(Screen::Options, 1); }
      else if (id == "credits") {
        music("credits_music", "mx_endcredits_ext_l.vag", 3); // 0x1e16f4, flags 3
        go(Screen::Credits);
      }
    }
    break;
  }
  case Screen::GameType:
    move_cursor(repeat);
    if (confirm && id == "normal") go(Screen::Players);
    else if (pressed & CANCEL) {
      sound(menu_sound::back); // cancel from the game-type list: menu_main_back.anm (Executed, 0x1e1d80)
      go(Screen::Main);
    }
    break;
  case Screen::Players:
    move_cursor(repeat);
    if (confirm && id == "one") { num_players_ = 1; go(Screen::Mode); }
    else if (pressed & CANCEL) go(Screen::GameType);
    break;
  case Screen::Mode:
    move_cursor(repeat);
    if (confirm && id == "missions") go(Screen::Difficulty);
    else if (pressed & CANCEL) go(Screen::Players);
    break;
  case Screen::Difficulty:
    move_cursor(repeat);
    if (confirm && id == "easy") {
      difficulty_ = 1;
      // frontend_startPlayerSelect 0x1e3ba0 clears g_paused and opens selection.
      paused_ = false;
      character_ = Character{};
      character_ready_ = false;
      go(Screen::CharClass);
    } else if (pressed & CANCEL) {
      go(Screen::Mode);
    }
    break;
  case Screen::Options:
    options_frame(held, pressed, repeat);
    break;
  case Screen::OptionsLoad:
    move_cursor(repeat);
    if (confirm && id == "choice1") go(Screen::Options, 1);
    else if (confirm && id == "choice2") request("load_options");
    else if (pressed & CANCEL) go(Screen::Options, 1);
    break;
  case Screen::Credits:
    if (pressed & CANCEL) {
      // credits_exit (CREDITS_EXIT_FINISHED): the front-end theme is requested again
      // with flags 1 (0x1e114c), then state 0. The exit input is host behaviour: what
      // ends credits_run is Unknown. The 0x211338(2) call before it is not modelled.
      music("front_end_music", "mx_maintheme_l.vag", 1);
      go(Screen::Main, 3);
    }
    break;
  case Screen::LoadGame:
    move_cursor(repeat);
    if (confirm && id == "session") request("load_session");
    else if (confirm && id == "ps2") { import_context_ = "load"; request("import_ps2", "load"); }
    else if (pressed & CANCEL) go(load_return_, load_return_ == Screen::Pause ? 2 : 1);
    break;
  case Screen::SaveGame:
    if (confirm && id == "session") {
      save_latch_ = 1; // choosing a slot stores latch 0x4f8f28 = 1 (Executed)
      request("save_session");
    } else if (pressed & CANCEL) {
      paused_ = card_paused_;
      request("pause", {}, paused_ ? 1 : 0);
      go(Screen::Playing);
    }
    break;
  case Screen::Saved:
    if (pressed & CONFIRM) {
      save_latch_ = 0; // sub-state 4 stores 0 when the "saved" box closes
      paused_ = card_paused_;
      request("pause", {}, paused_ ? 1 : 0);
      go(Screen::Playing);
    }
    break;
  case Screen::CharClass:
    move_cursor(repeat);
    if (pressed & TRIANGLE) {
      import_context_ = "char"; // page 1 triangle: Import, card state 9
      request("import_ps2", "char");
    } else if (confirm) {
      // No menu sound here: PLAYERSEL_SOUNDS belongs to phase 1 (import selection),
      // and class selection has no phase of its own (REFUSED, docs/FRONTEND.md).
      character_ = Character{};
      selected_carousel_position_ = cursor_;
      character_.model = carousel_model(cursor_);
      character_.class_index = class_of_model(character_.model);
      if (start_known_) {
        character_.base = start_stats_[static_cast<std::size_t>(character_.class_index)];
        character_.base_known = true;
      }
      go(Screen::CharAppearance);
    } else if (pressed & CANCEL) {
      go(Screen::Difficulty);
    }
    break;
  case Screen::CharAppearance:
    move_cursor(repeat);
    if (confirm && cursor_ == 0) go(Screen::CharAbility);
    else if (pressed & CONFIRM) error_ = "appearance choice refused: reachable value is Unknown";
    else if (pressed & CANCEL) go(Screen::CharClass, selected_carousel_position_);
    break;
  case Screen::CharAbility:
    move_cursor(repeat);
    if ((repeat & RIGHT) && ability_remaining() > 0)
      ++character_.spent[static_cast<std::size_t>(cursor_)];
    if (pressed & CONFIRM) {
      if (ability_remaining() == 0) {
        // The phase-3 sound sites have no documented trigger condition.
        go(Screen::CharName);
      } else {
        error_ = "20 ability points must be spent before the screen can be left";
      }
    } else if (pressed & CANCEL) {
      go(Screen::CharAppearance);
    }
    break;
  case Screen::CharName:
    move_cursor(repeat);
    if (confirm && id == "done") {
      if (character_.name.empty()) error_ = "the name is empty";
      else finish_character();
    } else if (pressed & CANCEL) {
      go(Screen::CharAbility);
    }
    break;
  case Screen::Playing:
    if (pressed & START) {
      // warpMenu 0x18e0e4 (offline): with a dialog open the start button cannot open
      // the menu. The client's one camera is the follow camera, camera mode 1, the
      // only mode in which the site lets it open (docs/spec/dialog-gaps.md 3).
      if (!net_ && champions::simulation::rta::dialog_gaps::site_outcome("warp_menu", {{"dialog", dialog_open_ ? 1 : 0}, {"cam_mode", 1}}) ==
                       "blocked")
        break;
      // pauseMenu_open 0x18e334: g_paused only offline; menu-state message 2.
      paused_ = !net_;
      request("pause", {}, 1);
      go(Screen::Pause);
    }
    break;
  case Screen::Pause:
    move_cursor(repeat);
    if (confirm) {
      if (id == "return") {
        commit(false); // warpMenu at 0x18e2f8 when the menu closes
        paused_ = false;
        request("pause", {}, 0);
        go(Screen::Playing);
      } else if (id == "gate") {
        request("recall"); // hudmsg_sendRecall 0x1de138
      } else if (id == "load") {
        load_return_ = Screen::Pause;
        load_from_frontend_ = 0;
        go(Screen::LoadGame);
      } else if (id == "options") {
        options_return_ = Screen::Pause;
        go(Screen::Options, 1);
      } else if (id == "exit") {
        go(Screen::ExitConfirm, 1); // the prompt opens with the cursor on no
      }
    } else if (pressed & (START | CANCEL)) {
      commit(false);
      paused_ = false;
      request("pause", {}, 0);
      go(Screen::Playing);
    }
    break;
  case Screen::ExitConfirm:
    if (pressed & LEFT) cursor_ = 0;
    if (pressed & RIGHT) cursor_ = 1;
    if (pressed & CONFIRM) {
      if (cursor_ == 0) {
        // hudmsg_sendExitCountdown: countdown 1 offline. The session exit code
        // it leads to is not documented; the host reports it via session_exit.
        request("exit_session");
      } else {
        go(Screen::Pause, 4);
      }
    } else if (pressed & CANCEL) {
      go(Screen::Pause, 4);
    }
    break;
  }

  // tasks_run: a positive lockout with the hold flag clear becomes 0 when no pad
  // holds a button, else it is decremented.
  if (lockout_ > 0 && !hold_flag_) {
    if ((real_held & LOCKOUT_HELD_MASK) == 0) lockout_ = 0;
    else --lockout_;
  }
}

void Flow::import_result(bool ok, const Character &c, const std::string &error) {
  if (screen_ != Screen::CharClass && screen_ != Screen::LoadGame)
    throw Refusal("no import was requested");
  if (!ok) {
    error_ = error.empty() ? "import refused" : error;
    return;
  }
  character_ = c;
  character_.imported = true;
  if (import_context_ == "char")
    character_.level_name.clear(); // page 1 import continues to the hub
  finish_character();
}

void Flow::load_result(bool ok, const std::string &error) {
  if (screen_ != Screen::LoadGame)
    throw Refusal("no load was requested");
  if (!ok) {
    error_ = error.empty() ? "load refused" : error;
    return;
  }
  paused_ = false;
  request("pause", {}, 0);
  go(Screen::Playing);
}

void Flow::options_loaded(std::optional<std::vector<std::uint8_t>> file) {
  if (screen_ != Screen::OptionsLoad)
    throw Refusal("no options load was requested");
  if (!file) {
    go(Screen::Options, 1); // browser -1: setState(-1, 0)
    return;
  }
  try {
    (void)ps2::parse_config(*file, true);
  } catch (const std::exception &e) {
    error_ = std::string("options file refused: ") + e.what();
    return;
  }
  // memcard_optionsScreen state 2: the 17 scalars (+0x04..+0x33, +0xc4..+0xd7)
  // go to the image and the globals; button rows and +0x00 are not copied.
  for (std::size_t at = 0x04; at < 0x34; ++at)
    image_[at] = (*file)[at];
  for (std::size_t at = 0xc4; at < 0xd8; ++at)
    image_[at] = (*file)[at];
  music_.options_screen(2, image_word(OPTIONS_MUSIC));
  request("apply_music_volume", {}, music_.live);
  go(Screen::Options, 1);
}

void Flow::attach_play() {
  paused_ = false;
  go(Screen::Playing);
}

void Flow::request_save() {
  if (screen_ != Screen::Playing)
    throw Refusal("a save screen is opened from play only");
  // A card screen offline pushes and sets g_paused (memcard_pushPause 0x21e850).
  card_paused_ = paused_;
  paused_ = true;
  request("pause", {}, 1);
  go(Screen::SaveGame);
}

void Flow::save_result(bool ok, const std::string &error) {
  if (screen_ != Screen::SaveGame)
    throw Refusal("no save was requested");
  if (!ok) {
    // A card error goes to sub-state 9 and leaves the latch at 1 (Executed).
    error_ = error.empty() ? "save failed" : error;
    return;
  }
  go(Screen::Saved);
}

void Flow::return_to_frontend() {
  if (screen_ != Screen::ExitConfirm)
    throw Refusal("no exit was requested");
  character_ready_ = false;
  enter_frontend(0);
}

void Flow::recall_executed() {
  // recall_execute closes the pause menu, sets g_paused 0 and the lockout to 60.
  if (screen_ == Screen::Pause || screen_ == Screen::ExitConfirm || screen_ == Screen::Options)
    go(Screen::Playing);
  paused_ = false;
  lockout_ = RECALL_LOCKOUT;
  request("pause", {}, 0);
}

std::vector<Entry> Flow::entries() const {
  std::vector<Entry> out;
  switch (screen_) {
  case Screen::BootCard:
    out.push_back(file_present_
                      ? entry("continue", "Continue (options file read)", "memcard_browser step 29, mode 1 (Executed)")
                      : entry("create", "Create save data", "blank card: the create question, yes (Ran, live-rig 4a)"));
    break;
  case Screen::Main:
    out.push_back(entry("new_game", "New Game", "menu_startnewgame, first entry of table 0x4f6b88 (Ran)"));
    out.push_back(entry("load_game", "Load Saved Game", "seen on the title (Ran); table position not documented"));
    out.push_back(entry("options", "Options", "seen on the title (Ran); list 0x4f6e88; table position not documented"));
    out.push_back(entry("credits", "Credits", "seen on the title (Ran); table position not documented"));
    break;
  case Screen::GameType:
    out.push_back(entry("normal", "Normal Game", "first entry of table 0x4f6bf8 (Ran)"));
    out.push_back(entry("internet", "Internet Game", "second entry, menu_netgame (Ran)", false,
                        "the online lobby is not part of this client"));
    break;
  case Screen::Players:
    out.push_back(entry("one", "One player", "first entry of table 0x4f6de8 reaches state 4 (Ran)"));
    for (int i = 1; i < 4; ++i) // the table has four rows (Executed); their text and effect are not recorded
      out.push_back(entry("row", "(row " + std::to_string(i) + ")", "row of table 0x4f6de8; text and effect not documented", false,
                          "this row's text and effect are not documented"));
    break;
  case Screen::Mode:
    out.push_back(entry("missions", "Mission game", "first entry of table 0x4f6c68 (Ran)"));
    out.push_back(entry("row", "(row 1)", "second row of table 0x4f6c68; text and effect not documented", false,
                        "this row's text and effect are not documented"));
    break;
  case Screen::Difficulty:
    out.push_back(entry("easy", "Easy", "menu_easy2, first of table 0x4f6e38 (Read); g_difficultyIdx 1"));
    for (auto [id, label, key] : {std::tuple{"normal", "Normal", "menu_normal2"},
                                  std::tuple{"hard", "Hard", "menu_hard2"},
                                  std::tuple{"harder", "Harder", "menu_harder2"}})
      out.push_back(entry(id, label, key, false,
                          "unlock test options_get4f8f38Level 0x1ec448 was not executed"));
    break;
  case Screen::Options: {
    if (!in_frontend()) {
      // The in-game table 0x44f218 was not captured. Keep only host controls
      // whose behaviour this flow implements; do not borrow the 13-row table.
      out.push_back(entry("entry0", "(entry 0)", "pause option row unknown", false,
                          "the in-game option table was not captured"));
      auto music = entry("music", "Music", "host pause music control");
      music.type = 4;
      music.value = music_.live;
      out.push_back(music);
      break;
    }
    out.push_back(entry("entry0", "(entry 0)", "list entry 0 is not documented", false,
                        "entry order is documented only for music at index 1"));
    auto music = entry("music", "Music", "menu_music, index 1, type 4 float slider (Executed)");
    music.type = 4;
    music.value = music_.live;
    out.push_back(music);
    for (int i = 2; i < 13; ++i) // thirteen rows in table 0x4f6e88 (Executed)
      out.push_back(entry("entry", "(entry " + std::to_string(i) + ")", "row of the options table; text and effect not documented", false,
                          "this option's order and type are not documented"));
    break;
  }
  case Screen::OptionsLoad:
    out.push_back(entry("choice1", "Cancel (choice 1)", "options_warn box: choice 1 -> setState(-1) (Executed)"));
    out.push_back(entry("choice2", "Load options (choice 2)", "choice 2 -> browser -> state 2 copy (Executed)"));
    break;
  case Screen::LoadGame:
    out.push_back(entry("session", "Load native session save", "host save file (.ocplay/.ocnsave); the card browser is not documented"));
    out.push_back(entry("ps2", "Import PS2 character", "ps2_save import of one RTA US v2.00 slot (save_load, Executed)"));
    break;
  case Screen::SaveGame:
    out.push_back(entry("session", "Save native session", "host save file; memcard_saveScreen latch 0x4f8f28 (Executed)"));
    break;
  case Screen::Saved:
    out.push_back(entry("ok", "Saved", "the \"saved\" box of memcard_saveScreen sub-state 4 (Executed)"));
    break;
  case Screen::CharClass:
    for (int p = 0; p < CAROUSEL_POSITIONS; ++p) {
      const int m = carousel_model(p);
      const int c = class_of_model(m);
      out.push_back(entry("model", "Model " + std::to_string(m) + " - " + class_word(c),
                          "carousel table 0x4fbc00 +8/+9 (Observed); class word Inferred"));
    }
    break;
  case Screen::CharAppearance:
    out.push_back(entry("continue", "Continue (default route)", "one default route Ran; menu choices Unknown"));
    for (const char *field : {"Hair style", "Hair/beard colour", "Beard style", "Body skin variant"})
      out.push_back(entry("unknown", std::string(field) + " (Unknown)", "record appearance field Observed; menu choice Unknown", false,
                          "reachable choice and selected value are not established"));
    break;
  case Screen::CharAbility: {
    const auto s = character_.stats();
    for (int i = 0; i < 4; ++i) {
      auto e = entry("stat", std::string(STAT_ROWS[i]) + " " +
                                 (character_.base_known ? std::to_string(s[static_cast<std::size_t>(i)])
                                                        : "+" + std::to_string(character_.spent[static_cast<std::size_t>(i)])),
                     "record +0x78 row order (Ran)");
      e.value = static_cast<std::uint32_t>(s[static_cast<std::size_t>(i)]);
      out.push_back(e);
    }
    break;
  }
  case Screen::CharName:
    out.push_back(entry("name", "Name: " + character_.name, "record +0x00 char[0x20] (Observed)"));
    out.push_back(entry("done", "Done", "the done key of the name grid (Ran)"));
    break;
  case Screen::Pause:
    out.push_back(entry("return", "Return to Game", "table 0x44f3b8 (Ran, native-online)"));
    out.push_back(entry("gate", "Use Gate Scroll", "hudmsg_sendRecall 0x1de138 (Observed)",
                        champions::simulation::rta::dialog_gaps::site_outcome("pause_recall", {{"dialog", dialog_open_ ? 1 : 0},
                                                                    {"cam_mode", 1}, {"network", net_ ? 1 : 0}}) ==
                            "enabled",
                        "not selectable while a conversation is open (menu_recall +4 = 5, 0x18d400)"));
    out.push_back(entry("load", "Load Game", "opens the load screen offline (Ran, native-app)", !net_,
                        "greyed in a net game"));
    out.push_back(entry("options", "Options...", "pause option list 0x44f218"));
    out.push_back(entry("exit", "Exit Game", "Exit Game? Yes / No (Ran)",
                        champions::simulation::rta::dialog_gaps::site_outcome("pause_exit", {{"dialog", dialog_open_ ? 1 : 0}}) != "disabled",
                        "not selectable while a conversation is open (menu_exitmission +4 = 5, 0x18cd08)"));
    break;
  case Screen::ExitConfirm:
    out.push_back(entry("yes", "Yes", "left then cross confirms (Ran, live-rig 21.7)"));
    out.push_back(entry("no", "No", "the prompt opens on no (Ran)"));
    break;
  case Screen::SoeLogo: case Screen::IntroMovie: case Screen::Credits: case Screen::Playing:
    break;
  }
  return out;
}

std::vector<std::string> Flow::notes() const {
  std::vector<std::string> n;
  switch (screen_) {
  case Screen::BootCard:
    n.push_back("Boot card check (mainloop_runBootCardCheck 0x32396c). The card browser's screens and slot prompt are not documented; a host file stands in for the options file, whose card name is unknown.");
    n.push_back("The OR of the word at 0x88d3cc into image +0x00 at browser step 29 is not modelled.");
    break;
  case Screen::SoeLogo:
    n.push_back("Publisher logo: done after 240 frames or on a press, which sets g_skipSplash and skips the film. Its picture is not drawn by this client.");
    break;
  case Screen::IntroMovie:
    n.push_back("Intro film: MPEG-2 640x448 at 29.97 is not decoded by this client, so movie_isFinished never holds; a press after 30 frames ends it. porko_l.vag is queued held on stream 0 and released only when the movie is ready, so nothing plays.");
    break;
  case Screen::Main:
    n.push_back("The first theme is requested with flags 1; the research lists flags 1 or 5 at 0x1e09b0/0x1e09c4 and does not document the condition that selects 5.");
    n.push_back("Row destinations are Executed (main_accept: game type, card load, options, credits with its music), dispatched by key. The row texts are the on-screen texts seen (Ran), host-authored: their lookup keys are not recorded.");
    n.push_back("Ran: the main menu takes input about 240 frames after the front end starts, and a choice in its first second can find the character screens' data unloaded. This host does not wait.");
    break;
  case Screen::GameType: case Screen::Players: case Screen::Mode:
    n.push_back("The table's row count is Executed; the text and effect of its other rows are not recorded, so they are drawn dim and refused.");
    n.push_back("g_pvpModeA, g_pvpModeB, 0x4f7124 and the kill limit are written by the offline setup, but their values per choice are not documented; this flow leaves them at main's zero.");
    break;
  case Screen::Difficulty:
    n.push_back("Ran: above the first difficulty the character screens take only an imported character.");
    break;
  case Screen::Options:
    n.push_back("Only the music slider is modelled (1/120 steps, clamped 0..1, timer 0x3e4fdef8: four idle calls after the cursor lands). Other names in both lists: brightness, effects, stereo, controls, combat camera, reverse yaw, vibration, blood, subtitles, damage display, chat, markers, voice; their order and types are not documented.");
    n.push_back("Frames whose auto-repeat word carries left or right are stepped like the executed no-edge calls (Inferred).");
    n.push_back("Triangle opens Load options (card state 10). Circle leaves: config_commitOptions copies the live music into the image and requests a config save when it differs.");
    if (options_return_ == Screen::Main)
      n.push_back("Front end state 14 'save config?' prompt is not documented and is not shown.");
    else
      n.push_back("Pause list: the controls entry is disabled only in a net game.");
    break;
  case Screen::OptionsLoad:
    n.push_back("memcard_optionsScreen state 0 message box 'options_warn' (0x3C, -1, 0, 0). Which on-screen choice is 1 and which is 2 is not documented.");
    break;
  case Screen::Credits:
    n.push_back("credits_load 0x223c64 loads credits.lmp and credits_run 0x223d3c scrolls it (medium font, layer 7, shadow pass +2); the data layout is Unknown, so no line is drawn. Circle returns and requests the front-end theme again with flags 1 (credits_exit); what ends credits_run is Unknown.");
    break;
  case Screen::LoadGame:
    n.push_back("memcard_requestLoad 0x2176e0 stores g_inFrontend in 0x8e1ab8. The card browser, slot list and its frames are not documented; the host opens a file chooser.");
    n.push_back("A PS2 import loads one character into native state; level bodies, decals and the camera load stay refused (docs/PS2_SAVE_IMPORT.md).");
    break;
  case Screen::SaveGame: case Screen::Saved:
    n.push_back("The normal pause menu has no save entry; a save screen opens from a savepoint, script saveGame or the host. A card error leaves latch 0x4f8f28 at 1.");
    break;
  case Screen::CharClass:
    n.push_back("The observed 13-position carousel order is 0,11,10,0..9. Exact on-screen focus movement and preview camera remain Unknown. Triangle imports a PS2 character (page 8 sheet).");
    n.push_back("Refused: class selection has no phase of its own in the research (phase 1 is import selection), so it plays no menu sound and draws no phase-1 text; the page footer is Host.");
    if (!start_known_)
      n.push_back("Class start stats g_classStartStats 0x4ef400 are read from the user's executable; none is loaded, so rows show spent points only.");
    break;
  case Screen::CharAppearance:
    n.push_back("Appearance bytes record +0x20 s8[4] (-1 = none): reachable choices, selected values and handoff are Unknown. The host permits the one default-route continuation without claiming appearance parity.");
    break;
  case Screen::CharAbility:
    n.push_back("Right adds one point to the row (Ran). Removing a point is not documented. 20 points must be spent before leaving.");
    break;
  case Screen::CharName:
    n.push_back("The initialized name grid has 10 columns and 64 glyphs: 26 lowercase letters, ten digits and 28 unidentified high-bit glyphs (Executed). Editing, case toggle, trim and DONE branches are Unknown; the host keyboard types under the record's 31-byte limit.");
    n.push_back("The full menu confirm/cancel handoff, character kind, first object-present recalc and finish transition remain unverified. The host creates only the documented fields and marks appearance and skill points Unknown.");
    n.push_back("Starting gear (player_giveStartingItems, 323 descriptor calls by class and model, no table) is refused. Skill points at creation are not documented; 0 is kept.");
    break;
  case Screen::Playing:
    n.push_back("Start opens the pause menu (Ran).");
    break;
  case Screen::Pause:
    n.push_back("Pause list: medium font, layer 7, colour 0x80464646, five rows from y=179 and the three frame calls (Executed). Kick Player is a net-game host entry and is hidden offline.");
    n.push_back("Closing with start or circle is host behaviour; warpMenu commits the options when the menu closes (0x18e2f8).");
    break;
  case Screen::ExitConfirm:
    n.push_back("Yes sends hudmsg_sendExitCountdown (countdown 1 offline). The exit code that leads back to the front end is not documented.");
    break;
  }
  for (const auto &u : layout().unknown)
    n.push_back("Unknown: " + u);
  n.push_back(SOUND_GAP);
  if (!entries().empty()) {
    n.push_back(CURSOR_GAP);
    n.push_back(BACK_GAP);
  }
  n.push_back(FADE_GAP);
  if (!error_.empty())
    n.push_back("Refused: " + error_);
  return n;
}

} // namespace champions::frontend
