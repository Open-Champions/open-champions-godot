#pragma once
#include "champions/arena_executable.hpp"
#include "champions/campaign_actors.hpp"
#include "champions/world_authority.hpp"
#include <functional>

// RTA US v2.00, research-mission2 ffff9a7. This is the bounded class
// dispatch contract, over the same World/memory as the Player and factory.
// Lower animation, AI, effects and loot calls require explicit service answers.
namespace champions::simulation::rta::mission2_actors {
using campaign_actors::Unsupported;
enum class Kind { Cutter, Hammer, Droid, IronGuardian };
struct Profile {
  Kind kind;
  std::uint32_t constructor, definition, vtable, run, set_state, size, entries;
};
const Profile &profile(Kind);
std::optional<Kind> classify(std::uint32_t constructor, const std::string &name);
struct Call {
  // A documented function identity, not an executable function pointer.
  // args/floats are normalized service inputs, NOT emulated ABI registers.
  std::uint32_t function, self;
  std::vector<Raw> args{}, floats{};
};
struct Services {
  std::function<Raw(const Call &)> call;
  // Constructor call-outs: holder/model/animation initialisation and the
  // entry IDs whose event parameters must be supplied. The research measures
  // IDs, not all masks/times or loaded-resource effects. Missing answers refuse.
  std::function<void(std::uint32_t, Kind, const std::vector<unsigned> &)> initialize;
  // Optional instrumentation; records local own draws as well as call-outs.
  std::function<void(const Call &)> observe;
};
struct Actor {
  Kind kind;
  objects::GameObject *object;
  std::optional<std::string> constructor_refusal, ai_refusal, death_refusal;
  bool death_started = false, death_reported = false;
  Actor(Kind k, objects::GameObject &o) : kind(k), object(&o) {}
  virtual ~Actor() = default;
};
struct ClockworkCutter : Actor { explicit ClockworkCutter(objects::GameObject &o) : Actor(Kind::Cutter, o) {} };
struct ClockworkHammer : Actor { explicit ClockworkHammer(objects::GameObject &o) : Actor(Kind::Hammer, o) {} };
struct Droid : Actor { explicit Droid(objects::GameObject &o) : Actor(Kind::Droid, o) {} };
struct IronGuardian : Actor { explicit IronGuardian(objects::GameObject &o) : Actor(Kind::IronGuardian, o) {} };
class Runtime {
  WorldAuthority &authority_;
  resources::Manager &resources_;
  const arena_modes::Executable &exe_;
  Services services_;
  std::map<std::uint32_t, std::unique_ptr<Actor>> actors_;
  std::map<std::uint32_t, std::shared_ptr<objects::ClassDef>> definitions_;
  std::vector<resources::Handle> resources_held_;
  std::map<std::string, std::size_t> refusals_;
  bool contain_ = false;
  bool particle_initialized_ = false;
  Raw read(std::uint32_t, unsigned = 4) const;
  void write(std::uint32_t, std::uint64_t, unsigned = 4);
  Raw call(std::uint32_t function, std::uint32_t self,
           std::vector<Raw> args = {}, std::vector<Raw> floats = {});
  Raw own(std::uint32_t);
  void run(Actor &);
  void death(Actor &);
  void tail(Actor &);
public:
  Runtime(WorldAuthority &, resources::Manager &, const arena_modes::Executable &, Services);
  // Finish the documented numeric class part after the host's base ctor;
  // remaining loaded services are recorded separately. No extra GameObject
  // allocation, shared seed draw or stat roll occurs.
  Actor &adopt(Kind, objects::GameObject &, std::uint32_t facing);
  Actor *find(std::uint32_t);
  const auto &actors() const { return actors_; }
  const auto &refusals() const { return refusals_; }
  void contain_refusals(bool on) { contain_ = on; }
  void tick(std::uint32_t);
  void set_state(std::uint32_t, std::uint32_t);
  void take_damage(std::uint32_t, std::uint32_t attacker, Raw flags, Raw amount);
};
} // namespace champions::simulation::rta::mission2_actors
