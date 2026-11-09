#include "champions/memcard_screen.hpp"

#include "champions/ps2_save.hpp"

#include <algorithm>
#include <cstring>

// Citations are t3/research-save-screen c8895e2: docs/save-screen.md (doc:N),
// tools/difftest_save_screen_flow.py (flow:N) and tools/ref_save_screen.py (ref:N).
namespace champions::frontend::memcard {

namespace ps2 = champions::simulation::rta::ps2_save;
static_assert(IMAGE_BYTES == ps2::FILE_SIZE, "one slot image is the 0x4bc78-byte ps2_save file");

namespace {

std::string cstr(const std::uint8_t *data, std::size_t room) {
  const auto *end = std::find(data, data + room, std::uint8_t{0});
  return std::string(reinterpret_cast<const char *>(data), static_cast<std::size_t>(end - data));
}

std::string record_name(const std::array<std::uint8_t, DIRECTORY_RECORD> &record) {
  return cstr(record.data() + DIRECTORY_NAME_AT, DIRECTORY_RECORD - DIRECTORY_NAME_AT);
}

bool directory_empty(const Words &w) {
  for (const auto &record : w.directory)
    if (record[DIRECTORY_NAME_AT] != 0)
      return false;
  return true;
}

// c.eq.s against $f0 = 0 at 0x21caa4 (doc:193; audit r2-fix RSS-1).
bool cursor_settled(std::uint32_t bits) {
  float value = 0;
  std::memcpy(&value, &bits, sizeof value);
  return value == 0.0f;
}

} // namespace

std::string make_slot_name(const Words &w) {
  // doc:228; flow:176-193, 534-559. A set 0x8e1ad8 was executed only with count 0.
  // Every executed case had an empty directory (record 0's name byte 0x88d060
  // clear; flow:181-185, 549-551), so a named record throws for every count.
  if (!directory_empty(w))
    throw Unknown("memcard_makeSlotName 0x21effc with a named directory record was not executed "
                  "(save-screen.md:228; difftest_save_screen_flow.py:534-559)");
  if (w.cache_count == 0)
    return SLOT0_NAME;
  if (w.fresh_name == 0 && w.cache_count >= 1 && w.cache_count <= 3)
    return SLOT1_NAME; // why a nonempty count selects slot 1 is Unknown (doc:228)
  throw Unknown("memcard_makeSlotName 0x21effc: only count 0, or counts 1-3 with an empty "
                "directory and a clear 0x8e1ad8, were executed (save-screen.md:228)");
}

std::string qualified_level(const std::string &name, const std::string &sub) {
  return ps2::qualified_name(name, sub);
}

std::string header_level(const Bytes &image) {
  if (image.size() != IMAGE_BYTES)
    throw Refusal("slot image is " + std::to_string(image.size()) + " bytes, not 0x4bc78");
  const auto name = cstr(image.data(), 0x14);
  if (name.empty())
    throw Unknown("save_load with an empty header level name was not executed (save-screen.md:268-271)");
  return qualified_level(name, cstr(image.data() + 0x14, 0x50));
}

void Screen::reset_save_links() {
  w.saved_step_armed = false;
  w.saved_box_dismissing = false;
  image_built_ = false;
}

void Screen::require_pump_idle(const char *where) const {
  if (w.pump_busy)
    throw Unknown(std::string(where) + ": every executed fixture had mc_pump 0x2233e4 return 0 "
                  "(save-screen.md:200, 210-211, 219, 261)");
}

bool Screen::request_save(int counted_living) {
  if (counted_living < 0)
    throw Refusal("a counted living player total cannot be negative");
  // doc:246-251; flow:273-285, 629-630. Negative hit points and a set bit 3 were read,
  // not executed, so the host supplies the count it would make.
  if (w.card_state != CARD_IDLE || w.open_block != 0 || w.slot_stage != 0 || counted_living == 0)
    return false;
  w.card_state = CARD_SAVE;
  reset_save_links(); // a new save starts
  return true;
}

bool Screen::request_load(std::uint8_t slot, std::uint8_t flag, std::uint8_t in_frontend) {
  // doc:251-256; flow:286-303, 631-634. 0x88d3c8 set returns before any store; the
  // latch sentinel survives.
  if (w.open_block != 0)
    return false;
  w.slot_byte = slot;
  w.from_frontend = in_frontend;
  w.restore_flag = flag;
  w.latch = 0; // sb $zero at 0x217760
  w.card_state = CARD_LOAD;
  read_queued_ = false;
  reset_save_links(); // a load leaves any save; its bytes must never be written as one
  return true;
}

Call Screen::call(std::uint32_t edge, std::uint32_t repeat) {
  if (w.card_state != CARD_SAVE && w.card_state != CARD_LOAD)
    throw Refusal("the save/load dispatcher 0x2189e0 runs for card state 3 or 4 only");
  if (w.browser_stage == 0)
    throw Unknown("memcard_browser 0x219b90 (card check, format, errors) is not ported: "
                  "retail card I/O is refused (save-screen.md:27-29)");
  if (w.slot_stage == 0) {
    Call out = slot_list(edge, repeat);
    if (out.committed && w.card_state == CARD_SAVE) {
      // The committed slot (doc:220-221) sets the latch (doc:31, 0x218ba4) and
      // moves to the screen step. Native handoff: the index the list chose is
      // the row it compared with the count (doc:202); the original writer of
      // 0x88d3b0 was planted by the probes.
      w.selected_index = w.scroll_base + w.selected_row;
      w.latch = 1;
      w.slot_stage = 1;
      w.step = 0;
      reset_save_links(); // a new save starts at step 0
    }
    return out;
  }
  if (w.slot_stage != 1)
    throw Unknown("slot stage 0x88d3d8 outside 0 and 1 was not executed");
  return w.card_state == CARD_SAVE ? save_step(edge) : load_step();
}

Call Screen::slot_list(std::uint32_t edge, std::uint32_t repeat_word) {
  require_pump_idle("memcard_slotList 0x21c3a4");
  // The list reads edge +0x0c and repeat +0x10 (doc:177-179, 0x21c478-0x21c4ac).
  // States 4, 5 and 9 read the masks and were executed with both words equal
  // (difftest_save_screen.py:33-34), so which word each mask is tested
  // against is Unknown; a difference throws before any store. State 4 with
  // the free row armed reads no mask and was executed with edge 0 and repeat
  // 0x40 (difftest_save_screen_flow.py:377-391), so it is not refused.
  const bool reads_masks = (w.slot_state == SLOT_LIST && w.list_action != FREE_ROW_ACTION) ||
                           w.slot_state == SLOT_OVERWRITE || w.slot_state == SLOT_DELETE;
  if (reads_masks && edge != repeat_word)
    throw Unknown("slot state " + std::to_string(w.slot_state) +
                  " was executed with the edge and repeat words equal; which word each mask is "
                  "tested against was not separated (difftest_save_screen.py:33-34)");
  Call out;
  switch (w.slot_state) {
  case SLOT_LIST: {
    // 0x21c9dc-0x21c9f8: a scroll mismatch copies 0x879e30 over 0x879e2c (doc:100-101, 206-207).
    if (w.scroll_base != w.scroll_target)
      w.scroll_base = w.scroll_target;
    // 0x21ca2c/0x21ca3c: the 1.0 snap clears the float and copies the settled row (doc:197-199).
    if (w.cursor_float == CURSOR_SNAP) {
      w.cursor_float = 0;
      w.selected_row = w.settled_row;
    }
    if (w.list_action == FREE_ROW_ACTION) {
      // When the fade reaches 0 the list queues operation 8 and enters 7 (doc:207-208, 0x21cce8).
      if (w.fade_level == 0) {
        out.ops.push_back({OP_SPACE, {}, 0});
        w.slot_state = SLOT_SPACE;
      }
      return out;
    }
    // The list's four inputs (doc:169; ref:115-122). difftest_save_screen.py:334-335
    // executed each of 0x204000, 0x40 and 0x80 alone, and no state-4 0x20.
    const bool repeat = (edge & MASK_REPEAT) != 0, confirm = (edge & MASK_CONFIRM) != 0,
               remove = (edge & MASK_COMMIT) != 0, cancel = (edge & MASK_CANCEL) != 0;
    const int pressed = int(repeat) + int(confirm) + int(remove) + int(cancel);
    if (pressed == 0)
      return out; // (4, 0) leaves the list (difftest_save_screen.py:334; ref:122)
    if (pressed > 1)
      throw Unknown("slot state 4 was executed with one of 0x204000, 0x40, 0x80 and 0x20 at a time; "
                    "a combined press was not (difftest_save_screen.py:334-335)");
    if (cancel)
      throw Unknown("the state-4 cancel 0x20 back fade 0x21ccc0 is named (save-screen.md:169) but "
                    "was not executed; no probe ran (4, 0x20) (difftest_save_screen.py:334-335)");
    const bool settled = cursor_settled(w.cursor_float) && w.settled_row == w.selected_row;
    if (repeat || remove) {
      // Executed only in the settled save-mode fixture: card state 3, the float
      // and both row words 0, scroll 0, count 2 (difftest_save_screen.py:21-46, 71-74).
      if (!settled)
        throw Unknown("state 4 0x204000 / 0x80 with an unsettled cursor was not executed "
                      "(difftest_save_screen.py:21-46)");
      if (w.card_state != CARD_SAVE)
        throw Unknown("state 4 0x204000 / 0x80 on the load list was not executed (the fixture "
                      "ran card state 3, difftest_save_screen.py:27)");
      // Both were executed with scroll 0 only (difftest_save_screen.py:21-46, 334).
      if (w.scroll_base != 0)
        throw Unknown(std::string("the state-4 ") + (repeat ? "repeat" : "bit 0x80") +
                      " with a nonzero scroll 0x879e2c was not executed (difftest_save_screen.py:21-46)");
      const std::int32_t at = w.scroll_base + w.selected_row;
      if (repeat) {
        // ref:116-117: 0x879e24 becomes the selection + 1 when another cached
        // slot exists; executed from row 0 of 2 with no scroll.
        if (at + 1 >= w.cache_count)
          throw Unknown("the state-4 repeat on the last cached row was not executed "
                        "(save-screen.md:169; difftest_save_screen.py:334)");
        w.settled_row = w.selected_row + 1; // 0x879e24; 0x879e28 follows by the settle
        return out;
      }
      // Bit 0x80 on a populated row enters 9 (doc:169; ref:120-121; executed on row 0).
      if (at < 0 || at >= w.cache_count)
        throw Unknown("state 4 bit 0x80 off a populated row was not executed (difftest_save_screen.py:334)");
      w.slot_state = SLOT_DELETE;
      return out;
    }
    // 0x21caa4: confirm is read only with the float at 0 and the two row words
    // equal. 0x3f000000, or unequal words, leave slot state, 0x4f8ef4 and the
    // fade step alone (doc:193-200; flow:335-359, 637-645).
    if (!settled)
      return out;
    if (w.card_state != CARD_SAVE)
      throw Unknown("the load list's own confirm path was not executed; the load acceptance "
                    "ran with the list stubbed to return 1 (save-screen.md:258-261)");
    const std::int32_t row = w.scroll_base + w.selected_row; // addu 0x21cad4 (doc:202)
    if (row == w.cache_count) {                              // compared at 0x21cb14
      w.list_action = FREE_ROW_ACTION;                       // doc:204-205
      w.fade_step = FADE_OUT;                                // 0x21cb2c
    } else if (row >= 0 && row < w.cache_count) {
      w.slot_state = SLOT_OVERWRITE; // populated slot confirm (doc:169; ref:115-119)
    } else {
      throw Unknown("a confirm past the free row was not executed (save-screen.md:143-150)");
    }
    return out;
  }
  case SLOT_OVERWRITE:
    // ref:123-128; doc:170.
    if (edge & MASK_CANCEL) {
      w.slot_state = SLOT_LIST;
    } else if (edge & MASK_COMMIT) {
      w.slot_state = SLOT_DIRECTORY;
      w.write_intent = 1; // 0x88d400
    }
    return out;
  case SLOT_DELETE:
    // doc:171; ref:129-132. difftest_save_screen.py:335 executed (9, 0) and (9, 0x20) only.
    if (edge == 0)
      return out;
    if (edge == MASK_CANCEL) {
      w.slot_state = SLOT_LIST;
      return out;
    }
    throw Unknown("slot state 9 was executed with no press and with 0x20 only; the delete confirm "
                  "fade and queue path 0x21d360-0x21d3c0 was not (difftest_save_screen.py:335)");
  case SLOT_SPACE:
    // doc:208-216; flow:385-439, 652-656.
    if (w.countdown < 0)
      throw Unknown("a negative countdown 0x88d3c4 was not executed; only 0 and 10 were planted "
                    "(difftest_save_screen_flow.py:350, 374, 395)");
    if (w.countdown != 0)
      return out; // waits until 0x88d3c4 has run down; the host runs it down
    if (w.fade_step > 0) {
      // andi 0x21cd60, add 0x21cd68, slt 0x21cd74. Units Unknown.
      if (w.space_have < w.space_need + (w.cache_count & 1)) {
        out.keys.push_back(KEY_NEED_FREE_SPACE);
      } else {
        w.fade_step = FADE_OUT;
      }
      return out;
    }
    if (w.fade_level == 0)
      w.slot_state = SLOT_COMMIT; // "with the fade already 0, state 7 becomes 12"
    return out;
  case SLOT_DIRECTORY:
    // State 11 queues 0x0a on /BASLUS-20973 and enters 12 (doc:218-219, 0x21d3f4).
    out.ops.push_back({OP_DIRECTORY, CARD_DIRECTORY, 0});
    w.slot_state = SLOT_COMMIT;
    return out;
  case SLOT_COMMIT:
    // doc:219-221, 0x21d474; executed offline with 0x4f9540 = 0 (flow:440-452, 657-660).
    if (w.network != 0)
      throw Unknown("slot state 12 was executed with the network byte clear only");
    if (w.list_result < 0)
      throw Unknown("slot state 12 with a negative 0x4f9540 was not executed");
    if (w.fade_level == 0) {
      w.slot_state = 0;
      out.committed = true;
    }
    return out;
  default:
    throw Unknown("slot state " + std::to_string(w.slot_state) +
                  " is outside the executed play-flow states 4, 5, 7, 9, 11 and 12");
  }
}

Call Screen::save_step(std::uint32_t edge) {
  Call out;
  // A nonzero card result moves to step 9 and closes the file. The latch and
  // the card state are not written (doc:234-236; flow:245-252, 615-621).
  // The save is abandoned from whichever step it was in, so its native links
  // are cleared: an arm, a dismiss or a build must not survive into a later save.
  if (w.card_result != 0 && w.step != 9) {
    w.step = 9;
    reset_save_links();
    out.closed_file = true;
    return out;
  }
  switch (w.step) {
  case 0: {
    // doc:228; flow:176-193, 534-559. Step 0 starts a save; no link carries in.
    reset_save_links();
    const bool fresh = w.fresh_name != 0 || w.selected_index == w.cache_count;
    if (fresh) {
      w.slot_name = make_slot_name(w);
    } else {
      if (w.selected_index < 0 || static_cast<std::size_t>(w.selected_index) >= w.directory.size())
        throw Refusal("occupied save row has no directory record at 0x88d040 + index*64");
      w.slot_name = record_name(w.directory[static_cast<std::size_t>(w.selected_index)]);
    }
    w.step = 1; // no write is queued here
    return out;
  }
  case 1:
    if (w.network != 0) {
      // 0x218d78-0x218d88: the net byte packs the local player instead (doc:229).
      if (!services.pack_local_player)
        throw Refusal("network save needs save_packLocalPlayer");
      services.pack_local_player();
      out.packed = true;
    } else {
      // jal 0x221914 with $4 = slot byte 0x4f923c and $5 = 0 (doc:229; flow:195-218, 563-567).
      if (!services.save_build)
        throw Refusal("offline save needs save_build");
      out.build_args = {{w.slot_byte, 0}};
      w.image = services.save_build(w.slot_byte, 0);
      out.save_built = true;
      if (w.image.size() != IMAGE_BYTES)
        throw Refusal("save_build returned " + std::to_string(w.image.size()) + " bytes, not 0x4bc78");
      image_built_ = true;
    }
    // Step 2 next: executed on the net path (flow:601-608); the offline probe
    // planted step 2 before its step-2 call (flow:214).
    w.step = 2;
    return out;
  case 2:
    if (w.image.size() != IMAGE_BYTES)
      throw Refusal("the save buffer 0x895580 does not hold a 0x4bc78-byte image");
    // The net path was executed only to step 2's entry (flow:602-608); what
    // 0x895580 holds after save_packLocalPlayer was not read.
    if (w.network != 0)
      throw Unknown("save step 2 on the network-byte path was not executed (save-screen.md:229; "
                    "difftest_save_screen_flow.py:602-608)");
    if (!image_built_)
      throw Unknown("offline save step 2 was executed only after step 1's save_build filled 0x895580 "
                    "(difftest_save_screen_flow.py:561-574)");
    image_built_ = false;
    // doc:230; flow:568-574.
    out.ops = {{OP_OPEN, w.slot_name, SAVE_OPEN_MODE},
               {OP_WRITE, {}, static_cast<std::uint32_t>(IMAGE_BYTES)},
               {OP_CLOSE, {}, 0},
               {OP_FILE_INFO, w.slot_name, 0}};
    w.step = 3;
    return out;
  case 3:
    // doc:231; flow:220-231, 583-584, executed with 0x4f8f29 at 0 on entry.
    // The byte is the screen's "done" flag that Savepoint_run clears
    // (native-soak.md:547; monsters.md:1064-1066), and it can stay up across
    // saves (native-soak.md:572). A native flag links the arm to the wait.
    if (!w.saved_step_armed) {
      if (w.saved_flag != 0)
        throw Unknown("save step 3 entered with 0x4f8f29 already set was not executed "
                      "(difftest_save_screen_flow.py:220-229, 583); Savepoint_run clears it "
                      "(native-soak.md:547)");
      w.saved_flag = 1;
      w.fade_step = FADE_OUT;
      w.saved_step_armed = true;
    } else if (w.fade_level == 0) {
      w.saved_step_armed = false;
      w.step = 4;
    }
    return out;
  case 4: {
    // doc:232; flow:232-243, 585-599. The new-row keys ran with 0x8e1ad8 set
    // and index == count (flow:591-599), the overwrite keys with the byte clear
    // and index != count (flow:562, 581-586). Which of the two picks the key
    // was not separated, so a disagreement throws.
    const bool new_row = w.selected_index == w.cache_count;
    if ((w.fresh_name != 0) != new_row)
      throw Unknown("save step 4 with 0x8e1ad8 " + std::string(w.fresh_name ? "set" : "clear") +
                    " and index " + (new_row ? "==" : "!=") +
                    " count was not executed (difftest_save_screen_flow.py:562, 581-599)");
    out.keys = {KEY_CONTINUE, KEY_SAVE_COMPLETE, new_row ? KEY_SAVE_GOOD_NEW : KEY_OVERWRITE_GOOD};
    if (w.saved_box_dismissing) {
      if (w.fade_level == 0) {
        w.latch = 0;               // 0x2190d0
        w.card_state = CARD_IDLE;  // memcard_setState(-1)
        reset_save_links();
      }
    } else if (edge & MASK_CONFIRM) {
      w.fade_step = FADE_OUT;
      w.saved_box_dismissing = true;
    }
    return out;
  }
  case 9:
    // The error box's cancel stores -1 at 0x21f254 (doc:237-244; flow:253-271,
    // 611-625). A pending level suppresses the save request only with the net
    // byte (0x2192cc-0x2192f4). While 0x88d3d8 is 1 that request does not open.
    if (edge & MASK_CANCEL) {
      const bool reopen = !(w.network != 0 && !w.pending_level.empty());
      // Refuse a bad count before the -1 store, so a refusal writes nothing.
      if (reopen && w.counted_living < 0)
        throw Refusal("a counted living player total cannot be negative");
      w.card_state = CARD_IDLE;
      reset_save_links();
      if (reopen) {
        out.reopen_called = true;
        // flow:248 plants a living player; 0x88d3d8 still 1 is what keeps the
        // request closed (doc:238-240; flow:624).
        (void)request_save(w.counted_living);
      }
    }
    return out;
  default:
    throw Unknown("save step " + std::to_string(w.step) + " is outside the executed table entries 0-4 and 9");
  }
}

Call Screen::accept_load() {
  if (w.card_state != CARD_LOAD || w.slot_stage != 0)
    throw Refusal("load acceptance needs card state 4 at slot stage 0");
  require_pump_idle("load acceptance 0x2195a8");
  if (w.selected_index < 0 || static_cast<std::size_t>(w.selected_index) >= w.directory.size())
    throw Refusal("load row has no directory record at 0x88d040 + index*64");
  // All 0x40 bytes, past the name too: eight ld/sd pairs 0x2194a4-0x2194e4
  // (doc:258-263; flow:504-523, 664-668).
  w.load_record = w.directory[static_cast<std::size_t>(w.selected_index)];
  w.step = 0;               // 0x21948c
  w.slot_stage = 1;
  w.latch = 1;              // 0x2195a8 over the 0xa5 sentinel
  w.fade_step = LOAD_ACCEPT_FADE;
  read_queued_ = false;
  reset_save_links();
  Call out;
  out.committed = true;
  return out;
}

Call Screen::queue_load_read() {
  if (w.card_state != CARD_LOAD || w.slot_stage != 1 || (w.step != 0 && w.step != 2))
    throw Refusal("the load read follows acceptance at slot stage 1");
  // doc:263-265; flow:454-469, 678-681.
  Call out;
  out.ops = {{OP_OPEN, record_name(w.load_record), LOAD_OPEN_MODE},
             {OP_READ, {}, static_cast<std::uint32_t>(IMAGE_BYTES)},
             {OP_CLOSE, {}, 0}};
  w.step = 2;
  read_queued_ = true;
  return out;
}

Call Screen::load_read_complete(Bytes image) {
  if (w.card_state != CARD_LOAD || !read_queued_)
    throw Refusal("no load read was queued");
  if (image.size() != IMAGE_BYTES)
    throw Refusal("card read returned " + std::to_string(image.size()) + " bytes, not 0x4bc78");
  if (w.restore_flag > 1)
    throw Unknown("0x8e1ab9 values other than 0 and 1 were not executed");
  Call out;
  // save_load 0x221d60 requests the header's level (doc:267-279); refuse an
  // unexecuted header before anything is written.
  const std::string level = w.restore_flag ? header_level(image) : std::string();
  // Step 4 copies the bytes into g_saveBuffer (doc:265; flow:470-486, 682-696).
  // The 0xf0 directory bytes at +0x16010 and the word at +0x16118 are left for
  // the level loader.
  w.image = std::move(image);
  read_queued_ = false;
  reset_save_links(); // 0x895580 now holds loaded bytes, not this save's build
  if (w.restore_flag == 0) {
    // doc:265-267: save_load is not called.
    w.latch = 0;
    w.card_state = CARD_IDLE;
    w.step = 0;
    return out;
  }
  // The live name 0x41b721 is left as it was; nothing here reaches
  // world_loadRecords, level_loadScriptAndObjects or save_reload.
  if (services.restore)
    services.restore(w.image);
  w.latch = 1; // "the latch is 1 afterward, over that same sentinel" (doc:267-268; flow:473, 687)
  w.pending_level = level;
  w.side_name = level;
  w.pending_marker = PENDING_MARKER;
  w.loaded_flag = 1;
  w.step = 5;
  out.restored = true;
  return out;
}

Call Screen::load_step() {
  Call out;
  switch (w.step) {
  case 2:
    if (!read_queued_)
      throw Refusal("load step 2 without a queued read");
    return out; // the host's card read is outstanding
  case 5:
    // 0x21984c: the live qualified name against 0x8e1a98 (doc:281-286; flow:487-500, 689-690).
    if (qualified_level(w.live_level, w.live_sub) == w.side_name) {
      w.card_state = CARD_IDLE;
      w.latch = 0;
    }
    return out;
  default:
    throw Unknown("load step " + std::to_string(w.step) +
                  ": only the acceptance, step 2, step 4 and step 5 were executed (save-screen.md:258-286)");
  }
}

} // namespace champions::frontend::memcard
