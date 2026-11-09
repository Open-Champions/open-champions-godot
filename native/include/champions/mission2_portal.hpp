#pragma once
// Mission 2 jump-gate portal: the menu, the next-world write, the handoff
// through the client's level-change path, arrival at the destination gate,
// and the destination-list save block.
// Research pin: research-mission2-route 3c10c07, docs/mission2-round6.md
// (audits research-mission2-route-r6.md and -r6-fix.md, PASS). Only Observed
// or Executed behaviour is carried. The loader internals that round 6 runs
// (VU1, DMA, boot and engine_init fixtures, sections 1, 5, 6) are research
// scaffolding: the client keeps its own level_flow::Loader.
#include "champions/mission2_route.hpp"
#include "champions/session_level_flow.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace champions::simulation::rta::mission2_route::portal {
// Static data words, not instructions. docs/mission2-round6.md:151-160.
inline constexpr std::uint32_t CANCEL_MASK = 0x120;  // 0x41bfb8, pad +0xc
inline constexpr std::uint32_t CONFIRM_MASK = 0x40;  // 0x41bfbc, pad +0xc
// Pad +0x10. docs/mission2-round6.md:161-163.
inline constexpr std::uint32_t UP_MASK = 0x101000;
inline constexpr std::uint32_t DOWN_MASK = 0x204000;
// State 1 adds 8 until 0x80, then stores 0x7f (:144-145). State 3 subtracts
// 0x10 until negative (:167).
inline constexpr std::int32_t FADE_IN_STEP = 8, FADE_FULL = 0x80,
                              FADE_HELD = 0x7f, FADE_OUT_STEP = 0x10;
// slti 0xb5 at 0x27e1b4 (:179).
inline constexpr unsigned TRAVEL_COUNT = 181;
// Orientation byte 0x41b720 written at 0x27e210 (:182).
inline constexpr std::uint8_t GATE_ORIENTATION = 0xff;
// net_lockLocalInput(chooser, 5) on cancel (:171, :248-249).
inline constexpr int CANCEL_INPUT_LOCK = 5;
// 0x8e3a50 + 32 x selection (:180); 480 bytes saved (:260-262).
inline constexpr std::size_t NAME_BYTES = 0x20;
inline constexpr std::size_t DESTINATION_BYTES = 0x1e0;
inline constexpr std::size_t DESTINATION_SLOTS = DESTINATION_BYTES / NAME_BYTES;
// jumpgate_saveDestinations / jumpgate_loadDestinations return 0x1e4 (:260-264).
inline constexpr std::uint32_t DESTINATION_BLOCK = 0x1e4;
// The save row the block occupies. docs/mission2-round6.md:271
// (research-save-world 31a0ca6 docs/audio-save.md:70).
inline constexpr std::size_t SAVE_OFFSET = 0x134;

// g_pads[chooser]: the pressed word at +0xc and the word at +0x10 (:129-131,
// :151, :161).
struct Pad {
  std::uint32_t pressed = 0;
  std::uint32_t moved = 0;
};

// jumpgate_menuRun 0x27dd74 state, plus the call counts the tests assert.
// The travel word 0x500ae8 is level_flow::State::travel.ticks: the client's
// level-change path reads it there when the destination gate is built.
struct Menu {
  std::uint8_t state = 0;          // 0x8e3a3a
  std::uint8_t chooser = 0;        // 0x8e3a3b
  std::vector<std::string> names;  // 0x8e3a50, count 0x8e3c30
  std::uint8_t selection = 0;      // 0x8e3a38
  std::uint8_t previous = 0;       // 0x8e3a39, cleared in state 0
  std::optional<std::uint8_t> current; // 0x8e3a40
  std::int32_t fade = 0;           // 0x8e3a3c
  std::uint32_t word_41b4fc = 0, word_41b500 = 0;
  std::uint32_t pause_word = 0;    // 0x44fa80
  std::uint8_t pause_byte = 0;     // 0x41b4f4
  bool slot_active = true;         // net_isSlotActive(chooser)
  bool tasks = true;               // run 0x27dd74 and draw 0x27e2d0 registered
  std::optional<int> input_lock;   // net_lockLocalInput's second argument
  unsigned accept_sounds = 0, pause_menu_closes = 0, travel_begins = 0;
};

// The portal use has installed both tasks with travel 0
// (docs/mission2-round6.md:197-214; round 5's JumpGate_openMenu). `names` is
// the list after the prompt appended the current level. Refuses: a net game
// (its state-0 and state-2 paths were read, not executed: :142, :148-150), a
// fixed destination 0x8e3c34 (read, not executed: :141, :180), a travel word
// that is not 0, more than 15 names, or a name that does not fit 32 bytes.
Menu open(std::vector<std::string> names, const level_flow::State &,
          bool net = false, bool fixed_destination = false);

// One call of the run task, once per frame (docs/mission2-round6.md:204-207).
// States 0-5 are carried (:133-184, executed :210-240, :245-254). State 6 and
// later refuse: the order of state 6 against the main loop's request is
// Inferred (:292-296), so the client does not run it. A frame after cancel
// removed the tasks refuses. Up and down together in one frame refuse; the
// test moved one way per frame (:218-220).
// Writes, as the original does: state 2 confirm sets travel to 1 (:155-156);
// state 5 counts travel to 181, then writes the selected name to
// g_pendingLevel and 0xff to the orientation byte, and nothing else for the
// next world (:177-184, :232-240). Cancel writes nothing for a next world
// (:247-254).
std::uint8_t frame(Menu &, const Pad &, level_flow::State &);

// The client's level-change path for the request the gate wrote:
// SessionAdapter::change, i.e. begin_change (mainloop_loadRequestedWorld
// 0x320edc preparation) and the loader. Requires the request from state 5
// (orientation 0xff, travel 181). docs/mission2-round6.md:302-312: the
// departure store gets the level being left, the loader gets the destination
// name, g_prevLevelName is the level left, the subname and pending name are
// empty, 0x41b7c1 holds 0xff and 0x41b720 holds 0xfe.
struct Handoff {
  std::string stored;       // the departure key (save_storeLevelState's name)
  bool departure_entry = false;
  std::string loaded;       // the name handed to the loader
  std::string level, sub, previous, pending;
  std::uint8_t orientation_copy = 0; // 0x41b7c1
  std::uint8_t orientation = 0;      // 0x41b720
  level_flow::LoadedLevel level_data;
};
Handoff handoff(level_flow::SessionAdapter &, level_flow::State &,
                const level_flow::ChangeOptions &,
                const std::vector<level_flow::LevelRow> &,
                level_flow::Placement);

// Destination-list save block (docs/mission2-round6.md:258-285).
struct Destinations {
  std::uint8_t count = 0;                             // 0x8e3c30
  std::array<std::uint8_t, DESTINATION_BYTES> bytes{}; // 0x8e3a50
};
// Zero-filled area with each name at 32-byte stride. Refuses more than 15
// names or a name that does not fit with its terminator.
Destinations from_names(const std::vector<std::string> &);
// The first `count` names. Refuses a count above 15 or a slot without a
// terminator.
std::vector<std::string> to_names(const Destinations &);
// jumpgate_saveDestinations 0x27dcf4: the sign-extended count byte as a word,
// then the 480 bytes. Returns 0x1e4 in `ret`.
struct Saved {
  std::vector<std::uint8_t> block;
  std::uint32_t ret = 0;
};
Saved save_destinations(const Destinations &);
// jumpgate_loadDestinations 0x27dd30: the word's low byte to the count, and
// the 480 bytes back. Returns 0x1e4 in `ret`. A block shorter than 0x1e4
// refuses.
struct Loaded {
  Destinations destinations;
  std::uint32_t ret = 0;
};
Loaded load_destinations(const std::vector<std::uint8_t> &block);
} // namespace champions::simulation::rta::mission2_route::portal
