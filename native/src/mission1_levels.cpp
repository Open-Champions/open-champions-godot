#include "champions/mission1_levels.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace champions::simulation::rta::mission1_levels {
namespace {
std::uint32_t u32(const std::uint8_t *p) {
  return std::uint32_t(p[0]) | std::uint32_t(p[1]) << 8 | std::uint32_t(p[2]) << 16 | std::uint32_t(p[3]) << 24;
}
std::int16_t s16(const std::uint8_t *p) {
  return std::int16_t(std::uint16_t(p[0] | p[1] << 8));
}
float f32(const std::uint8_t *p) {
  const auto bits = u32(p);
  float out;
  std::memcpy(&out, &bits, sizeof out);
  return out;
}
// A table of `count` records of `width` bytes at `offset` must lie inside the file.
void table(std::size_t size, std::uint32_t offset, std::uint32_t count, std::size_t width, const char *what) {
  if (std::uint64_t(offset) + std::uint64_t(count) * width > size)
    throw CutError(std::string("cutscene ") + what + " table outside the file");
}
bool counted(Origin o) {
  return o == Origin::Placed || o == Origin::PlayerSlot || o == Origin::CutsceneCreated;
}
} // namespace

Cut parse_cut(const std::uint8_t *data, std::size_t size) {
  if (!data || size < CUT_HEADER) throw CutError("cutscene header truncated");
  Cut out;
  out.flags = u32(data);
  const auto key_at = u32(data + 4), keys = u32(data + 8);
  const auto actor_at = u32(data + 0xc), actors = u32(data + 0x10);
  if (keys > CUT_MAX_KEYS || actors > CUT_MAX_ACTORS) throw CutError("cutscene table exceeds the host bound");
  table(size, key_at, keys, CUT_KEY, "key");
  table(size, actor_at, actors, CUT_ACTOR, "actor");
  for (std::uint32_t k = 0; k < keys; ++k) {
    const auto *p = data + key_at + std::size_t(k) * CUT_KEY;
    out.keys.push_back({f32(p), s16(p + 4), s16(p + 6)});
  }
  for (std::uint32_t k = 0; k < actors; ++k) {
    const auto *p = data + actor_at + std::size_t(k) * CUT_ACTOR;
    CutActor a;
    for (auto i = CUT_CLASS_AT; i < CUT_CLASS_END && p[i]; ++i) a.cls.push_back(char(p[i]));
    for (unsigned i = 0; i < 3; ++i) a.position[i] = f32(p + CUT_POSITION_AT + 4 * i);
    a.create = p[CUT_CREATE_AT] != 0;
    out.actors.push_back(std::move(a));
  }
  return out;
}

Box actor_box(const Vec3f &at) {
  const Vec3f half{BIND_HALF_XY, BIND_HALF_XY, BIND_HALF_Z};
  Box box;
  for (unsigned i = 0; i < 3; ++i) {
    box.lo[i] = at[i] - half[i];
    box.hi[i] = at[i] + half[i];
  }
  return box;
}

std::string compared_type_name(const std::string &info) {
  std::size_t i = 0;
  while (i < info.size() && info[i] >= '0' && info[i] <= '9') ++i;
  return info.substr(i);
}

Binding bind_actor(const CutActor &actor, const std::vector<Candidate> &query) {
  Binding out;
  if (actor.create) {
    out.status = BindStatus::Created;
    return out;
  }
  out.kept = std::min<std::size_t>(query.size(), BIND_KEPT);
  out.truncated = query.size() > out.kept;
  for (std::size_t i = 0; i < out.kept; ++i)
    if (!counted(query[i].origin)) {
      out.status = BindStatus::Refused;
      out.reason = open_reason(Open::RunTimeObjectsInSearch);
      return out;
    }
  // Nearest in 2-D strictly below 600; a held class-string match makes later
  // type-only matches ineligible (doc:353-357). Distances as the research's
  // asserted search (tools/difftest_mission1_levels.py:996-1001).
  double best = BIND_FIRST;
  bool held_by_name = false;
  for (std::size_t i = 0; i < out.kept; ++i) {
    const auto &c = query[i];
    if (c.flags & BOUND_FLAG) continue;
    const bool by_name = c.class_string == actor.cls;
    const bool by_type = !by_name && compared_type_name(c.type_info) == actor.cls;
    if (!by_name && !by_type) continue;
    if (held_by_name && !by_name) continue;
    const double d = std::hypot(double(c.position[0]) - double(actor.position[0]),
                                double(c.position[1]) - double(actor.position[1]));
    if (d < best) {
      best = d;
      out.index = i;
      held_by_name = by_name;
    }
  }
  if (out.index) {
    out.status = BindStatus::Bound;
    out.by_class_string = held_by_name;
    out.distance = float(best);
  }
  return out;
}

Slot3 slot3_calls(const Cut &cut, std::size_t actor) {
  Slot3 out;
  if (actor >= cut.actors.size()) {
    out.reason = "cutscene actor index outside the actor table";
    return out;
  }
  for (const auto &k : cut.keys) {
    if (k.actor < 0 || std::size_t(k.actor) != actor) continue;
    if (k.kind == KEY_TRIGGER_ON || k.kind == KEY_TRIGGER_OFF)
      out.calls.push_back({k.time, k.kind == KEY_TRIGGER_ON ? 1 : 0});
  }
  for (std::size_t i = 1; i < out.calls.size(); ++i)
    if (!(out.calls[i - 1].time <= out.calls[i].time)) {
      out.calls.clear();
      out.reason = "slot-3 keys out of ascending time; only ascending key order was observed "
                   "(docs/mission1-levels.md:380-382)";
      return out;
    }
  out.ok = true;
  return out;
}

std::vector<std::size_t> shown_actors(const Cut &cut) {
  std::vector<std::size_t> out;
  for (const auto &k : cut.keys)
    if (k.kind == KEY_SHOW && k.actor >= 0 && std::size_t(k.actor) < cut.actors.size())
      out.push_back(std::size_t(k.actor));
  std::sort(out.begin(), out.end());
  out.erase(std::unique(out.begin(), out.end()), out.end());
  return out;
}

const char *open_reason(Open o) {
  switch (o) {
  case Open::DoorStateAfterTrigger:
    return "the door state after the doors, cazic and bossdefeated cutscenes is Unknown: whether the door "
           "reads its slot-3 argument was not traced (docs/mission1-levels.md:318-321,547-548)";
  case Open::ShownStaysHidden:
    return "whether hideMonster's object flag 0x8000 and creature bit 3 are cleared after a show key is "
           "Unknown; a show key only puts the object back on the grid (docs/mission1-levels.md:296-303,546-547)";
  case Open::RunTimeObjectsInSearch:
    return "objects spawned at run time, moved creatures and objects a constructor or init script makes "
           "were not counted in the actor search; their share of the 63 kept results is Unknown "
           "(docs/mission1-levels.md:376-379,543-545; audit fix2 F2-2)";
  case Open::CutscenePlayback:
    return "cutscene playback, length, keys and camera are Unknown: cutScene was a stub "
           "(docs/mission1-levels.md:400-402,522)";
  case Open::UnrequestedCutscene:
    return "a cutscene file that no script requests; whether anything plays it is Unknown "
           "(docs/mission1-levels.md:396-399,549-550)";
  case Open::DemoBranch:
    return "exitzone's demoRunning branch was modelled but not executed (docs/mission1-levels.md:425-428,530)";
  }
  return "Unknown";
}
} // namespace champions::simulation::rta::mission1_levels
