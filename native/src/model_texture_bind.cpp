// Evidence: research t3/research-anim docs/formats/model-texture.md
// ("Every .vif has a .tex of the same base name", "Texture coordinates",
// "Passes, blending, shell"). Uses the released model and texture decoders.
#include "champions/model_texture_bind.hpp"
#include <cctype>

namespace champions::model_texture {
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
  Bound out{decodeRtaTexture(tex), model.alpha1, {}};
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
    bool on = false, off = false, unstated = false;
    for (const auto &p : mesh.packets) {
      if (p.gif_tags.empty()) {
        unstated = true;
        continue;
      }
      const auto low = p.gif_tags.front()[0];
      if (!((low >> 46) & 1)) { // PRE clear: the tag sets no PRIM
        unstated = true;
        continue;
      }
      ((((low >> 47) & 0x7ff) & 0x40) ? on : off) = true;
    }
    m.blend = unstated ? Blend::Unstated
              : on && off ? Blend::Mixed
              : on        ? Blend::On
                          : Blend::Off;
    out.meshes.push_back(std::move(m));
  }
  return out;
}
} // namespace champions::model_texture
