#include "champions/strings.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace champions::simulation::rta::strings {
namespace {
std::int32_t sx8(std::uint8_t b) { return static_cast<std::int8_t>(b); }
std::int32_t s32(std::uint32_t x) { return static_cast<std::int32_t>(x); }
const Fpu kFpu(Fpu::Model::Ps2);

std::string lower(std::string s) {
  for (auto &c : s)
    if (c >= 'A' && c <= 'Z')
      c = static_cast<char>(c - 'A' + 'a');
  return s;
}
} // namespace

// ---------------------------------------------------------------------------
// 1. The text table
// ---------------------------------------------------------------------------
std::uint32_t text_hash(const std::string &key) {
  // 0x173a74: the bytes are read with lb, so they are signed.
  std::uint32_t h = 0;
  for (const char c : key) {
    h += static_cast<std::uint32_t>(sx8(static_cast<std::uint8_t>(c)));
    h += h << 10;
    h ^= static_cast<std::uint32_t>(s32(h) >> 6);
  }
  h += h << 3;
  h ^= static_cast<std::uint32_t>(s32(h) >> 11);
  return h & 0x1fffu;
}

std::string strip_quotes(const std::string &key) {
  std::string out;
  for (const char c : key)
    if (c != '"')
      out.push_back(c);
  return out;
}

Units visible(const Units &units) {
  Units out;
  for (const auto u : units) {
    if (u == 0)
      break;
    out.push_back(u);
  }
  return out;
}

Units nf_text(const std::string &request) {
  const auto key = strip_quotes(request).substr(0, 16);
  Units out{0x4e, 0x46, 0x3a};
  for (const char c : key)
    out.push_back(static_cast<std::uint16_t>(sx8(static_cast<std::uint8_t>(c))));
  return out;
}

std::vector<UniEntry> uni_entries(const std::uint8_t *data, std::size_t size, std::size_t start) {
  std::vector<UniEntry> out;
  std::size_t pos = start;
  while (pos < size && data[pos] != 0) {
    const auto *end = static_cast<const std::uint8_t *>(std::memchr(data + pos, 0, size - pos));
    if (!end)
      throw Refused("uni stream ends inside a key");
    UniEntry entry;
    entry.key_offset = pos;
    entry.key.assign(reinterpret_cast<const char *>(data + pos), static_cast<std::size_t>(end - (data + pos)));
    entry.text_offset = (pos + entry.key.size() + 2) & ~static_cast<std::size_t>(1);
    std::size_t q = entry.text_offset;
    for (;;) {
      if (q + 2 > size)
        throw Refused("uni stream ends inside a text");
      const auto unit = static_cast<std::uint16_t>(data[q] | data[q + 1] << 8);
      if (unit == 0)
        break;
      entry.units.push_back(unit);
      q += 2;
    }
    entry.next = q + 2;
    pos = entry.next;
    out.push_back(std::move(entry));
  }
  return out;
}

bool is_uni_name(const std::string &entry_name) {
  const auto name = lower(entry_name);
  return name.size() > 4 && name.compare(name.size() - 4, 4, ".uni") == 0;
}

TextTable::TextTable() = default;

std::vector<std::pair<std::uint32_t, std::uint32_t>>
TextTable::register_table(const std::vector<std::uint8_t> &data, std::uint32_t base) {
  const auto entries = uni_entries(data.data(), data.size());
  for (const auto &[other, bytes] : blobs_)
    if (base < other + bytes.size() && other < base + data.size())
      throw Refused("text blob overlaps a registered blob");
  std::vector<std::pair<std::uint32_t, std::uint32_t>> placed;
  blobs_[base] = data;
  for (const auto &entry : entries) {
    const auto addr = static_cast<std::uint32_t>(base + entry.key_offset);
    auto slot = text_hash(entry.key);
    std::uint32_t tries = 0;
    while (slots_[slot] != 0) {
      slot = (slot + 1) & 0x1fffu;
      if (++tries >= TEXT_SLOTS)
        throw Refused("table full: text_registerTable would probe forever");
    }
    slots_[slot] = addr;
    keys_[addr] = entry.key;
    texts_[addr] = {static_cast<std::uint32_t>(base + entry.text_offset), entry.units};
    placed.emplace_back(addr, slot);
  }
  return placed;
}

void TextTable::unregister_table(const std::vector<std::uint8_t> &data, std::uint32_t base) {
  for (const auto &entry : uni_entries(data.data(), data.size())) {
    const auto addr = static_cast<std::uint32_t>(base + entry.key_offset);
    auto slot = text_hash(entry.key);
    std::uint32_t tries = 0;
    while (slots_[slot] != addr) {
      slot = (slot + 1) & 0x1fffu;
      if (++tries >= TEXT_SLOTS)
        throw Refused("key not in the table: text_unregisterTable would probe forever");
    }
    slots_[slot] = 0;
  }
}

std::pair<std::string, std::uint32_t> TextTable::probe(const std::string &request) const {
  const auto key = strip_quotes(request);
  if (key.size() + 1 > LOOKUP_KEY_BYTES)
    throw Refused("key overruns the 0x200-byte stack copy");
  auto slot = text_hash(key);
  probes_ = 0;
  for (;;) {
    ++probes_;
    const auto addr = slots_[slot];
    if (addr == 0)
      return {key, 0};
    const auto found = keys_.find(addr);
    if (found != keys_.end() && found->second == key)
      return {key, addr};
    slot = (slot + 1) & 0x1fffu;
    if (probes_ > TEXT_SLOTS)
      throw Refused("full table without the key: the probe never ends");
  }
}

std::uint32_t TextTable::find(const std::string &request) const {
  const auto [key, addr] = probe(request);
  return addr == 0 ? 0 : texts_.at(addr).first;
}

Lookup TextTable::lookup(const std::string &request, const std::vector<std::uint8_t> &stale) {
  const auto [key, addr] = probe(request);
  if (addr != 0) {
    const auto &text = texts_.at(addr);
    return {text.first, text.second, true};
  }
  nf_index_ = (nf_index_ + 1) & 0xf;
  std::array<std::uint16_t, NF_UNITS> entry{};
  entry[0] = 0x4e;
  entry[1] = 0x46;
  entry[2] = 0x3a;
  // 16 bytes of the stack copy: the key, its 0, then whatever the stack held.
  std::vector<std::uint8_t> copy(key.begin(), key.end());
  copy.push_back(0);
  copy.insert(copy.end(), stale.begin(), stale.end());
  copy.resize(16, 0);
  for (std::size_t i = 0; i < 16; ++i)
    entry[3 + i] = static_cast<std::uint16_t>(sx8(copy[i]));
  nf_ring_[static_cast<std::size_t>(nf_index_)] = entry;
  const auto address = G_NF_RING + static_cast<std::uint32_t>(nf_index_) * 40u;
  return {address, visible(Units(entry.begin(), entry.end())), false};
}

Units TextTable::peek(const std::string &request) const {
  const auto [key, addr] = probe(request);
  if (addr != 0)
    return texts_.at(addr).second;
  return nf_text(request);
}

bool TextTable::holds(std::uint32_t address) const {
  if (address >= G_NF_RING && address < G_NF_RING + NF_ENTRIES * 40)
    return true;
  auto it = blobs_.upper_bound(address);
  if (it == blobs_.begin())
    return false;
  --it;
  return address + 2 <= it->first + it->second.size();
}

std::uint16_t TextTable::unit_at(std::uint32_t address) const {
  auto it = blobs_.upper_bound(address);
  if (it != blobs_.begin()) {
    --it;
    if (address >= it->first && address + 2 <= it->first + it->second.size()) {
      const auto off = address - it->first;
      return static_cast<std::uint16_t>(it->second[off] | it->second[off + 1] << 8);
    }
  }
  if (address >= G_NF_RING && address < G_NF_RING + NF_ENTRIES * 40) {
    const auto off = (address - G_NF_RING) / 2;
    return nf_ring_[off / NF_UNITS][off % NF_UNITS];
  }
  throw Refused("read outside the modelled text memory");
}

Units TextTable::text_at(std::uint32_t address, std::size_t limit) const {
  Units out;
  for (std::size_t k = 0; k < limit; ++k) {
    const auto unit = unit_at(address + static_cast<std::uint32_t>(2 * k));
    if (unit == 0)
      return out;
    out.push_back(unit);
  }
  throw Refused("text without a terminator");
}

std::uint32_t TextTable::next_base() const {
  if (blobs_.empty())
    return 0x10000000u;
  const auto &last = *blobs_.rbegin();
  return static_cast<std::uint32_t>((last.first + last.second.size() + 0x10) & ~std::size_t(0xf));
}

// ---------------------------------------------------------------------------
// 2. Wide strings and the formatter
// ---------------------------------------------------------------------------
Units wstr_copy(const Units &src) {
  auto out = visible(src);
  out.push_back(0);
  return out;
}
std::size_t wstr_len(const Units &src) { return visible(src).size(); }
Units wstr_from_ascii(const std::vector<std::uint8_t> &src) {
  Units out;
  for (const auto b : src) {
    if (b == 0)
      break;
    out.push_back(b);
  }
  out.push_back(0);
  return out;
}
std::vector<std::uint8_t> wstr_to_ascii(const Units &units) {
  std::vector<std::uint8_t> out;
  for (const auto u : visible(units))
    out.push_back(static_cast<std::uint8_t>(u & 0xffu));
  out.push_back(0);
  return out;
}
std::int32_t cvt_w_s(std::uint32_t raw) { return Fpu::to_int(raw); }
std::uint32_t cvt_s_w(std::int32_t value) { return kFpu.from_int(static_cast<std::uint32_t>(value)); }

double widen(std::uint32_t raw) {
  const double sign = (raw & 0x80000000u) ? -1.0 : 1.0;
  const int exp = static_cast<int>((raw >> 23) & 0xffu);
  if (exp == 0)
    return sign * 0.0;
  const double mantissa = 1.0 + static_cast<double>(raw & 0x7fffffu) / 8388608.0;
  return sign * std::ldexp(mantissa, exp - 127);
}

std::string float_text(std::uint32_t raw) {
  // newlib prints the exact binary value rounded to two places.
  char buffer[512];
  const int n = std::snprintf(buffer, sizeof buffer, "%.2f", widen(raw));
  if (n < 0 || static_cast<std::size_t>(n) >= sizeof buffer)
    throw Refused("float text does not fit");
  return std::string(buffer, static_cast<std::size_t>(n));
}

bool reads_past_terminator(const UnitReader &fmt) {
  std::size_t i = 0;
  for (std::size_t guard = 0; guard < 0x20000; ++guard) {
    const auto c = fmt(i);
    if (c == 0)
      return true;
    if (c == 0x25) {
      if (fmt(i + 1) == 0)
        return true;
      i += 2;
      continue;
    }
    ++i;
    if (fmt(i) == 0)
      return false;
  }
  throw Refused("runaway format");
}

Formatted wstr_format(const UnitReader &fmt, const std::array<std::uint32_t, 8> &args,
                      const CStr &cstr, const WStr &wstr) {
  // docs/strings.md 2.3: the loop tests for the end only after a copied unit,
  // so these formats copy their terminator and read on. Refused here.
  if (reads_past_terminator(fmt))
    throw Refused("format reads past its terminator");
  Formatted out;
  auto arg = [&]() {
    if (out.used >= 8)
      throw Refused("a ninth conversion reads the formatter scratch buffer");
    return args[static_cast<std::size_t>(out.used++)];
  };
  auto bytes = [&](const std::string &text) {
    for (const char b : text)
      out.written.push_back(static_cast<std::uint16_t>(sx8(static_cast<std::uint8_t>(b))));
  };
  std::size_t i = 0;
  for (;;) {
    const auto c = fmt(i);
    if (c == 0x25) {
      const auto conv = fmt(i + 1);
      if (conv == 0x25) {
        out.written.push_back(0x25);
      } else if (conv == 0x64) { // d
        bytes(std::to_string(cvt_w_s(arg())));
      } else if (conv == 0x63) { // c
        out.written.push_back(static_cast<std::uint16_t>(cvt_w_s(arg()) & 0xffff));
      } else if (conv == 0x73) { // s: an 8-bit string, each byte zero-extended
        const auto ptr = static_cast<std::uint32_t>(cvt_w_s(arg()));
        if (!cstr)
          throw Refused("%s pointer has no string");
        for (const auto b : cstr(ptr)) {
          if (b == 0)
            break;
          out.written.push_back(b);
        }
      } else if (conv == 0x75) { // u: a 16-bit string
        const auto ptr = static_cast<std::uint32_t>(cvt_w_s(arg()));
        if (!wstr)
          throw Refused("%u pointer has no string");
        for (const auto u : visible(wstr(ptr)))
          out.written.push_back(u);
      } else if (conv == 0x66) { // f
        bytes(float_text(arg()));
      }
      i += 2;
      continue;
    }
    out.written.push_back(c);
    ++i;
    if (fmt(i) == 0) {
      out.written.push_back(0);
      return out;
    }
    if (out.written.size() > 0x10000)
      throw Refused("runaway copy");
  }
}

Formatted wstr_format(const Units &fmt, const std::array<std::uint32_t, 8> &args,
                      const CStr &cstr, const WStr &wstr) {
  return wstr_format(
      [&fmt](std::size_t k) -> std::uint16_t {
        if (k >= fmt.size())
          throw Refused("the formatter read past the supplied memory");
        return fmt[k];
      },
      args, cstr, wstr);
}

Formatted wstr_format_ascii(const std::vector<std::uint8_t> &fmt8,
                            const std::array<std::uint32_t, 8> &args, const CStr &cstr,
                            const WStr &wstr) {
  const auto widened = wstr_from_ascii(fmt8);
  if (widened.size() * 2 > 0x200)
    throw Refused("format overruns the 0x200-byte widening buffer");
  return wstr_format(widened, args, cstr, wstr);
}

Formatted format_one_int(const UnitReader &fmt, std::uint32_t value) {
  std::array<std::uint32_t, 8> args{};
  args[0] = cvt_s_w(static_cast<std::int32_t>(value));
  const CStr no_c = [](std::uint32_t) -> std::vector<std::uint8_t> {
    throw Refused("a %s in a help text would dereference a float argument");
  };
  const WStr no_w = [](std::uint32_t) -> Units {
    throw Refused("a %u in a help text would dereference a float argument");
  };
  return wstr_format(fmt, args, no_c, no_w);
}

// ---------------------------------------------------------------------------
// 3. HUD state
// ---------------------------------------------------------------------------
std::string Event::text() const {
  std::string out = name;
  for (const auto &a : args)
    out += "," + a;
  return out;
}

bool Hud::message_stores(std::int32_t slot, std::uint32_t force) const {
  const auto f = sx8(static_cast<std::uint8_t>(force & 0xffu));
  if (slot != -1 && !inputs.local(slot))
    return false;
  return !(f == 0 && inputs.local_menu);
}

bool Hud::show_message(const Units &text, std::uint32_t frames, std::int32_t slot,
                       std::uint32_t force) {
  if (!message_stores(slot, force))
    return false;
  const auto copy = visible(text);
  if (copy.size() > MESSAGE_LIMIT)
    throw Refused("message text over 99 units would run into g_msgIdGame");
  message = copy;
  msg_frames = static_cast<std::uint16_t>(frames & 0xffffu);
  msg_force = static_cast<std::uint8_t>(force & 0xffu);
  return true;
}

bool Hud::show_help(std::int32_t slot, std::uint32_t title, std::uint32_t body) {
  if (slot != -1 && !inputs.local(slot))
    return false;
  bool opened = !inputs.network;
  if (inputs.network) {
    for (const int alive : inputs.alive) {
      if (inputs.local(alive)) {
        events.push_back({"hudmsg_sendMenuState", {std::to_string(alive & 0xff), "1", "0"}});
        opened = true;
        break;
      }
    }
  }
  if (!opened)
    return false;
  events.push_back({"res_find", {HELP_CHIME_LUMP, HELP_CHIME_ENTRY}});
  events.push_back({"sound_playOnObject", {"chime", "0", "1.0"}});
  events.push_back({"sound_setPlaysWhileFrozen", {}});
  events.push_back({"ui_startPulse", {"0"}});
  help_open = true;
  help_title = title;
  help_body = body;
  help_close_delay = HELP_CLOSE_DELAY_OPEN;
  return true;
}

std::optional<bool> Hud::widget_gate() const {
  if (inputs.suppressed || inputs.local_menu)
    return false;
  if (!inputs.camera_mode)
    return std::nullopt;
  return !(*inputs.camera_mode != 1 || inputs.loadscreen || inputs.menu_open);
}

bool Hud::set_bust(const std::string &texture, const Units &text, std::uint32_t value,
                   std::uint32_t maximum) {
  const auto gate = widget_gate();
  if (!gate)
    throw Refused("no local camera: the gate reads +0x34 of a null pointer");
  if (!*gate)
    return false;
  const auto copy = visible(text);
  if (copy.size() > BUST_TEXT_LIMIT)
    throw Refused("bust text over 31 units would run into the compass label");
  if (texture.size() > BUST_TEXTURE_LIMIT)
    throw Refused("bust texture name over 39 bytes");
  bust_shown = SHOWN_TICKS;
  bust_texture = texture;
  bust_text = copy;
  bust_value = value;
  bust_max = maximum;
  return true;
}

bool Hud::set_compass(const std::array<std::uint32_t, 3> &position, const Units &text) {
  const auto gate = widget_gate();
  if (!gate)
    throw Refused("no local camera: the gate reads +0x34 of a null pointer");
  if (!*gate)
    return false;
  const auto copy = visible(text);
  if (copy.size() > COMPASS_LABEL_LIMIT)
    throw Refused("compass label over 31 units would run into the bust value");
  compass_shown = SHOWN_TICKS;
  compass_label = copy;
  compass_target = position;
  return true;
}

void Hud::tick_message() {
  if (inputs.suppressed || inputs.warmup)
    return;
  if (static_cast<std::int16_t>(msg_frames) <= 0)
    return;
  msg_frames = static_cast<std::uint16_t>(msg_frames - 1);
  if (inputs.num_players == 4 && !inputs.network)
    latch = 1;
}

void Hud::tick_widgets() {
  if (bust_shown)
    bust_shown = static_cast<std::uint8_t>(bust_shown - 1);
  if (compass_shown)
    compass_shown = static_cast<std::uint8_t>(compass_shown - 1);
}

HelpFrame Hud::help_task(bool confirm) {
  // docs/strings.md 3.3 (Observed, 0x1dcc78-0x1dcff0; not executed).
  HelpFrame frame;
  if (help_close_delay > 0)
    help_close_delay -= 1;
  if (help_open && !inputs.network) {
    paused = true;
    help_pause_byte = true;
  }
  if (help_open && help_close_delay == 0 && confirm) {
    help_open = false;
    help_close_delay = HELP_CLOSE_DELAY_CLOSE;
    menu_input_lockout = HELP_CLOSE_DELAY_CLOSE;
    frame.closed = true;
    frame.run_post_action = !inputs.network;
    help_pause_byte = false;
    if (!inputs.local_menu)
      paused = false;
    for (const int alive : inputs.alive)
      if (inputs.local(alive))
        events.push_back({"hudmsg_sendMenuState", {std::to_string(alive & 0xff), "0", "0"}});
  }
  frame.paused = paused;
  return frame;
}

Units Hud::text_at(std::uint32_t address) const {
  if (address == HELP_F_TITLE)
    return help_f_title;
  if (address == HELP_F_BODY)
    return help_f_body;
  throw Refused("help buffer address inside a buffer");
}

// ---------------------------------------------------------------------------
// The five handlers
// ---------------------------------------------------------------------------
Commands::Commands(TextTable &t, Hud &h) : table(t), hud(h) {}

Units Commands::resolve(std::uint32_t address) const {
  if (address == 0)
    return {};
  if (hud.holds(address))
    return hud.text_at(address);
  return table.text_at(address);
}

namespace {
UnitReader reader_of(const Units &units) {
  return [units](std::size_t k) -> std::uint16_t { return k < units.size() ? units[k] : 0; };
}
} // namespace

std::optional<std::string> Commands::check_set_info(const std::string &key) const {
  if (!hud.message_stores(-1, 0))
    return std::nullopt;
  if (table.peek(key).size() > MESSAGE_LIMIT)
    return std::string("setInfo text over 99 units");
  return std::nullopt;
}

std::optional<std::string> Commands::check_help_f(const std::string &key1, std::uint32_t v1,
                                                  const std::string &key2,
                                                  std::uint32_t v2) const {
  const std::pair<const std::string *, std::uint32_t> parts[2] = {{&key1, v1}, {&key2, v2}};
  std::size_t lengths[2] = {0, 0};
  for (int i = 0; i < 2; ++i) {
    auto units = table.peek(*parts[i].first);
    units.push_back(0);
    try {
      lengths[i] = visible(format_one_int(reader_of(units), parts[i].second).written).size();
    } catch (const Refused &error) {
      return std::string("addHelpMessageF: ") + error.what();
    }
  }
  if (lengths[0] > HELP_F_TITLE_LIMIT)
    return std::string("addHelpMessageF title over 31 units");
  if (lengths[1] > HELP_F_BODY_LIMIT)
    return std::string("addHelpMessageF body over 127 units");
  return std::nullopt;
}

std::optional<std::string> Commands::check_bust(const std::string &texture,
                                                const std::string &key) const {
  const auto gate = hud.widget_gate();
  if (!gate)
    return std::string("showBust without a local camera");
  if (!*gate)
    return std::nullopt;
  if (table.peek(key).size() > BUST_TEXT_LIMIT)
    return std::string("showBust text over 31 units");
  if (texture.size() > BUST_TEXTURE_LIMIT)
    return std::string("showBust texture name over 39 bytes");
  return std::nullopt;
}

std::optional<std::string> Commands::check_compass(const std::string &name,
                                                   const std::string &key) const {
  if (!find_object || !find_object(name))
    return std::string("showCompass of a missing object reads through a null pointer");
  const auto gate = hud.widget_gate();
  if (!gate)
    return std::string("showCompass without a local camera");
  if (!*gate)
    return std::nullopt;
  if (table.peek(key).size() > COMPASS_LABEL_LIMIT)
    return std::string("showCompass label over 31 units");
  return std::nullopt;
}

void Commands::set_info(const std::string &key) {
  const auto found = table.lookup(key);
  hud.show_message(found.units, MESSAGE_FRAMES, -1, 0);
}

void Commands::add_help_message(const std::string &title, const std::string &body) {
  const auto t = table.lookup(title);
  const auto b = table.lookup(body);
  hud.show_help(-1, t.address, b.address);
}

void Commands::add_help_message_f(const std::string &key1, std::uint32_t v1,
                                  const std::string &key2, std::uint32_t v2) {
  if (const auto refused = check_help_f(key1, v1, key2, v2))
    throw Refused(*refused);
  const auto a1 = table.lookup(key1);
  const auto out1 = format_one_int([this, &a1](std::size_t k) { return table.unit_at(a1.address + static_cast<std::uint32_t>(2 * k)); }, v1);
  const auto a2 = table.lookup(key2);
  const auto out2 = format_one_int([this, &a2](std::size_t k) { return table.unit_at(a2.address + static_cast<std::uint32_t>(2 * k)); }, v2);
  hud.help_f_title = visible(out1.written);
  hud.help_f_body = visible(out2.written);
  hud.show_help(-1, HELP_F_TITLE, HELP_F_BODY);
}

void Commands::show_bust(const std::string &texture, const std::string &key, std::uint32_t value,
                         std::uint32_t maximum) {
  if (const auto refused = check_bust(texture, key))
    throw Refused(*refused);
  const auto found = table.lookup(key);
  hud.set_bust(texture, found.units, value, maximum);
}

void Commands::show_compass(const std::string &name, const std::string &key) {
  if (const auto refused = check_compass(name, key))
    throw Refused(*refused);
  const auto position = find_object(name);
  const auto found = table.lookup(key);
  hud.set_compass(*position, found.units);
}

// ---------------------------------------------------------------------------
// 4. sound_playOnObject
// ---------------------------------------------------------------------------
std::array<std::int8_t, 256> disc_group_limits() {
  std::array<std::int8_t, 256> out{};
  for (int g = 0; g <= 98; ++g)
    out[static_cast<std::size_t>(g)] = 2;
  out[14] = 9;
  out[50] = 8;
  out[51] = 8;
  return out;
}

SoundSeqs::SoundSeqs(HandlePool *p) : pool(p) {}

Played SoundSeqs::play_on_object(std::uint32_t seq, const SoundTarget *target,
                                 std::uint32_t volume) {
  Played played;
  if (seq == 0)
    return played;
  const auto group_of = [this](std::uint32_t s) -> std::uint8_t {
    const auto found = groups.find(s);
    return found == groups.end() ? 0 : found->second;
  };
  const auto group = group_of(seq);
  int chosen = -1, count = 0;
  for (std::size_t i = 0; i < SEQ_SLOTS; ++i) {
    const auto &slot = slots[i];
    if (slot.tracks == 0) {
      chosen = static_cast<int>(i);
      continue;
    }
    if (group_of(slot.seq) != group)
      continue;
    ++count;
    if (group == 0)
      continue;
    if (!(count < limits[group]))
      return played;
  }
  if (chosen < 0)
    return played;
  auto &slot = slots[static_cast<std::size_t>(chosen)];
  slot.flags = 0;
  // anim_push(slot, seq, 4): one track when the slot has room.
  if (static_cast<std::int8_t>(slot.tracks) < static_cast<std::int8_t>(slot.capacity)) {
    slot.tracks = static_cast<std::uint8_t>(slot.tracks + 1);
    slot.flags = static_cast<std::uint16_t>(slot.flags | 4u);
    slot.seq = seq;
    slot.track_time = 0;
    played.cut_link = slot.track_link;
    slot.track_link = 0;
    slot.track_flags = 4;
    slot.track_words = {0x3f800000u, 0, 0x3f800000u, 0};
  }
  if (target) {
    if (!pool || !target->back)
      throw Refused("sound_playOnObject on an object without a handle pool");
    pool->clear(slot.handle_head);
    pool->link(slot.handle_head, *target->back, target->object);
    slot.pos = target->position;
    slot.no_pos = 0;
  } else {
    slot.no_pos = 1;
  }
  const auto before = serial;
  slot.serial = before;
  serial = static_cast<std::uint16_t>((before + 1) & 0x7f);
  slot.frozen = frozen;
  slot.released = 0;
  slot.volume = volume;
  slot.near = SOUND_NEAR;
  slot.far = SOUND_FAR;
  played.slot = static_cast<std::size_t>(chosen);
  played.handle = static_cast<std::uint32_t>(chosen) | static_cast<std::uint32_t>(before) << 8 | 0x8000u;
  return played;
}

// ---------------------------------------------------------------------------
// 5. Creature_setCharm with a live target
// ---------------------------------------------------------------------------
bool set_charm(HandlePool &pool, Creature &creature, std::uint32_t frames, std::uint32_t team) {
  if (static_cast<std::int16_t>(creature.charm) == -1)
    return false;
  if (!(creature.flags & 0x20u))
    return false;
  if (creature.flags & 0x40u)
    return false;
  creature.team = static_cast<std::uint8_t>(team & 0xffu);
  creature.charm = static_cast<std::uint16_t>(frames & 0xffffu);
  pool.clear(creature.target);
  creature.target.next = nullptr;
  return true;
}
} // namespace champions::simulation::rta::strings
