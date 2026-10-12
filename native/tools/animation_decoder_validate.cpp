// Optional private-disc validation. No data or paths are emitted by default.
#include "../tests/animation_decoder_trace.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
namespace A = champions::simulation::rta::animation_decoder;
namespace R = champions::simulation::rta;
namespace T = champions::test::animation_decoder_trace;
namespace {
std::string hex(const std::string &s) {
  const char *digits = "0123456789abcdef";
  std::string out;
  for (unsigned char c : s) {
    out += digits[c >> 4];
    out += digits[c & 15];
  }
  return out;
}
std::string metadata(const A::Clip &c) {
  std::vector<std::string> bind, events;
  for (const auto &v : c.bind)
    bind.push_back(T::integers(v));
  for (const auto &v : c.events)
    events.push_back(T::sequence({T::n(v.seconds), T::n(v.mask)}));
  return T::sequence({T::n(c.joint_count), T::n(c.length), T::sequence(bind),
                      T::integers(c.depth), T::sequence(events),
                      c.sound_times ? T::integers(*c.sound_times) : "null",
                      T::n(c.sound_group), T::n(c.joint_mask),
                      T::integers(c.words)});
}
std::string cache(const A::Context &c) {
  auto s = c.snapshot();
  std::vector<std::string> tracks, slots;
  for (const auto &v : s.tracks)
    tracks.push_back(T::sequence({T::n(v.id), T::track(v.value)}));
  for (const auto &v : s.slots) {
    std::vector<std::string> joints;
    for (const auto &j : v.joints)
      joints.push_back(T::joint(j));
    slots.push_back(
        T::sequence({T::n(v.index), T::n(v.clip), T::n(v.cursor), T::n(v.tick),
                     T::n(v.owner), T::n(v.yaw), T::sequence(joints)}));
  }
  return T::sequence(
      {T::sequence(tracks), T::sequence(slots), T::integers(s.order)});
}
template <class F> std::string outcome(F &&f) {
  try {
    f();
    return "ok";
  } catch (const A::HostFault &) {
    return "HostFault";
  } catch (const A::ValueError &) {
    return "ValueError";
  } catch (const A::IndexError &) {
    return "IndexError";
  } catch (const A::ResourceLimit &) {
    return "ResourceLimit";
  } catch (const A::Unsupported &) {
    return "Unsupported";
  }
}
int run(const std::vector<std::filesystem::path> &args) {
  std::filesystem::path directory;
  std::string game;
  unsigned config = 0;
  bool observations = false;
  for (std::size_t i = 0; i < args.size(); ++i) {
    auto option = args[i].u8string();
    if (option == "--observations") {
      observations = true;
      continue;
    }
    if (i + 1 == args.size())
      throw std::runtime_error("missing option value");
    auto value = args[++i];
    if (option == "--input-dir")
      directory = value;
    else if (option == "--game")
      game = value.u8string();
    else if (option == "--fpu-config") {
      auto s = value.u8string();
      if (s.empty() || s.size() > 2 ||
          s.find_first_not_of("0123456789") != s.npos)
        throw std::runtime_error("invalid FPU config");
      config = static_cast<unsigned>(std::stoul(s));
    } else
      throw std::runtime_error("unknown option");
  }
  if (game != "rta" || config > 15 || directory.empty() ||
      !std::filesystem::is_directory(directory))
    throw std::runtime_error(
        "requires --game rta --input-dir DIRECTORY and FPU config0..15");
  R::set_fpu(static_cast<R::Fpu::Model>(config / 4), (config & 2) != 0,
             (config & 1) != 0);
  std::vector<std::filesystem::path> paths;
  for (const auto &entry :
       std::filesystem::recursive_directory_iterator(directory)) {
    auto ext = entry.path().extension().u8string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
      return char(c >= 'A' && c <= 'Z' ? c + 32 : c);
    });
    if (ext == ".anm" && entry.is_regular_file())
      paths.push_back(entry.path());
  }
  std::sort(paths.begin(), paths.end(), [&](const auto &a, const auto &b) {
    return a.lexically_relative(directory).generic_u8string() <
           b.lexically_relative(directory).generic_u8string();
  });
  std::size_t decoded = 0, refused = 0, samples = 0;
  for (const auto &path : paths) {
    A::Context context(2);
    std::string meta = "[]";
    std::vector<std::string> steps;
    bool good = true;
    auto status = outcome([&] {
      const auto size = std::filesystem::file_size(path);
      if (std::filesystem::is_symlink(path) || size > A::MAX_INPUT_BYTES)
        throw A::ResourceLimit("private input bounds");
      std::ifstream input(path, std::ios::binary);
      if (!input)
        throw A::ValueError("cannot read input");
      std::vector<std::uint8_t> data;
      data.reserve(static_cast<std::size_t>(size));
      for (char ch; input.get(ch);) {
        if (data.size() == A::MAX_INPUT_BYTES)
          throw A::ResourceLimit("private input growth");
        data.push_back(static_cast<std::uint8_t>(ch));
      }
      if (input.bad())
        throw A::ValueError("input read failure");
      auto id = context.add_clip(data);
      const auto &clip = context.clip(id);
      meta = metadata(clip);
      const std::array<R::Raw, 3> times{
          R::ZERO,
          R::div(R::from_int(static_cast<std::uint32_t>(clip.length)),
                 0x40000000u),
          R::from_int(static_cast<std::uint32_t>(
              std::max<std::int64_t>(std::int64_t(clip.length) - 1, 0)))};
      for (auto time : times) {
        std::vector<std::string> results;
        auto result = outcome([&] {
          for (const auto &v : context.decode_clip(id, time))
            results.push_back(T::sequence(
                {T::integers(v.position), T::integers(v.quaternion)}));
        });
        ++samples;
        good = good && result == "ok";
        if (observations)
          steps.push_back(T::sequence({T::n(time), '"' + result + '"',
                                       T::sequence(results), cache(context)}));
      }
    });
    if (status == "ok")
      status = good ? "ok" : "refused";
    if (status == "ok")
      ++decoded;
    else
      ++refused;
    if (observations)
      std::cout << T::sequence({'"' +
                                    hex(path.lexically_relative(directory)
                                            .generic_u8string()) +
                                    '"',
                                '"' + status + '"', meta, T::sequence(steps)})
                << '\n';
  }
  if (observations)
    std::cout << '#';
  std::cout << "{\"format\":\"rta-animation-decoder\",\"fpu_config\":" << config
            << ",\"files\":" << paths.size() << ",\"decoded\":" << decoded
            << ",\"refused\":" << refused << ",\"samples\":" << samples
            << "}\n";
  std::cout.flush();
  if (!std::cout)
    throw std::runtime_error("report write failure");
  return 0;
}
} // namespace
#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
#else
int main(int argc, char **argv) {
#endif
  try {
    std::vector<std::filesystem::path> args;
    for (int i = 1; i < argc; ++i)
      args.emplace_back(argv[i]);
    return run(args);
  } catch (const std::filesystem::filesystem_error &) {
    std::cerr
        << "Private decoder validation refused: filesystem access failure\n";
    return 1;
  } catch (const std::exception &e) {
    std::cerr << "Private decoder validation refused: " << e.what() << '\n';
    return 1;
  }
}
