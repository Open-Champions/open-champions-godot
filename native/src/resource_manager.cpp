#include "champions/resource_manager.hpp"

#include <stdexcept>

namespace champions::resources {
namespace {
std::string key(std::string value) {
  for (auto &ch : value)
    if (ch >= 'A' && ch <= 'Z') ch = char(ch + ('a' - 'A'));
  return value;
}
} // namespace

void Manager::load_lmp(const std::string &name, Bytes image) {
  if (name.empty() || (!image.data && image.size))
    throw std::invalid_argument("resource lump needs a name and byte range");
  auto owned = std::make_shared<std::vector<std::uint8_t>>();
  if (image.size) owned->assign(image.data, image.data + image.size);
  const auto parsed = readLmp({owned->data(), owned->size()});
  Lump replacement;
  replacement.image = std::move(owned);
  for (const auto &entry : parsed) {
    if (!replacement.entries.emplace(key(entry.name), entry).second)
      throw FormatError("resource lump has duplicate entry names");
  }
  replacement.generation = next_generation_++;
  lumps_[key(name)] = std::move(replacement);
}

void Manager::unload(const std::string &name) { lumps_.erase(key(name)); }

std::optional<Handle> Manager::find(const std::string &lump,
                                    const std::string &entry) const {
  const auto source = lumps_.find(key(lump));
  if (source == lumps_.end() || !source->second.entries.count(key(entry)))
    return std::nullopt;
  return Handle{source->second.generation, key(lump), key(entry)};
}

std::optional<Bytes> Manager::bytes(const Handle &handle) const {
  const auto source = lumps_.find(key(handle.lump));
  if (source == lumps_.end() || source->second.generation != handle.generation)
    return std::nullopt;
  const auto entry = source->second.entries.find(key(handle.entry));
  if (entry == source->second.entries.end()) return std::nullopt;
  return payload({source->second.image->data(), source->second.image->size()},
                 entry->second);
}

} // namespace champions::resources
