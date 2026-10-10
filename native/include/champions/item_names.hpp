#pragma once
#include "champions/items.hpp"
#include <functional>
#include <string>
#include <utility>

namespace champions::simulation::rta::items {
// Python-reference text is a sequence of Unicode codepoints, not UTF-8 bytes or
// platform-dependent wchar_t. Tables hold Latin-1 resource-key bytes
// separately.
using NameText = std::u32string;
using TextLookup = std::function<NameText(const NameText &)>;
struct NameError : std::invalid_argument {
  using std::invalid_argument::invalid_argument;
};
struct NameTypeError : std::invalid_argument {
  using std::invalid_argument::invalid_argument;
};
NameText latin1_name_text(const std::string &bytes);
std::pair<NameText, int> strip_grammar_suffix(const NameText &text,
                                              int language);
// Only the reference's copy subset: '%' and 128+ codepoints are unsupported.
NameText formatted_name_text(const NameText &text);
NameText affix_form(const TextLookup &lookup, int language, int form,
                    const NameText &key);
// Noun-key formatting covers bytes %s/%b/%r/%a/%c, width/precision, flags,
// optional h/l/L and literal %%. Tuple/mapping arguments are not supplied.
NameText build_name(const Tables &tables, const Item &item, bool decorated,
                    const TextLookup &lookup, int language = 0);
} // namespace champions::simulation::rta::items
