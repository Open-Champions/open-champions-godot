#include "champions/frontend.hpp"
#include "champions/menus.hpp"
#include "champions/ps2_save.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace champions::frontend;
namespace audio = champions::audio;
namespace ps2 = champions::simulation::rta::ps2_save;

namespace {
int checks = 0;

void expect(bool ok, const char *name) {
  ++checks;
  if (!ok)
    throw std::runtime_error(name);
}

template <class Fn> void reject(Fn fn, const char *name) {
  bool threw = false;
  try {
    fn();
  } catch (const Refusal &) {
    threw = true;
  } catch (const champions::menus::Refusal &) {
    threw = true;
  }
  expect(threw, name);
}

std::vector<std::uint8_t> options_with_music(std::uint32_t bits) {
  auto image = ps2::config_set_defaults(-1);
  for (int i = 0; i < 4; ++i)
    image[OPTIONS_MUSIC + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(bits >> (8 * i));
  return image;
}

std::uint32_t word(const std::vector<std::uint8_t> &b, std::size_t at) {
  return std::uint32_t(b[at]) | std::uint32_t(b[at + 1]) << 8 | std::uint32_t(b[at + 2]) << 16 |
         std::uint32_t(b[at + 3]) << 24;
}

bool has(const std::vector<Request> &rs, const std::string &kind) {
  for (const auto &r : rs)
    if (r.kind == kind)
      return true;
  return false;
}

// Press and release one button: two frames.
void tap(Flow &f, std::uint16_t bits) {
  f.frame(bits);
  f.frame(0);
}

// From power-on with the given options file to the main menu, skipping the logo.
Flow to_main(std::optional<std::vector<std::uint8_t>> file = std::nullopt) {
  Flow f(std::move(file));
  tap(f, CROSS);  // boot card
  tap(f, CROSS);  // logo press: g_skipSplash
  (void)f.take_requests();
  return f;
}

void class_vectors() {
  const int expected[12] = {0, 0, 3, 3, 1, 1, 4, 4, 2, 2, 5, 6};
  for (int m = 0; m < 12; ++m)
    expect(class_of_model(m) == expected[m], "model to class");
  reject([] { (void)class_of_model(12); }, "model 12 refused");
  reject([] { (void)class_of_model(-1); }, "model -1 refused");
  expect(std::string(class_word(6)) == "shaman", "class word 6");
}

void boot_vectors() {
  {
    Flow f;
    expect(f.screen() == Screen::BootCard, "boot starts at the card check");
    expect(f.music().live == audio::music_volume_default, "save_init live 0.5");
    expect(f.music().image == audio::music_volume_default, "save_init image 0.5");
    expect(word(f.options_image(), OPTIONS_MUSIC) == 0x3f000000, "defaults image +0x08");
    expect(!f.options_file_present(), "blank card");
    expect(f.entries().at(0).id == "create", "blank card asks to create");
    tap(f, CROSS);
    expect(has(f.take_requests(), "write_options"), "create writes the options image");
    expect(f.screen() == Screen::SoeLogo, "logo after card");
    // The release frame of the tap is the logo's first frame.
    for (int i = 0; i < SOE_LOGO_FRAMES - 2; ++i)
      f.frame(0);
    expect(f.screen() == Screen::SoeLogo, "logo holds 239 frames");
    f.frame(0);
    expect(f.screen() == Screen::IntroMovie, "film after 240 frames");
    tap(f, CROSS);
    expect(f.screen() == Screen::IntroMovie, "press inside 30 frames ignored");
    for (int i = 0; i < 40; ++i)
      f.frame(0);
    f.frame(CROSS);
    expect(f.screen() == Screen::Main, "press after 30 frames ends the film");
    expect(!f.skip_splash(), "no g_skipSplash without a logo press");
    const auto rs = f.take_requests();
    expect(has(rs, "front_end_music") && has(rs, "apply_music_volume"), "front end music and apply");
  }
  {
    // A file's +0x08 is copied bit for bit, unclamped, and reaches the live word
    // only at game_addCoreTasks.
    Flow f(options_with_music(0x3fc00000));
    expect(f.options_file_present(), "file read");
    expect(f.music().image == 0x3fc00000, "browser copy 1.5 unclamped");
    expect(f.music().live == audio::music_volume_default, "live still 0.5 before the session");
    tap(f, CROSS);
    expect(f.take_requests().empty(), "a read file is not rewritten");
    f.frame(START);
    expect(f.skip_splash() && f.screen() == Screen::Main, "logo press skips the film");
    expect(f.music().live == 0x3fc00000, "game_addCoreTasks copies the image");
  }
  reject([] { Flow f(std::vector<std::uint8_t>(10, 0)); }, "short options file refused");
}

void route_vectors() {
  auto f = to_main();
  std::vector<int> states{screen_state(f.screen())};
  tap(f, CROSS);
  states.push_back(screen_state(f.screen()));
  tap(f, CROSS);
  states.push_back(screen_state(f.screen()));
  tap(f, CROSS);
  states.push_back(screen_state(f.screen()));
  tap(f, CROSS);
  states.push_back(screen_state(f.screen()));
  expect((states == std::vector<int>{0, 1, 2, 4, 3}), "Ran state order 0 1 2 4 3");
  expect(f.num_players() == 1, "one player");
  // Disabled difficulties do not confirm.
  tap(f, DOWN);
  tap(f, CROSS);
  expect(f.screen() == Screen::Difficulty, "normal is refused");
  tap(f, UP);
  tap(f, CROSS);
  expect(f.screen() == Screen::CharClass && f.difficulty_index() == 1, "easy -> selection, index 1");
  // Back navigation (host behaviour) walks the menus in reverse.
  tap(f, CIRCLE);
  expect(f.screen() == Screen::Difficulty, "back to difficulty");
  tap(f, CIRCLE);
  tap(f, CIRCLE);
  tap(f, CIRCLE);
  tap(f, CIRCLE);
  expect(f.screen() == Screen::Main, "back to main");
  // Internet Game is refused.
  tap(f, CROSS);
  tap(f, DOWN);
  tap(f, CROSS);
  expect(f.screen() == Screen::GameType, "internet refused");
  reject([&] { f.set_net_game(true); }, "net game refused");
}

void repeat_vectors() {
  auto f = to_main();
  // pad_apply: the press moves once; held, the auto-repeat word carries the
  // button on every 4th call once the counter passes 20 (calls 25, 29, ...).
  for (int i = 0; i < 24; ++i)
    f.frame(DOWN);
  expect(f.cursor() == 1, "one move in 24 held frames");
  f.frame(DOWN);
  expect(f.cursor() == 2, "auto-repeat on frame 25");
  for (int i = 0; i < 3; ++i)
    f.frame(DOWN);
  expect(f.cursor() == 2, "no repeat on 26-28");
  f.frame(DOWN);
  expect(f.cursor() == 3, "auto-repeat on frame 29");
  for (int i = 0; i < 20; ++i)
    f.frame(DOWN);
  expect(f.cursor() == 3, "clamped at the last entry");
}

void slider_vectors() {
  auto f = to_main();
  tap(f, DOWN);
  tap(f, DOWN);
  tap(f, CROSS);
  expect(f.screen() == Screen::Options && f.cursor() == 1, "options on the music entry");
  expect(f.entries().at(1).type == 4, "music is type 4");
  (void)f.take_requests();
  // Hold right: every frame is one menuList_handleInput call. Compare with the
  // executed slider model applied to the same held words.
  // The release frame after entering the list was already one call (no held bits).
  std::uint32_t value = f.music().live, timer = audio::menu_slider_call(value, 0, 0).timer;
  expect(f.slider_timer() == timer, "release frame advanced the timer");
  int previews = 0;
  for (int i = 0; i < 80; ++i) {
    f.frame(RIGHT);
    const auto call = audio::menu_slider_call(value, RIGHT, timer);
    value = call.value;
    timer = call.timer;
    expect(f.music().live == value, "slider walk matches the executed model");
    const auto rs = f.take_requests();
    expect(has(rs, "apply_music_volume"), "apply on every call");
    for (const auto &r : rs)
      if (r.kind == "preview_music") {
        ++previews;
        expect(r.arg == "mx_maintheme_l.vag" && r.value == 0x41, "front-end preview");
      }
    if (i < 3)
      expect(f.music().live == audio::music_volume_default, "four idle calls in all");
  }
  expect(f.music().live == 0x3f800000, "clamped at 1.0");
  expect(previews == 61, "61 steps from 0.5 reach 1.0");
  // Stream 2 playing: no preview.
  f.set_stream2_playing(true);
  for (int i = 0; i < 130; ++i)
    f.frame(LEFT);
  expect(!has(f.take_requests(), "preview_music"), "no preview while stream 2 plays");
  expect(f.music().live == 0, "held left from 1.0 reaches 0");
  // Cursor move resets the timer.
  tap(f, UP);
  expect(f.slider_timer() == 0 && f.cursor() == 0, "move resets timer");
  tap(f, DOWN);
  // Leave: commit copies live into the image and asks for a config save.
  tap(f, CIRCLE);
  expect(f.screen() == Screen::Main, "options left");
  expect(f.music().image == 0 && word(f.options_image(), OPTIONS_MUSIC) == 0, "commit into image");
  expect(has(f.take_requests(), "write_options"), "config save requested");
  // Unchanged: no save.
  tap(f, CROSS);
  tap(f, CIRCLE);
  expect(!has(f.take_requests(), "write_options"), "no save when unchanged");
}

void options_load_vectors() {
  auto f = to_main();
  tap(f, DOWN);
  tap(f, DOWN);
  tap(f, CROSS);
  tap(f, TRIANGLE);
  expect(f.screen() == Screen::OptionsLoad, "triangle opens card state 10");
  tap(f, CROSS);
  expect(f.screen() == Screen::Options, "choice 1 cancels");
  tap(f, TRIANGLE);
  tap(f, DOWN);
  tap(f, CROSS);
  expect(has(f.take_requests(), "load_options"), "choice 2 asks for the file");
  auto file = options_with_music(0x3e800000);
  file[0] = 0x55;   // +0x00 is not copied
  file[0x34] = 0x7; // button rows are not copied
  file[0x24] = 1;   // blood scalar is copied
  file[0xc8] = 0x9; // voice scalar is copied
  const auto before = f.options_image();
  f.options_loaded(file);
  expect(f.screen() == Screen::Options, "back on the list");
  expect(f.music().live == 0x3e800000 && f.music().image == 0x3e800000, "state 2 music");
  expect(f.options_image()[0] == before[0] && f.options_image()[0x34] == before[0x34], "+0x00 and rows kept");
  expect(f.options_image()[0x24] == 1 && f.options_image()[0xc8] == 9, "17 scalars copied");
  tap(f, TRIANGLE);
  tap(f, DOWN);
  tap(f, CROSS);
  f.options_loaded(std::nullopt);
  expect(f.screen() == Screen::Options, "no file: browser -1");
  reject([&] { f.options_loaded(file); }, "unrequested load refused");
}

void character_vectors() {
  auto f = to_main();
  for (int i = 0; i < 5; ++i)
    tap(f, CROSS);
  expect(f.screen() == Screen::CharClass, "at class page");
  expect(f.entries().size() == 13, "thirteen carousel positions");
  const std::array<int, CAROUSEL_POSITIONS> order{{0, 11, 10, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9}};
  const std::array<int, CAROUSEL_POSITIONS> classes{{0, 6, 5, 0, 0, 3, 3, 1, 1, 4, 4, 2, 2}};
  for (int p = 0; p < CAROUSEL_POSITIONS; ++p)
    expect(carousel_model(p) == order[static_cast<std::size_t>(p)] &&
           class_of_model(carousel_model(p)) == classes[static_cast<std::size_t>(p)] &&
           f.entries()[static_cast<std::size_t>(p)].label.find("Model " + std::to_string(order[static_cast<std::size_t>(p)])) == 0,
           "audited carousel row");
  expect(std::string(body_code_of_model(11)) == "im", "model 11 body resource code");
  reject([&] { (void)carousel_model(13); }, "out-of-range carousel position refused");
  const std::array<std::array<std::int32_t, 4>, CLASSES> rows{{
      {{35, 20, 25, 35}}, {{26, 36, 27, 26}}, {{28, 30, 27, 30}},
      {{28, 25, 35, 27}}, {{23, 39, 25, 28}}, {{32, 22, 31, 30}},
      {{26, 37, 24, 28}}}};
  auto wrong = rows;
  wrong[6][0] = 27;
  reject([&] { f.set_class_start_stats(wrong); }, "unmatched stat table refused");
  f.set_class_start_stats(rows);
  for (int i = 0; i < 3; ++i) tap(f, DOWN);
  tap(f, CROSS);
  expect(f.character().model == 0, "duplicated carousel position selects model 0");
  tap(f, CIRCLE);
  expect(f.screen() == Screen::CharClass && f.cursor() == 3,
         "cancel returns to the selected duplicate position");
  tap(f, UP);
  tap(f, UP);
  tap(f, UP);
  tap(f, DOWN); // the second carousel position is model 11, class 6
  tap(f, CROSS);
  expect(f.character().model == 11 && f.character().class_index == 6 &&
         f.character().base == rows[6], "model 11, class 6 starts with the audited row");
  tap(f, DOWN);
  tap(f, CROSS);
  expect(f.screen() == Screen::CharAppearance && f.last_error().find("Unknown") != std::string::npos,
         "unverified appearance option refused");
  tap(f, UP);
  tap(f, CROSS);
  expect(f.screen() == Screen::CharAbility, "ability page");
  tap(f, CROSS);
  expect(f.screen() == Screen::CharAbility && !f.last_error().empty(), "points must be spent");
  for (int i = 0; i < 25; ++i)
    tap(f, RIGHT);
  expect(f.ability_remaining() == 0, "20 spent, extra ignored");
  expect(f.character().stats()[0] == 26 + 20, "class 6 strength row takes the points");
  tap(f, CROSS);
  expect(f.screen() == Screen::CharName, "name page");
  tap(f, DOWN);
  tap(f, CROSS);
  expect(f.screen() == Screen::CharName, "empty name refused");
  reject([&] { f.set_name(std::string(32, 'a')); }, "name over 31 bytes refused");
  f.set_name("Host");
  tap(f, CROSS);
  expect(f.screen() == Screen::Playing && f.character_ready(), "character finished");
  expect(f.character().name == "Host" && f.character().stats() == std::array<std::int32_t, 4>{{46, 37, 24, 28}} &&
         !f.character().appearance_known && f.character().gold == 500 && f.character().level == 1 &&
         f.character().acknowledged == 1 && !f.character().skill_points_known,
         "created record keeps known fields and labels appearance and skill points Unknown");
  bool hub = false;
  for (const auto &r : f.take_requests())
    hub = hub || (r.kind == "start_play" && r.arg == "hub");
  expect(hub, "playerSel_finish queues the hub");

  champions::menus::Ledger ledger;
  champions::menus::Ledger::Adopted a;
  a.gold = f.character().gold;
  a.level = f.character().level;
  a.acknowledged = f.character().acknowledged;
  for (int i = 0; i < 4; ++i)
    a.stats[i] = f.character().stats()[static_cast<std::size_t>(i)];
  ledger.adopt(a);
  expect(ledger.gold() == 500 && ledger.level() == 1 && ledger.acknowledged() == 1, "ledger adopts init values");
  expect(ledger.base_stat(0) == 46 && ledger.bag_count(0) == 0, "ledger stats and empty bag");
  expect(ledger.equipment_index(0) == -1, "ledger equipment empty");
  a.level = 200;
  reject([&] { ledger.adopt(a); }, "level over s8 refused");
}

void import_vectors() {
  auto f = to_main();
  for (int i = 0; i < 5; ++i)
    tap(f, CROSS);
  tap(f, TRIANGLE);
  bool asked = false;
  for (const auto &r : f.take_requests())
    asked = asked || (r.kind == "import_ps2" && r.arg == "char");
  expect(asked, "page 1 triangle imports");
  f.import_result(false, Character{}, "bad slot");
  expect(f.screen() == Screen::CharClass && f.last_error() == "bad slot", "refused import stays");
  Character c;
  c.name = "Imported";
  c.level = 7;
  c.level_name = "crypt";
  f.import_result(true, c);
  bool hub = false;
  for (const auto &r : f.take_requests())
    hub = hub || (r.kind == "start_play" && r.arg == "hub");
  expect(hub && f.character().imported, "character import continues to the hub");
  // From Load Saved Game, the slot's level is asked for.
  auto g = to_main();
  tap(g, DOWN);
  tap(g, CROSS);
  expect(g.screen() == Screen::LoadGame && g.load_from_frontend() == 1, "load from the title");
  tap(g, DOWN);
  tap(g, CROSS);
  g.import_result(true, c);
  bool crypt = false;
  for (const auto &r : g.take_requests())
    crypt = crypt || (r.kind == "start_play" && r.arg == "crypt");
  expect(crypt, "import from load asks for the slot's level");
  reject([&] { g.import_result(true, c); }, "unrequested import refused");
}

void pause_vectors() {
  auto f = to_main();
  for (int i = 0; i < 5; ++i)
    tap(f, CROSS);
  tap(f, CROSS);
  tap(f, CROSS);
  for (int i = 0; i < 20; ++i)
    tap(f, RIGHT);
  tap(f, CROSS);
  f.set_name("P");
  tap(f, DOWN);
  tap(f, CROSS);
  (void)f.take_requests();
  tap(f, START);
  expect(f.screen() == Screen::Pause && f.paused(), "start opens pause, paused offline");
  expect(f.entries().size() == 5, "five entries offline");
  tap(f, DOWN);
  tap(f, CROSS);
  expect(has(f.take_requests(), "recall"), "gate scroll sends recall");
  f.recall_executed();
  expect(f.screen() == Screen::Playing && !f.paused() && f.lockout() == RECALL_LOCKOUT, "recall closes pause, lockout 60");
  f.frame(START);
  expect(f.screen() == Screen::Playing && f.lockout() == RECALL_LOCKOUT - 1, "dummy pad while held");
  f.frame(0);
  expect(f.lockout() == 0, "released: lockout 0");
  // A conversation is open (docs/spec/dialog-gaps.md 3): offline the start button cannot open the pause menu.
  f.set_dialog_open(true);
  tap(f, START);
  expect(f.screen() == Screen::Playing && !f.paused(), "start does not open the pause menu while a dialog is open");
  f.set_dialog_open(false);
  tap(f, START);
  expect(f.screen() == Screen::Pause, "pause again");
  // An open pause menu (a scripted dialog can open under it online) cannot recall or exit.
  f.set_dialog_open(true);
  {
    bool gate = true, exit_entry = true, load = false;
    for (const auto &e : f.entries()) {
      if (e.id == "gate") gate = e.enabled;
      if (e.id == "exit") exit_entry = e.enabled;
      if (e.id == "load") load = e.enabled;
    }
    expect(!gate && !exit_entry && load, "recall and exit are not selectable while a dialog is open; load is");
  }
  f.set_dialog_open(false);
  {
    bool gate = false, exit_entry = false;
    for (const auto &e : f.entries()) {
      if (e.id == "gate") gate = e.enabled;
      if (e.id == "exit") exit_entry = e.enabled;
    }
    expect(gate && exit_entry, "recall and exit are selectable with no dialog open");
  }
  // Options from pause preview mx_main_l.vag and return to the options entry.
  tap(f, DOWN); tap(f, DOWN); tap(f, DOWN);
  tap(f, CROSS);
  expect(f.screen() == Screen::Options, "pause options");
  {
    const auto s = f.layout();
    expect(f.entries().size() == 2, "pause options use two host controls");
    bool executed_frame = false, observed_row = false;
    for (const auto &d : s.draws) {
      executed_frame = executed_frame || (d.kind == Draw::Kind::Frame && d.evidence == Evidence::Executed);
      observed_row = observed_row || (d.kind == Draw::Kind::Text && d.evidence == Evidence::Observed);
    }
    expect(!executed_frame && !observed_row, "pause options borrow no front-end geometry or pause colour");
  }
  (void)f.take_requests();
  for (int i = 0; i < 6; ++i)
    f.frame(LEFT);
  bool main_l = false;
  for (const auto &r : f.take_requests())
    main_l = main_l || (r.kind == "preview_music" && r.arg == "mx_main_l.vag");
  expect(main_l, "pause preview mx_main_l.vag");
  tap(f, CIRCLE);
  expect(f.screen() == Screen::Pause && f.cursor() == 3, "back to the options entry");
  expect(has(f.take_requests(), "write_options"), "pause leave commits");
  // Exit prompt opens on no; left then cross confirms.
  tap(f, DOWN);
  tap(f, CROSS);
  expect(f.screen() == Screen::ExitConfirm && f.cursor() == 1, "exit prompt on no");
  tap(f, CROSS);
  expect(f.screen() == Screen::Pause, "no returns");
  tap(f, CROSS);
  tap(f, LEFT);
  tap(f, CROSS);
  expect(has(f.take_requests(), "exit_session"), "yes sends the exit");
  f.return_to_frontend();
  expect(f.screen() == Screen::Main && !f.paused(), "front end after exit");
  // Load from pause, then back.
  reject([&] { f.load_result(true); }, "unrequested load result refused");
}

void save_vectors() {
  auto f = to_main();
  for (int i = 0; i < 5; ++i)
    tap(f, CROSS);
  tap(f, CROSS);
  tap(f, CROSS);
  for (int i = 0; i < 20; ++i)
    tap(f, RIGHT);
  tap(f, CROSS);
  f.set_name("S");
  tap(f, DOWN);
  tap(f, CROSS);
  f.request_save();
  expect(f.screen() == Screen::SaveGame && f.paused(), "card screen pauses offline");
  tap(f, CROSS);
  expect(f.save_latch() == 1 && has(f.take_requests(), "save_session"), "slot chosen: latch 1");
  f.save_result(false, "disk full");
  expect(f.screen() == Screen::SaveGame && f.save_latch() == 1, "error keeps the latch");
  f.save_result(true);
  expect(f.screen() == Screen::Saved, "saved box");
  tap(f, CROSS);
  expect(f.screen() == Screen::Playing && f.save_latch() == 0 && !f.paused(), "box closes: latch 0");
  // Session exit codes restart the front end.
  f.session_exit(0x65);
  expect(f.screen() == Screen::Main && f.last_exit_code() == 1, "0x65 -> frontend_start(1)");
  reject([&] { f.session_exit(2); }, "unknown exit code refused");
}

void notes_vectors() {
  Flow f;
  bool sound = false, unknown = false;
  for (const auto &n : f.notes()) {
    sound = sound || n.find("not voiced") != std::string::npos;
    unknown = unknown || n.rfind("Unknown: ", 0) == 0;
  }
  expect(sound, "the unvoiced menu sounds are named");
  expect(unknown, "the Unknown layout parts are named");
}

// ---- layout (research-menus-layout dc458d5) ----------------------------------------
// Golden values come from tools/ref_frontend_layout.py list_geometry /
// list_frame_calls / pause_geometry run on the pinned head; they are numbers only.
struct Call {
  int x, y, w, h, layer;
  std::uint32_t tint;
  int style;
};
void expect_frames(const std::array<Draw, 3> &f, const Call (&want)[3], const char *name) {
  for (int i = 0; i < 3; ++i) {
    const auto &d = f[static_cast<std::size_t>(i)];
    const auto &w = want[i];
    expect(d.kind == Draw::Kind::Frame && d.x == w.x && d.y == w.y && d.w == w.w && d.h == w.h &&
               d.layer == w.layer && d.colour == w.tint && d.style == w.style,
           name);
  }
}

void layout_geometry_vectors() {
  using namespace layout;
  // Four-row front-end tables (main 0x4f6b88, players 0x4f6de8, difficulty 0x4f6e38).
  for (const auto &spec : {main_list(), players_list(), difficulty_list()}) {
    const auto a = list_anchors(spec);
    expect(a.size() == 4 && a[0].x == 320 && a[0].y == 259 && a[3].y == 325, "four-row anchors");
    expect_frames(list_frames(spec, 0),
                  {{300, 259, 40, 33, 7, 0x300000ff, 0}, {292, 248, 56, 122, 7, 0x80808080, 2}, {301, 260, 54, 114, 6, 0x3f000000, 0}},
                  "four-row frames");
  }
  // Two-row tables (game type 0x4f6bf8, mode 0x4f6c68).
  for (const auto &spec : {game_type_list(), mode_list()}) {
    const auto a = list_anchors(spec);
    expect(a.size() == 2 && a[0].y == 281 && a[1].y == 303, "two-row anchors");
    expect_frames(list_frames(spec, 0),
                  {{300, 281, 40, 33, 7, 0x300000ff, 0}, {292, 270, 56, 78, 7, 0x80808080, 2}, {301, 282, 54, 70, 6, 0x3f000000, 0}},
                  "two-row frames");
  }
  // Three rows: the selected frame is 32 high at an even y (0x4f6c28: y = 270).
  {
    const ListSpec three{3, 0, 320, 309};
    const auto a = list_anchors(three);
    expect(a[0].y == 270 && a[1].y == 292 && a[2].y == 314, "three-row anchors");
    expect_frames(list_frames(three, 0),
                  {{300, 270, 40, 32, 7, 0x300000ff, 0}, {292, 259, 56, 100, 7, 0x80808080, 2}, {301, 271, 54, 92, 6, 0x3f000000, 0}},
                  "three-row frames");
  }
  // Options (0x4f6e88, front-end origin y 238): rows 0-2 are value rows at x=297.
  {
    const auto a = list_anchors(options_list());
    expect(a.size() == 13 && a[0].x == 297 && a[0].y == 89 && a[2].x == 297 && a[2].y == 133, "options value rows");
    expect(a[3].x == 320 && a[3].y == 155 && a[12].x == 320 && a[12].y == 353, "options plain rows");
    expect_frames(list_frames(options_list(), 0),
                  {{180, 89, 280, 33, 7, 0x300000ff, 0}, {214, 78, 212, 320, 7, 0x80808080, 2}, {223, 90, 210, 312, 6, 0x3f000000, 0}},
                  "options frames");
  }
  // Pause (0x44f3b8, normal offline table: five enabled rows).
  {
    const auto a = list_anchors(pause_list());
    expect(a.size() == 5 && a[0].y == 179 && a[4].y == 267, "pause anchors");
    expect_frames(list_frames(pause_list(), 0),
                  {{300, 179, 40, 33, 7, 0x300000ff, 0}, {292, 168, 56, 144, 7, 0x80808080, 2}, {301, 180, 54, 136, 6, 0x3f000000, 0}},
                  "pause frames");
  }
  // No text-width rule is traced: a narrow list is always the documented minimum.
  {
    const auto narrow = list_frames(main_list(), 0);
    expect(narrow[1].min_w == 56 && narrow[1].w == 56 && narrow[1].evidence == Evidence::Executed,
           "narrow list: the executed minimum width only");
  }
  // Sprites.
  {
    const auto logo = layout::logo_sprites();
    expect(logo[0].resource == "soe_splash_url.tex" && logo[0].x == 192 && logo[0].y == 330 && logo[0].layer == 3 &&
               logo[0].colour == 0x80808080 && logo[0].lump == "SOELOGO.LMP",
           "logo url sprite");
    expect(logo[1].resource == "soe_splash.tex" && logo[1].x == 64 && logo[1].y == 50 && logo[1].layer == 3, "logo sprite");
    const auto title = layout::title_sprite();
    expect(title.resource == "title.tex" && title.lump == "FRONTEND.LMP" && title.x == 141 && title.y == 15 &&
               title.layer == 5 && title.colour == 0x80808080,
           "title sprite");
  }
  expect(std::string(menu_sound_resource(menu_sound::move)) == "menu_main_toggle.anm" &&
             std::string(menu_sound_resource(menu_sound::select)) == "menu_main_accept.anm" &&
             std::string(menu_sound_resource(menu_sound::back)) == "menu_main_back.anm",
         "sound resources");
}

int count_kind(const Scene &s, Draw::Kind k) {
  int n = 0;
  for (const auto &d : s.draws)
    n += d.kind == k;
  return n;
}
bool has_resource(const Scene &s, const char *name) {
  for (const auto &d : s.draws)
    if (d.kind == Draw::Kind::Sprite && d.resource == name)
      return true;
  return false;
}

void layout_screen_vectors() {
  Flow f;
  expect(f.layout().host_layout, "the card prompt is host-labelled");
  tap(f, CROSS);
  auto logo = f.layout();
  expect(logo.screen == "soe-logo" && !logo.host_layout && has_resource(logo, "soe_splash.tex") &&
             has_resource(logo, "soe_splash_url.tex"),
         "logo screen draws both documented sprites");
  for (int i = 0; i < SOE_LOGO_FRAMES; ++i)
    f.frame(0);
  expect(f.screen() == Screen::IntroMovie && f.layout().host_layout, "the film's picture stays a labelled host layout");
  for (int i = 0; i < 40; ++i)
    f.frame(0);
  f.frame(CROSS);
  f.frame(0);
  expect(f.screen() == Screen::Main, "main menu");
  auto check_card_art = [](const Scene &s) {
    bool frame = false, title = false, false_slot = false;
    for (const auto &d : s.draws) {
      frame = frame || (d.kind == Draw::Kind::Frame && d.evidence == Evidence::Executed &&
                        d.x == 60 && d.y == 122 && d.w == 521 && d.h == 105 &&
                        d.layer == 6 && d.style == 2 && d.colour == 0x7f616369);
      title = title || (d.kind == Draw::Kind::Text && d.x == 320 && d.y == 110 &&
                        d.font == Font::Small && d.evidence == Evidence::Host);
      false_slot = false_slot || (d.kind == Draw::Kind::Rect ||
                                  (d.kind == Draw::Kind::Text && d.y >= 128 && d.y <= 213));
    }
    expect(s.host_layout && frame && title && !false_slot,
           "card screen paints executed art and refuses fabricated slot rows");
  };
  {
    Flow load = to_main();
    tap(load, DOWN);
    tap(load, CROSS);
    expect(load.screen() == Screen::LoadGame, "load screen opened from main");
    check_card_art(load.layout());
    Flow save = to_main();
    save.attach_play();
    save.request_save();
    expect(save.screen() == Screen::SaveGame, "save screen opened from play");
    check_card_art(save.layout());
  }
  {
    const auto s = f.layout();
    expect(!s.host_layout && has_resource(s, "title.tex") && count_kind(s, Draw::Kind::Frame) == 3 &&
               count_kind(s, Draw::Kind::Text) == 4,
           "main: title, three frames, four rows");
    for (const auto &d : s.draws)
      if (d.kind == Draw::Kind::Text)
        expect(d.font == Font::Medium && d.layer == 7 && d.x == 320 && d.evidence == Evidence::Host &&
                   d.colour != layout::PAUSE_TEXT,
               "main row font and layer traced; the colour is a Host choice, not the pause colour");
  }
  tap(f, DOWN);
  {
    const auto rs = f.take_requests();
    int sounds = 0;
    for (const auto &r : rs)
      if (r.kind == "menu_sound") {
        ++sounds;
        expect(r.arg == "menu_main_toggle.anm", "a move looks up the toggle clip");
      }
    expect(sounds == 1, "one move, one sound");
    const auto s = f.layout();
    expect(s.draws[5].kind == Draw::Kind::Frame && s.draws[5].evidence == Evidence::Host,
           "uncaptured selected row uses a host highlight");
  }
  tap(f, UP);
  (void)f.take_requests();
  tap(f, UP); // at the top: no move, no sound
  expect(!has(f.take_requests(), "menu_sound"), "no move, no sound");
  // Cancel on the main list: no transition, no sound (Executed).
  tap(f, CIRCLE);
  expect(f.screen() == Screen::Main && !has(f.take_requests(), "menu_sound"), "main cancel is silent");
  // Accept row 0: accept clip first, then state 1.
  tap(f, CROSS);
  {
    const auto rs = f.take_requests();
    expect(f.screen() == Screen::GameType && !rs.empty() && rs.front().kind == "menu_sound" &&
               rs.front().arg == "menu_main_accept.anm",
           "accept clip, then game type");
    const auto s = f.layout();
    expect(has_resource(s, "title.tex") && s.draws[3].y == 281, "game type: title and the two-row frames");
  }
  tap(f, CIRCLE);
  {
    const auto rs = f.take_requests();
    expect(f.screen() == Screen::Main && !rs.empty() && rs.front().kind == "menu_sound" &&
               rs.front().arg == "menu_main_back.anm",
           "game-type cancel looks up the back clip");
  }
  // Table row counts.
  tap(f, CROSS);
  tap(f, CROSS);
  expect(f.screen() == Screen::Players && f.entries().size() == 4 && f.layout().host_layout, "players: four table rows, rows beyond the first are host-labelled");
  tap(f, CROSS);
  expect(f.screen() == Screen::Mode && f.entries().size() == 2, "mode: two table rows");
  tap(f, CROSS);
  expect(f.screen() == Screen::Difficulty && f.entries().size() == 4, "difficulty: four table rows");
  // Options: thirteen rows, no title sprite, the value shift.
  Flow g = to_main();
  tap(g, DOWN);
  tap(g, DOWN);
  tap(g, CROSS);
  expect(g.screen() == Screen::Options && g.entries().size() == 13, "options: thirteen rows");
  {
    const auto s = g.layout();
    expect(!has_resource(s, "title.tex"), "options has no title sprite");
    int at_297 = 0, at_320 = 0;
    for (const auto &d : s.draws)
      if (d.kind == Draw::Kind::Text && d.role.rfind("list row", 0) == 0)
        (d.x == 297 ? at_297 : at_320) += 1;
    expect(at_297 == 3 && at_320 == 10, "options: three value rows, ten plain rows");
    expect(s.host_layout, "the option value bar is a labelled host drawing");
  }
  (void)g.take_requests();
  // A non-finite or out-of-range slider word draws a bounded fill (no undefined cast).
  for (std::uint32_t bits : {0x7fc00000u, 0xbf800000u, 0x7f800000u, 0x40000000u}) {
    Flow n = to_main(options_with_music(bits));
    tap(n, DOWN);
    tap(n, DOWN);
    tap(n, CROSS);
    bool ok = false;
    for (const auto &d : n.layout().draws)
      if (d.role == "host slider fill")
        ok = d.w >= 0 && d.w <= 72;
    expect(ok, "slider fill width stays within the track");
  }
  // Credits: the music handoff is requested with flags 3, exit requests the theme with flags 1.
  Flow c = to_main();
  tap(c, DOWN);
  tap(c, DOWN);
  tap(c, DOWN);
  (void)c.take_requests();
  tap(c, CROSS);
  expect(c.screen() == Screen::Credits, "credits");
  {
    bool found = false;
    for (const auto &r : c.take_requests())
      if (r.kind == "credits_music" && r.arg == "mx_endcredits_ext_l.vag" && r.value == 3)
        found = true;
    expect(found, "credits music, flags 3");
    expect(c.layout().host_layout, "credits data layout is a labelled host layout");
  }
  tap(c, CIRCLE);
  {
    bool found = false;
    for (const auto &r : c.take_requests())
      if (r.kind == "front_end_music" && r.arg == "mx_maintheme_l.vag" && r.value == 1)
        found = true;
    expect(found && c.screen() == Screen::Main && c.cursor() == 3, "credits exit requests the theme with flags 1");
  }
  // Pause: the executed geometry, no host element offline.
  Flow p = to_main();
  for (int i = 0; i < 5; ++i)
    tap(p, CROSS);
  p.attach_play();
  tap(p, START);
  expect(p.screen() == Screen::Pause, "pause");
  {
    const auto s = p.layout();
    expect(!s.host_layout && count_kind(s, Draw::Kind::Frame) == 3 && count_kind(s, Draw::Kind::Text) == 5, "pause: five rows, three frames");
    expect(s.draws[5].x == 300 && s.draws[5].y == 179 && s.draws[6].y == 168, "pause frames");
    expect(s.draws[0].y == 179 && s.draws[4].y == 267, "pause rows");
  }
  // Class art is Unknown: the screen stays host-labelled.
  Flow k = to_main();
  for (int i = 0; i < 5; ++i)
    tap(k, CROSS);
  expect(k.screen() == Screen::CharClass && k.layout().host_layout, "class page stays a labelled host layout");
  (void)k.take_requests();
  tap(k, CROSS);
  expect(!has(k.take_requests(), "menu_sound"), "class selection plays no phase-1 sound (refused)");
  tap(k, CROSS);
  expect(k.screen() == Screen::CharAbility, "ability page");
  {
    const auto s = k.layout();
    // Phase 3 colours in call order (PLAYERSEL_PHASES): header and description
    // 0x0A316864, points row and value 0x08325D18, selected stat 0x0068C7CC, others 0x08585E5D.
    int checked = 0;
    for (const auto &d : s.draws) {
      if (d.role == "phase-3 header (text Host)" || d.role == "stat description (text Host)") {
        expect(d.colour == 0x0A316864 && !d.settled, "phase 3 colour is retained without invented fade");
        ++checked;
      } else if (d.role == "phase-3 points row (text Host)" || d.role.rfind("points value", 0) == 0) {
        expect(d.colour == 0x08325D18, "phase 3 points colour");
        ++checked;
      } else if (d.role == "stat label (text Host)") {
        expect(d.colour == (d.y == 106 ? 0x0068C7CCu : 0x08585E5Du), "phase 3 stat row colour");
        ++checked;
      }
    }
    expect(checked == 8, "all phase 3 colours checked");
    int rows = 0;
    for (const auto &d : s.draws)
      if (d.role == "stat label (text Host)") {
        expect(d.x == 41 && d.y == 106 + 17 * rows && d.font == Font::Small && d.layer == 6, "ability rows from y=106, 17 apart");
        ++rows;
      }
    expect(rows == 4, "four ability rows");
    bool footer = false;
    for (const auto &d : s.draws)
      footer = footer || (d.kind == Draw::Kind::Frame && d.x == 21 && d.y == 18 && d.h == 355 && d.layer == 6 &&
                          d.colour == 0x08000000 && d.style == 1);
    expect(footer, "common footer frame");
  }
}

void card_cache_vectors() {
  std::vector<std::uint8_t> metadata(8 * 16, 0), headers(8 * 0x70, 0);
  const std::uint8_t hours[5] = {0x00, 0x11, 0x12, 0x13, 0x23};
  const std::uint8_t minutes[5] = {0x05, 0x10, 0x00, 0x25, 0x59};
  const char *clocks[5] = {"12:05am", "11:10am", "12:00pm", "1:25pm", "11:59pm"};
  for (int r = 0; r < 5; ++r) {
    metadata[16 * (r + 2) + 3] = hours[r];
    metadata[16 * (r + 2) + 2] = minutes[r];
    metadata[16 * (r + 2) + 6] = 0x01;
    metadata[16 * (r + 2) + 5] = 0x09;
    metadata[16 * (r + 2) + 7] = 0x26;
    headers[0x70 * (r + 2)] = static_cast<std::uint8_t>(r + 1);
  }
  headers[2 * 0x70 + 0x64] = 59;
  headers[2 * 0x70 + 0x6a] = 1;
  headers[2 * 0x70 + 0x6c] = 1;
  metadata[3] = 0x12;
  metadata[2] = 0x00;
  metadata[6] = 0x01;
  metadata[5] = 0x09;
  metadata[7] = 0x26;
  const auto rows = layout::visible_card_rows(metadata, headers, 8, 2, false);
  expect(rows.size() == 5, "card cache exposes five visible rows");
  for (int r = 0; r < 5; ++r)
    expect(rows[static_cast<std::size_t>(r)].index == r + 2 &&
               rows[static_cast<std::size_t>(r)].y == 128 + 17 * r &&
               rows[static_cast<std::size_t>(r)].metadata_offset == std::size_t(16 * (r + 2)) &&
               rows[static_cast<std::size_t>(r)].header_offset == std::size_t(0x70 * (r + 2)) &&
               rows[static_cast<std::size_t>(r)].populated &&
               rows[static_cast<std::size_t>(r)].number == std::to_string(r + 3) + "." &&
               rows[static_cast<std::size_t>(r)].date == "1/9/2026" &&
               rows[static_cast<std::size_t>(r)].clock == clocks[r],
           "nonzero scroll uses observed strides, row numbers, date and clock boundaries");
  const auto sparse = layout::visible_card_rows(metadata, headers, 1, 0, false);
  expect(sparse.size() == 5 && sparse[0].populated && !sparse[1].populated &&
             sparse[4].number == "5." && sparse[4].date.empty(),
         "nonzero count keeps numbered markers beyond populated cache rows");
  const auto save_free = layout::visible_card_rows(metadata, headers, 1, 0, true);
  expect(save_free.size() == 5 && save_free[1].new_save &&
             save_free[1].number == "2." && !save_free[2].new_save,
         "save marks only the first free row");
  const auto empty_save = layout::visible_card_rows({}, {}, 0, 0, true);
  expect(empty_save.size() == 5, "empty save keeps five numbered rows");
  for (int r = 0; r < 5; ++r)
    expect(empty_save[static_cast<std::size_t>(r)].index == r &&
               empty_save[static_cast<std::size_t>(r)].y == 128 + 17 * r &&
               empty_save[static_cast<std::size_t>(r)].number == std::to_string(r + 1) + "." &&
               empty_save[static_cast<std::size_t>(r)].new_save == (r == 0) &&
               !empty_save[static_cast<std::size_t>(r)].populated &&
               empty_save[static_cast<std::size_t>(r)].date.empty() &&
               empty_save[static_cast<std::size_t>(r)].clock.empty(),
           "empty save numbers all five rows and marks only row one as free");
  expect(layout::visible_card_rows({}, {}, 0, 0, false).empty(),
         "empty load has no rows");
  const auto preview = layout::card_preview(headers, 8, 2);
  expect(preview.header_offset == 2 * 0x70 && preview.play_seconds == 59 &&
             preview.player_count == 1 && preview.difficulty_index == 1,
         "selected header exposes verified preview fields");
  reject([&] { (void)layout::visible_card_rows(metadata, headers, 8, 9, false); }, "unavailable scroll refused");
  reject([&] { (void)layout::visible_card_rows(metadata, std::vector<std::uint8_t>(7 * 0x70), 8, 2, false); },
         "short header cache refused");
  auto bad = metadata;
  bad[2 * 16 + 3] = 0x2a;
  reject([&] { (void)layout::visible_card_rows(bad, headers, 8, 2, false); }, "invalid BCD clock refused");
  reject([&] { (void)layout::card_preview(headers, 8, 8); }, "unavailable preview header refused");

  Flow load = to_main();
  tap(load, DOWN);
  tap(load, CROSS);
  load.set_card_cache(metadata, headers, 8, 2);
  load.set_card_preview(2, true);
  int clocks_drawn = 0;
  int frame_at = -1, last_clock_at = -1, draw_at = 0;
  int numbers_drawn = 0, dates_drawn = 0;
  bool players = false, difficulty = false, debug_seconds = false;
  for (const auto &d : load.layout().draws) {
    if (d.role == "slot-list tile frame (settled fixture)")
      frame_at = draw_at;
    if (d.role == "supplied card clock") {
      expect(d.kind == Draw::Kind::Text && d.font == Font::Small && d.x == 487 &&
                 d.y == 128 + 17 * clocks_drawn && d.text == clocks[clocks_drawn],
             "supplied clock uses the executed text anchor");
      last_clock_at = draw_at;
      ++clocks_drawn;
    }
    if (d.role == "supplied card number") {
      expect(d.x == 77 && d.y == 128 + 17 * numbers_drawn &&
                 d.text == std::to_string(numbers_drawn + 3) + ".", "numbered row anchor");
      ++numbers_drawn;
    }
    if (d.role == "supplied card date") {
      expect(d.x == 365 && d.y == 128 + 17 * dates_drawn && d.text == "1/9/2026", "date row anchor");
      ++dates_drawn;
    }
    players = players || (d.role == "preview player count (header field)" && d.x == 320 && d.y == 247 && d.text == "1");
    difficulty = difficulty || (d.role == "preview difficulty index (header field)" && d.x == 320 && d.y == 264 && d.text == "index 1");
    debug_seconds = debug_seconds || (d.role == "debug play seconds (minute display unverified)" && d.x == 320 && d.y == 281 && d.text == "59s");
    ++draw_at;
  }
  expect(clocks_drawn == 5, "supplied cache paints five clocks only");
  expect(numbers_drawn == 5 && dates_drawn == 5 && players && difficulty && debug_seconds,
         "supplied cache paints numbered dates and labelled preview fields");
  expect(last_clock_at >= 0 && last_clock_at < frame_at,
         "reverse layer painter places supplied clocks over the frame");
  load.set_card_preview(2, false);
  bool debug_off = true;
  for (const auto &d : load.layout().draws)
    debug_off = debug_off && d.role != "debug play seconds (minute display unverified)";
  expect(debug_off, "debug time line absent when debug byte is clear");
  tap(load, CIRCLE);
  expect(load.screen() == Screen::Main, "leaving load clears the supplied cache");
  reject([&] { load.set_card_cache(metadata, headers, 8, 2); }, "card cache outside card screen refused");
}

// The first theme obeys music_playGlobal's gate; the flags stay 1 (research dc458d5).
void theme_gate_vectors() {
  const struct {
    std::uint32_t bits;
    bool queued;
    const char *name;
  } rows[] = {{0x00000000, false, "volume 0"},
              {0xbf800000, false, "volume -1.0"},
              {0x3d4cccccu - 1, false, "just below the gate"},
              {0x3d4ccccc, true, "the gate value"},
              {0x3f000000, true, "volume 0.5"}};
  for (const auto &r : rows) {
    Flow f(options_with_music(r.bits));
    tap(f, CROSS);
    tap(f, CROSS); // logo press: front end starts
    bool theme = false;
    std::uint32_t flags = 0;
    for (const auto &q : f.take_requests())
      if (q.kind == "front_end_music") {
        theme = true;
        flags = q.value;
        expect(q.arg == "mx_maintheme_l.vag", "the front-end theme");
      }
    expect(theme == r.queued, r.name);
    expect(!theme || flags == 1, "the original flags are kept (no 0x40 fade-stop)");
    expect(f.gated_music() == (r.queued ? 0 : 1), "the refusal is counted");
  }
}
} // namespace

int main() {
  class_vectors();
  boot_vectors();
  route_vectors();
  repeat_vectors();
  slider_vectors();
  options_load_vectors();
  character_vectors();
  import_vectors();
  pause_vectors();
  save_vectors();
  notes_vectors();
  layout_geometry_vectors();
  layout_screen_vectors();
  card_cache_vectors();
  theme_gate_vectors();
  std::cout << "PASS frontend " << checks << " checks\n";
  return 0;
}
