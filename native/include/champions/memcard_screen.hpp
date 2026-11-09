#pragma once

#include "champions/frontend.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// The save and load screens' confirm, write and load handoff of Champions:
// Return to Arms (SLUS-20973 v2.00), as a word-level contract. Authored only
// from t3/research-save-screen c8895e2 docs/save-screen.md, "Play flow"
// (lines 181-325) and tools/difftest_save_screen_flow.py / ref_save_screen.py,
// audited PASS by research-save-screen-r2-fix.md. Each field names the address
// it stands for. Every branch the research did not execute throws Unknown
// instead of guessing. Card I/O is returned to the host as queued operations;
// no card bytes, localized text, pixels or retail card behaviour are held here.
// The world body stays with the save-world loader and death reload keeps its
// own refusal (docs/PLAYER_DEATH_FLOW.md); nothing here calls save_reload.
namespace champions::frontend::memcard {

struct Unknown : Refusal {
  using Refusal::Refusal;
};

using Bytes = std::vector<std::uint8_t>;

// g_memcardState 0x88d3a0 (save-screen.md:26): 3 saves, 4 loads, -1 idle.
inline constexpr std::int32_t CARD_IDLE = -1, CARD_SAVE = 3, CARD_LOAD = 4;
// Slot state 0x88d3dc (save-screen.md:30, 167-172, 193-221).
inline constexpr std::int32_t SLOT_LIST = 4, SLOT_OVERWRITE = 5, SLOT_SPACE = 7, SLOT_DELETE = 9,
                              SLOT_DIRECTORY = 11, SLOT_COMMIT = 12;
// Bit pattern of 1.0f: the cursor snap at 0x21ca2c/0x21ca3c (save-screen.md:197-199).
inline constexpr std::uint32_t CURSOR_SNAP = 0x3f800000u;
// Stored at 0x4f8ef4 when the free row arms (save-screen.md:204-205).
inline constexpr std::uint32_t FREE_ROW_ACTION = 7;
// Fade step stored by every armed transition (save-screen.md:205, 213, 231-232).
inline constexpr std::int32_t FADE_OUT = -16;
// Fade step left after load acceptance (save-screen.md:262-263; flow difftest :667-668).
inline constexpr std::int32_t LOAD_ACCEPT_FADE = 16;
// Button masks (save-screen.md:169-170, 174-179); scheme-dependent, not face names.
inline constexpr std::uint32_t MASK_CONFIRM = 0x40, MASK_CANCEL = 0x20, MASK_COMMIT = 0x80;
// State 4's repeat mask (save-screen.md:169; ref_save_screen.py:116). The slot
// list reads two owner words, edge +0x0c and repeat +0x10 (save-screen.md:177-179);
// the probe planted the same value in both (difftest_save_screen.py:33-34), so
// which word each mask is tested against was not separated. Screen::call takes
// both and refuses a difference in slot states 4, 5 and 9.
inline constexpr std::uint32_t MASK_REPEAT = 0x204000;
// Card queue operations (ref_save_screen.py:139-153; flow difftest :568-574, 657-659, 678-681).
inline constexpr int OP_OPEN = 0, OP_CLOSE = 1, OP_READ = 3, OP_WRITE = 5, OP_FILE_INFO = 6,
                     OP_SPACE = 8, OP_DIRECTORY = 0x0a;
inline constexpr std::uint32_t SAVE_OPEN_MODE = 0x202, LOAD_OPEN_MODE = 1;
// One slot image (save-screen.md:122-123, 230, 263-264).
inline constexpr std::size_t IMAGE_BYTES = 0x4bc78;
// One directory record at 0x88d040 + index*64; its name at +0x20 (0x88d060,
// 0x88d2e0). Load acceptance copies all 0x40 bytes (save-screen.md:228, 258-260).
inline constexpr std::size_t DIRECTORY_RECORD = 0x40, DIRECTORY_NAME_AT = 0x20;
// Card identifiers (not game text): ref_save_screen.py:144-145, flow difftest :557, 659.
inline constexpr const char *CARD_DIRECTORY = "/BASLUS-20973";
inline constexpr const char *SLOT0_NAME = "BASLUS-20973_slot0";
inline constexpr const char *SLOT1_NAME = "BASLUS-20973_slot1";
// Localization lookup keys, identifiers only (ref_save_screen.py:149-151).
inline constexpr const char *KEY_CONTINUE = "MemcardContinue";
inline constexpr const char *KEY_SAVE_COMPLETE = "MemcardSaveComplete";
inline constexpr const char *KEY_SAVE_GOOD_NEW = "MemcardSaveGood2";
inline constexpr const char *KEY_OVERWRITE_GOOD = "OverwriteGood";
inline constexpr const char *KEY_NEED_FREE_SPACE = "MemcardNeedFreeSpaceBrowser";
// save_load's marker at 0x41b720 (save-screen.md:271).
inline constexpr std::uint8_t PENDING_MARKER = 0xfd;

// A request the screen hands to the card queue 0x223470. `target` is the file
// or directory name; the image is the screen's buffer (0x895580 / g_saveBuffer).
struct CardOp {
  int op = 0;
  std::string target;
  std::uint32_t arg = 0; // open mode or byte count
  bool operator==(const CardOp &o) const { return op == o.op && target == o.target && arg == o.arg; }
};

// The words one screen call reads and writes. Defaults are zero: the research
// fixtures plant every word they rely on, and so must the host.
struct Words {
  std::int32_t card_state = CARD_IDLE;  // g_memcardState 0x88d3a0
  std::int32_t browser_stage = 0;       // 0x88d3e4; 0 runs memcard_browser 0x219b90
  std::int32_t slot_stage = 0;          // 0x88d3d8; 0 runs the slot list, 1 the screen step
  std::int32_t step = 0;                // 0x88d3a8, save table 0x53f170; the load step too
  std::int32_t card_result = 0;         // 0x88d3bc; nonzero is a card error
  std::int32_t selected_index = 0;      // 0x88d3b0
  std::int32_t open_block = 0;          // 0x88d3c8
  // Slot list 0x21c3a4.
  std::int32_t slot_state = SLOT_LIST;  // 0x88d3dc
  std::int32_t settled_row = 0;         // 0x879e24
  std::int32_t selected_row = 0;        // 0x879e28, displayed selection
  std::int32_t scroll_base = 0;         // 0x879e2c
  std::int32_t scroll_target = 0;       // 0x879e30
  std::uint32_t cursor_float = 0;       // 0x879e34, float bits
  std::int32_t cache_count = 0;         // 0x4f9238
  std::uint32_t list_action = 0;        // 0x4f8ef4
  std::int32_t countdown = 0;           // 0x88d3c4; the host runs it down (not in the research)
  std::int32_t space_have = 0;          // 0x88d3ac; units Unknown
  std::int32_t space_need = 0;          // 0x88d3ec; units Unknown
  std::int32_t list_result = 0;         // 0x4f9540
  std::uint8_t write_intent = 0;        // 0x88d400
  // Shared fade words. The host applies the fade before the call; how the
  // original integrates 0x8e11fc into 0x8e11f8 is not in the research.
  std::int32_t fade_level = 0;          // 0x8e11f8
  std::int32_t fade_step = 0;           // 0x8e11fc
  bool pump_busy = false;               // mc_pump 0x2233e4 nonzero (never executed)
  // Save and load.
  std::uint8_t fresh_name = 0;          // 0x8e1ad8; its writer is Unknown
  std::uint8_t network = 0;             // 0x402791
  std::uint8_t slot_byte = 0;           // 0x4f923c
  std::uint8_t latch = 0;               // g_cardXferLatch 0x4f8f28
  std::uint8_t saved_flag = 0;          // 0x4f8f29
  std::uint8_t from_frontend = 0;       // 0x8e1ab8, copied from 0x41b510
  std::uint8_t restore_flag = 0;        // 0x8e1ab9
  std::int32_t num_players = 0;         // g_numPlayers 0x4d8ae0; save_build does not take it
  // The living-player count memcard_requestSave 0x2175b4 would make
  // (save-screen.md:246-251). The host supplies it; the error-box cancel
  // passes it on, so slot stage 1 is what keeps that request closed.
  std::int32_t counted_living = 0;
  std::string pending_level;            // 0x41b6d0
  std::string side_name;                // 0x8e1a98
  std::uint8_t pending_marker = 0;      // 0x41b720
  std::uint8_t loaded_flag = 0;         // 0x8e1ad6
  std::string live_level, live_sub;     // 0x41b721, 0x41b771
  std::string slot_name;                // 0x879d08
  std::array<std::uint8_t, DIRECTORY_RECORD> load_record{}; // 0x88d2c0
  std::vector<std::array<std::uint8_t, DIRECTORY_RECORD>> directory; // 0x88d040 + 64*i
  Bytes image;                          // 0x895580 save image / g_saveBuffer
  // Native: the saved box's confirm was taken and its fade is running. The
  // research shows the confirm and the close, not the word that links them.
  bool saved_box_dismissing = false;
  // Native: step 3 has set 0x4f8f29 and -16 and is waiting for the fade. The
  // byte is cleared by Savepoint_run, not by the screen, and can stay up
  // across saves (native-soak.md:547, 572), so it cannot be this link.
  bool saved_step_armed = false;
};

struct Services {
  // save_build 0x221914(save argument, one player). Must return IMAGE_BYTES.
  std::function<Bytes(std::uint8_t, std::uint8_t)> save_build;
  // save_packLocalPlayer 0x2223d4, the network-byte path.
  std::function<void()> pack_local_player;
  // Optional extra save_load 0x221d60 effects on the read image (player
  // records, quests) supplied by the existing loaders. The level request below
  // is always made here; object records stay with the save-world loader.
  std::function<void(const Bytes &)> restore;
};

// What one call did, for the host.
struct Call {
  bool committed = false;              // the slot list returned 1
  std::vector<CardOp> ops;             // queued card operations, in order
  std::vector<std::string> keys;       // localization keys looked up
  bool closed_file = false;            // memcard_closeFile 0x21e5c8
  bool reopen_called = false;          // memcard_requestSave 0x2175b4 from the error cancel
  bool save_built = false;             // save_build was called
  std::array<std::uint8_t, 2> build_args{{0, 0}};
  bool packed = false;                 // save_packLocalPlayer was called
  bool restored = false;               // save_load ran
};

// memcard_makeSlotName 0x21effc outcomes the research executed
// (save-screen.md:228), all with an empty directory: count 0 gives slot0; a
// clear fresh byte and count 1, 2 or 3 gives slot1. Anything else is Unknown.
std::string make_slot_name(const Words &w);
// 0x1ec0f0: `name` or `name_sub` (save-screen.md:269-271, 281-285).
std::string qualified_level(const std::string &name, const std::string &sub);
// Header level name (+0x00, 0x14 bytes) and subname (+0x14, 0x50 bytes).
std::string header_level(const Bytes &image);

class Screen {
public:
  Words w;
  Services services;

  // memcard_requestSave 0x2175b4 (save-screen.md:246-251): opens card state 3
  // only from -1 with 0x88d3c8 and 0x88d3d8 both 0 and a counted living player.
  bool request_save(int counted_living);
  // memcard_requestLoad 0x2176e0 (save-screen.md:251-256).
  bool request_load(std::uint8_t slot, std::uint8_t flag, std::uint8_t in_frontend);
  // One call of the dispatcher 0x2189e0 for card state 3 or 4, with the
  // owner's edge word (+0x0c) and repeat word (+0x10). Stage 0 runs the slot
  // list, stage 1 the screen step. In slot states 4, 5 and 9 the two words were
  // executed equal only, so a difference throws Unknown before any store. The
  // screen steps were executed with the edge word only (flow:234, 258).
  Call call(std::uint32_t edge, std::uint32_t repeat);
  // Native, disclosed: one word passed as both edge and repeat, as the probe
  // planted them (difftest_save_screen.py:33-34).
  Call call(std::uint32_t edge) { return call(edge, edge); }
  // Load: the slot list returned 1 for w.selected_index. The research ran the
  // load acceptance with the list stubbed to that return (save-screen.md:258-263);
  // the list's own load confirm path was not executed, so call() refuses it.
  Call accept_load();
  // Load step 2 (save-screen.md:263-265). The research does not execute load
  // steps 0, 1 or 3; the native contract queues the read from step 0.
  Call queue_load_read();
  // Load step 4 with the bytes the card read returned (save-screen.md:265-279).
  Call load_read_complete(Bytes image);

private:
  bool read_queued_ = false;
  // Offline step 2 was executed only right after step 1's save_build filled
  // 0x895580 (difftest_save_screen_flow.py:561-574).
  bool image_built_ = false;
  // Clears the three native save links (saved_step_armed, saved_box_dismissing,
  // image_built_) on every path that starts, restarts or leaves a save, so no
  // aborted step can carry its arm into a later one (audit play-save-screen-r2 PSS2-1).
  void reset_save_links();
  Call slot_list(std::uint32_t edge, std::uint32_t repeat);
  Call save_step(std::uint32_t edge);
  Call load_step();
  void require_pump_idle(const char *where) const;
};

} // namespace champions::frontend::memcard
