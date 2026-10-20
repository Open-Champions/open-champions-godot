#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

// Run-tick decisions for the ten creature classes that own their state switch.
// Clip choice stays in creature_anim::select_dispatch. A request is the slot-27
// argument that causes that setState to run. Constants are the executed
// own-state-machine contract (research docs/spec/own-state-machines.md).
namespace champions::simulation::rta::own_machines {
struct View {
  // Defaults match the reference tick: clock 1000, queue 1, everything else 0.
  std::uint32_t clock = 1000, timer = 0, flight = 0, queue = 1, anim = 0,
                events = 0, rand = 0, fid_time = 0, z = 0, stuck = 0,
                cflags = 0, kind = 0, class_flags = 0, frame = 0, serial = 0;
  bool seen = false, away = false, see = false, enemy = false, ally = false,
       rooted = false, ready = false, stunned = false, arrived = false,
       demon = false, path = false, target = false, owner = false, idle = false,
       move = false, fidget_a = false, fidget_b = false, flee_clip = false,
       snap = false;
  std::map<std::string, std::vector<int>> seq;
};
struct Tick {
  std::vector<std::uint32_t> requests;
  std::vector<std::string> calls;
  std::map<std::uint32_t, std::uint32_t> words;
};
// One run tick. `view` is copied; its sequences are not consumed.
Tick step(const std::string &name, std::uint32_t state, const View &view = {});
// |raw32| > 0.5, strict. Compared as an integer magnitude, not a host float.
// 0x3f000000 does not clear; 0x3f000001 and 0xbf000001 do.
bool root_clears_speed(std::uint32_t word);
// True when the raw32 pattern is an integer value. The NPC restore was only
// measured for those patterns.
bool speed_word_is_integer(std::uint32_t word);

// Live probe. Hooks, when set, supply the return or the side effect and still
// record the call. An unset hook uses the fields above.
struct Probe {
  std::uint32_t clock = 1000, timer = 0, flight = 0, queue = 1, anim = 0,
                events = 0, rand_n = 0, fid = 0, z = 0, stuck = 0, cflags = 0,
                kind = 0, class_flags = 0, frame = 0, serial = 0;
  int seen = 0, see_v = 0, enemy = 0, ally = 0, demon = 0, path = 0;
  bool away = false, rooted = false, ready = false, stunned = false,
       arrived = false, target = false, owner = false, idle = false,
       move = false, fa = false, fb = false, flee = false, snap = false;
  std::map<std::string, std::vector<int>> seq;
  std::vector<std::uint32_t> requests;
  std::vector<std::string> calls;
  std::uint32_t timer_out = 0, flight_out = 0;
  std::function<int()> v25_hook, see_hook;
  std::function<std::uint32_t()> roll_hook;
  std::function<void(const std::string &)> side_hook;
  std::function<void(std::uint32_t)> request_hook;
  bool expired() const;
  bool flag(std::uint32_t bit) const;
  void call(const std::string &name);
  int take(const std::string &name, int fallback);
  int v25();
  int see();
  std::uint32_t roll();
  void side(const char *name);
  void request(std::uint32_t state);
};
Tick run(const std::string &name, std::uint32_t state, Probe &probe);
} // namespace champions::simulation::rta::own_machines
