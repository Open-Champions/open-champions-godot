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
  expect(f.entries().size() == 12, "twelve models");
  std::array<std::array<std::int32_t, 4>, CLASSES> rows{};
  for (int c = 0; c < CLASSES; ++c)
    rows[static_cast<std::size_t>(c)] = {c * 10 + 1, c * 10 + 2, c * 10 + 3, c * 10 + 4};
  f.set_class_start_stats(rows);
  for (int i = 0; i < 10; ++i)
    tap(f, DOWN);
  tap(f, CROSS);
  expect(f.character().model == 10 && f.character().class_index == 5, "model 10 is class 5");
  tap(f, CROSS);
  expect(f.screen() == Screen::CharAbility, "ability page");
  tap(f, CROSS);
  expect(f.screen() == Screen::CharAbility && !f.last_error().empty(), "points must be spent");
  for (int i = 0; i < 25; ++i)
    tap(f, RIGHT);
  expect(f.ability_remaining() == 0, "20 spent, extra ignored");
  expect(f.character().stats()[0] == 51 + 20, "first row takes the points");
  tap(f, CROSS);
  expect(f.screen() == Screen::CharName, "name page");
  tap(f, DOWN);
  tap(f, CROSS);
  expect(f.screen() == Screen::CharName, "empty name refused");
  reject([&] { f.set_name(std::string(32, 'a')); }, "name over 31 bytes refused");
  f.set_name("Host");
  tap(f, CROSS);
  expect(f.screen() == Screen::Playing && f.character_ready(), "character finished");
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
  expect(ledger.base_stat(0) == 71 && ledger.bag_count(0) == 0, "ledger stats and empty bag");
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
  tap(f, START);
  expect(f.screen() == Screen::Pause, "pause again");
  // Options from pause preview mx_main_l.vag and return to the options entry.
  tap(f, DOWN); tap(f, DOWN); tap(f, DOWN);
  tap(f, CROSS);
  expect(f.screen() == Screen::Options, "pause options");
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
  bool layout = false;
  for (const auto &n : f.notes())
    layout = layout || n.find("not in the research") != std::string::npos;
  expect(layout, "layout gap named");
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
  std::cout << "PASS frontend " << checks << " checks\n";
  return 0;
}
