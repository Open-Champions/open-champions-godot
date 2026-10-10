#include "champions/item_names.hpp"
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
using namespace champions::simulation::rta::items;
namespace {
struct MissingKey : std::out_of_range {
  using std::out_of_range::out_of_range;
};
std::vector<std::string> split(const std::string &text, char separator) {
  std::vector<std::string> result;
  std::size_t start = 0;
  for (;;) {
    auto end = text.find(separator, start);
    result.push_back(
        text.substr(start, end == std::string::npos ? end : end - start));
    if (end == std::string::npos)
      break;
    start = end + 1;
  }
  return result;
}
unsigned digit(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  throw std::runtime_error("invalid hex");
}
std::string unhex(const std::string &text) {
  if (text == "-")
    return {};
  if (text.size() % 2)
    throw std::runtime_error("odd hex length");
  std::string bytes;
  for (std::size_t i = 0; i < text.size(); i += 2)
    bytes += char(digit(text[i]) * 16 + digit(text[i + 1]));
  return bytes;
}
NameText unicode(const std::string &text) {
  auto bytes = unhex(text);
  NameText result;
  for (std::size_t i = 0; i < bytes.size();) {
    const auto lead = static_cast<unsigned char>(bytes[i++]);
    unsigned extra = 0;
    char32_t code = lead, minimum = 0;
    if (lead < 128) {
    } else if (lead >= 0xc2 && lead <= 0xdf) {
      code = lead & 31;
      extra = 1;
      minimum = 0x80;
    } else if (lead >= 0xe0 && lead <= 0xef) {
      code = lead & 15;
      extra = 2;
      minimum = 0x800;
    } else if (lead >= 0xf0 && lead <= 0xf4) {
      code = lead & 7;
      extra = 3;
      minimum = 0x10000;
    } else
      throw std::runtime_error("invalid UTF-8 lead");
    if (extra > bytes.size() - i)
      throw std::runtime_error("truncated UTF-8");
    while (extra--) {
      auto c = static_cast<unsigned char>(bytes[i++]);
      if ((c & 0xc0) != 0x80)
        throw std::runtime_error("invalid UTF-8 continuation");
      code = (code << 6) | (c & 63);
    }
    if (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff))
      throw std::runtime_error("invalid Unicode scalar");
    result += code;
  }
  return result;
}
std::string hextext(const NameText &text) {
  std::string bytes;
  for (auto c : text) {
    if (c < 128)
      bytes += char(c);
    else if (c < 0x800) {
      bytes += char(0xc0 | (c >> 6));
      bytes += char(0x80 | (c & 63));
    } else if (c < 0x10000) {
      bytes += char(0xe0 | (c >> 12));
      bytes += char(0x80 | ((c >> 6) & 63));
      bytes += char(0x80 | (c & 63));
    } else {
      bytes += char(0xf0 | (c >> 18));
      bytes += char(0x80 | ((c >> 12) & 63));
      bytes += char(0x80 | ((c >> 6) & 63));
      bytes += char(0x80 | (c & 63));
    }
  }
  const char digits[] = "0123456789abcdef";
  std::string out;
  for (unsigned char c : bytes) {
    out += digits[c >> 4];
    out += digits[c & 15];
  }
  return out.empty() ? "-" : out;
}
int integer(const std::string &s) {
  std::size_t n = 0;
  auto v = std::stoll(s, &n);
  if (n != s.size() || v < INT32_MIN || v > INT32_MAX)
    throw std::runtime_error("invalid int32");
  return int(v);
}
std::uint64_t unsigned_integer(const std::string &s) {
  std::size_t n = 0;
  auto v = std::stoull(s, &n);
  if (n != s.size() || s.empty() || s[0] == '-')
    throw std::runtime_error("invalid uint64");
  return v;
}
std::pair<std::string, std::string> execute(const std::string &op,
                                            const std::vector<std::string> &a) {
  std::vector<NameText> seen;
  std::map<NameText, NameText> localized;
  TextLookup lookup = [&](const NameText &key) {
    seen.push_back(key);
    auto found = localized.find(key);
    if (found == localized.end())
      throw MissingKey("missing localization key");
    return found->second;
  };
  std::string result;
  try {
    if (op == "strip") {
      auto value = strip_grammar_suffix(unicode(a.at(1)), integer(a.at(0)));
      result = hextext(value.first) + ":" + std::to_string(value.second);
    } else if (op == "formatted")
      result = hextext(formatted_name_text(unicode(a.at(0))));
    else if (op == "affix") {
      auto key = unicode(a.at(2));
      localized[key] = unicode(a.at(3));
      result =
          hextext(affix_form(lookup, integer(a.at(0)), integer(a.at(1)), key));
    } else if (op == "build") {
      if (a.size() != 12)
        throw std::runtime_error("build argument count");
      Tables tables;
      auto def = std::make_shared<Definition>();
      def->name = unhex(a[5]);
      Item item(def);
      item.flags = unsigned_integer(a[2]);
      auto prefix = unsigned_integer(a[3]), suffix = unsigned_integer(a[4]);
      if (prefix > 255 || suffix > 255)
        throw std::runtime_error("invalid affix index");
      item.prefix = std::uint8_t(prefix);
      item.suffix = std::uint8_t(suffix);
      tables.name_key_format = unhex(a[6]);
      tables.name_space = unhex(a[7]);
      tables.of_key = unhex(a[8]);
      tables.prefixes.resize(1);
      tables.prefixes[0].key = unhex(a[9]);
      tables.suffixes.resize(1);
      tables.suffixes[0].key = unhex(a[10]);
      for (const auto &entry : split(a[11], ',')) {
        auto pair = split(entry, '=');
        if (pair.size() != 2)
          throw std::runtime_error("invalid lookup pair");
        localized.emplace(unicode(pair[0]), unicode(pair[1]));
      }
      result = hextext(
          build_name(tables, item, integer(a[1]) != 0, lookup, integer(a[0])));
    } else
      throw std::runtime_error("unknown names operation");
  } catch (const NameTypeError &) {
    result = "!TypeError";
  } catch (const NameError &) {
    result = "!ValueError";
  } catch (const MissingKey &) {
    result = "!KeyError";
  } catch (const std::out_of_range &) {
    result = "!IndexError";
  }
  std::string state;
  for (const auto &key : seen) {
    if (!state.empty())
      state += ',';
    state += hextext(key);
  }
  return {result, state.empty() ? "-" : state};
}
void require(bool test, const char *message) {
  if (!test)
    throw std::runtime_error(message);
}
void anchors() {
  require(strip_grammar_suffix(U"blade_FS", 1) ==
              std::make_pair(NameText(U"blade"), 2),
          "grammar anchor");
  require(strip_grammar_suffix(U"x_q", 3) == std::make_pair(NameText{}, 0),
          "unknown tag anchor");
  require(formatted_name_text(NameText(127, U'\U0001f600')).size() == 127,
          "codepoint length anchor");
  bool rejected = false;
  try {
    formatted_name_text(NameText(128, U'\U0001f600'));
  } catch (const NameError &) {
    rejected = true;
  }
  require(rejected, "formatter limit anchor");
  require(latin1_name_text(std::string("\xff\x80", 2)) ==
              NameText({char32_t(255), char32_t(128)}),
          "Latin1 anchor");
}
} // namespace
int main(int argc, char **argv) {
  if (argc != 2 && argc != 3) {
    std::cerr << "usage: item_names_tests VECTORS.tsv [TRACE.tsv]\n";
    return 2;
  }
  std::ifstream input(argv[1], std::ios::binary);
  if (!input) {
    std::cerr << "cannot open vectors\n";
    return 2;
  }
  std::ofstream trace;
  if (argc == 3) {
    trace.open(argv[2], std::ios::binary);
    if (!trace) {
      std::cerr << "cannot open trace\n";
      return 2;
    }
  }
  std::size_t line_no = 0, total = 0;
  std::map<std::string, std::size_t> counts;
  std::uint64_t digest = UINT64_C(14695981039346656037);
  try {
    anchors();
    std::string line;
    bool schema = false;
    while (std::getline(input, line)) {
      ++line_no;
      if (!line.empty() && line.back() == '\r')
        line.pop_back();
      if (line == "# item_names_vectors_v1")
        schema = true;
      if (line.empty() || line[0] == '#')
        continue;
      auto fields = split(line, '\t');
      require(fields.size() == 5 && fields[0] == "names", "invalid names row");
      auto actual = execute(fields[1], split(fields[2], ';'));
      if (actual.first != fields[3] || actual.second != fields[4])
        throw std::runtime_error("mismatch " + fields[1] + " expected " +
                                 fields[3] + " / " + fields[4] + " got " +
                                 actual.first + " / " + actual.second);
      const auto record = fields[0] + "\t" + fields[1] + "\t" + fields[2] +
                          "\t" + actual.first + "\t" + actual.second + "\n";
      for (unsigned char c : record) {
        digest ^= c;
        digest *= UINT64_C(1099511628211);
      }
      if (trace.is_open())
        trace << record;
      ++counts[fields[1]];
      ++total;
    }
    require(schema && input.eof(), "missing schema or read failure");
    require(counts == std::map<std::string, std::size_t>{{"strip", 252},
                                                         {"formatted", 38},
                                                         {"affix", 1287},
                                                         {"build", 2881}},
            "incomplete names operation coverage");
    if (trace.is_open()) {
      trace.flush();
      require(bool(trace), "trace write failed");
    }
    std::cout << total
              << " exact name traces and 5 anchors passed; fnv1a64=" << std::hex
              << digest << '\n';
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "line " << line_no << ": " << error.what() << '\n';
    return 1;
  }
}
