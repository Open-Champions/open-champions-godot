// The level-script host's NPC dialog wiring (docs/DIALOG.md). The machine is
// champions-dialog; this file connects it to the binder's seams and to the
// host's own objects, and keeps the words the binder and the machine share in
// step. Research: t3/research-dialog 334578d docs/dialog.md.
#include "champions/script_commands.hpp"
#include "champions/session.hpp"
#include "champions/core_math.hpp"
#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace champions::simulation::rta::script_commands {
namespace {
// Words the binder (script_builtins.cpp) and the dialog machine share.
constexpr std::uint32_t DIALOG_DATA = 0x8e2d60, SWOOP = 0x4fdce4, SWOOP_DONE = 0x4fdce8,
                        NEXT_WORLD = 0x4fdcf0, STORE_PENDING = 0x4fdd30, ACTIVE_PLAYER = 0x4d8ae4,
                        PENDING_LEVEL = 0x41b6d0, PENDING_ORIENT = 0x41b720;
// Nested dialog operations (a post action that starts another conversation).
constexpr int MAX_DIALOG_DEPTH = 8;
// Player_interact's box (docs/dialog.md 2.1 step 1): +-48 in x and y, from the
// player's height to 60 above, at most 256 objects.
constexpr Raw BOX_XY = 0x42400000, BOX_UP = 0x42700000; // 48.0, 60.0

std::string lower(std::string text) {
  for (auto &c : text)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return text;
}
// Property lookup as tools/dialog_census.py counts it: the key without case,
// the first entry wins.
std::optional<std::string> prop(const objects::Props &props, const std::string &key) {
  if (!props)
    return std::nullopt;
  const auto want = lower(key);
  for (const auto &entry : *props) {
    const auto eq = entry.find('=');
    if (eq != std::string::npos && lower(entry.substr(0, eq)) == want)
      return entry.substr(eq + 1);
  }
  return std::nullopt;
}
// Classes whose talkers on the disc reach the base Creature_interact
// (docs/dialog.md 10 "by class"): NPC (research docs/monsters-2.md, NPC
// interact is the base 0x116bd8), GnomeNavigator and MattMarr (base slots 13
// and 14, docs/monsters.md class table), and the Orc class of the Orc2*
// factory names (same table). TestLocation is not in any class table: refused.
bool creature_class(const std::string &cls) {
  return cls == "NPC" || cls == "GnomeNavigator" || cls == "MattMarr" || cls.rfind("Orc2", 0) == 0;
}
// A refusal reason without the names it quotes (counts never carry level,
// object, entry or text names).
std::string head(const std::exception &e) {
  std::string text, in = e.what();
  bool quoted = false;
  for (char c : in) {
    if (c == '\'') {
      if (!quoted)
        text += "<name>";
      quoted = !quoted;
    } else if (!quoted) {
      text.push_back(c);
    }
  }
  if (text.size() > 80)
    text.resize(80);
  return text;
}
} // namespace

void Host::attach_dialog(DialogLevel level, DialogServices services) {
  dialog_level_ = std::move(level);
  dialog_services_ = std::move(services);
  dialog_files_.clear();
  dialog_views_.clear();
  dialog_ = dialog::Dialog{};
  dialog_host_ = dialog::Host{};
  dialog_counts_ = DialogCounts{};
  dialog_stalled_ = false;
  dialog_attached_ = true;
  camera_view_.reset();
  camera_pose_.reset();
  // dialog_loadBox 0x22f910: the word is written outside "intro" and arena mode B
  // only; otherwise it keeps what an earlier load left (docs/spec/dialog-gaps.md 4).
  box_word_ = dialog_gaps::load_box(dialog_level_.level, dialog_level_.pvp_mode_b,
                                    dialog_level_.previous_box ? 1u : dialog_gaps::BOX_WORD_INITIAL,
                                    dialog_level_.box_available ? 1u : 0u);
  auto &h = dialog_host_;
  h.network = false;
  h.local = {0};
  h.num_players = 1;
  h.alive = {0};
  h.player_obj = {{0, &dialog_player_}};
  h.confirm_mask = 0x40; // 0x41bfbc, observed on the live rig (research docs/issues/live-rig.md)
  h.runner = [this](const std::string &name) { return dialog_run_script(name, false); };
  if (dialog_services_.text)
    h.text_source = [this](const std::string &key) -> std::optional<dialog::Wide> {
      return dialog_services_.text(key);
    };
  if (dialog_services_.layout_height)
    h.layout = dialog_services_.layout_height;
  for (const auto &[name, entry] : dialog_level_.stream) {
    const auto dot = name.rfind('.');
    const auto ext = dot == std::string::npos ? std::string() : lower(name.substr(dot));
    if (ext == ".anm") {
      h.anm_length[name] = entry.second;
      h.anm_entry[name] = {entry.first, std::int64_t(entry.first) + entry.second};
    } else if (ext == ".vag") {
      h.vag_length[name] = entry.second;
    }
  }
  // Camera_setupDialogView 0x203bb8 (docs/spec/dialog-gaps.md 2.2). A speaker without
  // a complete dialogOffset leaves the sideways offset to a stack word nobody
  // initialised (section 10): refused.
  h.view = [this](int side, const dialog::Object &focus, const std::array<Raw, 3> &cam, int yaw, int pitch) {
    dialog_gaps::View view(yaw, pitch);
    std::vector<Vec3> players;
    for (const int slot : dialog_host_.alive) {
      const auto it = dialog_host_.player_obj.find(slot);
      if (it != dialog_host_.player_obj.end() && it->second)
        players.push_back(it->second->pos);
    }
    const auto offset = focus.props.find("dialogoffset");
    const std::optional<std::string> text =
        offset == focus.props.end() ? std::nullopt : std::optional<std::string>(offset->second);
    dialog_gaps::setup_dialog_view(view, focus.pos, s16(focus.facing), side, players, cam, text);
    camera_view_ = view;
    dialog_counts_.camera_views += 1;
    // Message 0's Camera_apply of the carried position (docs/dialog.md 2.4 step 3).
    dialog_camera_apply(cam, yaw, pitch, ZERO, false);
  };
  dialog_.on_send = [this](const dialog::Bytes &msg) { dialog_message(msg); };
  dialog_.on_post_action = [this] {
    dialog_counts_.post_actions += 1;
    if (!machine_)
      return;
    machine_->budget = session::SCRIPT_BUDGET;
    try {
      variables_->run_post_action();
    } catch (const std::exception &error) {
      // The post action's entry stops as any other entry does.
      remember(error);
    }
  };
  // The binder's seams. Outermost, so it answers before bind_campaign's
  // closed-dialog stand-in.
  auto inner = variables_->scene_service;
  variables_->scene_service = [this, inner](const std::string &name,
                                            const std::vector<builtins::Variables::ServiceValue> &args)
      -> std::uint32_t {
    if (name == "dialog_isOpen")
      return dialog_.is_open() ? 1u : 0u;
    if (in_dialog_command_) {
      const auto answered = dialog_scene(name, args);
      if (answered != UINT32_MAX - 1)
        return answered;
    }
    if (inner)
      return inner(name, args);
    throw amx::HostFault("undocumented scene service");
  };
}

// Camera_apply(x, y, z, cam, 0) 0x2035c8 with the conversation camera's words; the pose
// goes to the presenter as a "camera_pose" event (docs/spec/dialog-gaps.md 2.5).
void Host::dialog_camera_apply(const Vec3 &pos, int yaw, int pitch, Raw h, bool dialog_open) {
  auto &env = dialog_camera_env;
  if (lt(ZERO, env.shake) && !env.cosmetic_seed)
    throw dialog::Refused("camera shake with no cosmetic seed supplied: the shake draws it three times");
  const auto a = dialog_gaps::camera_apply(pos, yaw, pitch, env.shake, env.cosmetic_seed ? *env.cosmetic_seed : 0,
                                           env.frozen, dialog_open);
  if (env.cosmetic_seed)
    *env.cosmetic_seed = a.seed;
  env.shake = a.shake;
  camera_pose_ = DialogCameraPose{a.pos, s16(std::uint32_t(yaw)), s16(std::uint32_t(pitch)), h, a.matrix};
  dialog_events_.push_back({"camera_pose",
                            {std::int64_t(a.pos[0]), std::int64_t(a.pos[1]), std::int64_t(a.pos[2]),
                             std::int64_t(camera_pose_->yaw), std::int64_t(camera_pose_->pitch), std::int64_t(h)}});
}

std::string Host::dialog_gate(const std::string &site, dialog_gaps::Conditions conditions) const {
  conditions["dialog"] = dialog_.is_open() ? 1 : 0;
  return dialog_gaps::site_outcome(site, conditions);
}

std::uint32_t Host::dialog_cursor_glyph(std::int32_t layout) const {
  const auto glyph = dialog_gaps::cursor_glyph(layout);
  if (!glyph)
    throw dialog::Refused("cursor glyph: game_initCamera writes 0x41b978 only for layout bytes 0 and 1");
  return *glyph;
}

// Seams the dialog built-ins call (script_builtins.cpp startDialog,
// forceMutexRelease, activateStore). UINT32_MAX - 1: not a dialog seam.
std::uint32_t Host::dialog_scene(const std::string &name,
                                 const std::vector<builtins::Variables::ServiceValue> &args) {
  const auto word = [&](std::size_t i) -> std::uint32_t {
    if (i >= args.size())
      throw amx::HostFault(name + " argument count");
    if (const auto *w = std::get_if<std::uint32_t>(&args[i]))
      return *w;
    throw amx::HostFault(name + " argument is not a word");
  };
  if (name == "menuMutex_release") {
    dialog_.mutex_release();
    return 0;
  }
  if (name == "net_isLocalSlot")
    return dialog_host_.is_local(static_cast<int>(word(0))) ? 1u : 0u;
  // The level lump is resident while its script runs.
  if (name == "lump_load")
    return 0;
  // No original camera runs in this host: Camera_update does nothing and the
  // message carries yaw and pitch 0 and the binder's g_cameraPos words.
  if (name == "Camera_update" || name == "Camera_getYaw" || name == "Camera_getPitch") {
    if (name == "Camera_update")
      dialog_events_.push_back({"camera_update", {}});
    return 0;
  }
  if (name == "net_sendGameMsg") {
    if (args.size() < 2)
      throw amx::HostFault("net_sendGameMsg argument count");
    const auto *packet = std::get_if<std::string>(&args[1]);
    if (!packet)
      throw amx::HostFault("net_sendGameMsg packet");
    dialog_message(dialog::Bytes(packet->begin(), packet->end()));
    return 0;
  }
  if (name == "net_closeLocalMenus") {
    dialog_events_.push_back({"close_local_menus", {std::int64_t(0)}});
    return 0;
  }
  if (name == "shop_openOrStock") {
    // The shop is not part of this host: the request is counted and handed
    // to the presentation.
    dialog_counts_.shop_requests += 1;
    dialog_events_.push_back({"shop", {std::int64_t(word(0)), std::int64_t(0), std::int64_t(1)}});
    return 0;
  }
  return UINT32_MAX - 1;
}

// The words the binder and the machine share are kept in the binder's memory
// image while a script runs and in the machine while the machine runs.
void Host::dialog_push() {
  auto &mem = variables_->memory;
  mem.word(DIALOG_DATA, dialog_.is_open() ? 1u : 0u);
  mem.word(SWOOP, static_cast<std::uint32_t>(dialog_.swoop));
  mem.word(SWOOP_DONE, static_cast<std::uint32_t>(dialog_.swoop_done));
  mem.string(NEXT_WORLD, dialog_.next_world);
  mem.byte(STORE_PENDING, static_cast<std::uint8_t>(dialog_.store_pending));
  mem.word(ACTIVE_PLAYER, static_cast<std::uint32_t>(dialog_.active_player));
}
void Host::dialog_pull() {
  auto &mem = variables_->memory;
  dialog_.swoop = s32(mem.word(SWOOP));
  dialog_.swoop_done = s32(mem.word(SWOOP_DONE));
  dialog_.next_world = mem.cstr(NEXT_WORLD);
  dialog_.store_pending = mem.read(STORE_PENDING);
  dialog_.active_player = s32(mem.word(ACTIVE_PLAYER));
}

// script_runByName from the machine: node and choice conditions, actions,
// and the object's dialogScript.
std::int32_t Host::dialog_run_script(const std::string &name, bool) {
  dialog_counts_.scripts_run += 1;
  if (!machine_)
    return dialog::MISSING_ENTRY;
  dialog_push();
  machine_->budget = session::SCRIPT_BUDGET;
  amx::Address result = -1;
  try {
    result = variables_->run_script(name);
  } catch (...) {
    dialog_pull();
    throw;
  }
  dialog_pull();
  if (result == -1 && machine_->find_public(name).first != amx::ERR_NONE)
    dialog_counts_.missing_entries += 1;
  return static_cast<std::int32_t>(result);
}

template <class F> bool Host::dialog_guard(const char *what, F &&fn) {
  if (dialog_depth_ >= MAX_DIALOG_DEPTH) {
    dialog_counts_.refused["nesting"] += 1;
    return false;
  }
  const auto saved = dialog_;
  ++dialog_depth_;
  try {
    fn();
    --dialog_depth_;
    return true;
  } catch (const std::exception &error) {
    --dialog_depth_;
    // A refused script answer, a refused machine step or a script fault: the
    // machine goes back to where it was before this step (docs/DIALOG.md
    // "Refusals"). Scripts that already ran keep their effects.
    const bool open_before = saved.is_open();
    dialog_ = saved;
    dialog_counts_.refused[std::string(what) + ": " + head(error)] += 1;
    if (!open_before) {
      dialog_.mutex_release();
    } else {
      dialog_stalled_ = true;
      dialog_stall_reason_ = std::string(what) + ": " + head(error);
      dialog_counts_.stalls += 1;
    }
    dialog_push();
    return false;
  }
}

void Host::dialog_drain_log() {
  for (auto &e : dialog_.log) {
    if (e.kind == "stream_play" && e.args.size() >= 2) {
      dialog_counts_.nodes_shown += 1; // dialog_showNode queues one line each time
      const auto *name = std::get_if<std::string>(&e.args[1]);
      if (name && dialog_host_.vag_length.count(*name))
        dialog_counts_.voice_lines += 1;
      else
        dialog_counts_.silent_lines += 1;
    } else if (e.kind == "shop") {
      dialog_counts_.deferred_shops += 1;
    } else if (e.kind == "fade_restart" && !e.args.empty()) {
      // ui_restartFadeIn(i) 0x320ec4: g_uiFadeIn[i] = 0.0 (docs/ui-text-gaps.md 1).
      if (const auto *slot = std::get_if<std::int64_t>(&e.args[0]))
        ui_ramps_.fade[static_cast<std::size_t>(*slot) & 3u] = ui_text_gaps::uiRestartFadeIn();
    }
    if (dialog_events_.size() < 4096)
      dialog_events_.push_back(std::move(e));
  }
  dialog_.log.clear();
}

// One game message for the dialog channel, run at once (offline,
// docs/spec/game-messages.md 1.2).
// The original waits on lump_pollAsync until an .anm read ends (docs/dialog.md
// 4 step 4 and 5.2 type 3). This host reads from memory, so a read issued by
// one machine step is complete by the next one.
void Host::dialog_complete_read() {
  if (!dialog_.anm_ready) {
    dialog_.anm_ready = 1;
    dialog_counts_.anm_reads += 1;
  }
}

void Host::dialog_message(const dialog::Bytes &msg) {
  if (msg.empty())
    return;
  dialog_pull();
  dialog_complete_read();
  const auto kind = msg[0];
  const auto before = dialog_.pending_world;
  dialog_guard(kind == dialog::MSG_OPEN ? "open" : kind == dialog::MSG_CONFIRM ? "confirm" : "message", [&] {
    if (kind == dialog::MSG_OPEN) {
      dialog_counts_.start_messages += 1;
      const auto m = dialog::parse_start_message(msg);
      // world_findObjectsByName(name, ..., 100) of the host's own objects.
      auto &list = dialog_host_.objects[m.name];
      list.clear();
      for (auto *obj : variables_->find(m.name, 100))
        list.push_back(dialog_view(obj->addr, nullptr));
      auto file = dialog_files_.find(m.name);
      if (file == dialog_files_.end()) {
        const auto raw = dialog_level_.files.find(m.name + ".dbin");
        if (raw == dialog_level_.files.end())
          throw dialog::Refused("no " + std::string("<name>.dbin") + " in the level lump");
        file = dialog_files_.emplace(m.name, dialog::parse_dbin(raw->second.data(), raw->second.size())).first;
      }
      if (!dialog_services_.text)
        throw dialog::Refused("no text table");
      // The draw hands word 0x4fe538 straight to ui_drawSprite, which reads the
      // texture's size first: with the word 0 that is address 0 (section 4).
      dialog_gaps::box_sprite_reads(box_word_);
      if (dialog_.on_open(dialog_host_, msg, file->second, dialog_level_.level)) {
        dialog_counts_.opens += 1;
        dialog_stalled_ = false;
      } else {
        dialog_counts_.open_gate_refused += 1;
      }
    } else {
      const bool was_open = dialog_.is_open();
      // on_confirm takes the selected choice exactly when the text is done
      // (docs/dialog.md 5.2 type 1).
      const bool taking = kind == dialog::MSG_CONFIRM && was_open &&
                          !(dialog_.revealing != 0 && dialog_.reveal_done == 0) && dialog_.at_end != 0 &&
                          eq(dialog_.blend, ONE) && !dialog_.visible.empty();
      dialog_.on_message(dialog_host_, msg, nullptr, dialog_level_.level);
      if (taking)
        dialog_counts_.choices_taken += 1;
      if (kind == dialog::MSG_END && was_open)
        dialog_counts_.ends += 1;
    }
  });
  if (dialog_.pending_world && dialog_.pending_world != before) {
    // docs/dialog.md 5.2 type 3 and docs/quests.md 4: the pending level 0x41b6d0
    // with 0xff at 0x41b720; level flow takes the request as the mission menu's.
    variables_->memory.string(PENDING_LEVEL, *dialog_.pending_world);
    variables_->memory.byte(PENDING_ORIENT, 0xff);
    flow_.pending.request(*dialog_.pending_world, level_flow::ORIENT_NONE);
    dialog_counts_.pending_levels += 1;
  }
  dialog_drain_log();
  dialog_push();
}

// The machine's view of one of the host's objects. Unconstructed stubs get
// the flags their documented constructor sets (docs/DIALOG.md "Who can be
// talked to"); everything else of the constructor is not applied.
dialog::Object *Host::dialog_view(std::uint32_t address, const std::array<Raw, 3> *) {
  auto *obj = variables_->obj_at(address);
  if (!obj)
    throw dialog::Refused("no such object");
  auto &slot = dialog_views_[address];
  if (!slot)
    slot = std::make_unique<dialog::Object>();
  auto &v = *slot;
  v.addr = address;
  v.pos = obj->pos;
  v.facing = obj->angle & 0xffffu;
  const auto cls = obj->type_name;
  const bool shown = prop(obj->props, "showName").has_value();
  if (stubs_.count(address)) {
    std::uint64_t flags = 0;
    if (creature_class(cls)) {
      flags |= 0x20; // Creature_ctor 0x109d68
      if (cls == "NPC")
        flags |= 0x1100008; // NPC ctor 0x29df04
      else if (shown)
        flags |= 0x100000; // creatures.md: ShowName at +0x368 makes it interactable
      if (cls == "NPC" && !shown)
        flags &= ~std::uint64_t(0x100000);
    }
    v.flags = flags;
  } else {
    v.flags = obj->flags;
  }
  v.npc = cls == "NPC";
  v.word24 = 0;
  v.props.clear();
  if (obj->props)
    for (const auto &entry : *obj->props) {
      const auto eq = entry.find('=');
      if (eq != std::string::npos)
        v.props.emplace(lower(entry.substr(0, eq)), entry.substr(eq + 1));
    }
  v.display_name = shown ? 1u : 0u;
  return &v;
}

std::vector<std::uint32_t> Host::talkers() const {
  std::vector<std::uint32_t> out;
  for (const auto address : interactables())
    if (prop(variables_->obj_at(address)->props, "dialogScript"))
      out.push_back(address);
  return out;
}

// Every documented-class object with showName answers the use button, also
// without dialogScript: Creature_interact then takes the mutex and runs no
// script (docs/dialog.md 2.2).
std::vector<std::uint32_t> Host::interactables() const {
  std::vector<std::uint32_t> out;
  for (const auto address : stubs_) {
    const auto *obj = variables_->obj_at(address);
    if (obj && creature_class(obj->type_name) && prop(obj->props, "showName"))
      out.push_back(address);
  }
  return out;
}

std::string Host::interact(std::uint32_t address, int slot, const std::array<Raw, 3> &player) {
  if (!dialog_attached_)
    return "not attached";
  dialog_player_.pos = player;
  dialog_player_.flags = 0;
  dialog_host_.player_obj[slot] = &dialog_player_;
  dialog_pull();
  std::string result = "refused";
  dialog_guard("use", [&] {
    auto *view = dialog_view(address, &player);
    // net_anyAlivePlayerFlag800000: no player menu state runs here (0).
    result = dialog_.interact(dialog_host_, *view, slot, false);
  });
  dialog_counts_.interact[result] += 1;
  dialog_drain_log();
  dialog_push();
  return result;
}

std::array<Raw, 3> Host::in_front_of(std::uint32_t address) const {
  const auto *obj = variables_->obj_at(address);
  if (!obj)
    throw dialog::Refused("no such object");
  const ScopedFpu fpu{Fpu{Fpu::Model::Ps2, false, false}};
  const Raw thirty = 0x41f00000; // 30.0
  return {add(obj->pos[0], mul(thirty, cos16(obj->angle & 0xffffu))),
          add(obj->pos[1], mul(thirty, sin16(obj->angle & 0xffffu))), obj->pos[2]};
}

std::string Host::use(int slot, const std::array<Raw, 3> &player) {
  if (!dialog_attached_)
    return "not attached";
  dialog_counts_.uses += 1;
  if (dialog_.is_open()) { // Player_run reaches Player_interact only with no dialog open
    dialog_counts_.use_blocked_open += 1;
    return "dialog open";
  }
  const ScopedFpu fpu{Fpu{Fpu::Model::Ps2, false, false}};
  const objects::Vec3 lo{sub(player[0], BOX_XY), sub(player[1], BOX_XY), player[2]};
  const objects::Vec3 hi{add(player[0], BOX_XY), add(player[1], BOX_XY), add(player[2], BOX_UP)};
  // world_collectObjectsInBox's per-object test (objects_queries.cpp) on the
  // talkers: the host's stubs are not in an objects::World grid.
  std::optional<std::uint32_t> best;
  Raw best_d = 0;
  std::size_t found = 0;
  for (const auto address : interactables()) {
    const auto *obj = variables_->obj_at(address);
    const auto x = obj->pos[0], y = obj->pos[1], z = obj->pos[2], r = obj->radius;
    if (lt(add(x, r), lo[0]) || lt(hi[0], sub(x, r)) || lt(add(y, r), lo[1]) || lt(hi[1], sub(y, r)) ||
        lt(hi[2], z) || lt(add(z, obj->height), lo[2]))
      continue;
    ++found;
    const auto dx = sub(x, player[0]), dy = sub(y, player[1]), dz = sub(z, player[2]);
    const auto d = add(add(mul(dx, dx), mul(dy, dy)), mul(dz, dz));
    if (!best || lt(d, best_d)) {
      best = address;
      best_d = d;
    }
  }
  if (!best) {
    dialog_counts_.use_no_candidate += 1;
    return "no candidate";
  }
  if (found > 1)
    dialog_counts_.use_ambiguous += 1;
  return interact(*best, slot, player);
}

void Host::dialog_tick(const DialogInputs &in) {
  if (!dialog_attached_)
    return;
  if (!dialog_.is_open() && dialog_.cam_move == 0)
    return; // the run task returns at once (docs/dialog.md 5.1 steps 2-3)
  dialog_counts_.ticks_open += dialog_.is_open() ? 1 : 0;
  dialog_pull();
  dialog_complete_read();
  dialog_host_.pause_menu = in.pause_menu;
  dialog_host_.raw_pad = {{0, in.pad}};
  dialog::TickIO io;
  // Camera_lookAt: the port's conversation camera unless the caller owns the camera.
  io.look_at = [this, &in](int frames) -> std::int64_t {
    if (in.camera_arrived)
      return *in.camera_arrived;
    if (!camera_view_)
      throw dialog::Refused("no conversation camera view was set up");
    dialog_counts_.camera_steps += 1;
    const auto look = dialog_gaps::look_at(*camera_view_, frames);
    if (look.apply)
      dialog_camera_apply(*look.apply, camera_view_->yaw, camera_view_->pitch, look.h, true);
    return look.result;
  };
  // The camera's forward z, read once before the move (docs/spec/dialog-gaps.md 2.4).
  io.look = in.look ? *in.look : camera_pose_ ? camera_pose_->matrix[8] : ZERO;
  io.held = {in.voice.held_ready};
  io.queue = {in.voice.queue_count};
  io.position = {in.voice.position};
  io.status = {in.voice.status};
  io.fade = in.fade;
  io.text_option = in.text_option;
  if (dialog_services_.shown) {
    io.measure = [this](std::int64_t y, std::int64_t budget, int bottom) {
      return dialog_services_.shown(dialog_.text, y, budget, bottom);
    };
  }
  dialog_guard("tick", [&] {
    dialog_.tick(dialog_host_, io);
    // The run task calls dialog_startTalkAnim and then dialog_syncMouth where
    // the reference logs them (docs/dialog.md 5.1 step 6); neither touches a
    // word the rest of the run reads, so they run here, in that order.
    bool talk = false, sync = false;
    for (const auto &e : dialog_.log) {
      talk |= e.kind == "talk_anim";
      sync |= e.kind == "sync_mouth";
    }
    if (talk) {
      const auto before = dialog_.log.size();
      dialog_.talk_anim(dialog_host_);
      bool line = false, generic = false;
      for (auto i = before; i < dialog_.log.size(); ++i) {
        line |= dialog_.log[i].kind == "anm_relocate";
        generic |= dialog_.log[i].kind == "anim_start";
      }
      dialog_counts_.mouth[line ? "lip-sync" : generic ? "talk role" : "none"] += 1;
    }
    if (sync)
      dialog_.sync_mouth(dialog_host_, [&in] { return in.voice.position; });
  });
  dialog_drain_log();
  dialog_push();
}

std::vector<dialog::Event> Host::dialog_draw(Raw fade, int text_option, int glyph, std::int64_t status) {
  if (!dialog_attached_)
    return {};
  std::vector<dialog::Event> ops;
  try {
    // The first draw restarts the fade before it reads it: alpha starts at 0.0.
    if (dialog_.drawn == 0)
      fade = ui_text_gaps::uiRestartFadeIn();
    ops = dialog_.draw(dialog_host_, fade, text_option, glyph, [status] { return status; });
    for (const auto &e : ops)
      if (e.kind == "fade_restart" && !e.args.empty())
        if (const auto *slot = std::get_if<std::int64_t>(&e.args[0]))
          ui_ramps_.fade[static_cast<std::size_t>(*slot) & 3u] = ui_text_gaps::uiRestartFadeIn();
  } catch (const dialog::Refused &error) {
    dialog_counts_.refused[std::string("draw: ") + head(error)] += 1;
  }
  dialog_.log.clear();
  return ops;
}

std::vector<dialog::Event> Host::take_dialog_events() {
  std::vector<dialog::Event> out;
  out.swap(dialog_events_);
  return out;
}

bool Host::dialog_leave() {
  if (!dialog_stalled_ && !dialog_.is_open() && dialog_.mutex) {
    // Offline the inventory's close frees a held menu mutex (0x1c52b0); this
    // session has no inventory, so the client frees it here instead.
    dialog_.mutex_release();
    dialog_counts_.client_mutex_releases += 1;
    dialog_push();
    return true;
  }
  if (!dialog_stalled_)
    return false;
  dialog_.free();
  dialog_.hud_suppressed = 0;
  dialog_events_.push_back({"unfreeze_all", {}});
  dialog_stalled_ = false;
  dialog_counts_.client_leaves += 1;
  dialog_push();
  return true;
}
} // namespace champions::simulation::rta::script_commands
