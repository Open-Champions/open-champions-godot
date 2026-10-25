#include "champions/level_texture.hpp"
#include <algorithm>
#include <cstdint>

namespace champions::presentation::rta::level_texture {
namespace {
void require(bool good, const char *message) {
  if (!good)
    throw Error(message);
}
std::uint32_t u32(const std::vector<std::uint8_t> &bytes, std::uint64_t at,
                  std::uint64_t end) {
  require(at <= end && 4 <= end - at && end <= bytes.size(),
          "WORLD material packet is truncated");
  return std::uint32_t(bytes[at]) | (std::uint32_t(bytes[at + 1]) << 8) |
         (std::uint32_t(bytes[at + 2]) << 16) |
         (std::uint32_t(bytes[at + 3]) << 24);
}
std::uint64_t u64(const std::vector<std::uint8_t> &bytes, std::uint64_t at,
                  std::uint64_t end) {
  return std::uint64_t(u32(bytes, at, end)) |
         (std::uint64_t(u32(bytes, at + 4, end)) << 32);
}
void direct(const std::vector<std::uint8_t> &bytes, std::uint64_t at,
            std::uint64_t end, Surface::Material &state) {
  while (at < end) {
    require(end - at >= 16, "WORLD DIRECT GIF tag is truncated");
    const auto low = u64(bytes, at, end), high = u64(bytes, at + 8, end);
    at += 16;
    const auto count = low & 0x7fffu;
    const auto nreg = (low >> 60) & 15u;
    require(((low >> 58) & 3u) == 0 && nreg == 1 && (high & 15u) == 14,
            "unsupported WORLD material DIRECT GIF tag");
    require(count <= (end - at) / 16,
            "WORLD material DIRECT register list is truncated");
    for (std::uint64_t i = 0; i < count; ++i, at += 16) {
      const auto value = u64(bytes, at, end);
      const auto reg = u64(bytes, at + 8, end);
      require(reg <= 0xff, "invalid WORLD material GS register address");
      if (reg == 0x42) {
        require(value == 1 || value == 2 || value == 9 || value == 0x48,
                "unsupported WORLD ALPHA_1 value");
        state.alpha1 = std::uint8_t(value);
      } else if (reg == 0x47) {
        require(value == 0x587f4 || value == 0x582f5,
                "unsupported WORLD TEST_1 value");
        state.test1 = std::uint32_t(value);
      } else {
        require(reg == 0x43 && value == 0x2a,
                "unsupported WORLD material DIRECT register");
        state.alpha2_set = true;
        state.alpha2 = 0x2a;
      }
    }
  }
}
} // namespace

Surface::Material chunk_header_material(std::uint32_t flags) {
  const auto gs = chunk_gs(flags);
  const auto light = chunk_light(flags);
  Surface::Material state;
  state.alpha1 = gs.alpha1;
  state.alpha2_set = gs.alpha2_set;
  state.alpha2 = gs.alpha2;
  state.test1 = gs.test;
  state.sort_layer = static_cast<std::uint8_t>(gs.layer);
  state.environment = gs.env;
  state.flags_after = light.flags_after;
  state.dynamic_light = light.kind == ChunkLightKind::Dynamic;
  state.light_constant =
      static_cast<std::int16_t>(light.constant.value_or(0));
  return state;
}

std::vector<Surface::Material> packet_materials(
    const std::vector<std::uint8_t> &bytes,
    const world_geometry::Packet &packet, std::uint32_t flags) {
  auto state = chunk_header_material(flags);
  std::vector<Surface::Material> result;
  result.reserve(packet.batches.size());
  if (bytes.empty()) {
    for (const auto &batch : packet.batches) {
      auto material = state;
      material.blend_enabled = batch.gif_tags.empty() ||
                               (batch.gif_tags.front()[1] & 0x200000u) != 0;
      result.push_back(material);
    }
    return result;
  }
  const std::uint64_t end = std::uint64_t(packet.data_offset) +
                            std::uint64_t(packet.qwc) * 16;
  require(packet.data_offset <= end && end <= bytes.size(),
          "WORLD material packet exceeds input");
  std::uint64_t at = packet.data_offset;
  std::size_t batch_index = 0;
  while (at < end) {
    if (batch_index < packet.batches.size() &&
        at == packet.batches[batch_index].offset) {
      auto material = state;
      const auto &tags = packet.batches[batch_index].gif_tags;
      require(!tags.empty(), "WORLD material batch has no GIF tag");
      material.blend_enabled = (tags.front()[1] & 0x200000u) != 0;
      material.environment = packet.batches[batch_index].program == 0x48;
      result.push_back(material);
      ++batch_index;
    }
    const auto code = u32(bytes, at, end);
    at += 4;
    const auto command = (code >> 24) & 0x7fu;
    const auto count = (code >> 16) & 0xffu;
    const auto immediate = code & 0xffffu;
    if (command == 0x50) {
      const std::uint64_t size = std::uint64_t(immediate ? immediate : 65536) * 16;
      require(size <= end - at, "WORLD DIRECT exceeds material packet");
      direct(bytes, at, at + size, state);
      at += size;
    } else if ((command & 0x60u) == 0x60u && !(command & 0x10u)) {
      const auto components = ((command >> 2) & 3u) + 1;
      const auto unit = 4u >> (command & 3u);
      const auto size = std::uint64_t(count ? count : 256) * components * unit;
      const auto aligned = (size + 3) & ~std::uint64_t(3);
      require(aligned <= end - at, "WORLD UNPACK exceeds material packet");
      at += aligned;
    } else {
      require(command == 0 || command == 1 || command == 4 || command == 5 ||
                  command == 0x11 || command == 0x14,
              "unsupported WORLD material VIF command");
    }
  }
  require(batch_index == packet.batches.size(),
          "WORLD material batch offset is not in packet");
  return result;
}
} // namespace champions::presentation::rta::level_texture
