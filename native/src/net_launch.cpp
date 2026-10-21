#include "champions/net_launch.hpp"

namespace champions::simulation::rta::net_launch {

void lobby_set_host(Settings &s, std::int8_t lobby_own, const Room &room, std::uint8_t char_kind) {
  s.own = lobby_own;
  if (!(lobby_own < s.count))
    s.count = static_cast<std::int8_t>(static_cast<std::uint8_t>(lobby_own) + 1u);
  s.flags = 0; // always, in the branch's delay slot at 0x3276d0
  if (s.own == 0) {
    s.difficulty = room.difficulty;
    s.mode = room.mode;
    s.kill_limit = room.kill_limit;
    s.flags = FLAG_HOST;
    s.mission = static_cast<std::uint8_t>(room.mission + 1u);
  }
  s.flags = static_cast<std::uint8_t>(s.flags | ((char_kind & 3u) ? FLAG_KIND_B : FLAG_KIND_A));
}

void frontend_set_host(Settings &s, bool selection) {
  s.difficulty = 1;
  s.mode = 0;
  s.flags = 0;
  if (selection)
    s.flags = FLAG_HOST;
}

void frontend_set_kind(Settings &s, std::uint8_t char_kind) {
  s.byte_0a = 0xff;
  s.flags = static_cast<std::uint8_t>(s.flags | ((char_kind & 3u) ? FLAG_KIND_B : FLAG_KIND_A));
}

std::uint32_t own_record(std::int8_t own) {
  return LAUNCH_RECORDS + static_cast<std::uint32_t>(static_cast<std::int32_t>(own) *
                                                     static_cast<std::int32_t>(LAUNCH_RECORD_SIZE));
}

bool launch_fill_host_fields(const Settings &s, Record &r, const std::function<std::int32_t()> &scan) {
  if (!(s.flags & FLAG_HOST))
    return false;
  r.difficulty = s.difficulty;
  r.mode = s.mode;
  r.kill_limit = s.kill_limit;
  r.mission = s.mission;
  r.cm1 = static_cast<std::uint32_t>(scan());
  return true;
}

Received launch_receive(const Record &rec) {
  Received out;
  const auto mode = rec.mode;
  out.difficulty = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(rec.difficulty)));
  out.kill_limit = rec.kill_limit;
  out.pvp_mode_b = static_cast<std::uint32_t>(mode) - 1u < 3u;
  out.pvp_mode_a = static_cast<std::uint8_t>(mode - 2u) < 2u;
  out.mode_3 = static_cast<std::int8_t>(mode) == 3;
  if (out.pvp_mode_a)
    out.set_cm1 = static_cast<std::int32_t>(rec.cm1);
  return out;
}

std::int32_t cm1_word_after_launch(std::int32_t host_scan, std::int32_t own_word, std::uint8_t mode) {
  return (mode == 2 || mode == 3) ? host_scan : own_word;
}

} // namespace champions::simulation::rta::net_launch
