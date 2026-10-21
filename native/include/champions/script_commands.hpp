#pragma once

// Opt-in level-script host. Documented commands run the existing binder.
// Commands whose callee research executed at t3/research-script 52984db also
// run that binder, and the host answers the call. Every other command is a
// counted refusal. The composition census stays the 9649e6b table.
#include "champions/arena_executable.hpp"

#include "champions/fx_simulation.hpp"
#include "champions/level_factory.hpp"
#include "champions/world_queries.hpp"
#include "champions/level_flow.hpp"
#include "champions/native_audio.hpp"
#include "champions/quests.hpp"
#include "champions/quests_flow.hpp"
#include "champions/script_builtins.hpp"

#include "champions/script_effects.hpp"

#include "champions/script_items.hpp"

#include <array>
#include <cstdint>
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
  // Routed commands refused because, for this state, they would reach a
  // callee research did not execute: a live charm target, a music volume or
  // a fade level nobody supplied. Each is also in refused[].
  std::uint64_t callee_refusals = 0;
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
  // The selected level and its monsters.bal payload, read from the owner's
  // game folder at run time. The executable supplies the level table. Until
  // both are present, a constructed creature's statistics remain unknown.
  void set_level_stats(const std::string &level_name, const std::uint8_t *balance,
                       std::size_t size);
  // The selected level's WORLD payload supplies the terrain queries used by
  // Player1_ctor's four exact free-spot searches.
  void set_level_world(const std::uint8_t *world, std::size_t size);
  // Execute the observed intro/hud.lmp gate. A missing residency result
  // refuses seed advancement and counts the potentially omitted 223 draws.
  void advance_first_level_items(const std::string &level_name,
                                 std::optional<bool> hud_lmp_resident = std::nullopt);
  bool is_built(std::uint32_t address) const { return built_.count(address) != 0; }
  // Caller-supplied music volume gates level and boss music.
  void set_music_volume(std::uint32_t bits);


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

private:
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
  bool callee_unknown(std::size_t index, amx::Machine &machine) const;

  std::unique_ptr<builtins::Variables> variables_;
  std::unique_ptr<quests::Campaign> campaign_;
  std::unique_ptr<script_items::Service> items_;
  std::unique_ptr<champions::rta::fx::sim::EffectWorld> effects_;
  std::unique_ptr<champions::audio::Playback> audio_;
  std::unique_ptr<script_effects::Effects> script_fx_;
  std::uint32_t music_channel_ = 0;
  // startMusic titles by host handle. Never written into binder memory.
  std::map<std::string, std::uint32_t> music_titles_;
  bool fade_known_ = false;
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
  std::unique_ptr<world_queries::Level> terrain_level_;
  // Addresses of adopted objects.ob records: unconstructed stubs, and records
  // a ported constructor chain built (their hit points are not known).
  std::set<std::uint32_t> stubs_;
  std::set<std::uint32_t> built_;
  std::set<std::uint32_t> stats_built_;
  Counts counts_;
  std::vector<Spawn> spawns_;
  bool level_started_ = false;
  bool waiting_ = false;
  bool faulted_ = false;
  bool refusal_stopped_ = false;
  bool counted_wait_ = false;
  bool counted_complete_ = false;
  amx::Cell last_value_ = 0;
  std::string last_fault_;
  // Declared last so the machine, whose natives capture this, dies first.
  std::shared_ptr<amx::Machine> machine_;
};
} // namespace champions::simulation::rta::script_commands
