// Counts-only census of the string tables and of the level-script calls of
// setInfo, addHelpMessage, addHelpMessageF, showBust and showCompass, over an
// explicitly selected read-only RTA directory (research d4987fd
// tools/strings_census.py, docs/strings.md section 6, written natively).
// Prints totals and histograms only: never a key, a text, a path or a level name.
#include "champions/amx.hpp"
#include "champions/archive.hpp"
#include "champions/strings.hpp"
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;
namespace st = champions::simulation::rta::strings;
namespace amx = champions::simulation::rta::amx;

namespace {
constexpr std::uintmax_t MAX_FILE = 512ull * 1024 * 1024;

std::string lower(std::string text) {
  for (auto &c : text)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return text;
}
std::vector<std::uint8_t> read_file(const fs::path &path) {
  std::error_code ec;
  const auto size = fs::file_size(path, ec);
  if (ec || size > MAX_FILE)
    return {};
  std::ifstream in(path, std::ios::binary);
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
  if (size && !in.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(size)))
    return {};
  return bytes;
}
void scan(const fs::path &dir, int depth, std::vector<fs::path> &out) {
  if (depth > 3)
    return;
  std::error_code ec;
  std::vector<fs::path> here;
  for (auto it = fs::directory_iterator(dir, fs::directory_options::skip_permission_denied, ec);
       !ec && it != fs::directory_iterator(); it.increment(ec)) {
    if (it->is_symlink(ec))
      continue;
    here.push_back(it->path());
  }
  std::sort(here.begin(), here.end());
  for (const auto &p : here) {
    if (fs::is_directory(p, ec))
      scan(p, depth + 1, out);
    else
      out.push_back(p);
  }
}
std::string ext(const std::string &name) {
  const auto dot = name.rfind('.');
  return dot == std::string::npos ? std::string() : lower(name.substr(dot + 1));
}

struct Blob {
  std::string container_kind; // "lmp" or "gob"
  bool gtext = false;
  std::vector<std::uint8_t> bytes;
};

// One lump's entries: (name, bytes).
std::vector<std::pair<std::string, std::vector<std::uint8_t>>> lump_entries(const std::vector<std::uint8_t> &lump) {
  std::vector<std::pair<std::string, std::vector<std::uint8_t>>> out;
  try {
    const champions::Bytes b{lump.data(), lump.size()};
    for (const auto &e : champions::readLmp(b)) {
      const auto p = champions::payload(b, e);
      out.emplace_back(e.name, std::vector<std::uint8_t>(p.data, p.data + p.size));
    }
  } catch (const std::exception &) {
  }
  return out;
}

bool ends_in_conversion(const st::Units &u) {
  bool last = false;
  for (std::size_t i = 0; i < u.size();) {
    if (u[i] == 0x25) {
      last = i + 2 >= u.size();
      i += 2;
    } else {
      last = false;
      ++i;
    }
  }
  return last;
}

struct TableStats {
  std::map<std::string, std::uint64_t> counts;
};

void print_table(const std::string &label, const std::vector<std::uint8_t> &data) {
  std::vector<st::UniEntry> entries;
  try {
    entries = st::uni_entries(data.data(), data.size());
  } catch (const std::exception &) {
    std::cout << label << " malformed\n";
    return;
  }
  std::map<std::string, int> seen;
  std::size_t longest_key = 0, longest_text = 0, over31 = 0, over99 = 0, over127 = 0, over255 = 0;
  std::size_t with_percent = 0, ending = 0, quoted = 0, wide = 0, empty = 0;
  std::map<std::string, std::uint64_t> conv;
  for (const auto &e : entries) {
    seen[e.key] += 1;
    longest_key = std::max(longest_key, e.key.size());
    longest_text = std::max(longest_text, e.units.size());
    over31 += e.units.size() > 31;
    over99 += e.units.size() > 99;
    over127 += e.units.size() > 127;
    over255 += e.units.size() > 255;
    empty += e.units.empty();
    quoted += e.key.find('"') != std::string::npos;
    bool has_wide = false, has_percent = false;
    for (std::size_t i = 0; i < e.units.size(); ++i) {
      has_wide |= e.units[i] >= 0x100;
      if (e.units[i] == 0x25) {
        has_percent = true;
        const auto next = i + 1 < e.units.size() ? e.units[i + 1] : 0;
        const char c = next < 0x80 ? static_cast<char>(next) : 0;
        conv[c == '%' ? "%%" : c == 'd' ? "%d" : c == 'c' ? "%c" : c == 's' ? "%s" : c == 'u' ? "%u" : c == 'f' ? "%f" : next ? "other" : "lone % at end"] += 1;
        ++i;
      }
    }
    wide += has_wide;
    with_percent += has_percent;
    ending += ends_in_conversion(e.units);
  }
  st::TextTable table;
  std::size_t probe_max = 0;
  try {
    const auto placed = table.register_table(data, 0x100000);
    for (std::size_t i = 0; i < placed.size(); ++i)
      probe_max = std::max<std::size_t>(probe_max, (placed[i].second - st::text_hash(entries[i].key)) & 0x1fffu);
  } catch (const std::exception &) {
  }
  std::size_t repeated = 0;
  for (const auto &kv : seen)
    repeated += static_cast<std::size_t>(kv.second - 1);
  std::cout << label << " entries " << entries.size() << " distinct_keys " << seen.size() << " repeated "
            << repeated << " quoted_keys " << quoted << " longest_key " << longest_key << " longest_text "
            << longest_text << " empty " << empty << " over31 " << over31 << " over99 " << over99
            << " over127 " << over127 << " over255 " << over255 << " wide_unit_texts " << wide
            << " with_percent " << with_percent << " ending_in_conversion " << ending << " longest_probe "
            << probe_max << '\n';
  std::cout << label << " conversions";
  for (const auto &kv : conv)
    std::cout << ' ' << kv.first << ' ' << kv.second;
  std::cout << '\n';
}

// SYSREQ.C call sites and their constant arguments, as the research census
// reads them (PUSH.C pushes a constant, PUSH a non-literal; the last push
// before SYSREQ.C is the byte count).
struct Site {
  std::string name;
  std::vector<std::optional<amx::Cell>> args;
};
std::vector<Site> call_sites(amx::Machine &machine) {
  std::vector<Site> out;
  if (!machine.decoded)
    return out;
  const auto names = machine.native_names();
  std::vector<std::optional<amx::Cell>> pending;
  for (const auto &entry : *machine.decoded) {
    const auto &ins = entry.second;
    const auto op = ins.opcode;
    const auto operand = [&]() -> amx::Cell {
      if (const auto *v = std::get_if<amx::Cell>(&ins.operand))
        return *v;
      return 0;
    };
    if (op == 0x7d)
      continue;
    if (op == 0x7b) {
      const auto n = static_cast<std::size_t>(operand());
      Site site;
      site.name = n < names.size() ? names[n] : "?";
      if (!pending.empty())
        pending.pop_back();
      site.args.assign(pending.rbegin(), pending.rend());
      out.push_back(std::move(site));
      pending.clear();
      continue;
    }
    if (op == 0x27) {
      pending.push_back(operand());
      continue;
    }
    if (op == 0x28) {
      pending.push_back(std::nullopt);
      continue;
    }
    pending.clear();
  }
  return out;
}

std::optional<std::string> literal(amx::Machine &machine, const std::optional<amx::Cell> &arg) {
  if (!arg || machine.get_addr(*arg) != amx::ERR_NONE)
    return std::nullopt;
  try {
    return machine.get_string(*arg);
  } catch (const std::exception &) {
    return std::nullopt;
  }
}
} // namespace

int main(int argc, char **argv) {
  if (argc != 4 || std::string(argv[1]) != "--game" || std::string(argv[2]) != "rta" ||
      !fs::is_directory(argv[3])) {
    std::cerr << "usage: champions-strings-census --game rta DIR\n";
    return 2;
  }
  std::vector<fs::path> files;
  scan(argv[3], 0, files);
  // (lump is gtext.lmp, entry bytes) for every .uni; script images.
  std::vector<std::pair<std::string, std::vector<std::uint8_t>>> unis;
  std::vector<std::vector<std::uint8_t>> scripts;
  std::uint64_t txt_files = 0;
  auto take_lump = [&](const std::string &lump_name, const std::vector<std::uint8_t> &lump) {
    for (const auto &[name, bytes] : lump_entries(lump)) {
      const auto e = ext(name);
      if (e == "uni")
        unis.emplace_back(lower(lump_name) == "gtext.lmp" ? "gtext" : "other", bytes);
      else if (e == "txt")
        ++txt_files;
      else if (lower(name) == "script.scr")
        scripts.push_back(bytes);
    }
  };
  for (const auto &path : files) {
    const auto name = path.filename().string();
    const auto e = ext(name);
    if (e != "lmp" && e != "gob")
      continue;
    const auto data = read_file(path);
    if (e == "lmp") {
      take_lump(name, data);
      continue;
    }
    try {
      const champions::Bytes b{data.data(), data.size()};
      for (const auto &entry : champions::readGob(b)) {
        const auto p = champions::payload(b, entry);
        take_lump(entry.name, std::vector<std::uint8_t>(p.data, p.data + p.size));
      }
    } catch (const std::exception &) {
    }
  }
  std::cout << "string_files " << unis.size() + txt_files << " uni " << unis.size() << " txt " << txt_files << '\n';
  st::TextTable english;
  std::size_t index = 0, gtext = 0;
  for (const auto &[kind, bytes] : unis) {
    print_table("table " + std::to_string(index++) + " " + kind, bytes);
    if (kind == "gtext") {
      ++gtext;
      try {
        english.register_table(bytes, english.next_base());
      } catch (const std::exception &) {
      }
    }
  }
  std::cout << "gtext_tables " << gtext << " english_entries " << english.entries() << '\n';
  if (!english.entries()) {
    std::cout << "no GTEXT.LMP table\n";
    return 1;
  }
  const std::map<std::string, std::vector<unsigned>> keys_of = {
      {"setInfo", {0}}, {"addHelpMessage", {0, 1}}, {"addHelpMessageF", {0, 2}},
      {"showBust", {1}}, {"showCompass", {1}}};
  std::map<std::string, std::map<std::string, std::uint64_t>> out;
  std::map<std::string, std::set<std::string>> distinct;
  std::map<std::string, std::size_t> longest;
  std::uint64_t loaded = 0, refused_images = 0;
  for (const auto &image : scripts) {
    std::unique_ptr<amx::Machine> machine;
    try {
      machine = std::make_unique<amx::Machine>(amx::ByteVector(image.begin(), image.end()));
      if (machine->load() != amx::ERR_NONE) {
        ++refused_images;
        continue;
      }
    } catch (const std::exception &) {
      ++refused_images;
      continue;
    }
    ++loaded;
    for (const auto &site : call_sites(*machine)) {
      const auto found = keys_of.find(site.name);
      if (found == keys_of.end())
        continue;
      auto &c = out[site.name];
      c["calls"] += 1;
      bool all_literal = true, refused = false;
      std::vector<std::string> keys;
      for (const auto k : found->second) {
        const auto key = k < site.args.size() ? literal(*machine, site.args[k]) : std::nullopt;
        if (!key) {
          all_literal = false;
          c["key_not_literal"] += 1;
          continue;
        }
        keys.push_back(*key);
        distinct[site.name].insert(*key);
        const bool hit = english.find(*key) != 0;
        if (!hit)
          c["key_missing_shows_nf"] += 1;
        const auto units = english.peek(*key);
        longest[site.name] = std::max(longest[site.name], hit ? units.size() : std::size_t(0));
        const std::size_t limit = site.name == "setInfo" ? st::MESSAGE_LIMIT
                                  : site.name == "showBust" ? st::BUST_TEXT_LIMIT
                                  : site.name == "showCompass" ? st::COMPASS_LABEL_LIMIT
                                                               : SIZE_MAX;
        if (units.size() > limit) {
          c["text_over_buffer"] += 1;
          refused = true;
        }
        if (site.name == "addHelpMessageF") {
          bool has_d = false, has_other = false;
          for (std::size_t i = 0; i + 1 < units.size(); ++i)
            if (units[i] == 0x25) {
              (units[i + 1] == 'd' ? has_d : has_other) = true;
              ++i;
            }
          c[has_d ? (has_other ? "f_text_d_and_other" : "f_text_conversion_d")
                  : (has_other ? "f_text_other_conversion" : "f_text_no_conversion")] += 1;
          if (ends_in_conversion(units))
            c["f_text_ends_in_conversion"] += 1;
          const auto value_arg = k + 1 < site.args.size() ? site.args[k + 1] : std::nullopt;
          if (!value_arg)
            c["f_value_not_literal"] += 1;
          auto fmt = units;
          fmt.push_back(0);
          try {
            const auto formatted = st::format_one_int(
                [&fmt](std::size_t i) -> std::uint16_t { return i < fmt.size() ? fmt[i] : 0; },
                static_cast<std::uint32_t>(value_arg.value_or(0)));
            const auto length = st::visible(formatted.written).size();
            if (length > (k == 0 ? st::HELP_F_TITLE_LIMIT : st::HELP_F_BODY_LIMIT)) {
              c["f_formatted_over_buffer"] += 1;
              refused = true;
            }
          } catch (const st::Refused &) {
            c["f_format_refused"] += 1;
            refused = true;
          }
        }
      }
      if (site.name == "showBust") {
        const auto maximum = site.args.size() > 3 ? site.args[3] : std::nullopt;
        c[!maximum ? "bust_maximum_not_literal"
                   : *maximum == -1 ? "bust_maximum_minus1_number" : "bust_maximum_literal_meter"] += 1;
        const auto texture = literal(*machine, site.args.empty() ? std::nullopt : site.args[0]);
        if (!texture)
          c["bust_texture_not_literal"] += 1;
        else if (texture->size() > st::BUST_TEXTURE_LIMIT) {
          c["bust_texture_over_39"] += 1;
          refused = true;
        }
      }
      if (site.name == "showCompass" && !literal(*machine, site.args.empty() ? std::nullopt : site.args[0]))
        c["compass_object_not_literal"] += 1;
      if (all_literal)
        c["resolved"] += 1;
      if (refused)
        c["port_refuses"] += 1;
    }
  }
  std::cout << "scripts " << scripts.size() << " loaded " << loaded << " refused " << refused_images << '\n';
  for (const auto &[name, unused] : keys_of) {
    (void)unused;
    std::cout << name << " distinct_literal_keys " << distinct[name].size() << " longest_found_text "
              << longest[name] << '\n';
    for (const auto &[what, n] : out[name])
      std::cout << name << ' ' << what << ' ' << n << '\n';
  }
  return 0;
}
