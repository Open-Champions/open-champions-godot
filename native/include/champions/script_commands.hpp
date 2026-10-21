#pragma once
// Opt-in level-script host. Documented commands run the existing binder.
// Commands whose callee research executed at t3/research-script 52984db also
// run that binder, and the host answers the call. Every other command is a
// counted refusal. The composition census stays the 9649e6b table.
#include "champions/fx_simulation.hpp"
#include "champions/level_flow.hpp"
#include "champions/native_audio.hpp"
#include "champions/script_builtins.hpp"
#include "champions/script_effects.hpp"
#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace champions::simulation::rta::script_commands {
namespace builtins = champions::simulation::rta::builtins;
inline constexpr const char *POLICY = "rta-script-commands-v2";
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
  // objects.ob records. A name= property is the lookup name the binder already
  // registers. The class string is the object type only.
  bool adopt_objects(const std::uint8_t *data, std::size_t size);
  void add_object(const std::string &name, std::uint32_t flags,
                  const std::array<std::uint32_t, 3> &pos, std::uint32_t angle);
  // Null uses the binder world's shared generator. The combat fixture's seed
  // word is a different object and is not rewritten.
  void bind_seed(SharedRandom *seed);

  // preStart if present, then start if present, then one post action.
  // A queued deferred name is a wait. The deferred body runs on a later tick.
  Stop begin_level();
  // One ready tick: the binder runs the front half, then promotes the back.
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
  SharedRandom &seed();
  std::uint32_t scene(const std::string &name,
                      const std::vector<builtins::Variables::ServiceValue> &args);
  std::uint32_t intern_bytes(const std::string &text);
  std::uint32_t intern_wide(const std::u16string &text);
  void note_type(builtins::Obj &obj, const std::string &type);

  std::unique_ptr<builtins::Variables> variables_;
  std::unique_ptr<champions::rta::fx::sim::EffectWorld> effects_;
  std::unique_ptr<champions::audio::Playback> audio_;
  std::unique_ptr<script_effects::Effects> script_fx_;
  std::uint32_t music_channel_ = 0;
  std::uint32_t scratch_ = 0x700000;
  std::map<std::string, std::uint32_t> type_at_;
  level_flow::State flow_;
  SharedRandom *external_seed_ = nullptr;
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
