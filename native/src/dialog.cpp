#include "champions/dialog.hpp"
#include "champions/core_math.hpp"
#include <algorithm>
#include <cstring>

// Port of tools/ref_dialog.py (t3/research-dialog 334578d). Section and step
// numbers below are docs/dialog.md's.
namespace champions::simulation::rta::dialog {
namespace {
const Fpu kPs2{Fpu::Model::Ps2, false, false};

Event ev(std::string kind, std::vector<Arg> args = {}) { return Event{std::move(kind), std::move(args)}; }
Arg I(std::int64_t v) { return Arg{v}; }
Arg S(std::string v) { return Arg{std::move(v)}; }

std::int32_t le32(const std::uint8_t *p) {
  return static_cast<std::int32_t>(std::uint32_t(p[0]) | std::uint32_t(p[1]) << 8 |
                                   std::uint32_t(p[2]) << 16 | std::uint32_t(p[3]) << 24);
}
void put32(std::uint8_t *p, std::uint32_t v) {
  for (int i = 0; i < 4; ++i)
    p[i] = static_cast<std::uint8_t>(v >> (8 * i));
}
std::string cstr(const std::uint8_t *p, std::size_t size) {
  std::size_t n = 0;
  while (n < size && p[n])
    ++n;
  return std::string(reinterpret_cast<const char *>(p), n);
}
void put_text(Bytes &out, std::size_t off, std::size_t size, const std::string &text) {
  if (text.size() >= size)
    throw Refused("'" + text + "' does not fit " + std::to_string(size) + " bytes with its terminator");
  std::memcpy(out.data() + off, text.data(), text.size());
}
int s8(std::int64_t v) {
  const auto b = static_cast<std::uint8_t>(v & 0xFF);
  return b & 0x80 ? int(b) - 0x100 : int(b);
}
std::int64_t s32v(std::int64_t v) { return s32(static_cast<std::uint32_t>(v & 0xFFFFFFFF)); }
Raw from_i(std::int64_t v) { return from_int(static_cast<std::uint32_t>(v & 0xFFFFFFFF)); }
std::string hex(const Bytes &b) {
  static const char *digits = "0123456789abcdef";
  std::string out;
  for (auto c : b) {
    out.push_back(digits[c >> 4]);
    out.push_back(digits[c & 15]);
  }
  return out;
}
} // namespace

// ---- 1. files ---------------------------------------------------------------------
std::vector<Node> parse_dbin(const std::uint8_t *data, std::size_t size) {
  if (!data || !size || size % NODE_SIZE)
    throw Refused("size " + std::to_string(size) + " is not a multiple of 0x448");
  const int count = s8(data[0]);
  if (count < 0 || std::size_t(count) != size / NODE_SIZE)
    throw Refused("count byte " + std::to_string(count) + ", size says " + std::to_string(size / NODE_SIZE));
  std::vector<Node> nodes;
  for (int k = 0; k < count; ++k) {
    const auto *r = data + std::size_t(k) * NODE_SIZE;
    const auto n = le32(r + NODE_NCHOICES);
    if (n < 0 || n > MAX_CHOICES)
      throw Refused("node " + std::to_string(k) + " has " + std::to_string(n) + " choices");
    Node node;
    for (int i = 0; i < n; ++i) {
      const auto *c = r + NODE_CHOICES + std::size_t(i) * CHOICE_SIZE;
      const auto next = le32(c + CH_NEXT);
      if (next < -1 || next >= count)
        throw Refused("node " + std::to_string(k) + " choice " + std::to_string(i) + " jumps to " +
                      std::to_string(next));
      node.choices.push_back(Choice{cstr(c + CH_TEXT, CH_TEXT_SIZE), cstr(c + CH_COND, CH_COND_SIZE),
                                    cstr(c + CH_ACTION, CH_ACTION_SIZE), next});
    }
    node.condition = cstr(r + NODE_COND, NODE_COND_SIZE);
    node.text = cstr(r + NODE_TEXT, NODE_TEXT_SIZE);
    node.voice = cstr(r + NODE_VOICE, NODE_VOICE_SIZE);
    node.speaker = cstr(r + NODE_SPEAKER, NODE_SPEAKER_SIZE);
    node.raw_choices = n;
    nodes.push_back(std::move(node));
  }
  return nodes;
}

Bytes build_dbin(const std::vector<Node> &nodes) {
  if (nodes.empty() || nodes.size() >= 128)
    throw Refused("1..127 nodes");
  Bytes out(NODE_SIZE * nodes.size(), 0);
  for (std::size_t k = 0; k < nodes.size(); ++k) {
    const auto &node = nodes[k];
    const auto b = k * NODE_SIZE;
    if (k == 0)
      out[0] = static_cast<std::uint8_t>(nodes.size());
    put_text(out, b + NODE_COND, NODE_COND_SIZE, node.condition);
    put_text(out, b + NODE_TEXT, NODE_TEXT_SIZE, node.text);
    put_text(out, b + NODE_VOICE, NODE_VOICE_SIZE, node.voice);
    put_text(out, b + NODE_SPEAKER, NODE_SPEAKER_SIZE, node.speaker);
    if (node.choices.size() > std::size_t(MAX_CHOICES))
      throw Refused("more than 12 choices");
    put32(out.data() + b + NODE_NCHOICES, static_cast<std::uint32_t>(node.raw_choices));
    for (std::size_t i = 0; i < node.choices.size(); ++i) {
      const auto &ch = node.choices[i];
      const auto c = b + NODE_CHOICES + i * CHOICE_SIZE;
      put_text(out, c + CH_TEXT, CH_TEXT_SIZE, ch.text);
      put_text(out, c + CH_COND, CH_COND_SIZE, ch.condition);
      put_text(out, c + CH_ACTION, CH_ACTION_SIZE, ch.action);
      put32(out.data() + c + CH_NEXT, static_cast<std::uint32_t>(ch.next));
    }
  }
  return out;
}

std::vector<int> start_candidates(const std::vector<Node> &nodes) {
  std::vector<int> out{0};
  for (std::size_t k = 1; k < nodes.size(); ++k)
    if (!nodes[k].condition.empty())
      out.push_back(int(k));
  return out;
}

std::set<int> reachable_nodes(const std::vector<Node> &nodes) {
  std::set<int> seen;
  auto todo = start_candidates(nodes);
  while (!todo.empty()) {
    const int k = todo.back();
    todo.pop_back();
    if (seen.count(k) || k < 0 || std::size_t(k) >= nodes.size())
      continue;
    seen.insert(k);
    for (const auto &ch : nodes[std::size_t(k)].choices)
      if (ch.next >= 0)
        todo.push_back(ch.next);
  }
  return seen;
}

// dialog_onMsg case 0, loop at 0x22ef7c (section 2.4 step 5).
int pick_start(const std::vector<Node> &nodes, const ScriptRunner &run) {
  for (std::size_t k = 0; k < nodes.size(); ++k)
    if (!nodes[k].condition.empty() && run(nodes[k].condition) != 0)
      return int(k);
  return 0;
}

// ---- 2. message 0 -----------------------------------------------------------------
Bytes start_message(const std::string &name, int player, int a, int b, const std::array<Raw, 3> &cam,
                    int yaw, int pitch) {
  if (name.find('\0') != std::string::npos || name.size() > 127)
    throw Refused("name must be 0..127 bytes without NUL");
  Bytes out{MSG_OPEN, static_cast<std::uint8_t>(name.size())};
  out.insert(out.end(), name.begin(), name.end());
  while (out.size() & 3)
    out.push_back(0xFF);
  out.push_back(static_cast<std::uint8_t>(player));
  out.push_back(static_cast<std::uint8_t>(a));
  out.push_back(static_cast<std::uint8_t>(b));
  out.push_back(0);
  for (const auto w : cam) {
    std::uint8_t q[4];
    put32(q, w);
    out.insert(out.end(), q, q + 4);
  }
  for (const auto v : {s16(static_cast<std::uint32_t>(yaw)), s16(static_cast<std::uint32_t>(pitch))}) {
    out.push_back(static_cast<std::uint8_t>(v));
    out.push_back(static_cast<std::uint8_t>(v >> 8));
  }
  return out;
}

StartMessage parse_start_message(const Bytes &msg) {
  if (msg.size() < 2)
    throw Refused("message 0 is shorter than its header");
  StartMessage m;
  const int n = s8(msg[1]);
  const std::size_t len = std::size_t(std::max(n, 0));
  if (2 + len > msg.size())
    throw Refused("message 0 name runs past the message");
  m.name.assign(reinterpret_cast<const char *>(msg.data() + 2), len);
  std::size_t p = 2 + len;
  while (p & 3)
    ++p;
  if (p + 0x14 > msg.size())
    throw Refused("message 0 is shorter than its fields");
  m.player = s8(msg[p]);
  m.a = s8(msg[p + 1]);
  m.b = s8(msg[p + 2]);
  for (int i = 0; i < 3; ++i)
    m.cam[std::size_t(i)] = static_cast<Raw>(le32(msg.data() + p + 4 + 4 * std::size_t(i)));
  m.yaw = s16(std::uint32_t(msg[p + 0x10]) | std::uint32_t(msg[p + 0x11]) << 8);
  m.pitch = s16(std::uint32_t(msg[p + 0x12]) | std::uint32_t(msg[p + 0x13]) << 8);
  return m;
}

// ---- log text ---------------------------------------------------------------------
namespace {
std::string escape(const std::string &s) {
  std::string out;
  for (const unsigned char c : s) {
    if (c == '%' || c == ' ' || c == '\t' || c == ',' || c == '[' || c == ']' || c < 0x21 || c > 0x7e) {
      static const char *digits = "0123456789abcdef";
      out.push_back('%');
      out.push_back(digits[c >> 4]);
      out.push_back(digits[c & 15]);
    } else {
      out.push_back(char(c));
    }
  }
  return out;
}
std::string format_arg(const Arg &a) {
  if (const auto *i = std::get_if<std::int64_t>(&a))
    return std::to_string(*i);
  if (const auto *s = std::get_if<std::string>(&a))
    return "s:" + escape(*s);
  if (const auto *b = std::get_if<Bytes>(&a))
    return "b:" + hex(*b);
  const auto &v = std::get<std::vector<std::int64_t>>(a);
  std::string out = "[";
  for (std::size_t i = 0; i < v.size(); ++i)
    out += (i ? "," : "") + std::to_string(v[i]);
  return out + "]";
}
} // namespace

std::string format_event(const Event &e) {
  std::string out = e.kind;
  for (const auto &a : e.args)
    out += " " + format_arg(a);
  return out;
}

// ---- 3. host ----------------------------------------------------------------------
std::int32_t Host::run_script(const std::string &name) {
  ran.push_back(name);
  if (runner)
    return runner(name);
  const auto found = scripts.find(name);
  return found == scripts.end() ? MISSING_ENTRY : found->second;
}

Wide Host::text(const std::string &key) const {
  if (text_source) {
    if (auto found = text_source(key))
      return *found;
    throw Refused("text key '" + key + "' not provided");
  }
  const auto found = texts.find(key);
  if (found == texts.end())
    throw Refused("text key '" + key + "' not provided");
  return found->second;
}

std::int64_t Host::role_anim(const Object *obj, const std::string &role) const {
  const auto found = role_anims.find({obj ? obj->addr : 0u, role});
  return found == role_anims.end() ? 0 : found->second;
}

namespace {
std::int64_t next_of(std::vector<std::int64_t> &values) {
  if (values.empty())
    throw Refused("tick input list is empty");
  const auto v = values.front();
  if (values.size() > 1)
    values.erase(values.begin());
  return v;
}
} // namespace
std::int64_t TickIO::camera_step(int frames) {
  asked.emplace_back("camera", frames);
  if (look_at)
    return look_at(frames);
  return next_of(camera);
}
std::int64_t TickIO::held_ready() {
  asked.emplace_back("held", 0);
  return next_of(held);
}
std::int64_t TickIO::queue_count() {
  asked.emplace_back("queue", 0);
  return next_of(queue);
}
std::int64_t TickIO::stream_position() {
  asked.emplace_back("position", 0);
  return next_of(position);
}
std::int64_t TickIO::stream_status() {
  asked.emplace_back("status", 0);
  return next_of(status);
}

// ---- 4. the machine ---------------------------------------------------------------
void Dialog::reset_focus_anim() {
  Object *f = focus;
  if (!f)
    return;
  if (f->anim_count > 1)
    f->anim_count = 1;
  if (f->anim_cur != idle_anim) {
    // Only the count is written here; anim_push 0x170838 then adds the idle
    // track. Its effect on +0x118 / +0x11c is not modelled (as the reference).
    f->anim_count = 0;
    log.push_back(ev("anim_push", {I(f->addr), I(idle_anim), I(1)}));
  }
}

void Dialog::clear_text() {
  text.clear();
  text_len = 0;
}

int Dialog::mutex_try() {
  if (mutex == 0) {
    mutex = 1;
    return 0;
  }
  return -1;
}

std::optional<Bytes> Dialog::start(const Host &host, const std::string &name, int a, int b,
                                   const std::array<Raw, 3> &cam, int yaw, int pitch) {
  log.push_back(ev("camera_update"));
  swoop_done = 0;
  if (!host.is_local(active_player) || is_open())
    return std::nullopt;
  auto msg = start_message(name, active_player, a, b, cam, yaw, pitch);
  log.push_back(ev("send", {Arg{msg}}));
  return msg;
}

bool Dialog::on_open(Host &host, const Bytes &msg, const std::vector<Node> &file, const std::string &level) {
  const ScopedFpu fpu{kPs2};
  const auto m = parse_start_message(msg);
  if (host.recall || host.mission_menu) {
    mutex_release();
    return false;
  }
  log.push_back(ev("close_local_menus", {I(0)}));
  log.push_back(ev("camera_push"));
  log.push_back(ev("camera_yaw", {I(s16(static_cast<std::uint32_t>(m.yaw)))}));
  log.push_back(ev("camera_pitch", {I(s16(static_cast<std::uint32_t>(m.pitch)))}));
  log.push_back(ev("camera_apply", {I(m.cam[0]), I(m.cam[1]), I(m.cam[2])}));
  log.push_back(ev("lump", {S(level + ".lmp"), S(m.name + ".dbin")}));
  const int first = pick_start(file, [&host](const std::string &n) { return host.run_script(n); });
  Object *obj = find_speaker(host, m.name, m.player);
  if (obj->flags & 0x20) {
    if (!node) { // dialog_setFocusObj stores only while closed
      focus = obj;
      focus_word = obj->npc ? obj->addr + 0x3C8 : obj->word24;
    }
  }
  auto idle = host.role_anim(focus, "dialogidle");
  if (idle == 0)
    idle = host.role_anim(focus, "idle");
  idle_anim = idle;
  const auto key = obj->props.find("showname");
  name = key != obj->props.end() ? host.text(key->second) : Wide{};
  log.push_back(ev("freeze_all"));
  clear_text();
  nodes = file;
  has_nodes = true;
  name_len = std::int64_t(name.size());
  hud_suppressed = 1;
  revealing = 1;
  next_world.clear();
  drawn = 0;
  closing = 0;
  swoop = m.a;
  side = m.b;
  end_sent = 0;
  store_pending = 0;
  std::int64_t biggest = 0;
  for (const auto &n : nodes) {
    const auto found = host.anm_length.find(n.voice + ".anm");
    const auto length = s32v(found == host.anm_length.end() ? 0 : found->second);
    if (biggest < length)
      biggest = length;
  }
  const auto size = (static_cast<std::uint32_t>(biggest) + 0x7FFu) & ~0x7FFu;
  for (int i = 0; i < 2; ++i) {
    log.push_back(ev("alloc", {I(size)}));
    buffers[std::size_t(i)].ptr = 1 + i; // stands for the allocation
  }
  show_node(host, first);
  remote = host.is_local(active_player) ? 0 : 1;
  if (remote)
    log.push_back(ev("fade_wait", {I(FADE_WAIT_FRAMES)}));
  if (swoop) {
    if (host.view) {
      if (!focus)
        throw Refused("Camera_setupDialogView with no focus object: the original reads the stale focus word");
      host.view(side, *focus, m.cam, m.yaw, m.pitch);
    }
    log.push_back(ev("dialog_view", {I(side)}));
    cam_move = 1;
  }
  return true;
}

// The object the conversation is with (0x22efe4-0x22f15c, section 2.4 step 6).
Object *Dialog::find_speaker(Host &host, const std::string &who, int player) {
  const auto it = host.objects.find(who);
  static const std::vector<Object *> none;
  const auto &found = it == host.objects.end() ? none : it->second;
  Object *pobj = nullptr;
  if (0 <= player && player < host.num_players) {
    const auto p = host.player_obj.find(player);
    pobj = p == host.player_obj.end() ? nullptr : p->second;
  }
  if (found.empty())
    throw Refused("no object named '" + who + "': the original reads address 0xc");
  if (!pobj)
    return found[0];
  Object *best = nullptr;
  Raw best_d = MINUS_ONE;
  const std::size_t limit = std::min<std::size_t>(found.size(), 100);
  for (std::size_t i = 0; i < limit; ++i) {
    Object *o = found[i];
    const auto dx = sub(o->pos[0], pobj->pos[0]);
    const auto dy = sub(o->pos[1], pobj->pos[1]);
    const auto dz = sub(o->pos[2], pobj->pos[2]);
    const auto d = rta::sqrt(add(add(mul(dx, dx), mul(dy, dy)), mul(dz, dz)));
    if (eq(best_d, MINUS_ONE) || lt(d, best_d)) {
      best = o;
      best_d = d;
    }
  }
  return best;
}

void Dialog::show_node(Host &host, int index) {
  if (index < 0 || std::size_t(index) >= nodes.size())
    throw Refused("node " + std::to_string(index) + " outside the file");
  const Node n = nodes[std::size_t(index)];
  prompt_node = -1;
  blend = ONE;
  revealing = 1;
  reveal_done = 0;
  reveal = 0;
  at_end = more = 0;
  cursor = cursor_target = 0;
  scroll = scroll_target = 0;
  hold = 0;
  node = index;
  node_index = index;
  log.push_back(ev("stream_stop", {I(1)}));
  reset_focus_anim();
  Wide units{SPACE, SPACE};
  for (const auto u : host.text(n.text)) {
    if (u == 0)
      break;
    units.push_back(u == CARET ? WIDE_NEWLINE : u);
  }
  if (units.size() >= TEXT_BUFFER_UNITS)
    throw Refused("text runs past the 0x800-byte buffer");
  text = units;
  text_len = std::int64_t(units.size());
  queue_line_audio(host, n);
  visible.clear();
  for (int i = 0; i < n.raw_choices; ++i) {
    if (std::size_t(i) >= n.choices.size())
      throw Refused("raw choice count beyond the parsed choices");
    const auto &ch = n.choices[std::size_t(i)];
    const std::int32_t ok = ch.condition.empty() ? 1 : host.run_script(ch.condition);
    if (ok != 0)
      visible.push_back(i);
  }
  if (visible.size() > VISIBLE_ROOM)
    // The list at 0x8e2d20 holds 10 words; entries 10 and 11 land on the text
    // length 0x8e2d48 and the reveal count 0x8e2d4c (difftest 'open', overlap).
    throw Refused(std::to_string(visible.size()) + " visible choices overrun 0x8e2d20");
  log.push_back(ev("text_begin", {I(1), I(COLOUR_LAYOUT)}));
  log.push_back(ev("wrapped", {I(BODY_X), I(0), S("body"), I(BODY_W), I(0), I(0), I(LINE)}));
  log.push_back(ev("text_end"));
  const auto count = std::int64_t(visible.size());
  for (std::int64_t i = 0; i < count; ++i)
    choice_y[std::size_t(i)] = CHOICE_BASE - LINE * (count - i);
  const auto height = host.layout ? host.layout(text) : host.layout_height;
  total = std::max<std::int64_t>(height + LINE * count - HEIGHT_SLACK, 0);
}

void Dialog::queue_line_audio(Host &host, const Node &n) {
  if (!anm_ready)
    throw Refused("the original spins on lump_pollAsync until the .anm read ends");
  const auto vag = n.voice + ".vag", anm = n.voice + ".anm";
  log.push_back(ev("stream_play", {I(1), S(vag), I(0x28)}));
  auto &buf = buffers[std::size_t(buf_sel)];
  const auto v = host.vag_length.find(vag);
  buf.vag = v == host.vag_length.end() ? 0 : v->second;
  hold = 1;
  const auto entry = host.anm_entry.find(anm);
  if (entry == host.anm_entry.end()) {
    anm_ready = 1;
    buf.has = 0;
  } else {
    const auto off = entry->second.first, nxt = entry->second.second;
    const auto size = s32v(nxt - off);
    log.push_back(ev("cd_read", {I(size), I(off)}));
    buf.size = size;
    buf.has = 1;
    anm_ready = 0; // cd_readAsync: the host sets it when done
  }
  talking = 0;
  cur_buf = buf_sel;
  buf_sel = buf_sel == 0 ? 1 : 0;
}

void Dialog::on_confirm(Host &host) {
  const ScopedFpu fpu{kPs2};
  if (!node)
    return;
  if (revealing != 0 && reveal_done == 0) {
    revealing = 0;
    reveal_done = 1;
    return;
  }
  if (at_end == 0 || !eq(blend, ONE)) {
    if (more && scroll == scroll_target) {
      more = 0;
      reveal_done = revealing == 0 ? 1 : 0;
      scroll_target = std::min(scroll + SCROLL_PAGE, total);
      span = from_i(scroll_target - scroll);
    }
    return;
  }
  log.push_back(ev("stream_stop", {I(1)}));
  if (!visible.empty()) {
    if (cursor < 0 || std::size_t(cursor) >= visible.size())
      throw Refused("cursor outside the visible choices");
    const Choice ch = nodes[std::size_t(*node)].choices[std::size_t(visible[std::size_t(cursor)])];
    if (!ch.action.empty())
      host.run_script(ch.action);
    if (!visible.empty() && ch.next != -1) {
      clear_text();
      show_node(host, ch.next);
      return;
    }
  }
  if (closing == 0) {
    log.push_back(ev("fade_restart", {I(0)}));
    closing = 1;
  }
}

void Dialog::on_move(Host &host, int delta) {
  if (visible.size() > 1 && more == 0 && at_end != 0) {
    const auto next = cursor + delta;
    if (0 <= next && next < std::int64_t(visible.size())) {
      cursor_target = next;
      if (host.network) {
        blend = ONE;
        cursor = next;
      } else {
        blend = ZERO;
      }
    }
  }
}

void Dialog::on_end(Host &host) {
  if (!node)
    return;
  if (!anm_ready)
    throw Refused("the original spins on lump_pollAsync until the .anm read ends");
  for (auto &b : buffers)
    if (b.ptr) {
      log.push_back(ev("free"));
      b.ptr = 0;
    }
  hud_suppressed = 0;
  mutex = 0;
  node.reset();
  log.push_back(ev("camera_mode", {I(1)}));
  log.push_back(ev("stream_stop", {I(1)}));
  hold = 0;
  if (focus) {
    reset_focus_anim();
    focus = nullptr;
  }
  log.push_back(ev("unfreeze_all"));
  log.push_back(ev("far_clip", {I(CLIP_FAR_CLOSE)}));
  if (!next_world.empty()) {
    pending_world = next_world;
    orient = 0xFF;
  }
  log.push_back(ev("post_action"));
  if (on_post_action)
    on_post_action();
  if (store_pending) {
    if (!host.network) {
      log.push_back(ev("shop", {I(active_player), I(0), I(1)}));
    } else {
      log.push_back(ev("close_local_menus", {I(0)}));
      for (int slot = 0; slot < host.num_players; ++slot)
        if (host.is_local(slot)) {
          log.push_back(ev("shop", {I(slot), I(0), I(1)}));
          break;
        }
    }
  }
  if (remote)
    log.push_back(ev("fade_wait", {I(FADE_WAIT_FRAMES)}));
}

bool Dialog::on_message(Host &host, const Bytes &msg, const std::vector<Node> *file, const std::string &level) {
  if (msg.empty())
    throw Refused("empty game message");
  switch (msg[0]) {
  case MSG_OPEN:
    if (!file)
      throw Refused("message 0 needs the .dbin file");
    return on_open(host, msg, *file, level);
  case MSG_CONFIRM:
    on_confirm(host);
    return false;
  case MSG_MOVE:
    if (msg.size() < 2)
      throw Refused("message 2 without its delta");
    on_move(host, s8(msg[1]));
    return false;
  case MSG_END:
    on_end(host);
    return false;
  case MSG_LEAVE:
    on_leave();
    return false;
  default:
    return false;
  }
}

// ---- dialog_runTask 0x22ff48 (section 5.1) ----------------------------------------
std::vector<Bytes> Dialog::tick(Host &host, TickIO &io) {
  const ScopedFpu fpu{kPs2};
  std::vector<Bytes> sent;
  const auto send = [&](Bytes msg) {
    log.push_back(ev("send", {Arg{msg}}));
    sent.push_back(msg);
    if (on_send)
      on_send(msg);
  };
  prompt_shown = 0;
  if (cam_move != 0) {
    const Raw look = io.look;
    std::int64_t arrived = 0;
    if (cam_move == 1) {
      while (true) {
        arrived = io.camera_step(CAMERA_LOOK_FRAMES);
        if (lt(OPEN_LOOK_LIMIT, look))
          log.push_back(ev("far_clip", {I(CLIP_FAR_OPEN)}));
        if (!(swoop_done != 0 && arrived == 0))
          break;
      }
    } else {
      if (host.shop_open == 0) {
        arrived = io.camera_step(-CAMERA_LOOK_FRAMES);
        if (lt(look, CLOSE_LOOK_LIMIT))
          log.push_back(ev("far_clip", {I(CLIP_FAR_CLOSE)}));
      }
    }
    if (arrived != 1)
      return sent;
    if (cam_move != -1) {
      cam_move = 0;
      return sent;
    }
    send({MSG_END});
    cam_move = 0;
    return sent;
  }
  if (!node) {
    prompt_shown = 0;
    return sent;
  }
  std::uint32_t pad = 0;
  log.push_back(ev("duck", {I(DUCK)}));
  prompt_node = node_index;
  if (!host.pause_menu) {
    if (!host.network) {
      const int ap = active_player;
      const auto p = host.raw_pad.find(ap);
      pad = p == host.raw_pad.end() ? 0 : p->second;
      const auto o = host.player_obj.find(ap);
      const Object *pobj = o == host.player_obj.end() ? nullptr : o->second;
      if (!pobj || (pobj->flags & 8))
        active_player = ap ^ 1;
    } else {
      for (const int slot : host.alive)
        if (host.is_local(slot)) {
          const auto p = host.raw_pad.find(slot);
          pad = p == host.raw_pad.end() ? 0 : p->second;
          break;
        }
    }
  }
  // voice and mouth (0x2301c0-0x230288)
  bool sync = false;
  if (hold == 1 && io.held_ready()) {
    if (anm_ready) {
      hold = 0;
      queue_mark = io.queue_count();
      log.push_back(ev("clear_hold", {I(1)}));
      sync = audio_ready_branch(io);
    } else {
      sync = talking != 0;
    }
  } else {
    sync = audio_ready_branch(io);
  }
  if (sync)
    log.push_back(ev("sync_mouth"));
  // input and closing (0x230290-0x2303f8)
  if (closing == 0 || !eq(io.fade, ONE)) {
    if (io.text_option != 0 || (hold == 0 && io.stream_status() == 0)) {
      if (eq(blend, ONE)) {
        if (pad & UP_BITS)
          send({MSG_MOVE, 0xFF});
        if (pad & DOWN_BITS)
          send({MSG_MOVE, 1});
      }
      if (pad & host.confirm_mask)
        send({MSG_CONFIRM});
    }
  } else if (end_sent == 0) {
    end_sent = 1;
    send({swoop ? MSG_LEAVE : MSG_END});
  }
  // scroll (0x2303fc-0x2304ac)
  if (scroll == scroll_target && more == 0 && at_end == 0) {
    if (scroll == total) {
      more = 0;
      at_end = 1;
    } else if (io.text_option == 0) {
      scroll_target = std::min(scroll + SCROLL_PAGE, total);
      span = from_i(scroll_target - scroll);
    } else {
      more = 1;
    }
  } else {
    scroll = std::min(scroll + SCROLL_STEP, scroll_target);
  }
  // measuring pass (0x2304b0-0x23058c): the text with a zero-width clip
  log.push_back(ev("text_begin", {I(TEXT_LAYER), I(COLOUR_MEASURE)}));
  if (revealing)
    log.push_back(ev("budget", {I(reveal)}));
  log.push_back(ev("clip", {I(0), I(ROW0 + LINE + 6), I(0), I(ROW0 + 6 * LINE)}));
  const int bottom = io.text_option == 0 ? BODY_BOTTOM_NOTEXT : BODY_BOTTOM_TEXT;
  log.push_back(ev("wrapped", {I(BODY_X), I(ROW0 + LINE - scroll), S("body"), I(BODY_W), I(BODY_TOP),
                               I(bottom), I(LINE)}));
  log.push_back(ev("text_end"));
  shown = io.measure ? io.measure(ROW0 + LINE - scroll, revealing ? reveal : -1, bottom) : io.shown;
  if (revealing && reveal_done == 0) {
    if (shown < reveal)
      reveal_done = 1;
    else
      reveal += 1;
  }
  blend = add(blend, BLEND_STEP);
  if (lt(ONE, blend)) {
    blend = ONE;
    if (cursor_target != cursor)
      cursor = cursor_target;
  }
  return sent;
}

// 0x230210: start the talk animation once the voice has begun.
bool Dialog::audio_ready_branch(TickIO &io) {
  if (!anm_ready)
    return talking != 0;
  if (talking)
    return true;
  if (hold == 0 && io.queue_count() != queue_mark && io.stream_position() > 0) {
    log.push_back(ev("talk_anim"));
    nudge = ZERO;
    talking = 1;
  }
  return talking != 0;
}

// ---- section 5.4 ------------------------------------------------------------------
std::optional<std::pair<std::int64_t, Raw>> Dialog::talk_anim(Host &host) {
  const ScopedFpu fpu{kPs2};
  Object *f = focus;
  if (!f)
    return std::nullopt;
  const auto &buf = buffers[std::size_t(cur_buf)];
  if (buf.has == 0) {
    const auto talk = host.role_anim(f, "talk");
    if (talk == 0)
      return std::nullopt;
    log.push_back(ev("anim_start", {I(TALK_BLEND), I(f->addr), I(talk), I(3)}));
    if (idle_anim == 0)
      return std::nullopt;
    log.push_back(ev("anim_start", {I(IDLE_BLEND), I(f->addr), I(idle_anim), I(3)}));
    const auto secs = mul(from_i(s32v(buf.vag)), SECONDS_PER_BYTE);
    const auto value = neg(mul(secs, TICKS_PER_SECOND));
    const std::int64_t off = 0x114 + std::int64_t(s8(f->anim_count)) * 0x20;
    f->track_store = std::make_pair(off, value);
    return f->track_store;
  }
  log.push_back(ev("anm_relocate"));
  log.push_back(ev("anim_start", {I(TALK_BLEND), I(f->addr), S("line"), I(0x26)}));
  if (idle_anim != 0)
    log.push_back(ev("anim_chain", {I(f->addr), I(idle_anim), I(3)}));
  return std::nullopt;
}

void Dialog::sync_mouth(Host &, const std::function<std::int64_t()> &position) {
  const ScopedFpu fpu{kPs2};
  Object *f = focus;
  if (!f || f->flag20_track < 0)
    return;
  const auto pos = position();
  if (pos != last_position) {
    if (f->flag20_track >= std::int64_t(f->tracks.size()))
      throw Refused("lip-sync track index outside the track table");
    const auto anim = sub(mul(f->tracks[std::size_t(f->flag20_track)], ONE_SIXTIETH), LIP_LEAD);
    const auto audio = mul(from_i(s32v(pos - 0x400)), SECONDS_PER_BYTE);
    last_position = pos;
    if (lt(rta::fabs(sub(audio, anim)), SYNC_TOLERANCE))
      nudge = ZERO;
    else if (lt(anim, audio))
      nudge = NUDGE;
    else
      nudge = neg(NUDGE);
  }
  if (eq(nudge, ZERO))
    return;
  const int count = std::max(s8(f->anim_count), 0);
  if (count > int(f->tracks.size()))
    throw Refused("more animation tracks than the port models");
  for (int i = 0; i < count; ++i)
    f->tracks[std::size_t(i)] = add(f->tracks[std::size_t(i)], nudge);
}

// ---- dialog_drawTask 0x230628 (section 7) -----------------------------------------
std::vector<Event> Dialog::draw(Host &host, Raw fade, int text_option, int glyph,
                                const std::function<std::int64_t()> &stream_status) {
  const ScopedFpu fpu{kPs2};
  std::vector<Event> ops;
  if (!node || cam_move != 0)
    return ops;
  if (drawn == 0) {
    drawn = 1;
    ops.push_back(ev("fade_restart", {I(0)}));
  }
  const Raw level = closing ? sub(ONE, fade) : fade;
  const std::uint32_t alpha = static_cast<std::uint32_t>(to_int(mul(level, 0x43000000))) << 24;
  const std::uint32_t pick = alpha | COLOUR_PICK;
  ops.push_back(ev("text_begin", {I(TEXT_LAYER), I(alpha | COLOUR_BODY)}));
  if (revealing)
    ops.push_back(ev("budget", {I(reveal)}));
  ops.push_back(ev("clip", {I(0), I(ROW0 + LINE + 6), I(0x280), I(ROW0 + 6 * LINE)}));
  const int bottom = text_option == 0 ? BODY_BOTTOM_NOTEXT : BODY_BOTTOM_TEXT;
  ops.push_back(ev("wrapped", {I(BODY_X), I(ROW0 + LINE - scroll), S("body"), I(BODY_W), I(BODY_TOP),
                               I(bottom), I(LINE)}));
  ops.push_back(ev("budget", {I(-1)}));
  ops.push_back(ev("clip", {I(0), I(0), I(0x280), I(0x1C0)}));
  if (reveal_done) {
    const Node &n = nodes[std::size_t(*node)];
    if (at_end && (text_option != 0 || (hold == 0 && stream_status() == 0))) {
      if (visible.empty()) {
        ops.push_back(ev("colour", {I(pick)}));
        ops.push_back(ev("wide", {I(CHOICE_X), I(ROW0 + 6 * LINE), S("Affirmation"), I(FORCED_BUDGET)}));
      } else {
        for (std::size_t i = 0; i < visible.size(); ++i) {
          ops.push_back(ev("colour", {I(std::int64_t(i) == cursor ? pick : (alpha | COLOUR_OTHER))}));
          ops.push_back(ev("wrapped", {I(CHOICE_X), I(choice_y[i]), S(n.choices[std::size_t(visible[i])].text),
                                       I(BODY_W), I(CHOICE_TOP), I(CHOICE_BOTTOM), I(LINE)}));
        }
      }
      ops.push_back(ev("colour", {I(pick)}));
      std::int64_t y = 0;
      if (n.raw_choices == 0) {
        y = MORE_Y;
      } else {
        if (cursor < 0 || cursor >= MAX_CHOICES || cursor_target < 0 || cursor_target >= MAX_CHOICES)
          throw Refused("cursor outside the choice rows");
        const auto h = hermite(blend, ZERO, ONE, MINUS_ONE, MINUS_ONE);
        const auto y0 = choice_y[std::size_t(cursor)];
        const auto y1 = choice_y[std::size_t(cursor_target)];
        const auto v = add(from_i(y0), mul(from_i(s32v(y1 - y0)), h));
        y = to_int(add(from_i(to_int(v)), HALF));
      }
      ops.push_back(ev("colour", {I(alpha | COLOUR_GLYPH)}));
      ops.push_back(ev("ascii", {I(BODY_X), I(y), I(glyph), I(GLYPH_BUDGET)}));
      prompt_shown = 1;
    }
    if (more) {
      ops.push_back(ev("colour", {I(alpha | COLOUR_GLYPH)}));
      ops.push_back(ev("ascii", {I(0x33), I(MORE_Y), I(glyph), I(GLYPH_BUDGET)}));
      ops.push_back(ev("colour", {I(pick)}));
      ops.push_back(ev("wide", {I(0x51), I(MORE_Y), S("dialogMore"), I(FORCED_BUDGET)}));
    }
  }
  ops.push_back(ev("text_end"));
  ops.push_back(ev("sprite", {I(BOX_X), I(BOX_Y), I(BOX_LAYER), I(alpha | COLOUR_BOX)}));
  if (text_option != 0) {
    const auto keep = std::size_t(std::clamp<std::int64_t>(name_len, 0, std::int64_t(name.size())));
    std::vector<std::int64_t> shown_name(name.begin(), name.begin() + std::ptrdiff_t(keep));
    if (closing && name_len) {
      const auto cut = to_int(mul(from_i(name_len), NAME_SHRINK));
      name_len = std::max<std::int64_t>(name_len - std::max<std::int64_t>(1, cut), 0);
    }
    ops.push_back(ev("text_begin", {I(TEXT_LAYER), I(alpha | COLOUR_NAME)}));
    ops.push_back(ev("wide_centered", {I(NAME_X), I(NAME_Y), Arg{shown_name}, I(FORCED_BUDGET)}));
    ops.push_back(ev("text_end"));
    if (!host.is_local(active_player)) {
      ops.push_back(ev("text_begin", {I(TEXT_LAYER), I(alpha | COLOUR_NAME)}));
      ops.push_back(ev("fit", {I(active_player), I(PLAYER_FIT)}));
      ops.push_back(ev("ascii_centered", {I(PLAYER_X), I(PLAYER_Y), I(active_player), I(PLAYER_BUDGET)}));
      ops.push_back(ev("text_end"));
    }
  }
  log.insert(log.end(), ops.begin(), ops.end());
  return ops;
}

void Dialog::free() {
  for (auto &b : buffers)
    if (b.ptr) {
      log.push_back(ev("free"));
      b.ptr = 0;
    }
  prompt_node = -1;
  focus = nullptr;
  focus_word = 0;
  mutex = 0;
  node.reset();
  active_player = 0;
  node_index = -1;
  prompt_shown = 0;
  cam_move = 0;
  swoop = 0;
}

// ---- script built-ins ---------------------------------------------------------------
void Dialog::activate_store(Host &host, std::int64_t index) {
  if (static_cast<std::uint64_t>(index & 0xFFFFFFFF) < 21)
    store_index = s16(static_cast<std::uint32_t>(index));
  if (is_open()) {
    store_pending = 1;
    return;
  }
  if (!host.network) {
    log.push_back(ev("shop", {I(active_player), I(0), I(1)}));
  } else {
    for (const int slot : host.alive)
      if (host.is_local(slot)) {
        log.push_back(ev("close_local_menus", {I(0)}));
        log.push_back(ev("shop", {I(slot), I(0), I(1)}));
      }
  }
}

void Dialog::activate_mission_menu(Host &host, int first_active) {
  if (is_open() || host.recall)
    return;
  log.push_back(ev("mission_menu", {I(host.network ? first_active : active_player)}));
}

std::string Dialog::interact(Host &host, Object &obj, int slot, bool flag800000) {
  const ScopedFpu fpu{kPs2};
  const auto p = host.player_obj.find(slot);
  const Object *pobj = p == host.player_obj.end() ? nullptr : p->second;
  if (!obj.display_name || !pobj)
    return "nothing";
  const auto shop = obj.props.find("shop");
  if (shop != obj.props.end() && prop_int(shop->second) != 0 && flag800000) {
    if (host.is_local(slot)) {
      log.push_back(ev("shop", {I(slot), I(0), I(1)}));
      return "shop";
    }
    return "nothing";
  }
  const auto dx = sub(pobj->pos[0], obj.pos[0]);
  const auto dy = sub(pobj->pos[1], obj.pos[1]);
  const auto dot = add(mul(dx, cos16(obj.facing)), mul(dy, sin16(obj.facing)));
  if (!lt(ZERO, dot))
    return "behind";
  if (mutex_try() != 0)
    return "busy";
  active_player = slot;
  const auto script = obj.props.find("dialogscript");
  if (script == obj.props.end())
    return "no script"; // script_runByName(0) returns -1; the mutex stays held
  host.run_script(script->second);
  return "script";
}

std::string Dialog::trigger(Host &host, Object &obj) {
  const auto script = obj.props.find("dialogscript");
  if (!obj.display_name || script == obj.props.end())
    return "nothing";
  if (mutex_try() != 0)
    return "busy";
  host.run_script(script->second);
  return "script";
}

// ---- text table ---------------------------------------------------------------------
TextTable TextTable::parse(const std::uint8_t *data, std::size_t size) {
  TextTable out;
  if (!data)
    throw Refused("no text table");
  std::size_t at = 0;
  while (true) {
    if (at >= size)
      throw Refused("text table ends without its terminating key");
    if (data[at] == 0)
      break;
    std::size_t len = 0;
    while (at + len < size && data[at + len])
      ++len;
    if (at + len >= size)
      throw Refused("text key runs past the table");
    const std::string key(reinterpret_cast<const char *>(data + at), len);
    std::size_t text = (at + len + 2) & ~std::size_t(1);
    Wide units;
    while (true) {
      if (text + 1 >= size)
        throw Refused("text runs past the table");
      const auto u = static_cast<std::uint16_t>(data[text] | data[text + 1] << 8);
      text += 2;
      if (!u)
        break;
      units.push_back(u);
    }
    out.table_.emplace(key, std::move(units)); // an earlier equal key stays
    ++out.entries_;
    at = text;
  }
  return out;
}

namespace {
std::string strip_quotes(const std::string &key) {
  std::string out;
  for (char c : key)
    if (c != '"')
      out.push_back(c);
  return out;
}
} // namespace

bool TextTable::contains(const std::string &key) const { return table_.count(strip_quotes(key)) != 0; }

Wide TextTable::lookup(const std::string &key) const {
  const auto stripped = strip_quotes(key);
  const auto found = table_.find(stripped);
  if (found != table_.end())
    return found->second;
  // Which spelling the miss text copies is not stated for a quoted key.
  if (stripped != key)
    throw Refused("missing text key with double quotes");
  Wide out{'N', 'F', ':'};
  for (std::size_t i = 0; i < key.size() && i < 16; ++i)
    out.push_back(static_cast<std::uint8_t>(key[i]));
  return out;
}

std::int64_t prop_int(const std::string &raw) {
  // str.strip() on the latin-1 text: ASCII whitespace, 0x1c-0x1f, 0x85, 0xa0.
  const auto space = [](char ch) {
    const auto c = static_cast<unsigned char>(ch);
    return c == ' ' || (c >= 0x09 && c <= 0x0d) || (c >= 0x1c && c <= 0x1f) || c == 0x85 || c == 0xa0;
  };
  // str.isdigit() is also true for the latin-1 superscripts, which int() then
  // refuses; the reference raises there, so this port refuses too.
  const auto superscript = [](char ch) {
    const auto c = static_cast<unsigned char>(ch);
    return c == 0xb2 || c == 0xb3 || c == 0xb9;
  };
  std::size_t b = 0, e = raw.size();
  while (b < e && space(raw[b]))
    ++b;
  while (e > b && space(raw[e - 1]))
    --e;
  const std::string text = raw.substr(b, e - b);
  std::int64_t sign = 1;
  std::size_t i = 0;
  if (!text.empty() && (text[0] == '+' || text[0] == '-')) {
    sign = text[0] == '-' ? -1 : 1;
    i = 1;
  }
  std::size_t j = i;
  while (j < text.size() && ((text[j] >= '0' && text[j] <= '9') || superscript(text[j]))) {
    if (superscript(text[j]))
      throw Refused("property integer with a superscript digit");
    ++j;
  }
  if (j == i)
    return 0;
  // Only "non-zero" is ever asked of it; a longer run is not modelled.
  if (j - i > 18)
    throw Refused("property integer longer than 18 digits");
  return sign * std::stoll(text.substr(i, j - i));
}
} // namespace champions::simulation::rta::dialog
