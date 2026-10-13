#pragma once
#include "champions/core_math.hpp"
#include "champions/creature_hp.hpp"
#include <variant>

namespace champions::simulation::rta::player {
using Memory = creature_hp::Memory;
using Write = creature_hp::Write;
using Address = creature_hp::Address;
using Error = creature_hp::Error;
using MissingHandler = creature_hp::MissingHandler;
using ResourceLimit = creature_hp::ResourceLimit;
struct Unsupported : Error {
  using Error::Error;
};
struct StackHandle {}; // Reference callback argument "stack", never a host pointer.
using Argument = std::variant<std::uint32_t, Vec3, StackHandle>;
using Result = std::variant<std::uint32_t, Vec3, std::vector<std::uint32_t>>;
// Closed callback set of the supported subset; argument lanes retain raw bits.
enum class Service {
  CreatureRun,
  Knockback,
  LightRelink,
  LightUnlink,
  CamFlag,
  Frozen,
  Voice,
  MemcardBusy,
  Rumble,
  AnimStep,
  Footstep,
  MoveToDest,
  FxSpawn,
  DialogOpen,
  TaskRunning,
  SlotActive,
  ResFind,
  AnimStart,
  AnimBlend,
  BlessedType,
  FindSpell,
  BlessedStat,
  RegenerationStat,
  LocalSlot,
  SoundPlay,
  SendDrop,
  RemoveItem,
  CountPotions,
  ItemWeight,
  NoticeCount,
  AttachPoint,
  PlayerAim,
  FindTrack,
  SpellStop,
  EquipItem,
  RecalcDamage,
  SkillCanUse,
  CollectArc,
  PlayerEnemy,
  PlayerRank,
  SkillCast,
  AddSpell,
  HandleLink,
  SkillCost,
  HandleClear
};
struct Call {
  Service service;
  std::vector<Argument> args;
};
struct Event {
  bool is_call = false;
  Write write{};
  Call call{};
};
class Tick {
  std::size_t depth_ = 0;
  Result invoke(Service, std::vector<Argument> = {});
  std::uint32_t vcall(Service, std::uint32_t, unsigned, unsigned,
                      std::vector<Argument> = {});
  std::uint32_t call(Service, std::vector<Argument> = {});
  void write(Address, std::size_t, std::uint64_t);
  std::uint32_t draw();
  std::uint32_t own_draw();
  void entry();
  std::int32_t item_modifier(Address, std::int32_t, unsigned) const;
  std::string cstr(Address) const;
  void hud_notice(unsigned, std::uint32_t);

public:
  // A callable is copied before invocation to preserve self-replacement. Keep
  // persistent callback state in memory or stable external captures; mutable
  // value-capture identity is a host boundary, not simulated player state.
  using Callback =
      std::function<Result(Tick &, const std::vector<Argument> &)>;
  Memory &mem; // Borrowed synchronous memory; caller retains its lifetime.
  const std::uint32_t P;
  std::map<Service, Callback> callbacks;
  std::vector<Write> writes;
  std::vector<Call> calls;
  std::vector<Event> events;
  std::uint64_t draws = 0;
  Address R0 = 0;
  // Missing services refuse by default. ReferenceZero explicitly opts into
  // ref_player's scalar-zero callback defaults, not real service behavior.
  enum class Policy { RequireHandlers, ReferenceZero };
  const Policy policy;
  explicit Tick(Memory &, std::uint32_t player,
                Policy = Policy::RequireHandlers);
  Tick(const Tick &) = delete;
  Tick &operator=(const Tick &) = delete;
  std::uint32_t u32(Address a) const;
  std::int32_t signed8(Address a) const;
  std::int32_t signed16(Address a) const;
  std::int32_t signed32(Address a) const;
  Vec3 vec(Address) const;
  std::uint32_t skill_of(std::uint32_t ability) const;
  std::pair<Vec3, Vec3> hands_midpoint(std::uint32_t player, bool limit_height);
  void update_cast(std::uint32_t player, Address pad);
  std::uint32_t weapon_swap_input(std::uint32_t player, Address pad,
                                  std::uint32_t force);
  void cast_skill(std::uint32_t player, std::uint32_t ability);
  std::int32_t slot() const;
  std::uint32_t record() const; // This accessor alone wraps like reference R.
  bool is_copy() const;
  // Complete run only for scripted ticks. Ordinary ticks retain entry writes
  // then refuse before +0x4c4, pad selection, switch or tail. No rollback.
  std::string run();
  Address
  choose_pad(); // Returns address; does not assign the reference local pad.
  void idle_fidget(std::uint32_t player, Address pad);
  std::uint32_t can_block(std::uint32_t slot, std::uint32_t state);
  void try_block_move(std::uint32_t player, Address pad);
  std::uint32_t has_anim(std::uint32_t player, std::uint32_t name);
  std::int32_t skill_rank(std::uint32_t slot, std::uint32_t id);
  void regen_health(std::uint32_t player);
  void regen_mana(std::uint32_t player);
  void tick_enchant_timers(std::uint32_t player);
  void use_potion(std::uint32_t kind); // run's (P,kind,0,0,-1) form
  void use_potion_real(std::uint32_t player, std::uint32_t kind,
                       std::uint32_t list_bag, std::uint32_t list_cell,
                       std::uint32_t list_panel);
  std::int32_t passive_level(std::uint32_t slot, std::uint64_t mask);
};
const char *service_name(Service);
} // namespace champions::simulation::rta::player
