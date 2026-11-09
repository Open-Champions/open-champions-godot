// Synthetic fixtures for the save/load screen play flow. Each case mirrors a
// probe of t3/research-save-screen c8895e2 tools/difftest_save_screen_flow.py
// (cited as flow:N) and asserts the outcome docs/save-screen.md records. Every
// planted word is a fixture value from that probe, not a card capture.
#include "champions/memcard_screen.hpp"

#include <array>
#include <cstdint>
#include <initializer_list>
#include <utility>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace champions::frontend::memcard;
using champions::frontend::Refusal;

namespace {
int checks = 0;

void expect(bool ok, const char *name) {
  ++checks;
  if (!ok)
    throw std::runtime_error(name);
}

template <class Fn> void unknown(Fn fn, const char *name) {
  bool threw = false;
  try {
    fn();
  } catch (const Unknown &) {
    threw = true;
  }
  expect(threw, name);
}

template <class Fn> void refused(Fn fn, const char *name) {
  bool threw = false;
  try {
    fn();
  } catch (const Unknown &) {
    threw = false; // a plain refusal is expected, not an unknown branch
  } catch (const Refusal &) {
    threw = true;
  }
  expect(threw, name);
}

std::array<std::uint8_t, DIRECTORY_RECORD> record(const std::string &name) {
  std::array<std::uint8_t, DIRECTORY_RECORD> out{};
  std::memcpy(out.data() + DIRECTORY_NAME_AT, name.data(), name.size());
  return out;
}

// flow:117-133 prepare_screen, with the browser finished.
Screen save_screen(int step) {
  Screen s;
  s.w.browser_stage = 1;
  s.w.slot_stage = 1;
  s.w.step = step;
  s.w.card_state = CARD_SAVE;
  s.w.fade_level = 0x40;
  return s;
}

// flow:335-359 confirm_case: one state-4 call with sentinels on the results.
Screen confirm_case(std::uint32_t float_bits, int settled, int selected, int scroll, int count) {
  Screen s;
  s.w.browser_stage = 1;
  s.w.card_state = CARD_SAVE;
  s.w.slot_state = SLOT_LIST;
  s.w.cache_count = count;
  s.w.settled_row = settled;
  s.w.selected_row = selected;
  s.w.scroll_base = scroll;
  s.w.scroll_target = scroll;
  s.w.cursor_float = float_bits;
  s.w.list_action = 0xa5;
  s.w.fade_step = 0x2a;
  s.w.fade_level = 0x7f;
  s.w.countdown = 10;
  (void)s.call(MASK_CONFIRM);
  return s;
}

bool unchanged(const Screen &s) {
  return s.w.slot_state == SLOT_LIST && s.w.list_action == 0xa5 && s.w.fade_step == 0x2a;
}

void confirm_settle() {
  // doc:193-200; flow:637-645.
  auto held = confirm_case(0x3f000000, 2, 2, 0, 2);
  expect(unchanged(held), "0x3f000000 leaves slot state, 0x4f8ef4 and the fade step");
  expect(held.w.cursor_float == 0x3f000000, "0x3f000000 is not the snap");
  auto uneven = confirm_case(0, 1, 2, 0, 2);
  expect(unchanged(uneven), "unequal row words leave the same three");
  auto snapped = confirm_case(CURSOR_SNAP, 1, 2, 1, 2);
  expect(snapped.w.slot_state == SLOT_LIST && snapped.w.list_action == FREE_ROW_ACTION &&
             snapped.w.fade_step == FADE_OUT,
         "the 1.0 snap arms the free row (4, 7, -16)");
  expect(snapped.w.cursor_float == 0 && snapped.w.selected_row == 1,
         "the snap clears 0x879e34 and copies 0x879e24 to 0x879e28");
}

Screen free_row_list() {
  // flow:361-382 new_row_and_handoff.
  Screen s;
  s.w.browser_stage = 1;
  s.w.card_state = CARD_SAVE;
  s.w.cache_count = 2;
  s.w.settled_row = 1;
  s.w.selected_row = 1;
  s.w.scroll_base = 1;
  s.w.scroll_target = 1;
  s.w.countdown = 10;
  s.w.fade_level = 0x7f;
  s.w.fade_step = 8;
  return s;
}

void free_row_and_space() {
  auto s = free_row_list();
  auto c = s.call(MASK_CONFIRM);
  expect(s.w.slot_state == SLOT_LIST && s.w.list_action == FREE_ROW_ACTION && s.w.fade_step == FADE_OUT,
         "scroll 1 + cursor 1 == count 2 arms the free row");
  expect(c.ops.empty() && !c.committed, "arming queues nothing");
  // Fade reached 0: operation 8 and state 7 (flow:383-391, 651).
  s.w.fade_level = 0;
  s.w.fade_step = 0;
  c = s.call(0);
  expect(s.w.slot_state == SLOT_SPACE, "fade 0 enters state 7");
  expect(c.ops.size() == 1 && c.ops[0].op == OP_SPACE, "fade 0 queues card operation 8");

  // A scroll mismatch copies 0x879e30 over 0x879e2c before the add (doc:206-207).
  auto m = free_row_list();
  m.w.scroll_base = 0;
  (void)m.call(MASK_CONFIRM);
  expect(m.w.scroll_base == 1 && m.w.list_action == FREE_ROW_ACTION, "scroll mismatch copied before the add");

  // State 7 (flow:392-439, 652-656).
  auto space = [](int count, int have, int need) {
    Screen t = free_row_list();
    t.w.slot_state = SLOT_SPACE;
    t.w.cache_count = count;
    t.w.countdown = 0;
    t.w.fade_step = 8;
    t.w.fade_level = 0x20;
    t.w.space_have = have;
    t.w.space_need = need;
    auto r = t.call(0);
    return std::make_pair(t, r);
  };
  auto [even_ok, even_ok_call] = space(2, 100, 100);
  expect(even_ok.w.slot_state == SLOT_SPACE && even_ok.w.fade_step == FADE_OUT && even_ok_call.keys.empty(),
         "count 2, 100 against 100 sets -16 and stays in 7");
  auto [even_short, even_short_call] = space(2, 99, 100);
  expect(even_short.w.slot_state == SLOT_SPACE && even_short.w.fade_step == 8 &&
             even_short_call.keys == std::vector<std::string>{KEY_NEED_FREE_SPACE},
         "count 2, 99 looks up MemcardNeedFreeSpaceBrowser and leaves fade 8");
  auto [odd_short, odd_short_call] = space(3, 100, 100);
  expect(odd_short.w.fade_step == 8 && odd_short_call.keys == std::vector<std::string>{KEY_NEED_FREE_SPACE},
         "count 3 shifts the threshold: 100 against 100 is short");
  auto [odd_ok, odd_ok_call] = space(3, 101, 100);
  expect(odd_ok.w.fade_step == FADE_OUT && odd_ok_call.keys.empty(), "count 3, 101 against 100 sets -16");

  auto waiting = space(2, 100, 100).first;
  waiting.w.countdown = 3;
  waiting.w.fade_step = 8;
  (void)waiting.call(0);
  expect(waiting.w.fade_step == 8 && waiting.w.slot_state == SLOT_SPACE, "state 7 waits for 0x88d3c4");

  s.w.countdown = 0;
  s.w.fade_level = 0;
  s.w.fade_step = 0;
  (void)s.call(0);
  expect(s.w.slot_state == SLOT_COMMIT, "fade already 0: state 7 becomes 12");
  c = s.call(0);
  expect(c.committed && s.w.slot_state == 0, "free row: state 12 returns 1");
  expect(s.w.slot_stage == 1 && s.w.step == 0 && s.w.latch == 1 && s.w.selected_index == s.w.cache_count,
         "the committed free row is index == count");
}

void overwrite_and_commit() {
  // Populated row: state 5 (doc:169; ref_save_screen.py:115-119).
  auto s = confirm_case(0, 0, 0, 0, 2);
  expect(s.w.slot_state == SLOT_OVERWRITE && s.w.list_action == 0xa5, "populated row confirm enters 5");
  (void)s.call(MASK_CANCEL);
  expect(s.w.slot_state == SLOT_LIST, "state 5 cancel returns to 4");
  s.w.slot_state = SLOT_OVERWRITE;
  (void)s.call(MASK_COMMIT);
  expect(s.w.slot_state == SLOT_DIRECTORY && s.w.write_intent == 1, "bit 0x80 enters 11 and marks 0x88d400");
  // State 11 then 12 (flow:440-452, 657-660).
  auto c = s.call(0);
  expect(s.w.slot_state == SLOT_COMMIT && c.ops.size() == 1 && c.ops[0] == CardOp{OP_DIRECTORY, CARD_DIRECTORY, 0},
         "state 11 queues 0x0a on /BASLUS-20973 and enters 12");
  s.w.fade_level = 0;
  s.w.fade_step = 0;
  s.w.list_result = 0;
  c = s.call(0);
  expect(c.committed && s.w.slot_state == 0, "state 12 returns 1 and clears the slot state");
  expect(s.w.slot_stage == 1 && s.w.step == 0 && s.w.latch == 1 && s.w.selected_index == 0,
         "the save screen takes the committed slot");
  s.w.slot_stage = 0;
  s.w.slot_state = SLOT_COMMIT;
  s.w.network = 1;
  unknown([&] { (void)s.call(0); }, "state 12 with the net byte was not executed");
  s.w.network = 0;
  s.w.list_result = -1;
  unknown([&] { (void)s.call(0); }, "state 12 with a negative 0x4f9540 was not executed");
  s.w.list_result = 0;
  s.w.pump_busy = true;
  unknown([&] { (void)s.call(0); }, "a busy pump was not executed");
}

// flow:176-193 save_filename.
std::string step0_name(int selected, int count, bool fresh, bool plant) {
  Screen s = save_screen(0);
  s.w.selected_index = selected;
  s.w.cache_count = count;
  s.w.fresh_name = fresh ? 1 : 0;
  s.w.directory.resize(4);
  if (!fresh && plant)
    s.w.directory[static_cast<std::size_t>(selected)] = record(SLOT1_NAME);
  auto c = s.call(0);
  expect(s.w.step == 1 && c.ops.empty(), "step 0 queues no write and moves to step 1");
  return s.w.slot_name;
}

void slot_naming() {
  // doc:228; flow:534-559.
  expect(step0_name(0, 0, true, false) == SLOT0_NAME, "set 0x8e1ad8, count 0: slot0");
  expect(step0_name(1, 0, true, false) == SLOT0_NAME, "set 0x8e1ad8, index 1, count 0: slot0");
  expect(step0_name(0, 0, false, false) == SLOT0_NAME, "clear byte, index == count == 0: slot0");
  for (int count = 1; count <= 3; ++count)
    expect(step0_name(count, count, false, false) == SLOT1_NAME, "empty directory, index == count 1-3: slot1");
  expect(step0_name(1, 2, false, true) == SLOT1_NAME, "occupied row copies 0x88d060 + index*64");

  Screen named = save_screen(0);
  named.w.selected_index = 1;
  named.w.cache_count = 2;
  named.w.directory = {record("BASLUS-20973_slotA"), record("BASLUS-20973_slotB")};
  (void)named.call(0);
  expect(named.w.slot_name == "BASLUS-20973_slotB", "the occupied copy is the directory's name, not a made one");

  Words w;
  w.cache_count = 4;
  unknown([&] { (void)make_slot_name(w); }, "count 4 was not executed");
  w.cache_count = 2;
  w.fresh_name = 1;
  unknown([&] { (void)make_slot_name(w); }, "a set byte with a nonzero count was not executed");
  w.fresh_name = 0;
  w.directory = {record(SLOT0_NAME)};
  unknown([&] { (void)make_slot_name(w); }, "a nonempty directory was not executed");
  Screen missing = save_screen(0);
  missing.w.selected_index = 1;
  missing.w.cache_count = 2;
  refused([&] { (void)missing.call(0); }, "an occupied row without a directory record");
}

Bytes synthetic_image(const std::string &name, const std::string &sub) {
  Bytes b(IMAGE_BYTES, 0);
  std::memcpy(b.data(), name.data(), name.size());
  std::memcpy(b.data() + 0x14, sub.data(), sub.size());
  // flow:156-173: sentinels over the directory and the first body word.
  for (std::size_t i = 0; i < 0xf0; ++i)
    b[0x16010 + i] = (i % 2 == 0) ? 0xa5 : 0x5a;
  const std::uint32_t word = 0x13579bdf;
  std::memcpy(b.data() + 0x16118, &word, sizeof word);
  return b;
}

void save_write() {
  // flow:195-218, 563-575: slot byte 3 and g_numPlayers 5 tell the pair apart.
  Screen s = save_screen(1);
  s.w.slot_byte = 3;
  s.w.num_players = 5;
  s.w.slot_name = SLOT1_NAME;
  s.w.live_level = "innovat1";
  std::vector<std::array<std::uint8_t, 2>> calls;
  s.services.save_build = [&](std::uint8_t a, std::uint8_t b) {
    calls.push_back({{a, b}});
    return synthetic_image(s.w.live_level, {});
  };
  s.services.pack_local_player = [] { throw std::runtime_error("offline save packed"); };
  auto c = s.call(0);
  expect(c.save_built && !c.packed && c.build_args == std::array<std::uint8_t, 2>{{3, 0}},
         "offline step 1 calls save_build(slot byte, 0)");
  expect(calls.size() == 1 && calls[0] == std::array<std::uint8_t, 2>{{3, 0}}, "save_build recorded (3, 0)");
  expect(s.w.image.size() == IMAGE_BYTES && std::string(reinterpret_cast<const char *>(s.w.image.data())) == "innovat1",
         "the image begins with the live level name");
  expect(s.w.step == 2, "step 1 moves to step 2");
  c = s.call(0);
  const std::vector<CardOp> want{{OP_OPEN, SLOT1_NAME, SAVE_OPEN_MODE},
                                 {OP_WRITE, {}, 0x4bc78},
                                 {OP_CLOSE, {}, 0},
                                 {OP_FILE_INFO, SLOT1_NAME, 0}};
  expect(c.ops == want, "step 2: open 0x202, one 0x4bc78-byte write, close, file info");
  expect(s.w.step == 3, "step 2 moves to step 3");

  Screen net = save_screen(1);
  net.w.network = 1;
  bool packed = false;
  net.services.pack_local_player = [&] { packed = true; };
  net.services.save_build = [](std::uint8_t, std::uint8_t) -> Bytes { throw std::runtime_error("net save built"); };
  c = net.call(0);
  expect(packed && c.packed && !c.save_built && net.w.step == 2, "the net byte packs instead of save_build (flow:601-608)");

  Screen bad = save_screen(1);
  bad.services.save_build = [](std::uint8_t, std::uint8_t) { return Bytes(0x100, 0); };
  refused([&] { (void)bad.call(0); }, "a short save image is refused");
  Screen none = save_screen(2);
  refused([&] { (void)none.call(0); }, "step 2 without an image is refused");
}

void saved_box() {
  // flow:220-243, 583-587: occupied row 1 of 2.
  Screen s = save_screen(3);
  s.w.selected_index = 1;
  s.w.cache_count = 2;
  s.w.latch = 1;
  s.w.fade_level = 0;
  s.w.fade_step = 4;
  (void)s.call(0);
  expect(s.w.saved_flag == 1 && s.w.fade_step == FADE_OUT && s.w.step == 3, "step 3 sets 0x4f8f29 and -16");
  s.w.fade_level = 0x10;
  (void)s.call(0);
  expect(s.w.step == 3, "step 3 waits for the fade");
  s.w.fade_level = 0;
  (void)s.call(0);
  expect(s.w.step == 4, "fade 0 advances to step 4");
  s.w.fade_level = 0x10;
  s.w.fade_step = 8;
  auto c = s.call(MASK_CONFIRM);
  expect(c.keys == std::vector<std::string>{KEY_CONTINUE, KEY_SAVE_COMPLETE, KEY_OVERWRITE_GOOD},
         "occupied row: MemcardContinue, MemcardSaveComplete, OverwriteGood");
  expect(s.w.fade_step == FADE_OUT && s.w.latch == 1 && s.w.card_state == CARD_SAVE, "confirm sets -16 only");
  (void)s.call(0);
  expect(s.w.latch == 1, "the box stays until the fade is 0");
  s.w.fade_level = 0;
  (void)s.call(0);
  expect(s.w.latch == 0 && s.w.card_state == CARD_IDLE, "dismiss clears the latch and stores -1");

  // flow:589-599: a new row (index 0, count 0).
  Screen n = save_screen(4);
  n.w.fade_step = 8;
  c = n.call(0);
  expect(c.keys == std::vector<std::string>{KEY_CONTINUE, KEY_SAVE_COMPLETE, KEY_SAVE_GOOD_NEW},
         "new row: MemcardSaveGood2");
}

void card_error() {
  // doc:234-244; flow:245-271, 611-625.
  struct Case {
    bool network, pending, called;
  };
  for (const Case k : {Case{false, false, true}, Case{false, true, true}, Case{true, false, true},
                       Case{true, true, false}}) {
    Screen s = save_screen(2);
    s.w.latch = 1;
    s.w.card_result = 1;
    s.w.image = Bytes(IMAGE_BYTES, 0);
    auto c = s.call(0);
    expect(s.w.step == 9 && c.closed_file && c.ops.empty() && s.w.latch == 1, "error: step 9, close, latch kept");
    expect(s.w.card_state == CARD_SAVE, "error leaves card state 3");
    s.w.card_result = 0;
    s.w.network = k.network ? 1 : 0;
    s.w.pending_level = k.pending ? "hub" : "";
    c = s.call(MASK_CANCEL);
    expect(s.w.card_state == CARD_IDLE, "cancel stores -1");
    expect(c.reopen_called == k.called, "pending level suppresses the request only with the net byte");
    expect(s.w.latch == 1 && s.w.slot_stage == 1 && s.w.card_state == CARD_IDLE,
           "stage 1 keeps the reopened request closed; latch stays 1");
  }
}

void requests() {
  // flow:273-303, 629-634.
  Screen s;
  s.w.card_state = 7;
  expect(!s.request_save(0) && s.w.card_state == 7, "no counted player leaves the card state");
  s.w.card_state = CARD_IDLE;
  expect(s.request_save(1) && s.w.card_state == CARD_SAVE, "one living player opens state 3");
  s.w.card_state = CARD_IDLE;
  s.w.slot_stage = 1;
  expect(!s.request_save(1) && s.w.card_state == CARD_IDLE, "stage 1 does not open");
  s.w.slot_stage = 0;
  s.w.open_block = 1;
  expect(!s.request_save(1), "0x88d3c8 does not open");

  Screen l;
  l.w.card_state = 7;
  l.w.open_block = 1;
  l.w.latch = 0xa5;
  expect(!l.request_load(2, 1, 1) && l.w.card_state == 7 && l.w.latch == 0xa5,
         "0x88d3c8 returns before any store; 0xa5 is kept");
  l.w.open_block = 0;
  expect(l.request_load(2, 1, 1), "requestLoad opens");
  expect(l.w.slot_byte == 2 && l.w.from_frontend == 1 && l.w.restore_flag == 1 && l.w.card_state == CARD_LOAD &&
             l.w.latch == 0,
         "slot 2, 0x8e1ab8 1, 0x8e1ab9 1, card state 4, latch 0");
}

Screen accepted(std::uint8_t flag) {
  Screen s;
  s.w.browser_stage = 1;
  s.w.card_state = CARD_LOAD;
  s.w.restore_flag = flag;
  s.w.selected_index = 1;
  s.w.latch = 0xa5;
  s.w.directory.resize(2);
  // flow:504-523: the whole 0x40 bytes, past the name too.
  const std::string name = "BASLUS-20973_slot1";
  auto &r = s.w.directory[1];
  for (std::size_t i = 0; i < DIRECTORY_RECORD; ++i)
    r[i] = static_cast<std::uint8_t>(i + 1);
  std::memcpy(r.data() + DIRECTORY_NAME_AT, name.c_str(), name.size() + 1);
  return s;
}

void load_flow() {
  Screen s = accepted(1);
  auto c = s.accept_load();
  expect(c.committed && s.w.load_record == s.w.directory[1], "acceptance copies all 0x40 bytes to 0x88d2c0");
  expect(s.w.slot_stage == 1 && s.w.step == 0 && s.w.latch == 1 && s.w.fade_step == LOAD_ACCEPT_FADE,
         "acceptance: stage 1, step 0, latch stored 1 over 0xa5, fade step 16");
  c = s.queue_load_read();
  const std::vector<CardOp> want{{OP_OPEN, "BASLUS-20973_slot1", LOAD_OPEN_MODE}, {OP_READ, {}, 0x4bc78}, {OP_CLOSE, {}, 0}};
  expect(c.ops == want && s.w.step == 2, "step 2: open mode 1, read 0x4bc78, close");
  c = s.call(0);
  expect(c.ops.empty() && s.w.step == 2, "step 2 waits for the host read");
  s.w.live_level = "origin";
  refused([&] { (void)s.load_read_complete(Bytes(0x10, 0)); }, "a short read is refused");
  const Bytes image = synthetic_image("strife", "");
  bool restored = false;
  s.services.restore = [&](const Bytes &b) { restored = b == image; };
  c = s.load_read_complete(image);
  expect(c.restored && restored, "flag 1 calls save_load");
  expect(s.w.pending_level == "strife" && s.w.side_name == "strife", "strife: pending and side names");
  expect(s.w.pending_marker == PENDING_MARKER && s.w.loaded_flag == 1, "marker 0xfd, 0x8e1ad6 1");
  expect(s.w.live_level == "origin", "the live name is left as it was");
  expect(s.w.image == image, "directory bytes and the +0x16118 word are left for the level loader");
  expect(s.w.step == 5 && s.w.latch == 1 && s.w.card_state == CARD_LOAD, "step 5, latch 1, card state 4");
  (void)s.call(0);
  expect(s.w.step == 5 && s.w.latch == 1 && s.w.card_state == CARD_LOAD, "a different live name waits");
  s.w.live_level = "strife";
  (void)s.call(0);
  expect(s.w.latch == 0 && s.w.card_state == CARD_IDLE, "the live name matches: -1 and latch clear");

  Screen h = accepted(1);
  (void)h.accept_load();
  (void)h.queue_load_read();
  h.w.live_level = "hub";
  (void)h.load_read_complete(synthetic_image("hub", "west"));
  expect(h.w.pending_level == "hub_west" && h.w.side_name == "hub_west", "hub + west: hub_west");
  (void)h.call(0);
  expect(h.w.card_state == CARD_LOAD, "hub alone does not match hub_west");
  h.w.live_sub = "west";
  (void)h.call(0);
  expect(h.w.card_state == CARD_IDLE && h.w.latch == 0, "hub + west matches hub_west");

  Screen z = accepted(0);
  (void)z.accept_load();
  (void)z.queue_load_read();
  z.w.latch = 0xa5;
  bool called = false;
  z.services.restore = [&](const Bytes &) { called = true; };
  c = z.load_read_complete(synthetic_image("strife", ""));
  expect(!c.restored && !called && z.w.pending_level.empty(), "flag 0 does not call save_load");
  expect(z.w.latch == 0 && z.w.card_state == CARD_IDLE && z.w.step == 0, "flag 0 clears 0xa5, -1, step 0");

  Screen busy = accepted(1);
  busy.w.pump_busy = true;
  unknown([&] { (void)busy.accept_load(); }, "acceptance with a busy pump was not executed");
  Screen two = accepted(2);
  (void)two.accept_load();
  (void)two.queue_load_read();
  unknown([&] { (void)two.load_read_complete(synthetic_image("strife", "")); }, "0x8e1ab9 = 2 was not executed");
  Screen empty = accepted(1);
  (void)empty.accept_load();
  (void)empty.queue_load_read();
  unknown([&] { (void)empty.load_read_complete(Bytes(IMAGE_BYTES, 0)); }, "an empty header name was not executed");
  Screen step0 = accepted(1);
  (void)step0.accept_load();
  unknown([&] { (void)step0.call(0); }, "load step 0 through the dispatcher was not executed");
}

void refusals() {
  Screen s = confirm_case(0, 0, 0, 0, 2);
  s.w.card_state = CARD_LOAD;
  s.w.slot_state = SLOT_LIST;
  unknown([&] { (void)s.call(MASK_CONFIRM); }, "the load list's own confirm path was not executed");
  Screen b;
  b.w.card_state = CARD_SAVE;
  unknown([&] { (void)b.call(0); }, "memcard_browser is not ported");
  Screen idle;
  idle.w.browser_stage = 1;
  refused([&] { (void)idle.call(0); }, "the dispatcher runs for card states 3 and 4 only");
  unknown([&] { (void)confirm_case(0, 3, 3, 0, 2); }, "a confirm past the free row was not executed");
  Screen odd = save_screen(6);
  unknown([&] { (void)odd.call(0); }, "save steps outside 0-4 and 9 were not executed");
  expect(qualified_level("hub", "west") == "hub_west" && qualified_level("strife", "") == "strife",
         "0x1ec0f0 qualified names");
}
} // namespace

int main() {
  try {
    confirm_settle();
    free_row_and_space();
    overwrite_and_commit();
    slot_naming();
    save_write();
    saved_box();
    card_error();
    requests();
    load_flow();
    refusals();
  } catch (const std::exception &e) {
    std::cerr << "FAIL after " << checks << " checks: " << e.what() << "\n";
    return 1;
  }
  std::cout << "PASS memcard-screen: " << checks << " checks\n";
  return 0;
}
