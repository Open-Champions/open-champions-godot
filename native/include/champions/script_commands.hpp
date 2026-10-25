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
#include "champions/world_queries.hpp"
#include "champions/level_flow.hpp"
#include "champions/native_audio.hpp"
#include "champions/net_launch.hpp"
#include "champions/quests.hpp"
#include "champions/quests_flow.hpp"
#include "champions/script_builtins.hpp"

#include "champions/script_effects.hpp"

#include "champions/script_items.hpp"
#include "champions/resource_manager.hpp"

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

// ScriptGenerator's constructor (research items-gaps ecf9742 docs/spec/hide-monster.md 4.1).
inline constexpr std::uint32_t SCRIPT_GENERATOR_CTOR = 0x265f54;

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
  // An eligible non-front-end load without an observed hud.lmp residency
  // result cannot apply the 223 item draws. Count the gate and potential
  // draws separately from constructor draws actually missed.
  std::uint64_t item_load_gate_refusals = 0;
  std::uint64_t item_load_draws_inferred = 0;
  // Unbuilt records by constructor address, and build refusals by reason.
  std::map<std::uint32_t, std::uint64_t> unbuilt_constructors;
  std::map<std::string, std::uint64_t> build_refusals;
  // Documented commands refused because their binder path would read an
  // unconstructed stub's creature flag, hit points or type name, or a built
  // object's field no ported code wrote (hit points). Each is also in
  // refused[].
  std::uint64_t stub_refusals = 0;
  // Why: "command: reason" (an undecided factory row by constructor, a class
  // the factory made no row for, or an unresolved name argument).
  std::map<std::string, std::uint64_t> stub_refusal_reasons;
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
  explicit Host(std::shared_ptr<WorldAuthority>);
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
  // net_launchUpdate's end of launch (docs/spec/items-gaps.md 2.3): record 0, the
  // host's, sets g_difficultyIdx, the kill limit and the PvP mode words on every
  // machine, and in modes 2 and 3 game_set4f7110(+0xf4). Nothing in the play
  // session launches a net game yet; this is the contract's receiving half.
  void receive_launch(const net_launch::Record &record0);
  // givePlayerRandom remains refused: its stack-record contents after ten failed
  // generations are Unknown (item-drops.md 6). The level-name global at
  // 0x41b721 has no pinned capacity, so bind_level_name also refuses.
  void bind_monster_level(std::uint32_t raw_float);
  void bind_level_name(const std::string &name);
  // The owner's executable, for the object factory's strings and the class
  // table, and the image under the ported constructors. Without it
  // adopt_objects registers every record as a stub.
  void set_executable(level_factory::Executable);
  // The selected level and its monsters.bal payload, read from the owner's
  // game folder at run time. The executable supplies the level table. Until
  // both are present, a constructed creature's statistics remain unknown.
  void set_level_stats(const std::string &level_name, const std::uint8_t *balance,
                       std::size_t size);
  // The selected level's WORLD payload supplies the terrain queries used by
  // Player1_ctor's four exact free-spot searches.
  void set_level_world(const std::uint8_t *world, std::size_t size);
  // Read-only bytes from the caller's selected disc, installed before object
  // adoption. Resource names are read from the executable definition table.
  void load_resource_lmp(const std::string &name, champions::Bytes image) {
    resources_.load_lmp(name, image);
  }
  // The level-entry inputs of Player1_ctor's marker ranking (research
  // ref_objects.py player1_marker: prev_level, entrance, gate_travel, and the
  // start rank 3 after a load from a save, docs/issues/arena-entry.md). Nothing
  // in this host derives them, so Player1 placement is refused until the
  // caller supplies them here. The caller owns tracing them to the original's
  // sources (the previous level's qualified name, the entrance name, the
  // gate-travel byte 0x41b51e, the load-from-save state).
  struct Player1Entry {
    std::string previous_level, entrance;
    bool gate_travel = false, loaded_from_save = false;
  };
  void set_player1_entry(Player1Entry entry) { player1_entry_ = std::move(entry); }
  // A class port finishing a factory creature (campaign_actors WarBoar and
  // WarBoarSmall). When set, a boar-family record runs the executed base
  // Creature_ctor (arena model) over the shared memory first, with the
  // selected level's balance row and monster level, and then the builder runs
  // the class constructor. The builder throws (its what() is the refusal
  // label) to leave the record a labelled stub. Without a builder these
  // records stay unbuilt, as before.
  struct CreatureRequest {
    std::string class_name;
    bool small = false;
    std::uint32_t address = 0, angle = 0;
    objects::GameObject *object = nullptr;
  };
  using CreatureBuilder = std::function<void(const CreatureRequest &)>;
  void set_creature_builder(CreatureBuilder builder) { creature_builder_ = std::move(builder); }
  champions::resources::Manager &resources() { return resources_; }
  // Keeps a class port's runtime alive exactly as long as this host (and
  // destroys it before the host's objects).
  void retain(std::shared_ptr<void> owner) { keepalive_.push_back(std::move(owner)); }
  // Execute the observed intro/hud.lmp gate. A missing residency result
  // refuses seed advancement and counts the potentially omitted 223 draws.
  void advance_first_level_items(const std::string &level_name,
                                 std::optional<bool> hud_lmp_resident = std::nullopt);
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
  // Object-driven script starter (Trigger_run / obj_runActionScripts): sets
  // the selected object and active player globals around a named public.
  // The index is the original objects.ob record index; a record that made no
  // object refuses instead of substituting another object's context.
  Stop run_object_entry(std::size_t record_index, const std::string &entry,
                        unsigned active_player = 0);

  const Counts &counts() const { return counts_; }
  // Records a refused optional input (the host carries on without it).
  void count_refusal(const std::string &reason) { counts_.build_refusals[reason] += 1; }
  const std::vector<Spawn> &spawns() const { return spawns_; }
  // Script trigger(portal, 0) reaches virtual slot 3 on the named object.
  // The target method and visual result are unverified; count the request only.
  std::uint64_t portal_trigger_requests() const { return portal_trigger_requests_; }
  level_flow::State &level() { return flow_; }
  const level_flow::State &level() const { return flow_; }
  champions::rta::fx::sim::EffectWorld &effects() { return *effects_; }
  script_effects::Effects &callees() { return *script_fx_; }
  champions::audio::Playback &audio() { return *audio_; }
  // The f12 music_playLevel passes to channel 2. Zero until a call passes the gate.
  std::uint32_t music_channel() const { return music_channel_; }
  builtins::Variables &variables() { return *variables_; }
  // The two globals game_loadLevel leaves for the loaded level: g_levelMonsterLevel
  // 0x4f714c (level-table byte + 2 on difficulty 1, research-pickups 9911a6b
  // docs/data-tables.md section 5; level_flow::monster_level) and g_difficultyIdx
  // 0x4f719c. Present only once set_level_stats loaded the level's table and
  // while the difficulty word is the 1 the value was computed for (single
  // player, no PvP: the only mode this host models); otherwise empty.
  struct LevelLoadGlobals {
    std::uint32_t monster_level = 0;
    std::int32_t difficulty = 0;
  };
  std::optional<LevelLoadGlobals> level_load_globals() const;
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
  bool touches_stub(std::size_t index, amx::Machine &machine);
  bool adopt_classified(const std::vector<level_flow::Record> &);
  enum class Build : std::uint8_t { NotBuilt, Built, Stubbed };
  Build build(const level_flow::Record &, const level_factory::Outcome &,
              std::uint32_t address, const std::vector<std::string> &props);
  // Mounted and on-foot Horsemen rows (constructor 0x271c70, mounted 1 or 0)
  // and the Nightmare row (0x270968), horsemen-mount.md sections 1-2.
  Build build_mounted(const level_flow::Record &, const level_factory::Outcome &,
                      std::uint32_t address, const std::vector<std::string> &props);
  // True when the last build_mounted made every shared-seed draw its record owes.
  bool last_draws_complete_ = false;
  void stub(const level_flow::Record &, std::uint32_t address,
            std::vector<std::string> props, std::int64_t size);
  // The ScriptGenerator constructor 0x265f54: the base object with the
  // record's property list (research items-gaps ecf9742 docs/spec/hide-monster.md
  // 4.1 and 5). world_createObject for ScriptGenerator_spawn: the factory row,
  // then a ported constructor chain; any other constructor refuses.
  Build build_creature_family(const level_flow::Record &, const level_factory::Outcome &,
                              std::uint32_t address, const std::vector<std::string> &props);
  CreatureBuilder creature_builder_;
  Build build_generator(const level_factory::Outcome &, const level_flow::Record &,
                        std::uint32_t address, const std::vector<std::string> &props);
  std::uint32_t create_object(const std::string &class_name, const std::array<std::uint32_t, 3> &pos,
                              std::int32_t angle, std::uint32_t props_word);
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
  void mark_built(std::uint32_t);

  // Linked target cells must outlive Variables teardown, which unregisters
  // external objects and unlinks their paired handle nodes.
  std::map<std::uint32_t, std::unique_ptr<Cell>> target_cells_;
  std::shared_ptr<WorldAuthority> authority_;
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
  // Rider +0x3c8 and mount +0x3c8 are separate host handle-list heads.
  std::map<std::uint32_t, std::unique_ptr<Cell>> mounted_cells_;
  std::uint32_t music_channel_ = 0;
  // startMusic titles by host handle. Never written into binder memory.
  std::map<std::string, std::uint32_t> music_titles_;
  level_flow::State flow_;
  SharedRandom *external_seed_ = nullptr;
  SeedPolicy seed_policy_ = SeedPolicy::Exact;
  std::unique_ptr<level_factory::Executable> exe_;
  struct BalanceRow {
    std::uint16_t hash = 0;
    std::array<std::uint32_t, 5> factors{};
  };
  std::vector<BalanceRow> balance_rows_;
  std::uint32_t monster_level_ = 0;
  std::string level_name_;
  bool stats_ready_ = false;
  bool seed_gate_observed_ = false;
  std::optional<Player1Entry> player1_entry_;
  std::unique_ptr<world_queries::Level> terrain_level_;
  champions::resources::Manager resources_;
  // Addresses of adopted objects.ob records: unconstructed stubs, and records
  // a ported constructor chain built (their hit points are not known).
  std::set<std::uint32_t> stubs_;
  std::set<std::uint32_t> built_;
  std::set<std::uint32_t> stats_built_;
  // For a stub the factory row decided: whether its class is a Creature, and
  // its constructor. A stub the row did not decide has no entry's creature
  // answer (Unknown) and keeps hideMonster / showMonster refused.
  struct StubInfo {
    level_factory::Creature creature = level_factory::Creature::Unknown;
    std::uint32_t constructor = 0;
    bool load_property_refused = false;
  };
  std::map<std::uint32_t, StubInfo> stub_info_;
  bool monster_level_bound_ = false;
  std::vector<std::uint32_t> record_addresses_;
  Counts counts_;
  bool player_currents_known_ = true;
  std::vector<Spawn> spawns_;
  std::uint64_t portal_trigger_requests_ = 0;
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
  // After variables_: a class port's runtime dies before the host's objects.
  std::vector<std::shared_ptr<void>> keepalive_;
  // Declared last so the machine, whose natives capture this, dies first.
  std::shared_ptr<amx::Machine> machine_;
};
} // namespace champions::simulation::rta::script_commands
