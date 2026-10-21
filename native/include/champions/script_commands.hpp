#pragma once
// Opt-in level-script host. Documented commands run the existing binder and,
// where that binder's whole effect is a call into a subsystem that already
// exists, the host routes that call. Every other command is a counted refusal.
// Research: t3/research-script 9649e6b, tools/ref_script_compose.py.
#include "champions/fx_simulation.hpp"
#include "champions/level_flow.hpp"
#include "champions/script_builtins.hpp"
#include <array>
#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace champions::simulation::rta::script_commands {
namespace builtins = champions::simulation::rta::builtins;
inline constexpr const char *POLICY = "rta-script-commands-v1";
inline constexpr std::size_t COMMANDS = 191;

enum class Effect : std::uint8_t { Documented, Partial, Unknown };
enum class Stop : std::uint8_t { Missing, Complete, Wait, Fault };

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
  builtins::Variables &variables() { return *variables_; }
  amx::Machine *machine() { return machine_.get(); }
  amx::Cell last_value() const { return last_value_; }
  const std::string &last_fault() const { return last_fault_; }
  bool waiting() const { return waiting_; }
  bool faulted() const { return faulted_; }
  bool is_stub(std::uint32_t address) const { return stubs_.count(address) != 0; }
  Queue queue() const;
  // Deferred name table plus the two counters, without the flip. A repeated
  // key is a polling loop, not a new chain of entry points.
  std::string queue_key() const;
  std::uint32_t public_count() const;

private:
  void install();
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

  std::unique_ptr<builtins::Variables> variables_;
  std::unique_ptr<champions::rta::fx::sim::EffectWorld> effects_;
  level_flow::State flow_;
  SharedRandom *external_seed_ = nullptr;
  // Addresses of adopted objects.ob records: unconstructed stubs.
  std::set<std::uint32_t> stubs_;
  Counts counts_;
  std::vector<Spawn> spawns_;
  bool level_started_ = false;
  bool waiting_ = false;
  bool faulted_ = false;
  bool counted_wait_ = false;
  bool counted_complete_ = false;
  amx::Cell last_value_ = 0;
  std::string last_fault_;
  // Declared last so the machine, whose natives capture this, dies first.
  std::shared_ptr<amx::Machine> machine_;
};
} // namespace champions::simulation::rta::script_commands
