// Evidence: research t3/research-textures 1528708,
// docs/formats/model-texture.md (companion texture, UVs, flag 0x10, header
// +0x18/+0x20, "Passes, blending, shell") and docs/spec/render-pipeline.md
// 4.7 (program 0x44 environment UV, V), section 6 (ALPHA equations) and 9.6.
// Uses the released model and texture decoders. No equation is invented for a
// register the tables do not list.
#include "champions/model_texture_bind.hpp"
#include <cctype>

namespace champions::model_texture {
namespace {
Blend classify(const std::vector<ModelPacket> &packets, std::size_t tag,
               bool required) {
  bool on = false, off = false, unstated = false, any = false;
  for (const auto &packet : packets) {
    if (packet.gif_tags.size() <= tag) {
      if (required)
        unstated = true;
      continue;
    }
    any = true;
    const auto low = packet.gif_tags[tag][0];
    if (!((low >> 46) & 1)) { // PRE clear: the tag sets no PRIM
      unstated = true;
      continue;
    }
    // PRIM ABE is bit 6 of the 11-bit primitive field at bit 47.
    ((((low >> 47) & 0x7ff) & 0x40) ? on : off) = true;
  }
  if (!any || unstated)
    return Blend::Unstated;
  if (on && off)
    return Blend::Mixed;
  return on ? Blend::On : Blend::Off;
}
} // namespace

AlphaMode alpha_mode(std::uint64_t value) {
  AlphaMode mode;
  switch (value) {
  case 1:
  case 2:
  case 9:
  case 0x2a:
  case 0x44:
  case 0x48:
    mode.equation = static_cast<std::uint32_t>(value);
    return mode;
  default:
    break;
  }
  const auto low = static_cast<std::uint32_t>(value);
  const auto high = value >> 32;
  if (high <= kMaxDocumentedFix && (low == 0x64 || low == 0x68)) {
    mode.equation = low;
    mode.fix = static_cast<std::uint8_t>(high);
  }
  return mode;
}

std::optional<std::array<double, 3>>
blend_rgb(std::uint32_t equation, double fix, const std::array<double, 3> &src,
          const std::array<double, 3> &dst, double as, bool blend_enabled) {
  if (!blend_enabled)
    return src;
  std::array<double, 3> out{};
  auto each = [&](auto pixel) {
    for (unsigned i = 0; i < 3; ++i)
      out[i] = pixel(src[i], dst[i]);
    return out;
  };
  switch (equation) {
  case 1:
    return each([&](double s, double d) { return s * (1.0 - as) + d * as; });
  case 2:
    return each([&](double s, double d) {
      (void)d;
      return s * (1.0 - as);
    });
  case 9:
    return each([&](double s, double d) { return s + d * as; });
  case 0x2a:
    return src;
  case 0x44:
    return each([&](double s, double d) { return s * as + d * (1.0 - as); });
  case 0x48:
    return each([&](double s, double d) { return d + s * as; });
  case 0x64:
    return each([&](double s, double d) { return s * fix + d * (1.0 - fix); });
  case 0x68:
    return each([&](double s, double d) { return d + s * fix; });
  default:
    return std::nullopt;
  }
}

std::array<double, 2> environment_uv(double right, double down, double width,
                                     double height) {
  const double cx = right * kEnvironmentNormalScale;
  const double cy = down * kEnvironmentNormalScale;
  return {(192.0 + 64.0 * cx) / width, (192.0 + 64.0 * cy) / height};
}

std::string companion_texture_name(const std::string &name) {
  if (name.size() < 4)
    return {};
  std::string ext = name.substr(name.size() - 4);
  for (auto &c : ext)
    c = char(std::tolower(static_cast<unsigned char>(c)));
  if (ext != ".vif")
    return {};
  // Keep the base's case; the lump lookup compares names case-insensitively.
  return name.substr(0, name.size() - 4) + ".tex";
}
Bound bind(Bytes vif, Bytes tex) {
  const auto model = decodeRtaModel(vif);
  Bound out{decodeRtaTexture(tex),
            model.alpha1,
            model.fix2,
            (model.flags & 0x10) != 0,
            alpha_mode(model.alpha1),
            {}};
  out.fix2_above_documented = out.two_pass && out.fix2 > kMaxDocumentedFix;
  const double w = out.texture.width, h = out.texture.height;
  if (!(w > 0 && h > 0))
    throw FormatError("Model texture has no used area.");
  for (const auto &mesh : model.meshes) {
    MeshUv m;
    m.index = mesh.index;
    m.visible_by_default = mesh.visible_by_default;
    m.uv.reserve(mesh.corners.size());
    for (const auto &c : mesh.corners)
      m.uv.push_back({double(c.uv_texels[0]) / w, double(c.uv_texels[1]) / h});
    m.environment_tag = !mesh.packets.empty();
    for (const auto &packet : mesh.packets) {
      if (packet.gif_tags.size() < 2)
        m.environment_tag = false;
      else if (!out.two_pass)
        out.env_tag_without_two_pass = true;
    }
    if (out.two_pass && !m.environment_tag)
      out.two_pass_mesh_without_env_tag = true;
    m.environment = out.two_pass && m.environment_tag &&
                    !out.fix2_above_documented;
    m.blend = classify(mesh.packets, 0, true);
    m.environment_blend =
        m.environment ? classify(mesh.packets, 1, true) : Blend::Unstated;
    out.meshes.push_back(std::move(m));
  }
  return out;
}
} // namespace champions::model_texture
