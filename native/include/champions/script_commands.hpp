#pragma once
// Opt-in level-script host. Documented commands run the existing binder.
// Commands whose callee research executed at t3/research-script 52984db also
// run that binder, and the host answers the call. Every other command is a
// counted refusal. The composition census stays the 9649e6b table.
#include "champions/fx_simulation.hpp"
#include "champions/level_flow.hpp"
#include "champions/native_audio.hpp"
#include "champions/quests.hpp"
#include "champions/quests_flow.hpp"
#include "champions/script_builtins.hpp"
#include "champions/script_effects.hpp"
#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace champions::simulation::rta::script_commands {
namespace builtins = champions::simulation::rta::builtins;
inline constexpr const char *POLICY = "rta-script-commands-v2";
inline constexpr std::size_t COMMANDS = 191;

enum class Effect : std::uint8_t { Documented, Partial, Unknown };
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
  // Documented commands refused because their binder path would read an
  // unconstructed stub's creature flag, hit points or type name. Each is also
  // in refused[].
  std::uint64_t stub_refusals = 0;
  std::array<std::uint64_t, COMMANDS> executed{};
  std::array<std::uint64_t, COMMANDS> refused{};
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
  // The play session's shared generator. Without one getRand is refused. It
  // is also refused once an objects.ob record is adopted: records are not
  // constructed, and construction draws from the shared seed.
  void bind_seed(SharedRandom *seed);

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
  // Deferred name table plus the two counters, without the flip. A repeated
  // key is a polling loop, not a new chain of entry points.
  std::string queue_key() const;
  std::uint32_t public_count() const;
  // The campaign the quest commands write. setCFlag stores creature bits 0 and
  // 26 and counts a boss-music request. activateMissionMenu opens the menu
  // when the recall byte is 0. givePlayerExp runs only after a table is set.
  quests::Campaign &campaign() { return *campaign_; }
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
  void refuse(std::size_t index, amx::Machine &machine);
  Stop stopped() const;
  std::uint32_t scene(const std::string &name,
                      const std::vector<builtins::Variables::ServiceValue> &args);
  std::uint32_t intern_bytes(const std::string &text);
  std::uint32_t intern_wide(const std::u16string &text);
  void note_type(builtins::Obj &obj, const std::string &type);

  std::unique_ptr<builtins::Variables> variables_;
  std::unique_ptr<quests::Campaign> campaign_;
  std::unique_ptr<champions::rta::fx::sim::EffectWorld> effects_;
  std::unique_ptr<champions::audio::Playback> audio_;
  std::unique_ptr<script_effects::Effects> script_fx_;
  std::uint32_t music_channel_ = 0;
  std::uint32_t scratch_ = 0x700000;
  std::map<std::string, std::uint32_t> type_at_;
  level_flow::State flow_;
  SharedRandom *external_seed_ = nullptr;
  // Addresses of adopted objects.ob records: unconstructed stubs.
  std::set<std::uint32_t> stubs_;
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
