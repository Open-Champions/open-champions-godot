#include "champions/mission2_portal.hpp"
#include <algorithm>
#include <utility>

// Citations are research-mission2-route 3c10c07 docs/mission2-round6.md
// unless another file is named.
namespace champions::simulation::rta::mission2_route::portal {
namespace {
void check_name(const std::string &name) {
  if (name.empty() || name.size() + 1 > NAME_BYTES)
    throw Refused("a destination name must fit its 32-byte slot with the terminator");
}
std::string qualified(const level_flow::State &world) {
  // level_getQualifiedName 0x1ec0f0 (:135): level, then '_' and the subname.
  return world.sub.empty() ? world.level : world.level + "_" + world.sub;
}
} // namespace

Menu open(std::vector<std::string> names, const level_flow::State &world, bool net,
          bool fixed_destination) {
  if (net)
    throw Refused("the net-game menu paths were read, not executed (round 6 :142, :148-150)");
  if (fixed_destination)
    throw Refused("the fixed-destination path 0x8e3c34 was read, not executed (round 6 :141, :180)");
  if (world.travel.ticks)
    throw Refused("the executed menu opened with travel 0 (round 6 :210-214)");
  if (names.size() > DESTINATION_SLOTS)
    throw Refused("more than 15 destinations do not fit the 480-byte list");
  for (const auto &name : names)
    check_name(name);
  Menu menu;
  menu.names = std::move(names);
  return menu;
}

std::uint8_t frame(Menu &menu, const Pad &pad, level_flow::State &world) {
  if (!menu.tasks)
    throw Refused("the menu run and draw tasks were removed");
  switch (menu.state) {
  case 0: {
    // :133-142. Executed: state 0 stores the current index (the last entry)
    // and sets both pause words (:212-215; difftest_mission2_r6.py:477-479).
    const auto here = qualified(world);
    const auto it = std::find(menu.names.begin(), menu.names.end(), here);
    if (it == menu.names.end())
      throw Refused("the current level is not listed; the executed menu always found it (round 6 :212-214)");
    menu.selection = 0;
    menu.previous = 0;
    menu.current = static_cast<std::uint8_t>(it - menu.names.begin());
    menu.word_41b4fc = menu.word_41b500 = 1;
    // Offline pause: 0x44fa80 = 1, 0x41b4f4 = 1 (:142).
    menu.pause_word = 1;
    menu.pause_byte = 1;
    world.travel.menu_open = true;
    world.travel.paused = true;
    menu.state = 1;
    break;
  }
  case 1:
    // :144-145. Executed: 16 frames (:216).
    menu.fade += FADE_IN_STEP;
    if (menu.fade >= FADE_FULL) {
      menu.fade = FADE_HELD;
      menu.state = 2;
    }
    break;
  case 2: {
    // :147-163.
    if ((pad.pressed & CANCEL_MASK) || !menu.slot_active) {
      ++menu.accept_sounds;
      menu.state = 3;
      break;
    }
    if (menu.current && menu.selection != *menu.current && (pad.pressed & CONFIRM_MASK)) {
      // Travel 1 at 0x27e00c. Confirming the current level is refused at
      // 0x27dfa0 (:154-156; executed :222-228).
      world.travel.ticks = 1;
      world.travel.fixed = false;
      world.travel.request_written = false;
      menu.state = 3;
      break;
    }
    const bool up = (pad.moved & UP_MASK) != 0, down = (pad.moved & DOWN_MASK) != 0;
    if (up && down)
      throw Refused("up and down in one frame were not executed (round 6 :218-220)");
    if (up && menu.selection > 0)
      --menu.selection; // 0x27e05c
    else if (down && static_cast<std::size_t>(menu.selection) + 1 < menu.names.size())
      ++menu.selection; // 0x27e0b8
    break;
  }
  case 3:
    // :165-172. Executed: 8 frames, then pauseMenu_close and both pause words
    // clear (:229-230), or the cancel unlock and task removal (:247-249).
    menu.fade -= FADE_OUT_STEP;
    if (menu.fade < 0) {
      menu.pause_word = 0;
      menu.pause_byte = 0;
      world.travel.paused = false;
      if (world.travel.ticks) {
        menu.word_41b500 = menu.word_41b4fc = 1;
        ++menu.pause_menu_closes; // pauseMenu_close 0x18df90
        menu.state = 4;
      } else {
        menu.input_lock = CANCEL_INPUT_LOCK;
        menu.tasks = false; // task_remove at 0x27e170-0x27e188
        world.travel.menu_open = false;
      }
    }
    break;
  case 4:
    // jumpgate_beginTravel 0x27e6a8 (:174-175; executed :231).
    ++menu.travel_begins;
    menu.state = 5;
    break;
  case 5:
    // :177-184. Executed: 181 frames; the first 180 write nothing for the
    // next world, the last writes the name and then 0xff at 0x41b720
    // (:232-240).
    if (world.travel.ticks < TRAVEL_COUNT) {
      ++world.travel.ticks;
      break;
    }
    world.pending.request(menu.names[menu.selection], GATE_ORIENTATION);
    world.travel.destination = menu.names[menu.selection];
    world.travel.request_written = true;
    menu.state = 6;
    break;
  default:
    // State 6 goes to 7, and 7 clears travel and removes both tasks
    // (:186-189, :241-243). Which main-loop pass takes the request against
    // state 6 is Inferred (:292-296), so the client does not run them.
    throw Refused("menu state 6 and later are not run: their order against the main-loop request is Inferred (round 6 :292-296)");
  }
  return menu.state;
}

Handoff handoff(level_flow::SessionAdapter &adapter, level_flow::State &world,
                const level_flow::ChangeOptions &options,
                const std::vector<level_flow::LevelRow> &rows, level_flow::Placement placement) {
  if (world.pending.name.empty() || world.pending.orientation != GATE_ORIENTATION ||
      world.travel.ticks != TRAVEL_COUNT || !world.travel.request_written || world.travel.fixed)
    throw Refused("the handoff takes the request state 5 wrote: a name, orientation 0xff and travel 181");
  Handoff out;
  out.stored = qualified(world);
  out.level_data = adapter.change(options, rows, std::move(placement));
  // begin_change wrote these before the loader ran (:307-318).
  out.departure_entry = std::any_of(world.states.entries.begin(), world.states.entries.end(),
                                    [&](const auto &entry) { return entry.first == out.stored; });
  out.loaded = world.level;
  out.level = world.level;
  out.sub = world.sub;
  out.previous = world.prev;
  out.pending = world.pending.name;
  out.orientation_copy = world.orientation_copy;
  out.orientation = world.pending.orientation;
  return out;
}

Destinations from_names(const std::vector<std::string> &names) {
  if (names.size() > DESTINATION_SLOTS)
    throw Refused("more than 15 destinations do not fit the 480-byte list");
  Destinations out;
  out.count = static_cast<std::uint8_t>(names.size());
  for (std::size_t i = 0; i < names.size(); ++i) {
    check_name(names[i]);
    std::copy(names[i].begin(), names[i].end(), out.bytes.begin() + i * NAME_BYTES);
  }
  return out;
}

std::vector<std::string> to_names(const Destinations &area) {
  if (area.count > DESTINATION_SLOTS)
    throw Refused("a count above 15 reads past the 480-byte list");
  std::vector<std::string> out;
  for (std::size_t i = 0; i < area.count; ++i) {
    const auto begin = area.bytes.begin() + i * NAME_BYTES;
    const auto end = std::find(begin, begin + NAME_BYTES, std::uint8_t(0));
    if (end == begin + NAME_BYTES)
      throw Refused("a destination slot has no terminator");
    out.emplace_back(begin, end);
  }
  return out;
}

Saved save_destinations(const Destinations &area) {
  // :260-262. The count byte is sign-extended to the stored word.
  Saved out;
  const auto word = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(area.count)));
  for (unsigned i = 0; i < 4; ++i)
    out.block.push_back(static_cast<std::uint8_t>(word >> (8 * i)));
  out.block.insert(out.block.end(), area.bytes.begin(), area.bytes.end());
  out.ret = DESTINATION_BLOCK;
  return out;
}

Loaded load_destinations(const std::vector<std::uint8_t> &block) {
  // :263-264. The low byte of the word becomes the count.
  if (block.size() < DESTINATION_BLOCK)
    throw Refused("a destination block is 0x1e4 bytes");
  Loaded out;
  out.destinations.count = block[0];
  std::copy(block.begin() + 4, block.begin() + 4 + DESTINATION_BYTES, out.destinations.bytes.begin());
  out.ret = DESTINATION_BLOCK;
  return out;
}
} // namespace champions::simulation::rta::mission2_route::portal
