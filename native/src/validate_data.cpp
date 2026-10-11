#include "champions/animation.hpp"
#include "champions/archive.hpp"
#include "champions/model.hpp"
#include "champions/terrain_file.hpp"
#include "champions/texture.hpp"
#include "champions/world_geometry.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace {
namespace fs = std::filesystem;
constexpr std::size_t file_limit = 512U * 1024U * 1024U;
struct Count {
  std::size_t decoded = 0;
  std::size_t refused = 0;
  std::map<std::string, std::size_t> reasons;
};
using Counts = std::map<std::string, Count>;

std::string extension(const std::string &name) {
  for (const auto *ext :
       {".anm", ".tex", ".etex", ".vif", ".lmp", ".gob", ".world"})
    if (champions::hasExtension(name, ext))
      return ext;
  return {};
}

std::string format(const std::string &ext) {
  if (ext == ".world")
    return "terrain";
  if (ext == ".anm")
    return "animations";
  if (ext == ".vif")
    return "models";
  if (ext == ".tex" || ext == ".etex")
    return "textures";
  return "archives";
}

void inspect(const std::string &ext, champions::Bytes bytes, Counts &counts,
             unsigned depth = 0) {
  if (ext == ".world") {
    // Collision projection and static presentation are independent acceptance
    // gates. Count the same payload occurrence once under each contract.
    auto decode = [&](const std::string &name, auto &&operation) {
      auto &count = counts.at(name);
      try {
        if (bytes.size > 64U * 1024U * 1024U)
          throw std::runtime_error(
              "WORLD input exceeds validation byte budget");
        std::vector<std::uint8_t> owned;
        if (bytes.size)
          owned.assign(bytes.data, bytes.data + bytes.size);
        operation(owned);
        ++count.decoded;
      } catch (const std::exception &error) {
        ++count.refused;
        ++count.reasons[error.what()];
      }
    };
    decode("terrain", [](const auto &owned) {
      (void)champions::simulation::rta::terrain_file::decode(owned);
    });
    decode("world-geometry", [](const auto &owned) {
      (void)champions::presentation::rta::world_geometry::decode(owned);
    });
    return;
  }
  auto &count = counts.at(format(ext));
  try {
    if (ext == ".anm") {
      (void)champions::decodeRtaAnimationKeys(bytes);
    } else if (ext == ".vif") {
      (void)champions::decodeRtaModel(bytes);
    } else if (ext == ".tex" || ext == ".etex") {
      (void)champions::decodeRtaTexture(bytes);
    } else {
      if (depth >= 2)
        throw champions::FormatError(
            "archive nesting exceeds GOB/LMP hierarchy");
      const auto entries =
          ext == ".gob" ? champions::readGob(bytes) : champions::readLmp(bytes);
      // A valid container can still contain refused assets. Count each payload
      // occurrence separately; do not infer pairing or semantic correctness.
      for (const auto &entry : entries) {
        const auto child_ext = extension(entry.name);
        if (!child_ext.empty())
          inspect(child_ext, champions::payload(bytes, entry), counts,
                  depth + 1);
      }
    }
    ++count.decoded;
  } catch (const std::exception &error) {
    ++count.refused;
    ++count.reasons[error.what()];
  }
}

std::vector<std::uint8_t> readFile(const fs::path &path) {
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input)
    throw std::runtime_error("cannot open input file");
  const auto length = input.tellg();
  if (length < 0 || static_cast<std::uintmax_t>(length) > file_limit)
    throw std::runtime_error(
        "input exceeds 512MiB validation limit or has unknown size");
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
  input.seekg(0);
  if (!bytes.empty() && !input.read(reinterpret_cast<char *>(bytes.data()),
                                    static_cast<std::streamsize>(bytes.size())))
    throw std::runtime_error("cannot read complete input file");
  return bytes;
}

int run(const std::vector<fs::path> &args) {
  if (args.size() == 1 && args[0] == "--help") {
    std::cout << "Usage: champions-data-validate --game rta <private extracted "
                 "directory>\n"
                 "Recursively decode ANM, TEX/ETEX, VIF, WORLD terrain/static "
                 "geometry and "
                 "GOB/LMP payloads.\n"
                 "Counts payload occurrences; duplicate loose/container assets "
                 "count twice.\n"
                 "Skips symbolic links and unknown extensions. No extraction "
                 "or asset output.\n"
                 "Acceptance is structural decoding, not gameplay or rendering "
                 "fidelity.\n";
    return 0;
  }
  if (args.size() != 3 || args[0] != "--game" || args[1] != "rta") {
    std::cerr << "Select --game rta and one directory. Norrath format "
                 "validation is not established.\n";
    return 2;
  }
  try {
    if (fs::is_symlink(fs::symlink_status(args[2])) ||
        !fs::is_directory(args[2]))
      throw std::runtime_error(
          "input must be a directory, not a symbolic link");
    std::vector<fs::path> paths;
    for (const auto &entry : fs::recursive_directory_iterator(args[2])) {
      if (fs::is_symlink(entry.symlink_status()) || !entry.is_regular_file())
        continue;
      if (!extension(entry.path().filename().u8string()).empty())
        paths.push_back(entry.path());
    }
    std::sort(paths.begin(), paths.end());
    Counts counts{{"animations", {}}, {"textures", {}}, {"models", {}},
                  {"archives", {}},   {"terrain", {}},  {"world-geometry", {}}};
    for (const auto &path : paths) {
      const auto ext = extension(path.filename().u8string());
      try {
        const auto bytes = readFile(path);
        inspect(ext, {bytes.data(), bytes.size()}, counts);
      } catch (const std::exception &error) {
        auto &count = counts.at(format(ext));
        ++count.refused;
        ++count.reasons[error.what()];
        if (ext == ".world") {
          auto &geometry = counts.at("world-geometry");
          ++geometry.refused;
          ++geometry.reasons[error.what()];
        }
      }
    }
    std::cout
        << "profile\trta-reference-subsets\nformat\tdecoded\trefused\ttotal\n";
    for (const auto &pair : counts) {
      const auto &count = pair.second;
      std::cout << pair.first << '\t' << count.decoded << '\t' << count.refused
                << '\t' << count.decoded + count.refused << '\n';
      for (const auto &reason : count.reasons)
        std::cout << "refusal\t" << pair.first << '\t' << reason.second << '\t'
                  << champions::escapeTsv(reason.first) << '\n';
    }
    if (paths.empty()) {
      std::cerr
          << "No recognized input files found; no coverage was measured.\n";
      return 1;
    }
    return 0; // A coverage report may contain refusals; invocation errors fail.
  } catch (const std::exception &error) {
    std::cerr << "Validation failed: " << error.what() << '\n';
    return 1;
  }
}
} // namespace

#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
  std::vector<fs::path> args;
  for (int i = 1; i < argc; ++i)
    args.emplace_back(argv[i]);
  return run(args);
}
#else
int main(int argc, char **argv) {
  std::vector<fs::path> args;
  for (int i = 1; i < argc; ++i)
    args.push_back(fs::u8path(argv[i]));
  return run(args);
}
#endif
