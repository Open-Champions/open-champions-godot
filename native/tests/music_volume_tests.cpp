#include "champions/music_volume.hpp"
#include "champions/native_audio.hpp"
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// g_musicVolume 0x4f8944: host checks of the executed anchors in
// docs/spec/music-volume.md (research 22408a1), and, given a vector file from
// tools/generate_music_volume_vectors.py, a bit-exact replay of every record.
using namespace champions::audio;
namespace {
int checks = 0;
void check(bool ok, const std::string &msg) {
  ++checks;
  if (!ok)
    throw std::runtime_error(msg);
}
std::vector<std::uint32_t> words(std::istringstream &in) {
  std::vector<std::uint32_t> out;
  std::string t;
  while (in >> t)
    out.push_back(std::uint32_t(std::stoul(t, nullptr, 16)));
  return out;
}
std::string hex(std::uint32_t v) {
  std::ostringstream s;
  s << std::hex << v;
  return s.str();
}

void anchors() {
  check(boot_music_volume(std::nullopt) == 0x3f000000u, "no options file: 0.5 at the first request");
  check(boot_music_volume(0x3fc00000u) == 0x3fc00000u && boot_music_volume(0xbf800000u) == 0xbf800000u &&
            boot_music_volume(0x7fffffffu) == 0x7fffffffu && boot_music_volume(0) == 0,
        "the options-file word is copied with no clamp");
  MusicVolumeWords w;
  check(w.live == 0x3f800000u && w.image == 0, "ELF image words");
  w.set_defaults(2);
  check(w.live == 0x3f800000u && w.image == 0, "config_setDefaults(2) leaves the music words");
  w.set_defaults(-1);
  check(w.live == 0x3f000000u && w.image == 0x3f000000u, "config_setDefaults(-1) stores 0.5 twice");
  w.browser_copy(0, 0x3e800000u);
  check(w.image == 0x3f000000u, "mode 0 does not copy");
  w.browser_copy(1, 0x3e800000u);
  check(w.live == 0x3f000000u && w.image == 0x3e800000u, "boot copy writes the image, not the live word");
  w.add_core_tasks();
  check(w.live == 0x3e800000u, "game_addCoreTasks copies the image");

  std::uint32_t v = music_volume_default;
  int steps = 0, below = 0;
  while (true) {
    const auto n = slider_step(v, 0x400000);
    ++steps;
    if (!below && !music_gate_open(n)) {
      below = steps;
      check(n == 0x3d4ccc08u, "first value below the gate is 0x3d4ccc08");
    }
    if (n == v)
      break;
    v = n;
  }
  check(v == 0 && steps - 1 == 60 && below == 54, "down from 0.5: 0 after 60 steps, below the gate at 54");
  v = music_volume_default;
  steps = 0;
  while (true) {
    const auto n = slider_step(v, 0x800000);
    ++steps;
    if (n == v)
      break;
    v = n;
  }
  check(v == float_one && steps - 1 == 61, "up from 0.5: 1.0 after 61 steps");
  std::uint32_t timer = 0;
  int idle = 0;
  for (;;) {
    const auto c = menu_slider_call(music_volume_default, 0x400000, timer);
    timer = c.timer;
    if (c.active)
      break;
    ++idle;
  }
  check(idle == 4 && timer == float_one, "the first four calls are idle");
  check(slider_step(0x3fc00000u, 0x2000) == float_one && slider_step(0xbe800000u, 0x8000) == 0,
        "one press pulls an out-of-range file value into 0..1");
  check(music_gate_open(music_volume_gate) && !music_gate_open(music_volume_gate - 1) &&
            !music_gate_open(0) && !music_gate_open(0x80000000u) && music_gate_open(0x7fffffffu) &&
            !music_gate_open(0xffffffffu),
        "the gate pattern itself is queued");
  Playback p;
  check(p.music_volume() == music_volume_default, "Playback starts from save_init's 0.5");
}

std::size_t replay(const char *path) {
  std::ifstream f(path);
  check(bool(f), "vector file opens");
  std::size_t records = 0;
  std::string line;
  while (std::getline(f, line)) {
    if (line.empty() || line[0] == '#')
      continue;
    std::istringstream in(line);
    std::string tag;
    in >> tag;
    const std::string where = tag + " record " + std::to_string(records + 1);
    if (tag == "CHAIN") {
      std::string file;
      in >> file;
      const auto w = words(in);
      check(w.size() == 6, where + ": shape");
      MusicVolumeWords m;
      check(m.live == w[0], where + ": ELF word");
      m.set_defaults(-1);
      check(m.live == w[1], where + ": save_init");
      if (file != "none")
        m.browser_copy(1, std::uint32_t(std::stoul(file, nullptr, 16)));
      check(m.live == w[2], where + ": boot card copy leaves the live word");
      m.add_core_tasks();
      check(m.live == w[3] && m.live == w[4], where + ": game_addCoreTasks");
      const std::optional<std::uint32_t> opt =
          file == "none" ? std::nullopt : std::optional<std::uint32_t>(std::stoul(file, nullptr, 16));
      check(boot_music_volume(opt) == w[4] && music_gate_open(w[4]) == (w[5] != 0), where + ": first request");
      ++records;
      continue;
    }
    const auto w = words(in);
    auto need = [&](std::size_t n) { check(w.size() == n, where + ": shape"); };
    if (tag == "CONST") {
      need(8);
      check(w[0] == music_volume_elf_image && w[1] == options_music_elf_image &&
                w[2] == music_volume_default && w[3] == music_volume_gate && w[4] == music_slider_step &&
                w[5] == menu_repeat_step && w[6] == slider_lower_bits && w[7] == slider_raise_bits,
            where + ": constants");
    } else if (tag == "DEFAULTS") {
      need(5);
      MusicVolumeWords m{w[1], w[2]};
      m.set_defaults(std::int32_t(w[0]));
      check(m.live == w[3] && m.image == w[4], where);
    } else if (tag == "CORE") {
      need(6);
      MusicVolumeWords m{0x12345678u, w[0]};
      m.add_core_tasks();
      const auto a = music_apply_volume(m.live, w[1], w[2]);
      check(m.live == w[3] && a.sent == (w[4] != 0) && a.channel == w[5], where + " image " + hex(w[0]));
    } else if (tag == "BOOT") {
      need(6);
      MusicVolumeWords m{0x12345678u, w[1]};
      m.browser_copy(w[0], w[2]);
      check(m.image == w[4] && m.live == 0x12345678u, where);
    } else if (tag == "OPTS") {
      need(6);
      MusicVolumeWords m{w[1], w[2]};
      m.options_screen(std::int32_t(w[0]), w[3]);
      check(m.live == w[4] && m.image == w[5], where);
    } else if (tag == "SLIDE") {
      need(3);
      check(slider_step(w[0], w[1]) == w[2], where + " value " + hex(w[0]) + " held " + hex(w[1]));
    } else if (tag == "CALL") {
      need(6);
      const auto c = menu_slider_call(w[0], w[1], w[2]);
      check(c.value == w[3] && c.timer == w[4] && c.active == (w[5] != 0), where);
    } else if (tag == "WALK") {
      check(w.size() >= 3 && w.size() == 3 + w[2], where + ": shape");
      auto v = w[0];
      for (std::size_t i = 0; i < w[2]; ++i) {
        v = slider_step(v, w[1]);
        check(v == w[3 + i], where + " step " + std::to_string(i + 1));
      }
    } else if (tag == "COMMIT") {
      need(8);
      const auto c = config_commit_music(w[0], w[1], w[2] != 0, w[3] != 0, w[4] != 0);
      check(c.count == w[5] && c.image == w[6] && c.save_requested == (w[7] != 0), where);
    } else if (tag == "GATE") {
      need(2);
      check(music_gate_open(w[0]) == (w[1] != 0), where + " volume " + hex(w[0]));
    } else if (tag == "APPLY") {
      need(5);
      const auto a = music_apply_volume(w[0], w[1], w[2]);
      check(a.sent == (w[3] != 0) && a.channel == w[4], where);
    } else {
      check(false, "unknown record " + tag);
    }
    ++records;
  }
  return records;
}
} // namespace

int main(int argc, char **argv) {
  try {
    anchors();
    if (argc > 1) {
      const auto records = replay(argv[1]);
      std::cout << "music volume: " << records << " records, " << checks << " checks passed\n";
    } else {
      std::cout << "music volume: " << checks << " checks passed\n";
    }
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "music volume: " << e.what() << "\n";
    return 1;
  }
}
