#include "champions/dialog_gaps.hpp"
#include <cstdlib>
#include <string>

namespace champions::simulation::rta::dialog_gaps {
namespace {
constexpr std::uint32_t U(std::int64_t v) { return static_cast<std::uint32_t>(v); }
Fpu ps2() { return Fpu{Fpu::Model::Ps2, false, false}; }

// The %d conversion of newlib's scanner: white space, a sign, at least one
// digit. Returns the value and the new index, or nullopt.
std::optional<std::pair<std::int32_t, std::size_t>> scan_int(const std::string &text, std::size_t i) {
  const auto n = text.size();
  const auto space = [](char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'; };
  while (i < n && space(text[i]))
    ++i;
  const auto start = i;
  if (i < n && (text[i] == '+' || text[i] == '-'))
    ++i;
  const auto digits = i;
  while (i < n && text[i] >= '0' && text[i] <= '9')
    ++i;
  if (i == digits)
    return std::nullopt;
  // Longer than any 32-bit value, or outside 32 bits: not modelled.
  if (i - digits > 12)
    throw Refused("dialogOffset field out of 32-bit range: not modelled");
  const auto value = std::strtoll(text.c_str() + start, nullptr, 10);
  if (value < INT32_MIN || value > INT32_MAX)
    throw Refused("dialogOffset field out of 32-bit range: not modelled");
  return std::make_pair(static_cast<std::int32_t>(value), i);
}
std::string lower(const std::string &key) {
  std::string out = key;
  for (auto &c : out)
    if (c >= 'A' && c <= 'Z')
      c = char(c - 'A' + 'a');
  return out;
}
} // namespace

Offset parse_dialog_offset(const std::optional<std::string> &text, const Stale &stale) {
  Offset out;
  out.dist = stale.dist;
  out.height = stale.height;
  out.lateral = stale.lateral;
  out.pitch = stale.pitch;
  if (!text) {
    out.dist = 0;
    out.height = 0;
    out.pitch = 0;
    return out;
  }
  std::optional<std::int32_t> *slots[4] = {&out.dist, &out.height, &out.lateral, &out.pitch};
  std::size_t i = 0;
  for (int k = 0; k < 4; ++k) {
    if (k) {
      if (i < text->size() && (*text)[i] == ',')
        ++i;
      else
        break;
    }
    const auto r = scan_int(*text, i);
    if (!r)
      break;
    *slots[k] = r->first;
    i = r->second;
    out.converted += 1;
  }
  return out;
}

int side_index(bool a, bool b, bool c, const std::function<bool()> &behind) {
  if (!a && !b && !c)
    return 1;
  if (!a && b && !c)
    return behind() ? 2 : 0;
  if (!a && c)
    return 0;
  if (a && !c)
    return 2;
  const int lead = 1 + (b ? 1 : 0) + ((b && c) ? 1 : 0);
  return lead == 3 ? 1 : lead;
}

ViewInfo setup_dialog_view(View &view, const Vec3 &npc_pos, std::int32_t npc_facing, int side,
                           const std::vector<Vec3> &players, const Vec3 &camera_pos,
                           const std::optional<std::string> &offset_text, const Stale &stale) {
  const ScopedFpu fpu{ps2()};
  ViewInfo info;
  info.fields = parse_dialog_offset(offset_text, stale);
  const auto &f = info.fields;
  if (!f.dist || !f.height || !f.lateral || !f.pitch)
    throw Refused("Camera_setupDialogView reads an uninitialised stack slot here (no dialogOffset, or fewer "
                  "than four fields): value not modelled");
  const std::int32_t dist = *f.dist, height = *f.height, lateral = *f.lateral, pitch_off = *f.pitch;
  const Raw nx = npc_pos[0], ny = npc_pos[1], nz = npc_pos[2];
  std::int32_t off = 0;
  if (side != 0) {
    const Raw fx = cos16(U(std::int64_t(npc_facing) + FACING_X_ADD));
    const Raw fy = sin16(U(std::int64_t(npc_facing) + FACING_Y_ADD));
    const Raw fz = ZERO;
    bool a = false, b = false, c = false;
    for (const auto &p : players) {
      const Raw dx = sub(p[0], nx), dy = sub(p[1], ny), dz = sub(p[2], nz);
      const Raw d = rta::sqrt(add(add(mul(dx, dx), mul(dy, dy)), mul(dz, dz)));
      if (!lt(d, NEAR))
        continue;
      const Raw r = rsqrt(ONE, add(mul(dx, dx), mul(dy, dy)));
      const Raw ux = mul(dx, r), uy = mul(dy, r), uz = mul(ZERO, r);
      const Raw dot = add(add(mul(ux, fx), mul(uy, fy)), mul(uz, fz));
      if (lt(dot, SIDE_DOT_NEG))
        c = true;
      else if (lt(SIDE_DOT, dot))
        a = true;
      else
        b = true;
    }
    info.flags = {a, b, c};
    const auto idx = side_index(a, b, c, [&] {
      return lt(vec3_dot_delta({nx, ny, nz}, {fx, fy, fz}, camera_pos), ZERO);
    });
    off = std::int32_t(idx * -SIDE_ANGLE + SIDE_ANGLE);
  }
  info.angle = off;
  const Raw cs = cos16(U(std::int64_t(npc_facing) + off));
  const Raw sn = sin16(U(std::int64_t(npc_facing) + off));
  Raw r = rsqrt(ONE, add(mul(cs, cs), mul(sn, sn)));
  const Raw s1 = mul(sn, r), c1 = mul(cs, r);
  const Raw up0 = ZERO, up1 = ZERO, up2 = ONE;
  const Raw sv0 = neg(mul(up2, s1)), sv1 = mul(up2, c1), sv2 = sub(mul(up0, s1), mul(up1, c1));
  const Raw lat = from_int(U(lateral));
  const Raw r2 = rsqrt(ONE, add(add(mul(c1, c1), mul(s1, s1)), mul(ZERO, ZERO)));
  const Raw cx = mul(c1, r2), cy = mul(s1, r2);
  const Raw dist_f = from_int(U(std::int64_t(dist) + DIST_ADD));
  const Raw height_f = from_int(U(std::int64_t(height) + HEIGHT_ADD));
  view.target = {add(add(nx, mul(cx, dist_f)), mul(sv0, lat)), add(add(ny, mul(cy, dist_f)), mul(sv1, lat)),
                 add(add(nz, height_f), mul(sv2, lat))};
  view.target_pitch = s16(U(std::int64_t(pitch_off & 0xFFFF) + PITCH_ADD));
  view.target_yaw = s16(U(-(std::int64_t(npc_facing & 0xFFFF) + off + 0x4000)));
  view.start = camera_pos;
  view.start_pitch = view.pitch;
  view.start_yaw = view.yaw;
  Raw a0 = cos16(U(view.yaw)), b0 = sin16(U(view.start_yaw));
  Raw a2 = cos16(U(view.target_yaw)), b2 = sin16(U(view.target_yaw));
  r = rsqrt(ONE, add(mul(a0, a0), mul(b0, b0)));
  a0 = mul(a0, r);
  b0 = mul(b0, r);
  const Raw z0 = mul(ZERO, r);
  r = rsqrt(ONE, add(add(mul(a2, a2), mul(b2, b2)), mul(ZERO, ZERO)));
  a2 = mul(a2, r);
  b2 = mul(b2, r);
  const Raw z2 = mul(ZERO, r);
  const Raw dot = add(add(mul(a0, a2), mul(b0, b2)), mul(z0, z2));
  view.k = sub(ONE, mul(add(dot, ONE), HALF));
  const Raw ddx = sub(view.start[0], view.target[0]), ddy = sub(view.start[1], view.target[1]),
            ddz = sub(view.start[2], view.target[2]);
  if (lt(FAR_START, rta::sqrt(add(add(mul(ddx, ddx), mul(ddy, ddy)), mul(ddz, ddz)))))
    view.k = FAR_K;
  view.progress = ZERO;
  return info;
}

Look look_at(View &view, int steps) {
  const ScopedFpu fpu{ps2()};
  Look out;
  Raw p = view.progress;
  if (steps > 0 && eq(p, ONE)) {
    out.result = 1;
    return out;
  }
  if (steps < 0 && eq(p, ZERO)) {
    out.result = 1;
    return out;
  }
  if (steps == 0)
    throw Refused("Camera_lookAt with 0 steps divides by zero");
  const Raw inv = rta::div(ONE, from_int(U(steps)));
  p = add(p, sub(inv, mul(mul(inv, view.k), LOOK_K_SCALE)));
  if (lt(ONE, p))
    p = ONE;
  if (lt(p, ZERO))
    p = ZERO;
  view.progress = p;
  const Raw h = hermite(p, ZERO, ONE, ZERO, ZERO);
  constexpr Raw MINUS_ONE = 0xBF800000;
  view.pitch = angle_approach(mul(mul(h, h), h), MINUS_ONE, MINUS_ONE, U(view.start_pitch), U(view.target_pitch));
  view.yaw = angle_approach(h, MINUS_ONE, MINUS_ONE, U(view.start_yaw), U(view.target_yaw));
  out.h = h;
  out.apply = Vec3{add(view.start[0], mul(sub(view.target[0], view.start[0]), h)),
                   add(view.start[1], mul(sub(view.target[1], view.start[1]), h)),
                   add(view.start[2], mul(sub(view.target[2], view.start[2]), h))};
  return out;
}

RunCamera run_task_camera(View &view, int direction, bool swoop_done, bool shop_open, Raw forward_z) {
  const ScopedFpu fpu{ps2()};
  RunCamera out;
  const auto push_apply = [&](const Look &l) {
    if (l.apply)
      out.actions.push_back({"apply", ZERO, *l.apply});
  };
  if (direction == 1) {
    Look l;
    while (true) {
      l = look_at(view, SWOOP_STEPS);
      push_apply(l);
      out.h = l.result == 0 ? std::optional<Raw>(l.h) : std::nullopt;
      if (lt(FWD_Z_OPEN, forward_z))
        out.actions.push_back({"far_clip", FAR_CLIP_OPEN, {}});
      if (!swoop_done || l.result != 0)
        break;
    }
    if (l.result == 1)
      out.actions.push_back({"arrived", ZERO, {}});
    return out;
  }
  if (shop_open)
    return out;
  const auto l = look_at(view, -SWOOP_STEPS);
  push_apply(l);
  out.h = l.result == 0 ? std::optional<Raw>(l.h) : std::nullopt;
  if (lt(forward_z, FWD_Z_CLOSE))
    out.actions.push_back({"far_clip", FAR_CLIP_CLOSE, {}});
  if (l.result == 1) {
    out.actions.push_back({"arrived", ZERO, {}});
    out.actions.push_back({"send", 3, {}});
  }
  return out;
}

std::uint32_t lcg(std::uint32_t seed) { return seed * LCG_MUL + LCG_ADD; }

Applied camera_apply(const Vec3 &pos, std::int32_t yaw, std::int32_t pitch, Raw shake, std::uint32_t seed,
                     bool frozen, bool dialog_open) {
  const ScopedFpu fpu{ps2()};
  Applied out;
  auto m = mat34_set_identity();
  m = mat34_rotate_axis(m, 0, U(s16(U(std::int64_t(pitch & 0xFFFF) + APPLY_PITCH_ADD))));
  m = mat34_rotate_axis(m, 2, U(s16(U(yaw))));
  Raw x = pos[0], y = pos[1], z = pos[2];
  if (lt(ZERO, shake) && (!frozen || dialog_open)) {
    const auto s1 = lcg(seed), s2 = lcg(s1), s3 = lcg(s2);
    seed = s3;
    out.draws = 3;
    const Raw u = sub(mul(from_int(s1 >> 16), SHAKE_SCALE), HALF);
    const Raw v = sub(mul(from_int(s2 >> 16), SHAKE_SCALE), HALF);
    const Raw w = sub(mul(from_int(s3 >> 16), SHAKE_SCALE), HALF);
    x = add(x, mul(add(u, u), shake));
    y = add(y, mul(add(v, v), shake));
    z = add(z, mul(w, shake));
    shake = sub(shake, HALF);
  }
  out.pos = {x, y, z};
  out.matrix = m;
  out.shake = shake;
  out.seed = seed;
  return out;
}

std::int64_t game_is_frozen(const Frozen &s, std::vector<std::string> *log) {
  if (s.network)
    return s.paused;
  if (s.cutscene)
    return s.warmup;
  if (log)
    log->push_back("task");
  if (s.player_select && s.sim_frame < 60)
    return 0;
  if (s.paused || s.warmup)
    return 1;
  if (!s.controller_out)
    return 0;
  if (log)
    log->push_back("dialog");
  return s.dialog ? 0 : 1;
}

std::vector<int> recall_poll(const std::vector<RecallPlayer> &players) {
  std::vector<int> out;
  for (const auto &p : players) {
    if (p.lockout || p.frozen || p.dialog || p.obj4b4 || p.recall || p.b4f66c8 || p.camflag || p.jumpgate ||
        p.pausemenu || p.obj540 || p.flag400000)
      continue;
    if (p.use && p.b100)
      out.push_back(p.slot);
  }
  return out;
}

int camera_update_control(bool frozen, bool world, bool menu_open, bool warmup, bool b4f66c8, bool dialog,
                          bool lockout, bool flag400000) {
  if (frozen || !world)
    return 0;
  if (menu_open || warmup || b4f66c8 || dialog || lockout)
    return 0;
  return flag400000 ? 0 : 1;
}

const std::vector<Site> &sites() {
  static const std::vector<Site> table = {
      {"prop_draw", 0x1548D0, 0x1548BC, SiteKind::Draw, "Prop_draw returns without drawing the prop"},
      {"phprop_draw", 0x156DCC, 0x156DB4, SiteKind::Draw, "PhProp_draw returns without drawing"},
      {"pause_exit", 0x18CD08, 0x18CC08, SiteKind::Input, "pause menu: menu_exitmission +4 = 5"},
      {"pause_kick", 0x18CEB4, 0x18CC08, SiteKind::Input, "pause menu: menu_kickplayers +4 = 5 (else 0)"},
      {"pause_recall", 0x18D400, 0x18CC08, SiteKind::Input, "pause menu: menu_recall +4 = 5"},
      {"pause_change", 0x18D498, 0x18CC08, SiteKind::Input, "pause menu: menu_changeplayers +4 = 5"},
      {"warp_menu", 0x18E0E4, 0x18E00C, SiteKind::Sim, "warpMenu (offline): the start button cannot open the pause menu"},
      {"player_interact", 0x1A47BC, 0x1A46C0, SiteKind::Draw, "Player_draw skips Player_interact(P, 0, 1)"},
      {"player_name", 0x1A4ED8, 0x1A46C0, SiteKind::Draw, "Player_draw skips a remote player's name tag"},
      {"player_marker", 0x1BC07C, 0x1BBFF8, SiteKind::Draw, "Player_drawMarker draws no marker"},
      {"recall_button", 0x1BCF18, 0x1BCE88, SiteKind::Sim, "no hudmsg_sendRecall"},
      {"controller_out", 0x1DB5B0, 0x1DB3C8, SiteKind::Sim, "controller removed: no automatic pause menu offline"},
      {"spell_select", 0x1DD380, 0x1DD270, SiteKind::Input, "hud_spellSelect reads no pad (unless g_localMenuOpen)"},
      {"hud_panel_input", 0x1DE9E0, 0x1DE8C4, SiteKind::Input, "0x1de8c4: no weapon swap / ability cycle / cursor input"},
      {"hud_panel_button", 0x1DEDC4, 0x1DE8C4, SiteKind::Input, "0x1de8c4: second pad test skipped"},
      {"compass", 0x1DF7BC, 0x1DF720, SiteKind::Draw, "hud_drawCompass draws nothing"},
      {"net_cam_prompt", 0x1E0260, 0x1E01B8, SiteKind::Input, "spectator prompt: not drawn, confirm does not cycle"},
      {"is_frozen", 0x1EBE8C, 0x1EBDF8, SiteKind::Sim, "game_isFrozen: controller-out does not freeze"},
      {"status_fx", 0x1F6B78, 0x1F6AC8, SiteKind::Seed, "fx_modelStatusEffects returns: no effects, no cosmetic draws"},
      {"camera_control", 0x203464, 0x203400, SiteKind::Input, "Camera_update: +0x60 (user control) = 0"},
      {"camera_shake", 0x203820, 0x2035C8, SiteKind::Seed, "Camera_apply: shake (3 cosmetic draws) even while frozen"},
      {"save_screen", 0x218F14, 0x218AFC, SiteKind::Sim, "memcard_saveScreen state 4: aborted"},
      {"net_messages", 0x31DF40, 0x31DEEC, SiteKind::Draw, "0x31deec (net game): message list not drawn"},
      {"idle_voice", 0x322B8C, 0x322998, SiteKind::Seed, "mainloop_runSession: idle voice countdown held"},
  };
  return table;
}

std::string site_outcome(const std::string &name, const Conditions &c) {
  const auto g = [&](const char *k) -> std::int64_t {
    const auto it = c.find(k);
    return it == c.end() ? 0 : it->second;
  };
  const bool d = g("dialog") != 0;
  if (name == "prop_draw" || name == "phprop_draw")
    return d ? "skip" : "draw";
  if (name == "pause_exit") {
    if (g("pvp_b"))
      return "hidden";
    return d ? "disabled" : "unchanged";
  }
  if (name == "pause_kick") {
    const bool hidden = g("active") < 2;
    return std::string(hidden ? "hidden+" : "shown+") + (d ? "disabled" : "enabled");
  }
  if (name == "pause_recall") {
    if (g("hp_le0") && g("network"))
      return "disabled";
    if (d || g("jumpgate"))
      return "disabled";
    return g("cam_mode") == 1 ? "enabled" : "disabled";
  }
  if (name == "pause_change") {
    if (g("no_save") || g("network") || d)
      return "disabled";
    return g("cam_mode") == 1 ? "enabled" : "disabled";
  }
  if (name == "warp_menu")
    return (g("cam_mode") != 1 || d) ? "blocked" : "continue";
  if (name == "player_interact")
    return (g("paused") || g("lockout") || g("selector") || d) ? "skip" : "go";
  if (name == "player_name")
    return (!g("network") || !g("flag400000") || g("local") || d) ? "skip" : "tag";
  if (name == "player_marker")
    return d ? "skip" : "draw";
  if (name == "controller_out")
    return (g("network") || g("pause_open") || g("frontend") || d) ? "skip" : "go";
  if (name == "spell_select")
    return (g("lockout") || d) ? "fallback" : "next";
  if (name == "hud_panel_input" || name == "hud_panel_button")
    return (g("lockout") || d || g("cam_mode") != 1 || g("paused")) ? "skip" : "go";
  if (name == "compass")
    return (!g("compass_on") || d) ? "skip" : "go";
  if (name == "net_cam_prompt")
    return (d || g("cutscene") || g("pause_open")) ? "skip" : "go";
  if (name == "status_fx")
    return (g("flag10000000") || !g("model") || d) ? "skip" : "go";
  if (name == "save_screen")
    return ((g("network") && g("pending_level")) || g("cutscene") || d) ? "abort" : "go";
  if (name == "net_messages") {
    if (!g("network"))
      return "offline";
    return (g("frontend") || d || !g("list_on")) ? "skip" : "go";
  }
  if (name == "idle_voice") {
    if (g("countdown") <= 0)
      return "idle";
    if (g("cutscene") || g("paused") || g("local_menu") || d)
      return "held";
    if (g("countdown") - 1 == 1)
      return g("players") > 0 ? "voice" : "count";
    return "count";
  }
  throw Refused("unknown dialog_isOpen site '" + name + "'");
}

IdleVoice idle_voice(std::int64_t countdown, bool cutscene, bool paused, bool local_menu, bool dialog,
                     std::int64_t players, std::uint32_t seed) {
  IdleVoice out{countdown, seed, std::nullopt};
  if (countdown <= 0 || cutscene || paused || local_menu || dialog)
    return out;
  out.countdown = countdown - 1;
  if (out.countdown != 1 || players <= 0)
    return out;
  out.seed = lcg(seed);
  out.voice = static_cast<std::uint32_t>((out.seed >> 16) % std::uint64_t(players));
  return out;
}

std::uint32_t load_box(const std::string &level, bool pvp_b, std::uint32_t old, std::uint32_t res_find_result) {
  if (level == "intro" || pvp_b)
    return old;
  return res_find_result;
}

std::pair<std::uint32_t, std::uint32_t> box_sprite_reads(std::uint32_t word) {
  if (word == 0)
    throw Refused("box sprite with no texture: reads s16 at addresses 0 and 2 (not modelled)");
  return {word, word + 2};
}

std::map<std::uint32_t, std::uint32_t> init_camera_buttons(std::int32_t layout) {
  std::map<std::uint32_t, std::uint32_t> words = {{0x4F85B4, 0}, {0x4F7134, 0}};
  if (layout == 1) {
    for (const auto &w : std::map<std::uint32_t, std::uint32_t>{
             {0x41B968, 0x10}, {0x41B96C, 0x7C}, {0x41B970, 0x80}, {0x41B974, 0x7D}, {0x41B978, 0x7E},
             {0x41B97C, 0x7B}, {0x41BFBC, 0x40}, {0x41BFB8, 0x20}, {0x41B960, 0x40}, {0x41B964, 0x7E}})
      words[w.first] = w.second;
  } else if (layout == 0) {
    for (const auto &w : std::map<std::uint32_t, std::uint32_t>{{0x41B968, 0x80}, {0x41B96C, 0x7C}, {0x41B978, 0x7B},
                                                                  {0x41B97C, 0x7E}, {0x41BFBC, 0x20}, {0x41BFB8, 0x140},
                                                                  {0x41B960, 0x20}, {0x41B964, 0x7B}})
      words[w.first] = w.second;
  }
  return words;
}

std::optional<std::uint32_t> cursor_glyph(std::int32_t layout) {
  const auto words = init_camera_buttons(layout);
  const auto it = words.find(G_GLYPH);
  if (it == words.end())
    return std::nullopt;
  return it->second;
}

std::optional<std::string> props_get_str(const Entries &entries, const std::string &key, int nth) {
  const auto k = lower(key);
  for (const auto &e : entries) {
    if (e.size() > k.size() && e.compare(0, k.size(), k) == 0 && e[k.size()] == '=') {
      if (nth == 0)
        return e.substr(k.size() + 1);
      --nth;
    }
  }
  return std::nullopt;
}

std::int32_t props_get_int(const Entries &entries, const std::string &key, int nth) {
  const auto v = props_get_str(entries, key, nth);
  if (!v)
    return 0;
  const auto r = scan_int(*v, 0);
  return r ? r->first : 0;
}

bool has_any_light(const Entries &entries) {
  for (const char *k : {"ParticleEmitter", "DynamicLight", "Halo"})
    if (props_get_int(entries, k))
      return true;
  for (int n = 0; n < 4; ++n)
    for (const char *k : {"ParticleEmitter", "DynamicLight", "Halo"})
      if (props_get_int(entries, k + std::to_string(n)))
        return true;
  return false;
}

const std::vector<Ctor> &fallback_ctors() {
  static const std::vector<Ctor> table = {
      {"CreatureContainer", 0x243624, 0x3D0}, {"AutoContainerLP", 0x24259C, 0x150},
      {"AutoContainer", 0x23D93C, 0x148},      {"AutoChestLP", 0x241468, 0x100},
      {"AutoChest", 0x23AD18, 0xF8},           {"AutoPropLP", 0x2416C4, 0x448},
      {"AutoProp/Door/Switch", 0x23C44C, 0x198}, {"AutoEmitter", 0x240DC8, 0x60},
      {"Critter", 0x2547FC, 0x448},            {"Generator", 0x2657A0, 0x3E8},
      {"SmokeGen", 0x31D180, 0x490},
  };
  return table;
}

std::optional<std::string> factory_fallback(const Entries &e) {
  if (props_get_int(e, "CreatureContainer"))
    return "CreatureContainer";
  if (props_get_int(e, "AutoContainer"))
    return has_any_light(e) ? "AutoContainerLP" : "AutoContainer";
  if (props_get_int(e, "AutoChest"))
    return has_any_light(e) ? "AutoChestLP" : "AutoChest";
  if (props_get_int(e, "AutoProp"))
    return has_any_light(e) ? "AutoPropLP" : "AutoProp/Door/Switch";
  if (props_get_int(e, "AutoEmitter"))
    return "AutoEmitter";
  if (props_get_int(e, "AutoDoor") || props_get_int(e, "AutoSwitch"))
    return "AutoProp/Door/Switch";
  if (props_get_int(e, "Critter"))
    return "Critter";
  if (props_get_str(e, "Generator"))
    return "Generator";
  if (props_get_str(e, "SmokeGen"))
    return "SmokeGen";
  return std::nullopt;
}
} // namespace champions::simulation::rta::dialog_gaps
