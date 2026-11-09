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
  // A byte pattern the module cannot make itself: the screen must keep the
  // service's return. The level name in the image is save_build's own result
  // (doc:229) and belongs to the save_build port, not this module.
  Bytes pattern(IMAGE_BYTES);
  for (std::size_t i = 0; i < pattern.size(); ++i)
    pattern[i] = static_cast<std::uint8_t>((i * 131u + 17u) ^ (i >> 9));
  s.services.save_build = [&](std::uint8_t a, std::uint8_t b) {
    calls.push_back({{a, b}});
    return pattern;
  };
  s.services.pack_local_player = [] { throw std::runtime_error("offline save packed"); };
  auto c = s.call(0);
  expect(c.save_built && !c.packed && c.build_args == std::array<std::uint8_t, 2>{{3, 0}},
         "offline step 1 calls save_build(slot byte, 0)");
  expect(calls.size() == 1 && calls[0] == std::array<std::uint8_t, 2>{{3, 0}}, "save_build recorded (3, 0)");
  expect(s.w.image == pattern, "step 1 stores save_build's return in 0x895580");
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

  // flow:589-599: a new row (index 0, count 0) after save_filename(fresh=True),
  // which set 0x8e1ad8 (flow:181, 591).
  Screen n = save_screen(4);
  n.w.fresh_name = 1;
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

// difftest_save_screen.py:21-46 fixture(): settled save list, count 2, row 0.
Screen list_fixture() {
  Screen s;
  s.w.browser_stage = 1;
  s.w.card_state = CARD_SAVE;
  s.w.slot_state = SLOT_LIST;
  s.w.cache_count = 2;
  s.w.countdown = 10;
  s.w.fade_level = 0x7f;
  s.w.fade_step = 8;
  s.w.list_action = 0xa5;
  return s;
}

void state4_inputs() {
  // PSS-1. doc:169; ref:115-122; difftest_save_screen.py:334-337.
  auto d = list_fixture();
  (void)d.call(MASK_COMMIT);
  expect(d.w.slot_state == SLOT_DELETE && d.w.settled_row == 0 && d.w.write_intent == 0,
         "(4, 0x80): state 9, 0x879e24 0, 0x88d400 0");
  expect(d.w.list_action == 0xa5 && d.w.fade_step == 8, "(4, 0x80) leaves 0x4f8ef4 and the fade step");
  (void)d.call(0);
  expect(d.w.slot_state == SLOT_DELETE, "(9, 0) stays in 9");
  (void)d.call(MASK_CANCEL);
  expect(d.w.slot_state == SLOT_LIST, "(9, 0x20) returns to 4");
  auto dc = list_fixture();
  (void)dc.call(MASK_COMMIT);
  unknown([&] { (void)dc.call(MASK_CONFIRM); }, "the delete confirm fade 0x21d360 was not executed");
  dc.w.slot_state = SLOT_DELETE;
  unknown([&] { (void)dc.call(MASK_COMMIT); }, "state 9 bit 0x80 was not executed");

  auto r = list_fixture();
  r.w.selected_index = 7;
  (void)r.call(MASK_REPEAT);
  expect(r.w.slot_state == SLOT_LIST && r.w.settled_row == 1 && r.w.write_intent == 0,
         "(4, 0x204000): state 4, 0x879e24 = 1, 0x88d400 0");
  expect(r.w.selected_row == 0 && r.w.list_action == 0xa5 && r.w.fade_step == 8 && r.w.selected_index == 7,
         "the repeat writes 0x879e24 only");
  expect(r.call(MASK_CONFIRM).ops.empty() && r.w.slot_state == SLOT_LIST,
         "after the repeat the rows differ, so a confirm waits for the settle");
  for (std::uint32_t bit : {0x4000u, 0x200000u}) {
    auto one = list_fixture();
    (void)one.call(bit);
    expect(one.w.settled_row == 1, "either bit of the repeat mask 0x204000 advances");
  }
  auto last = list_fixture();
  last.w.settled_row = last.w.selected_row = 1;
  unknown([&] { (void)last.call(MASK_REPEAT); }, "the repeat on the last cached row was not executed");
  auto scrolled = list_fixture();
  scrolled.w.cache_count = 4;
  scrolled.w.scroll_base = scrolled.w.scroll_target = 1;
  unknown([&] { (void)scrolled.call(MASK_REPEAT); }, "the repeat with a scroll was not executed");

  auto cancel = list_fixture();
  unknown([&] { (void)cancel.call(MASK_CANCEL); }, "the state-4 cancel back fade was not executed");
  expect(cancel.w.slot_state == SLOT_LIST && cancel.w.fade_step == 8 && cancel.w.list_action == 0xa5,
         "the refused cancel writes nothing");
  auto both = list_fixture();
  unknown([&] { (void)both.call(MASK_CONFIRM | 0x4000u); }, "0x40 | 0x4000 is not acted on as a confirm");
  expect(both.w.slot_state == SLOT_LIST && both.w.list_action == 0xa5 && both.w.fade_step == 8 &&
             both.w.settled_row == 0,
         "the combined press arms nothing and moves nothing");
  auto both_free = list_fixture();
  both_free.w.cache_count = 0;
  unknown([&] { (void)both_free.call(MASK_CONFIRM | 0x200000u); }, "0x40 | 0x200000 on the free row");
  expect(both_free.w.list_action == 0xa5, "the combined press does not arm the free row");
  auto mixed = list_fixture();
  unknown([&] { (void)mixed.call(MASK_CONFIRM | MASK_COMMIT); }, "0x40 | 0x80 was not executed");

  auto unsettled = list_fixture();
  unsettled.w.cursor_float = 0x3f000000;
  unknown([&] { (void)unsettled.call(MASK_COMMIT); }, "0x80 with an unsettled cursor was not executed");
  unknown([&] { (void)unsettled.call(MASK_REPEAT); }, "0x204000 with an unsettled cursor was not executed");
  auto free_delete = list_fixture();
  free_delete.w.cache_count = 0;
  unknown([&] { (void)free_delete.call(MASK_COMMIT); }, "0x80 on the free row was not executed");
  auto load_list = list_fixture();
  load_list.w.card_state = CARD_LOAD;
  unknown([&] { (void)load_list.call(MASK_COMMIT); }, "0x80 on the load list was not executed");
  unknown([&] { (void)load_list.call(MASK_REPEAT); }, "0x204000 on the load list was not executed");
}

void saved_key_discriminator() {
  // PSS-2. The byte and index == count agreed in both executed cases
  // (flow:562, 581-586 and 591-599); a split throws.
  Screen clear_new = save_screen(4);
  clear_new.w.fade_step = 8;
  unknown([&] { (void)clear_new.call(0); }, "step 4 with 0x8e1ad8 clear and index == count");
  Screen set_old = save_screen(4);
  set_old.w.fresh_name = 1;
  set_old.w.selected_index = 1;
  set_old.w.cache_count = 2;
  unknown([&] { (void)set_old.call(0); }, "step 4 with 0x8e1ad8 set and index != count");
  Screen old_row = save_screen(4);
  old_row.w.selected_index = 1;
  old_row.w.cache_count = 2;
  expect(old_row.call(0).keys.back() == KEY_OVERWRITE_GOOD, "byte clear, index != count: OverwriteGood");
}

void saved_flag_stale() {
  // PSS-3. flow:220-229 ran step 3 with 0x4f8f29 at 0; Savepoint_run clears it
  // and it can stay up across saves (native-soak.md:547, 572).
  Screen stale = save_screen(3);
  stale.w.saved_flag = 1;
  stale.w.fade_level = 0;
  stale.w.fade_step = 4;
  unknown([&] { (void)stale.call(0); }, "step 3 with 0x4f8f29 already set was not executed");
  expect(stale.w.step == 3 && stale.w.fade_step == 4, "the stale flag does not skip to step 4");

  Screen s = save_screen(3);
  s.w.selected_index = 1;
  s.w.cache_count = 2;
  s.w.fade_level = 0;
  (void)s.call(0);
  expect(s.w.saved_flag == 1 && s.w.saved_step_armed && s.w.step == 3, "the arm is the native flag");
  (void)s.call(0);
  expect(s.w.step == 4 && !s.w.saved_step_armed && s.w.saved_flag == 1,
         "fade 0 enters step 4; the screen does not clear 0x4f8f29");
  // A second save in the same Screen with the flag still up.
  s.w.step = 3;
  unknown([&] { (void)s.call(0); }, "a second save with 0x4f8f29 still up was not executed");
  s.w.saved_flag = 0; // what Savepoint_run does (native-soak.md:547)
  (void)s.call(0);
  expect(s.w.saved_flag == 1 && s.w.fade_step == FADE_OUT, "after the host clears it, step 3 arms again");
}

void net_step2() {
  // PSS-4. The net path was executed only to step 2's entry (flow:602-608).
  Screen net = save_screen(1);
  net.w.network = 1;
  net.w.image = Bytes(IMAGE_BYTES, 0x3c); // a buffer left by an earlier load
  net.services.pack_local_player = [] {};
  (void)net.call(0);
  expect(net.w.step == 2, "the net byte reaches step 2");
  Call c;
  unknown([&] { c = net.call(0); }, "net-path step 2 was not executed");
  expect(c.ops.empty() && net.w.step == 2, "no write of the old buffer is queued");

  Screen planted = save_screen(2);
  planted.w.image = Bytes(IMAGE_BYTES, 0x3c);
  unknown([&] { (void)planted.call(0); }, "offline step 2 without this save's save_build");
}

void overwrite_row_one() {
  // PSS-6. Row 1 of 2 with 0x88d3b0 planted to 7: the commit must write it.
  auto s = confirm_case(0, 1, 1, 0, 2);
  expect(s.w.slot_state == SLOT_OVERWRITE, "row 1 of 2 enters 5");
  s.w.selected_index = 7;
  (void)s.call(MASK_COMMIT);
  (void)s.call(0);
  s.w.fade_level = 0;
  s.w.fade_step = 0;
  auto c = s.call(0);
  expect(c.committed && s.w.selected_index == 1, "the commit writes row 1 over the planted 7");
}

void load_latch_flag1() {
  // PSS-7. doc:267-268; flow:473, 687: flag 1 leaves the latch 1 over 0xa5.
  Screen s = accepted(1);
  (void)s.accept_load();
  (void)s.queue_load_read();
  s.w.latch = 0xa5;
  (void)s.load_read_complete(synthetic_image("strife", ""));
  expect(s.w.latch == 1 && s.w.step == 5, "flag 1 stores the latch 1 over 0xa5");
}

void card_error_living() {
  // PSS-8. flow:248 plants a living player in every corner; slot stage 1 is
  // what keeps the reopened request closed (doc:238-240; flow:624).
  for (bool network : {false, true}) {
    Screen s = save_screen(2);
    s.w.latch = 1;
    s.w.card_result = 1;
    s.w.counted_living = 1;
    (void)s.call(0);
    s.w.card_result = 0;
    s.w.network = network ? 1 : 0;
    auto c = s.call(MASK_CANCEL);
    expect(c.reopen_called && s.w.card_state == CARD_IDLE && s.w.slot_stage == 1,
           "a living player and stage 1: the request is called and stays closed");
  }
  Screen bad = save_screen(9);
  bad.w.counted_living = -1;
  refused([&] { (void)bad.call(MASK_CANCEL); }, "a negative counted total is refused");
  // PSS2-3: the count is checked before the -1 store, so the refusal writes nothing.
  expect(bad.w.card_state == CARD_SAVE && bad.w.step == 9 && bad.w.slot_stage == 1,
         "the refused cancel leaves card state 3, step 9 and stage 1");
}

void slot_name_named_count0() {
  // PSS-9. Every count-0 case had an empty directory (flow:181-185, 549-551).
  for (std::uint8_t fresh : {std::uint8_t{0}, std::uint8_t{1}}) {
    Words w;
    w.fresh_name = fresh;
    w.directory = {record("BASLUS-20973_slot0")};
    unknown([&] { (void)make_slot_name(w); }, "count 0 with a named record was not executed");
  }
  Words prefix;
  prefix.fresh_name = 1;
  prefix.directory.resize(1);
  std::memcpy(prefix.directory[0].data() + DIRECTORY_NAME_AT + 1, "BASLUS-20973", 12); // 0x88d061 (flow:185)
  expect(make_slot_name(prefix) == SLOT0_NAME, "the probe's 0x88d061 prefix leaves record 0's name byte clear");
}

void countdown_negative() {
  // PSS-10. Only 0 and 10 were planted (flow:350, 374, 395).
  Screen t = free_row_list();
  t.w.slot_state = SLOT_SPACE;
  t.w.countdown = -1;
  t.w.fade_step = 8;
  unknown([&] { (void)t.call(0); }, "a negative 0x88d3c4 was not executed");
}

// Load `image` into an existing Screen through the executed load path
// (flag 0: no save_load), as a host would after leaving a save.
void load_into(Screen &s, const Bytes &image) {
  s.w.card_state = CARD_IDLE;
  s.w.slot_stage = 0;
  s.w.open_block = 0;
  expect(s.request_load(1, 0, 0), "the load opens");
  s.w.selected_index = 1;
  s.w.directory = accepted(0).w.directory;
  (void)s.accept_load();
  (void)s.queue_load_read();
  (void)s.load_read_complete(image);
  expect(s.w.image == image && s.w.card_state == CARD_IDLE, "0x895580 holds the loaded bytes");
}

void aborted_arm_resave() {
  // PSS2-1. A write fails during step 3's fade wait (flow:245-252 corner,
  // reached from an armed step 3), then the same Screen saves again.
  Screen s = save_screen(3);
  s.w.selected_index = 1;
  s.w.cache_count = 2;
  s.w.latch = 1;
  s.w.fade_level = 0x10;
  (void)s.call(0);
  expect(s.w.saved_step_armed && s.w.saved_flag == 1 && s.w.fade_step == FADE_OUT, "step 3 arms");
  (void)s.call(0);
  expect(s.w.step == 3, "step 3 waits for the fade");
  s.w.card_result = 1;
  auto c = s.call(0);
  expect(s.w.step == 9 && c.closed_file && !s.w.saved_step_armed && !s.w.saved_box_dismissing,
         "the card-error divert clears the step-3 arm");
  s.w.card_result = 0;
  s.w.counted_living = 1;
  (void)s.call(MASK_CANCEL);
  expect(s.w.card_state == CARD_IDLE, "cancel stores -1");

  // The same Screen saves again with 0x4f8f29 still up: step 3 must not skip
  // the stale-byte check and the arm.
  s.w.card_state = CARD_SAVE;
  s.w.step = 3;
  s.w.fade_level = 0;
  s.w.fade_step = 4;
  unknown([&] { (void)s.call(0); }, "after an aborted arm, step 3 with 0x4f8f29 still up throws");
  expect(s.w.step == 3 && s.w.fade_step == 4, "the aborted arm does not let step 3 enter step 4");
  s.w.saved_flag = 0; // Savepoint_run (native-soak.md:547)
  (void)s.call(0);
  expect(s.w.step == 3 && s.w.saved_flag == 1 && s.w.fade_step == FADE_OUT && s.w.saved_step_armed,
         "after the host clears 0x4f8f29, step 3 arms again with -16");
  (void)s.call(0);
  expect(s.w.step == 4 && !s.w.saved_step_armed, "then fade 0 enters step 4");

  // A confirmed saved box aborted by a card error must not close a later box
  // without its confirm.
  Screen d = save_screen(4);
  d.w.selected_index = 1;
  d.w.cache_count = 2;
  d.w.latch = 1;
  d.w.fade_level = 0x10;
  (void)d.call(MASK_CONFIRM);
  expect(d.w.saved_box_dismissing, "the saved box's confirm starts the dismiss");
  d.w.card_result = 1;
  (void)d.call(0);
  expect(d.w.step == 9 && !d.w.saved_box_dismissing, "the divert clears the dismiss");
  d.w.card_result = 0;
  (void)d.call(MASK_CANCEL);
  d.w.card_state = CARD_SAVE;
  d.w.step = 4;
  d.w.latch = 1;
  d.w.fade_level = 0;
  (void)d.call(0);
  expect(d.w.card_state == CARD_SAVE && d.w.latch == 1, "a later box at fade 0 stays until its confirm");

  // Every save start clears links a host left planted.
  Screen n;
  n.w.saved_step_armed = n.w.saved_box_dismissing = true;
  expect(n.request_save(1) && !n.w.saved_step_armed && !n.w.saved_box_dismissing,
         "memcard_requestSave opening a save clears the links");
  auto k = confirm_case(0, 0, 0, 0, 2);
  (void)k.call(MASK_COMMIT);
  (void)k.call(0);
  k.w.fade_level = 0;
  k.w.fade_step = 0;
  k.w.saved_step_armed = k.w.saved_box_dismissing = true;
  expect(k.call(0).committed && k.w.step == 0 && !k.w.saved_step_armed && !k.w.saved_box_dismissing,
         "the committed slot starts a save with no link set");
  Screen z = save_screen(0);
  z.w.fresh_name = 1;
  z.w.saved_step_armed = z.w.saved_box_dismissing = true;
  (void)z.call(0);
  expect(z.w.step == 1 && !z.w.saved_step_armed && !z.w.saved_box_dismissing, "step 0 clears the links");
}

void stale_build_then_load() {
  // PSS2-1. Step 1 builds, the step-2 call hits a card error, a load then
  // replaces 0x895580, and a host-planted step 2 must not write those bytes.
  const Bytes loaded = synthetic_image("strife", "");
  auto built = [] {
    Screen s = save_screen(1);
    s.services.save_build = [](std::uint8_t, std::uint8_t) { return Bytes(IMAGE_BYTES, 0x11); };
    (void)s.call(0);
    expect(s.w.step == 2, "step 1 builds and moves to step 2");
    return s;
  };
  auto planted_step2 = [&](Screen &s, const char *name) {
    s.w.card_state = CARD_SAVE;
    s.w.slot_stage = 1;
    s.w.step = 2;
    s.w.network = 0;
    Call c;
    unknown([&] { c = s.call(0); }, name);
    expect(c.ops.empty() && s.w.step == 2 && s.w.image == loaded, "no write of the loaded buffer is queued");
  };

  Screen a = built();
  a.w.card_result = 1;
  (void)a.call(0);
  expect(a.w.step == 9, "the step-2 call diverts to step 9");
  a.w.card_result = 0;
  (void)a.call(MASK_CANCEL);
  load_into(a, loaded);
  planted_step2(a, "after a card error and a load, a planted step 2 throws");

  Screen b = built(); // the host leaves the save at step 2 and loads
  load_into(b, loaded);
  planted_step2(b, "after a load over an unwritten build, a planted step 2 throws");

  Screen c = built();
  c.w.card_result = 1;
  (void)c.call(0);
  c.w.card_result = 0;
  c.w.image = loaded; // the buffer replaced by anything but this save's step 1
  planted_step2(c, "after a card error, a planted step 2 over another buffer throws");
}

void delete_with_scroll() {
  // PSS2-2. Bit 0x80 was executed with scroll 0 only (difftest_save_screen.py:21-46, 334).
  auto s = list_fixture();
  s.w.scroll_base = s.w.scroll_target = 1; // scroll 1, row 0, count 2: index 1 is populated
  unknown([&] { (void)s.call(MASK_COMMIT); }, "state 4 bit 0x80 with a nonzero scroll was not executed");
  expect(s.w.slot_state == SLOT_LIST && s.w.settled_row == 0 && s.w.write_intent == 0 &&
             s.w.list_action == 0xa5 && s.w.fade_step == 8,
         "the refused 0x80 writes nothing");
}

void two_words() {
  // PSS2-4. The slot list reads edge +0x0c and repeat +0x10 (doc:177-179);
  // states 4, 5 and 9 ran with both equal (difftest_save_screen.py:33-34).
  auto eq = list_fixture();
  (void)eq.call(MASK_COMMIT, MASK_COMMIT);
  expect(eq.w.slot_state == SLOT_DELETE, "equal words: (4, 0x80) enters 9");
  auto split = list_fixture();
  unknown([&] { (void)split.call(MASK_COMMIT, 0); }, "state 4 with edge 0x80 and repeat 0");
  expect(split.w.slot_state == SLOT_LIST && split.w.settled_row == 0, "the split press writes nothing");
  auto rep = list_fixture();
  unknown([&] { (void)rep.call(0, MASK_REPEAT); }, "state 4 with edge 0 and repeat 0x204000");
  expect(rep.w.settled_row == 0 && rep.w.slot_state == SLOT_LIST, "the split repeat moves nothing");
  auto conf = list_fixture();
  unknown([&] { (void)conf.call(MASK_CONFIRM, 0); }, "state 4 with edge 0x40 and repeat 0");
  expect(conf.w.slot_state == SLOT_LIST && conf.w.list_action == 0xa5, "the split confirm arms nothing");
  auto ov = confirm_case(0, 0, 0, 0, 2);
  unknown([&] { (void)ov.call(MASK_COMMIT, 0); }, "state 5 with split words");
  expect(ov.w.slot_state == SLOT_OVERWRITE && ov.w.write_intent == 0, "state 5 split writes nothing");
  auto del = list_fixture();
  (void)del.call(MASK_COMMIT);
  unknown([&] { (void)del.call(MASK_CANCEL, 0); }, "state 9 with split words");
  expect(del.w.slot_state == SLOT_DELETE, "state 9 split stays in 9");
  // State 4 with the free row armed reads no mask; flow:377-391 ran it with
  // edge 0 and repeat 0x40.
  auto fr = free_row_list();
  (void)fr.call(MASK_CONFIRM, MASK_CONFIRM);
  expect(fr.w.list_action == FREE_ROW_ACTION, "equal words arm the free row");
  fr.w.fade_level = 0;
  fr.w.fade_step = 0;
  auto c = fr.call(0, MASK_CONFIRM);
  expect(fr.w.slot_state == SLOT_SPACE && c.ops.size() == 1 && c.ops[0].op == OP_SPACE,
         "armed free row with edge 0, repeat 0x40: operation 8 and state 7");
  // The save steps were run with the edge word only (flow:234, 258).
  Screen box = save_screen(4);
  box.w.fresh_name = 1;
  box.w.fade_level = 0x10;
  (void)box.call(MASK_CONFIRM, 0);
  expect(box.w.saved_box_dismissing && box.w.fade_step == FADE_OUT, "step 4 reads the edge word");
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
    state4_inputs();
    saved_key_discriminator();
    saved_flag_stale();
    net_step2();
    overwrite_row_one();
    load_latch_flag1();
    card_error_living();
    slot_name_named_count0();
    countdown_negative();
    aborted_arm_resave();
    stale_build_then_load();
    delete_with_scroll();
    two_words();
  } catch (const std::exception &e) {
    std::cerr << "FAIL after " << checks << " checks: " << e.what() << "\n";
    return 1;
  }
  std::cout << "PASS memcard-screen: " << checks << " checks\n";
  return 0;
}
