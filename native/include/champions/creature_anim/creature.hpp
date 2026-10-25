#pragma once
#include "champions/creature_anim/runtime.hpp"
#include "champions/monsters_1.hpp"
namespace champions::simulation::rta::creature_anim {
using Memory = creature_hp::Memory;
using Address = creature_hp::Address;
Raw class_resource(const Memory &, Address, std::uint32_t index);
std::pair<Raw, Raw> virtual_target(const Memory &, Address, std::uint32_t slot);
Raw role_lump(const Memory &, Address, bool npc);
using Property = std::function<std::optional<std::string>(const std::string &)>;
using FindResource = std::function<Raw(Raw, const std::string &)>;
Raw find_role_animation(Raw lump, const std::string &role, const Property &,
                        const FindResource &);
void set_class_events(Memory &, Address, std::uint32_t index,
                      const std::array<Raw, 4> &, const std::array<Raw, 4> &);
Mat34 draw_matrix(const Memory &, Address);
struct Influence {
  std::uint8_t joint = 0, weight = 0;
};
struct Vertex {
  std::array<std::int16_t, 3> position{};
  std::array<std::int8_t, 3> normal{};
  std::vector<Influence> influences;
};
// Exact geometric numerator; denominator is 255*4096*16. No VU1 claim.
std::array<std::int64_t, 3> skin_point(const Pose &, const Vertex &);
inline constexpr std::int64_t POSITION_DENOMINATOR = 16711680;
// Exact rotational normal numerator; denominator is 255*4096*127.
std::array<std::int64_t, 3> skin_normal(const Pose &, const Vertex &);
inline constexpr std::int64_t NORMAL_DENOMINATOR = 132648960;
struct Mesh {
  std::vector<Vertex> vertices;
};
struct Pairing {
  Raw definition = 0, model = 0, texture = 0;
  ClipId base = 0;
  std::map<Raw, ClipId>
      clips; // resolved class resource address -> shared clip ID
};
// Class-specific bodies are explicit. More classes require their own contracted
// dispatch implementation; no filename or generic state-to-role heuristic.
struct Dispatch {
  std::function<void(monsters_1::Model &, Address, Raw, Vec3, Raw, Raw)>
      construct;
  std::function<void(monsters_1::Model &, Address)> run;
  std::function<void(monsters_1::Model &, Address, Raw)> change_state, touch;
};
Dispatch small_spider_dispatch();
struct Frame {
  Pose pose;
  Mat34 object;
  Motion motion; // stored WORLD delta after original Creature_stepAnim
  Raw model = 0, texture = 0;
  std::vector<std::array<std::int64_t, 3>> positions, normals;
};
// Share one Environment across actors of the same engine world. Resource
// addresses identify ONE clip, including its mutable event table, across
// actors.
struct Environment {
  Runtime runtime;
  Memory memory;
  std::map<Raw, ClipId> resources;
  std::map<Address, StateId> actors;
  explicit Environment(std::size_t slots = 165, std::size_t poses = 100)
      : runtime(slots, Limits{}, poses) {}
  ClipId register_clip(Raw resource, const std::vector<std::uint8_t> &bytes);
};
// An explicit standalone owner, independent of Session/Controller/save.
// Engine services remain synchronous callbacks on model; missing services
// throw.
class Creature {
  std::shared_ptr<Environment> environment_;
  Address address_;
  StateId state_;
  Pairing pairing_;
  Mesh mesh_;
  Dispatch dispatch_;
  std::optional<std::uint32_t> last_tick_;
  std::int32_t last_events_ = 0;
  ClipId resolve(Raw) const;
  void check_pairing() const;
  void publish_flags();

public:
  Runtime &runtime;
  Memory &memory;
  monsters_1::Model model;
  explicit Creature(Address address = 0x1000, std::size_t slots = 165,
                    std::size_t poses = 100);
  Creature(std::shared_ptr<Environment> environment, Address address);
  ~Creature();
  std::shared_ptr<Environment> environment() const { return environment_; }
  Creature(const Creature &) = delete;
  Creature &operator=(const Creature &) = delete;
  // Call after clips, object/class memory and mesh are supplied. Every clip is
  // checked against the declared base ordered skeleton before publication.
  void bind(Pairing pairing, Mesh mesh,
            Dispatch dispatch = small_spider_dispatch());
  void construct(Raw type_name, Vec3 position, Raw facing, Raw properties);
  void change_state(Raw state);
  void touch(Raw other);
  Frame tick(std::uint32_t simulation_frame, std::uint32_t draw_frame);
  Frame draw(std::uint32_t draw_frame); // never advances the animation clock
  StateId state_id() const noexcept { return state_; }
  Address address() const noexcept { return address_; }
};
} // namespace champions::simulation::rta::creature_anim
