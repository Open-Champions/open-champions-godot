#include "champions/item_names.hpp"
#include <vector>

namespace champions::simulation::rta::items {
namespace {
char32_t lower_tag(char32_t c) { return c >= U'A' && c <= U'Z' ? c + 32 : c; }
std::string bytes_repr(const std::string &name) {
  const char quote = name.find('\'') != std::string::npos &&
                             name.find('"') == std::string::npos
                         ? '"'
                         : '\'';
  const char hex[] = "0123456789abcdef";
  std::string result = "b";
  result += quote;
  for (unsigned char c : name) {
    if (c == static_cast<unsigned char>(quote) || c == '\\') {
      result += '\\';
      result += char(c);
    } else if (c == '\n')
      result += "\\n";
    else if (c == '\r')
      result += "\\r";
    else if (c == '\t')
      result += "\\t";
    else if (c >= 32 && c < 127)
      result += char(c);
    else {
      result += "\\x";
      result += hex[c >> 4];
      result += hex[c & 15];
    }
  }
  result += quote;
  return result;
}
std::string noun_key(const std::string &format, const std::string &name) {
  std::string out;
  bool substituted = false;
  for (std::size_t i = 0; i < format.size(); ++i) {
    if (format[i] != '%') {
      out += format[i];
      continue;
    }
    if (++i == format.size())
      throw NameError("incomplete noun key format");
    if (format[i] == '%') {
      out += '%';
      continue;
    }
    if (substituted)
      throw NameTypeError("not enough arguments for noun key format");
    if (format[i] == '(')
      throw NameTypeError("format requires a mapping");
    bool left = false;
    while (i < format.size() &&
           (format[i] == '#' || format[i] == '0' || format[i] == '-' ||
            format[i] == ' ' || format[i] == '+')) {
      left |= format[i] == '-';
      ++i;
    }
    auto decimal = [&]() {
      std::size_t value = 0;
      while (i < format.size() && format[i] >= '0' && format[i] <= '9') {
        const auto digit = std::size_t(format[i] - '0');
        if (value > (std::size_t(INT64_MAX) - digit) / 10)
          throw NameError("format width too large");
        value = value * 10 + digit;
        ++i;
      }
      return value;
    };
    if (i < format.size() && format[i] == '*')
      throw NameTypeError("format width requires a tuple");
    const auto width = decimal();
    bool precision_set = false;
    std::size_t precision = 0;
    if (i < format.size() && format[i] == '.') {
      ++i;
      precision_set = true;
      if (i < format.size() && format[i] == '*')
        throw NameTypeError("format precision requires a tuple");
      precision = decimal();
      if (precision > INT32_MAX)
        throw NameError("format precision too large");
    }
    if (i < format.size() &&
        (format[i] == 'h' || format[i] == 'l' || format[i] == 'L'))
      ++i;
    if (i == format.size())
      throw NameError("incomplete noun key format");
    const auto code = format[i];
    std::string value;
    if (code == 's' || code == 'b')
      value = name;
    else if (code == 'r' || code == 'a')
      value = bytes_repr(name);
    else if (code == 'c') {
      if (name.size() != 1)
        throw NameTypeError("character format needs one byte");
      value = name;
    } else if (code == 'd' || code == 'i' || code == 'u' || code == 'o' ||
               code == 'x' || code == 'X' || code == 'e' || code == 'E' ||
               code == 'f' || code == 'F' || code == 'g' || code == 'G')
      throw NameTypeError("format conversion does not accept bytes");
    else
      throw NameError("unsupported noun key conversion");
    if (precision_set && code != 'c' && value.size() > precision)
      value.resize(precision);
    const auto padding = width > value.size() ? width - value.size() : 0;
    if (!left)
      out.append(padding, ' ');
    out += value;
    if (left)
      out.append(padding, ' ');
    substituted = true;
  }
  if (!substituted)
    throw NameTypeError("unused noun key argument");
  return out;
}
} // namespace
NameText latin1_name_text(const std::string &bytes) {
  NameText out;
  out.reserve(bytes.size());
  for (unsigned char c : bytes)
    out += char32_t(c);
  return out;
}
std::pair<NameText, int> strip_grammar_suffix(const NameText &text,
                                              int language) {
  const auto n = text.size();
  if (language == 4) {
    if (n < 2)
      throw NameError("text shorter than a tag");
    if (text[n - 2] != U'_')
      return {text, 0};
    const auto c = lower_tag(text[n - 1]);
    const int form = c == U'm'   ? 1
                     : c == U'f' ? 2
                     : c == U'n' ? 3
                     : c == U'p' ? 4
                                 : 0;
    return {text.substr(0, n - 2), form};
  }
  if (language != 1 && language != 3)
    return {text, 0};
  if (n < 3)
    throw NameError("text shorter than a tag");
  if (text[n - 3] != U'_' && text[n - 2] != U'_')
    return {text, 1};
  const auto a = lower_tag(text[n - 2]), b = lower_tag(text[n - 1]);
  int form = 0;
  if (a == U'm' && b == U's')
    form = 1;
  if (a == U'f' && b == U's')
    form = 2;
  if (a == U'm' && b == U'p')
    form = 3;
  if (a == U'f' && b == U'p')
    form = 4;
  if (form)
    return {text.substr(0, n - 3), form};
  if (b == U'm')
    return {text.substr(0, n - 2), 1};
  if (b == U'f')
    return {text.substr(0, n - 2), 2};
  return {text.substr(0, n - 3), 0};
}
NameText formatted_name_text(const NameText &text) {
  if (text.find(U'%') != NameText::npos || text.size() >= 128)
    throw NameError("text is outside the reference formatter subset");
  return text;
}
NameText affix_form(const TextLookup &lookup, int language, int form,
                    const NameText &key) {
  auto text = formatted_name_text(lookup(key));
  bool has_bar = false;
  if (form != -1) {
    has_bar = text.find(U'|') != NameText::npos;
    if (has_bar) {
      std::vector<NameText> parts;
      std::size_t start = 0;
      for (;;) {
        auto end = text.find(U'|', start);
        parts.push_back(
            text.substr(start, end == NameText::npos ? end : end - start));
        if (end == NameText::npos)
          break;
        start = end + 1;
      }
      if (form >= 0 && std::size_t(form) >= parts.size())
        throw NameError("affix form exceeds alternatives");
      const auto index =
          form < 0 ? std::int64_t(parts.size()) + form : std::int64_t(form);
      if (index < 0)
        throw std::out_of_range("negative affix form exceeds alternatives");
      text = parts.at(std::size_t(index));
    }
  }
  if ((language == 1 || language == 3) && has_bar) {
    if (text.size() < 3)
      throw NameError("alternative shorter than a tag");
    text.resize(text.size() - 3);
  }
  return text;
}
NameText build_name(const Tables &tables, const Item &item, bool decorated,
                    const TextLookup &lookup, int language) {
  if (!item.definition)
    throw NameError("item has no definition");
  const auto &name = item.definition->name;
  const auto space = latin1_name_text(tables.name_space);
  const auto noun = [&] {
    return strip_grammar_suffix(
        lookup(latin1_name_text(
            noun_key(tables.name_key_format, name.substr(0, name.find('+'))))),
        language);
  };
  if (!decorated || (item.flags & 0x200))
    return strip_grammar_suffix(
               lookup(latin1_name_text(noun_key(tables.name_key_format, name))),
               language)
        .first;
  const bool prefix = item.prefix != 0xff, suffix = item.suffix != 0xff;
  const auto prefix_key =
      prefix ? latin1_name_text(tables.prefixes.at(item.prefix).key)
             : NameText{};
  const auto suffix_key =
      suffix ? latin1_name_text(tables.suffixes.at(item.suffix).key)
             : NameText{};
  if (language == 1 || language == 3) {
    auto value = noun();
    if (prefix)
      value.first += space + affix_form(lookup, language,
                                        value.second >= 1 && value.second <= 4
                                            ? value.second - 1
                                            : -1,
                                        prefix_key);
    if (suffix)
      value.first += space + lookup(suffix_key);
    return value.first;
  }
  NameText out;
  if (language == 4) {
    if (prefix) {
      const auto form = noun().second;
      out = affix_form(lookup, language,
                       form == 1   ? 0
                       : form == 2 ? 1
                       : form == 4 ? 3
                                   : 2,
                       prefix_key) +
            space;
    }
    out += noun().first;
    if (suffix)
      out += space + lookup(suffix_key);
    return out;
  }
  if (prefix)
    out = affix_form(lookup, language, 0, prefix_key) + space;
  out += noun().first;
  if (suffix) {
    out += space + lookup(latin1_name_text(tables.of_key)) + space;
    out += affix_form(lookup, language, 0, suffix_key);
  }
  return out;
}
} // namespace champions::simulation::rta::items
