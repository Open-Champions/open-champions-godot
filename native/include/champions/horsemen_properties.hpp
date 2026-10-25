#pragma once
#include <array>
#include <cstdint>

namespace champions::simulation::rta::horsemen_properties {
// Observed mount lists, research t3/research-horsemen-mount 338d459,
// docs/spec/horsemen-mount.md section 2.2 and ref_horsemen_props.py.
// Each list is {"miniboss=1", "name=<name>", null}. These lists belong
// only to the constructed Nightmare, never to an on-foot or mounted rider.
struct Mount {
  std::uint32_t list, miniboss_text, name_text;
  const char *name;
};
inline constexpr std::array<Mount, 4> mounts{{
    {0x500848, 0x547320, 0x547330, "horsedisease"},
    {0x500854, 0x547320, 0x547348, "horsewar"},
    {0x500860, 0x547320, 0x547358, "horsepestilence"},
    {0x50086c, 0x547320, 0x547370, "horsedeath"},
}};
} // namespace champions::simulation::rta::horsemen_properties
