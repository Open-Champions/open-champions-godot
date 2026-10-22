#pragma once

// Opt-in level-script host. Documented commands run the existing binder.
// Commands whose callee research executed at t3/research-script 52984db also
// run that binder, and the host answers the call. Every other command is a
// counted refusal. The composition census stays the 9649e6b table.
#include "champions/arena_executable.hpp"
#include "champions/dialog.hpp"
#include "champions/ui_text_gaps.hpp"
#include "champions/dialog_gaps.hpp"
#include "champions/fx_simulation.hpp"
#include "champions/level_factory.hpp"
#include "champions/level_flow.hpp"
#include "champions/native_audio.hpp"
#include "champions/quests.hpp"
#include "champions/quests_flow.hpp"
#include "champions/script_builtins.hpp"

#include "champions/script_effects.hpp"

#include "champions/script_items.hpp"

#include "champions/strings.hpp"
#include <array>
#include <deque>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace champions::simulation::rta::script_commands {
namespace builtins = champions::simulation::rta::builtins;
inline constexpr const char *POLICY = "rta-script-commands-v2";
inline constexpr std::size_t COMMANDS = 191;

enum class Effect : std::uint8_t { Documented, Partial, Unknown };
// How getRand treats load draws the host did not make (research
// t3/research-script-session 6841b8a, docs/script-session.md section 1).
// Exact: refused once an adopted record's construction was not run, because
// the original draws from the shared seed while it constructs. OfflineSession:
// the session's generator stands in for the offline session's clock seed
// (COP0 Count, 0x322a04), so its draws are answered; the records whose draws
// the host did not make are counted in load_draws_unmodelled.
enum class SeedPolicy : std::uint8_t { Exact, OfflineSession };
// Refused: a refused command's answer was used, so the entry stopped there.
enum class Stop : std::uint8_t { Missing, Complete, Wait, Fault, Refused };

struct Row {
  int index;
  const char *name;
  Effect effect;
  const char *subsystem;
  const char *native;
  const char *seam;
};
const Row &row(std::size_t index);

struct Counts {
  std::uint64_t started = 0;
  std::uint64_t reached_wait = 0;
  std::uint64_t reached_complete = 0;
  std::uint64_t faults = 0;
  std::uint64_t missing_entries = 0;
  // Entries stopped because the script used a refused command's answer.
  std::uint64_t refusal_stops = 0;
  std::uint64_t objects_registered = 0;
  std::uint64_t objects_refused = 0;
  // With an executable (set_executable), each record goes through the object
  // factory (level_factory): built by a ported constructor chain, unbuilt (the
  // factory builds an object whose constructor is not ported here, or whose
  // construction takes a path that is not; a stub), not created (the factory
  // builds no object; nothing is registered), or refused by the pool.
  std::uint64_t objects_built = 0;
  std::uint64_t objects_unbuilt = 0;
  std::uint64_t objects_not_created = 0;
  std::uint64_t objects_pool_refused = 0;
  // Without an executable every record is a stub, as before.
  std::uint64_t objects_unclassified = 0;
  // Records the original constructs whose shared-seed draws the host did not
  // make (each draws at least once, in GameObject_ctor).
  std::uint64_t load_draws_unmodelled = 0;
  // Unbuilt records by constructor address, and build refusals by reason.
  std::map<std::uint32_t, std::uint64_t> unbuilt_constructors;
  std::map<std::string, std::uint64_t> build_refusals;
  // Documented commands refused because their binder path would read an
  // unconstructed stub's creature flag, hit points or type name, or a built
  // object's field no ported code wrote (hit points). Each is also in
  // refused[].
  std::uint64_t stub_refusals = 0;
  // Routed commands refused because, for this state, they would reach a
  // callee research did not execute or a case the port refuses: a charm
  // target word the host's handle pool does not hold, a music volume or a
  // fade level nobody supplied, and for the five text commands no string
  // table, no help chime, no camera, a text over its buffer, a format that
  // reads past its terminator or a compass object that is missing. Each is
  // also in refused[].
  std::uint64_t callee_refusals = 0;
  // Refused commands that would read or overwrite an unknown created-player
  // current HP/mana field. Each also appears in refused[].
  std::uint64_t unknown_player_current_refusals = 0;
  // Text-command refusals by reason (a subset of callee_refusals).
  std::map<std::string, std::uint64_t> text_refusals;
  std::array<std::uint64_t, COMMANDS> executed{};
  std::array<std::uint64_t, COMMANDS> refused{};

  // Executions whose route runs an inferred body: setCFlag and
  // selectSetCFlag with bit 0, which reach creature_startBossMusic (the
  // boss-music stop and queue, research-audio 6b08886). Each is also in
  // executed[].
  std::array<std::uint64_t, COMMANDS> inferred{};

  // Refusals whose answer the script used, so the entry stopped (refusal_stops
  // by command).
  std::array<std::uint64_t, COMMANDS> stopped{};

};

// One particle_spawn the binder asked for. Words are the binder's, unscaled
// here a second time.
struct Spawn {
  std::uint32_t definition = 0;
  std::array<std::uint32_t, 3> pos{};
  std::array<std::uint32_t, 3> vel{};
  std::uint32_t backdate = 0;
};

struct Queue {
  std::uint32_t count = 0;
  std::uint32_t queued = 0;
  std::uint32_t flip = 0;
};

// Thrown out of a refused command whose answer the script uses; the host
// counts it in refusal_stops and stops the entry.
class RefusedResult : public amx::HostFault {
public:
  using amx::HostFault::HostFault;
};

// The owner's executable as the object factory reads it. The view shares it.
level_factory::Executable executable_view(std::shared_ptr<const arena_modes::Executable>);

// NPC dialog wiring (docs/DIALOG.md). attach_dialog opts the host in: the
// dialog built-ins then run against a champions-dialog machine instead of
// being refused. Without it the host behaves exactly as before.
struct DialogLevel {
  // The level name: message 0 loads "<level>.lmp" and finds "<name>.dbin" in it.
  std::string level;
  // Lump entries ending in .dbin, under their exact names.
  std::map<std::string, std::vector<std::uint8_t>> files;
  // The level stream directory (ntsc_dialog.bin): entry name -> offset, length.
  std::map<std::string, std::pair<std::uint32_t, std::uint32_t>> stream;
  // The box (docs/spec/dialog-gaps.md 4). dialog_loadBox 0x22f910 runs at level load and
  // writes the word 0x4fe538 only outside "intro" and outside arena mode B
  // (g_pvpModeB 0x4f7120), from res_find("dlgbox.lmp", "box.tex"): box_available is
  // whether that lookup finds it. previous_box is whether an earlier level's load
  // left a box.tex address in the word (the word is initialised to 0). A dialog
  // that opens with the word 0 is refused: the original's draw reads address 0.
  bool box_available = true;
  bool pvp_mode_b = false;
  bool previous_box = false;
};
struct DialogServices {
  // text_lookup. Required: a conversation is refused without it.
  std::function<dialog::Wide(const std::string &)> text;
  // text_drawWrapped(56, 0, text, 524, 0, 0, 19) of dialog_showNode: the height.
  std::function<std::int64_t(const dialog::Wide &)> layout_height;
  // The run task's measuring pass (docs/dialog.md 5.1 step 9): the characters
  // text_drawWrapped(56, y, text, 524, 295, bottom, 19) shows with this budget
  // (0x44e028). budget < 0 is the unlimited budget the draw pass leaves.
  std::function<std::int64_t(const dialog::Wide &, std::int64_t y, std::int64_t budget, int bottom)> shown;
};
// What dialog_runTask reads from the voice stream. The default answers are
// those of a line with no audio entry (docs/dialog.md 4 step 4): the held
// entry reports ready, nothing is queued or playing.
struct DialogVoice {
  std::int64_t held_ready = 1, queue_count = 0, position = 0, status = 0;
};
struct DialogInputs {
  // The raw pad word of the reading slot, pressed this tick (0x4028dc).
  std::uint32_t pad = 0;
  Raw fade = ONE;          // g_uiFadeIn[0]
  int text_option = 1;     // 0x4fdcc8, options file default 1
  bool pause_menu = false; // task "pauseMenu"
  DialogVoice voice;
  // Set: Camera_lookAt's answer from outside (1 = arrived), replacing the port's own
  // conversation camera (a test or a presenter that owns the camera).
  std::optional<std::int64_t> camera_arrived;
  // Set: the float at 0x44f130 (the camera's forward z) from outside; otherwise it is
  // taken from the last matrix the conversation camera built (docs/DIALOG.md "Camera").
  std::optional<Raw> look;
};
// The conversation camera as Camera_apply last left it (docs/spec/dialog-gaps.md 2.5).
struct DialogCameraPose {
  std::array<Raw, 3> pos{};
  std::int32_t yaw = 0, pitch = 0;
  Raw h = ZERO;                 // the smoothstep of the last swoop step
  std::array<Raw, 12> matrix{}; // the matrix passed to camera_setView
};
// What Camera_apply reads besides the pose: g_cameraShake 0x4f8488, the cosmetic seed
// 0x404460 (the shake draws three words from it; null with a shake above 0 is refused)
// and game_isFrozen() (the shake runs while frozen only with a dialog open).
struct DialogCameraEnv {
  Raw shake = ZERO;
  std::uint32_t *cosmetic_seed = nullptr;
  bool frozen = false;
};
struct DialogCounts {
  std::uint64_t uses = 0, use_no_candidate = 0, use_ambiguous = 0, use_blocked_open = 0;
  std::map<std::string, std::uint64_t> interact; // Creature_interact results
  std::uint64_t start_messages = 0, opens = 0, open_gate_refused = 0;
  std::map<std::string, std::uint64_t> refused; // refusals by reason
  std::uint64_t nodes_shown = 0, choices_taken = 0, scripts_run = 0;
  std::uint64_t missing_entries = 0, ends = 0, pending_levels = 0, post_actions = 0;
  std::uint64_t deferred_shops = 0, shop_requests = 0;
  std::uint64_t stalls = 0, client_leaves = 0, ticks_open = 0, voice_lines = 0, silent_lines = 0;
  std::uint64_t anm_reads = 0, client_mutex_releases = 0;
  // Camera steps (Camera_lookAt calls) taken by the conversation camera, and
  // talker records (dialogScript) the object factory makes no object for.
  std::uint64_t camera_steps = 0, camera_views = 0, no_object_talker_records = 0;
  std::map<std::string, std::uint64_t> mouth; // dialog_startTalkAnim paths
};

// One level script. Not copied: its command closures capture this.
class Host {
public:
  Host();
  Host(const Host &) = delete;
  Host &operator=(const Host &) = delete;
  Host(Host &&) = delete;
  Host &operator=(Host &&) = delete;
  ~Host();

  // False when the image is empty, over the play session's script byte limit,
  // fails to load, or declares a native outside the 191-command table.
  bool load(const amx::ByteVector &);
  // objects.ob records, registered as unconstructed stubs: flags 0, the
  // record's position, angle and properties (a name= property is the lookup
  // name), and the factory class string as the stub's type string. That string
  // is not the C++ type-info name and no constructor has run, so a documented
  // command that would read a stub's creature flag, hit points or type name is
  // refused (stub_refusals). A record without a class string is refused.
  bool adopt_objects(const std::uint8_t *data, std::size_t size);
  // A constructed object with known flags, for synthetic tests. The type
  // string is ref_natives.GameState.new_object's default.
  void add_object(const std::string &name, std::uint32_t flags,
                  const std::array<std::uint32_t, 3> &pos, std::uint32_t angle);

  // The play session's shared generator. Without one getRand is refused.
  // Built objects draw from it in GameObject_ctor; without it nothing is built.
  // Exact: getRand is also refused once an adopted record was not constructed
  // here. OfflineSession: such records are counted instead (SeedPolicy).
  void bind_seed(SharedRandom *seed, SeedPolicy policy = SeedPolicy::Exact);
  // A single-player game (docs/script-session.md section 2): g_numPlayers
  // 0x4d8ae0 = 1 and g_difficultyIdx 0x4f719c = 1, the executable image values
  // and the offline setup's stores. Call before run_prestart.
  void start_single_player();
  // The owner's executable, for the object factory's strings and the class
  // table, and the image under the ported constructors. Without it
  // adopt_objects registers every record as a stub.
  void set_executable(level_factory::Executable);
  bool is_built(std::uint32_t address) const { return built_.count(address) != 0; }
  // Override the audio model's documented 0.5 default with session options.
  void set_music_volume(std::uint32_t bits);
  // The ELF image starts this presentation word at zero. Replacement hosts
  // receive the previous level's value; the loader has no direct reset.
  void set_fade_level(std::uint32_t level);
  std::uint32_t fade_level() const;
  // The string table (docs/STRINGS.md): every .uni entry of GTEXT.LMP, then
  // those of the level lump, in load order. Each is registered at the next
  // free base. False for a malformed stream. Without a table the five text
  // commands are refused.
  bool register_text(const std::vector<std::uint8_t> &uni);
  bool has_text() const { return text_->entries() != 0; }
  // The visible units a key would show, without advancing the miss ring.
  strings::Units text_peek(const std::string &key) const { return text_->peek(key); }
  strings::TextTable &text() { return *text_; }
  const strings::TextTable &text() const { return *text_; }
  // The help chime: byte +0x3c (sound group) of fx.lmp/butn_message.anm from
  // the game folder. Without it addHelpMessage and addHelpMessageF are
  // refused: hud_showHelpMessage plays it through sound_playOnObject.
  void set_help_chime(std::uint8_t group);
  // The HUD state the five commands write, and its gate inputs. camera_mode
  // starts empty (no camera installed): showBust and showCompass are refused
  // until the caller supplies the local camera mode.
  strings::Hud &hud() { return *hud_; }
  const strings::Hud &hud() const { return *hud_; }
  strings::SoundSeqs &sound() { return *sound_; }
  // ui_startPulse(0) calls made by hud_showHelpMessage.
  std::uint64_t ui_pulse_starts() const { return ui_pulse_starts_; }
  // g_uiPulse / g_uiFadeIn (docs/ui-text-gaps.md 1). The fade starts at 1.0: its
  // data value is not documented, and a dialog's own restart sets it to 0.0.
  // ui_frame_step is session step 13: once per displayed frame, after the draw.
  const ui_text_gaps::Ramps &ui_ramps() const { return ui_ramps_; }
  void ui_frame_step() { ui_ramps_ = ui_text_gaps::rampsStep(ui_ramps_); }
  // help_task 0x1dcc78 for one displayed frame. A close runs the post action
  // offline (script_runPostAction), within the play session's budget.
  strings::HelpFrame help_task(bool confirm);
  // Links a constructed creature's target handle (+0x234) to another object
  // through the host's handle pool, as the AI's handle_link does. For tests and
  // for callers that drive creature targets; returns the node word stored.
  std::uint32_t link_target(std::uint32_t creature, std::uint32_t target);


  // Level entry in the loader's order (research docs/script.md 2.1):
  // run_prestart, then adopt_objects, then run_start. run_start runs start,
  // then one post action (0x1e60b8), then the deferred queue once outside any
  // tick (script_runDeferred, 0x322e6c), so a name start queued runs on the
  // first ready tick. A queued deferred name is a wait.
  Stop run_prestart();
  Stop run_start();
  // One simulated ready tick: the Script task runs the front half, then
  // promotes the back. It runs with an empty queue too.
  void on_ready_tick();
  // One named public. Does not by itself mark the level as started.
  Stop run(const std::string &entry);

  const Counts &counts() const { return counts_; }
  const std::vector<Spawn> &spawns() const { return spawns_; }
  level_flow::State &level() { return flow_; }
  const level_flow::State &level() const { return flow_; }
  champions::rta::fx::sim::EffectWorld &effects() { return *effects_; }
  script_effects::Effects &callees() { return *script_fx_; }
  champions::audio::Playback &audio() { return *audio_; }
  // The f12 music_playLevel passes to channel 2. Zero until a call passes the gate.
  std::uint32_t music_channel() const { return music_channel_; }
  builtins::Variables &variables() { return *variables_; }
  amx::Machine *machine() { return machine_.get(); }
  amx::Cell last_value() const { return last_value_; }
  const std::string &last_fault() const { return last_fault_; }
  bool waiting() const { return waiting_; }
  bool faulted() const { return faulted_; }
  bool stopped_on_refusal() const { return refusal_stopped_; }
  bool is_stub(std::uint32_t address) const { return stubs_.count(address) != 0; }
  Queue queue() const;
  // Deferred name table and its two counters (not the flip), the script's
  // global variables, the quest variables and the post-action name. The
  // measurement tool stops resuming when this key repeats: nothing a later
  // tick reads has changed. It does not prove the wait never ends.
  std::string queue_key() const;
  std::uint32_t public_count() const;
  // The campaign the quest commands write. setCFlag stores creature bits 0 and
  // 26 and counts a boss-music request. activateMissionMenu opens the menu
  // when the recall byte is 0. givePlayerExp runs only after a table is set.
  quests::Campaign &campaign() { return *campaign_; }
  // The item commands' callees (script_items.hpp). The give commands need its
  // item tables and a bound seed; without them they refuse.
  script_items::Service &items() { return *items_; }
  const quests::Campaign &campaign() const { return *campaign_; }
  void set_player_currents_known(bool known) { player_currents_known_ = known; }
  // Level-flow act change; the campaign takes level flow's act and clears
  // when level flow resets the act. Copies the cleared record back into the
  // binder.
  level_flow::ChangeResult change_level(const level_flow::ChangeOptions &);
  // One missionMenu_run tick. State 0 stores 1 in the binder's g_paused word
  // 0x41b4f4. The state-8 tick copies the chosen level into the binder's
  // pending name 0x41b6d0 with 0xff at 0x41b720, and into level flow's
  // pending request with ORIENT_NONE. Returns true on that tick.
  bool tick_mission_menu(int button, const quests::MenuInputs &inputs = {});
  // The campaign image (quests::Campaign::save) after copying the binder's
  // record and quest variables into it. load_campaign replaces the campaign
  // and writes its records and the whole quest-variable table into the
  // binder. Neither is the PS2 card image.
  std::vector<std::uint8_t> save_campaign();
  void load_campaign(const std::vector<std::uint8_t> &);

  // ---- NPC dialog (docs/DIALOG.md) ----
  void attach_dialog(DialogLevel level, DialogServices services);
  bool dialog_attached() const { return dialog_attached_; }
  const dialog::Dialog &dialog() const { return dialog_; }
  const DialogCounts &dialog_counts() const { return dialog_counts_; }
  // Objects this level lets the player talk to: a documented creature class
  // with showName and dialogScript (docs/DIALOG.md "Who can be talked to").
  std::vector<std::uint32_t> talkers() const;
  // What the use button considers: a documented creature class with showName.
  std::vector<std::uint32_t> interactables() const;
  // Player_interact for slot 0's use button at the player's raw position, then
  // Creature_interact on the nearest talker (docs/dialog.md 2.1-2.2). Returns
  // the result word: "no candidate", "dialog open", or Creature_interact's.
  std::string use(int slot, const std::array<Raw, 3> &player);
  // Talk to one talker by address (tests and the census): Creature_interact.
  std::string interact(std::uint32_t address, int slot, const std::array<Raw, 3> &player);
  // A position 30 units in front of a talker along its facing, where
  // Creature_interact's facing test passes (tests, census and the scene's
  // labelled "talk to" diagnostic; not an original routine).
  std::array<Raw, 3> in_front_of(std::uint32_t address) const;
  // dialog_runTask once (priority 110, before the Script task at 125).
  void dialog_tick(const DialogInputs &);
  // dialog_drawTask once: the calls, for the presentation.
  std::vector<dialog::Event> dialog_draw(Raw fade, int text_option, int glyph,
                                         std::int64_t stream_status = 0);
  // Calls outside the dialog state since the last take (voice, camera, shop).
  std::vector<dialog::Event> take_dialog_events();
  // Not in the original: ends a conversation that a refused script stalled
  // (docs/DIALOG.md "Refusals"): runs dialog_free and clears the dialog's
  // freeze and HUD words. With no dialog open it frees a held menu mutex,
  // standing in for the inventory close this session lacks. False when
  // there is nothing to do.
  bool dialog_leave();
  bool dialog_stalled() const { return dialog_stalled_; }
  // The conversation camera: the words Camera_setupDialogView wrote, and the
  // pose of the last Camera_apply (nullptr before the first open).
  const dialog_gaps::View *dialog_camera_view() const { return camera_view_ ? &*camera_view_ : nullptr; }
  const DialogCameraPose *dialog_camera_pose() const { return camera_pose_ ? &*camera_pose_ : nullptr; }
  DialogCameraEnv dialog_camera_env;
  // The outcome of one dialog_isOpen site (docs/spec/dialog-gaps.md 3) with this
  // host's dialog state: the caller passes the other conditions the site reads.
  std::string dialog_gate(const std::string &site, dialog_gaps::Conditions conditions = {}) const;
  // The cursor and "more" glyph byte 0x41b978 for the options layout byte 0x41b69c.
  std::uint32_t dialog_cursor_glyph(std::int32_t layout = dialog_gaps::LAYOUT_INITIAL) const;
  // The box word 0x4fe538 this level's load left (0 = none).
  std::uint32_t dialog_box_word() const { return box_word_; }
  const std::string &dialog_stall_reason() const { return dialog_stall_reason_; }

private:
  void dialog_push();
  void dialog_pull();
  std::uint32_t dialog_scene(const std::string &, const std::vector<builtins::Variables::ServiceValue> &);
  std::int32_t dialog_run_script(const std::string &name, bool condition);
  void dialog_message(const dialog::Bytes &msg);
  dialog::Object *dialog_view(std::uint32_t address, const std::array<Raw, 3> *player);
  template <class F> bool dialog_guard(const char *what, F &&fn);
  void dialog_drain_log();
  void dialog_camera_apply(const Vec3 &pos, int yaw, int pitch, Raw h, bool dialog_open);
  void dialog_complete_read();

  void install();
  void bind_campaign();
  void mirror();
  amx::NativeMap command_map();
  void sync_seed_in();
  void sync_seed_out();
  void apply_entrance();
  void note_fault();
  void remember(const std::exception &);
  void note_queue();
  Stop classify();
  Stop enter(const std::string &name, bool optional);
  Stop enter_level(const char *name);
  bool seed_refused() const;
  bool touches_stub(std::size_t index, amx::Machine &machine) const;
  bool adopt_classified(const std::vector<level_flow::Record> &);
  enum class Build : std::uint8_t { NotBuilt, Built, Stubbed };
  Build build(const level_flow::Record &, const level_factory::Outcome &,
              std::uint32_t address, const std::vector<std::string> &props);
  void stub(const level_flow::Record &, std::uint32_t address,
            std::vector<std::string> props, std::int64_t size);
  void refuse(std::size_t index, amx::Machine &machine);
  Stop stopped() const;
  std::uint32_t scene(const std::string &name,
                      const std::vector<builtins::Variables::ServiceValue> &args);
  std::optional<std::vector<const objects::GameObject *>> named(amx::Machine &machine,
                                                               std::int64_t limit) const;
  std::string boss_name(const objects::GameObject &obj) const;
  bool callee_unknown(std::size_t index, amx::Machine &machine);
  std::optional<std::string> text_refusal(const std::string &name, amx::Machine &machine) const;
  void refresh_hud_inputs();
  void mirror_hud();
  Cell *target_cell(std::uint32_t creature, bool create);

  std::unique_ptr<builtins::Variables> variables_;
  std::unique_ptr<quests::Campaign> campaign_;
  std::unique_ptr<script_items::Service> items_;
  std::unique_ptr<champions::rta::fx::sim::EffectWorld> effects_;
  std::unique_ptr<champions::audio::Playback> audio_;
  std::unique_ptr<script_effects::Effects> script_fx_;
  std::unique_ptr<strings::TextTable> text_;
  std::unique_ptr<strings::Hud> hud_;
  std::unique_ptr<strings::Commands> text_commands_;
  std::unique_ptr<strings::SoundSeqs> sound_;
  // wstr_format results waiting for their wstr_copy, in call order.
  std::deque<strings::Units> formatted_;
  std::optional<std::uint8_t> chime_group_;
  std::uint64_t ui_pulse_starts_ = 0;
  ui_text_gaps::Ramps ui_ramps_;
  // Host-owned handle cells for creature +0x234 words linked by link_target.
  std::map<std::uint32_t, std::unique_ptr<Cell>> target_cells_;
  std::uint32_t music_channel_ = 0;
  // startMusic titles by host handle. Never written into binder memory.
  std::map<std::string, std::uint32_t> music_titles_;
  level_flow::State flow_;
  SharedRandom *external_seed_ = nullptr;
  SeedPolicy seed_policy_ = SeedPolicy::Exact;
  std::unique_ptr<level_factory::Executable> exe_;
  // Addresses of adopted objects.ob records: unconstructed stubs, and records
  // a ported constructor chain built (their hit points are not known).
  std::set<std::uint32_t> stubs_;
  std::set<std::uint32_t> built_;
  Counts counts_;
  bool player_currents_known_ = true;
  std::vector<Spawn> spawns_;
  bool level_started_ = false;
  bool waiting_ = false;
  bool faulted_ = false;
  bool refusal_stopped_ = false;
  bool counted_wait_ = false;
  bool counted_complete_ = false;
  amx::Cell last_value_ = 0;
  std::string last_fault_;
  bool dialog_attached_ = false;
  std::optional<dialog_gaps::View> camera_view_;
  std::optional<DialogCameraPose> camera_pose_;
  std::uint32_t box_word_ = 0;
  DialogLevel dialog_level_;
  DialogServices dialog_services_;
  dialog::Dialog dialog_;
  dialog::Host dialog_host_;
  std::map<std::uint32_t, std::unique_ptr<dialog::Object>> dialog_views_;
  std::map<std::string, std::vector<dialog::Node>> dialog_files_;
  dialog::Object dialog_player_;
  DialogCounts dialog_counts_;
  std::vector<dialog::Event> dialog_events_;
  bool dialog_stalled_ = false;
  std::string dialog_stall_reason_;
  int dialog_depth_ = 0;
  // A dialog built-in is running: the binder's camera, message, shop and menu
  // seams it calls are answered by the dialog wiring.
  bool in_dialog_command_ = false;
  // Declared last so the machine, whose natives capture this, dies first.
  std::shared_ptr<amx::Machine> machine_;
};
} // namespace champions::simulation::rta::script_commands
