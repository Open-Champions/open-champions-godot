#pragma once
#include "champions/core_math.hpp"
#include "champions/creature_hp.hpp"
#include <variant>
#include <optional>

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
struct Leave : Error { using Error::Error; };
struct NullArgument {}; // Reference None argument, distinct from address zero.
struct StackHandle {}; // Reference callback argument "stack", never a host pointer.
using Argument = std::variant<std::uint32_t, Vec3, StackHandle, NullArgument>;
using QueryResult = std::array<std::uint32_t, 2>; // (accepted, raw factor)
using AttachResult = std::array<Vec3, 2>; // Position and optional direction output.
using Result = std::variant<std::uint32_t, Vec3, std::vector<std::uint32_t>, QueryResult, AttachResult>;
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
  HandleClear,
  Move, SoundStop, Decal, Water, PoolNew, LightEffect, VehicleMove, UpdateGrid,
  DamageRadius, PoundEffect, BashType, ShieldBashType, SpellActive, ParticleCtor, ParticleSpawn,
  TextLookup, HudMessage, Sprintf, FindTarget, PlayerTurn, PrefixDiffers, ButtonValue, AncestralType, AncestralQuery, BattleCryType,
  BattleCryAffects, BerserkerFactor, CollectBox, CreatureReact, IterBegin, IterNext,
  IterEnd, MeleeHit, RemoveNode, PickAttackAnim, RecalcLoad, ResourceIndex,
  ProjectileCtor, ModifiedField, ScriptRun, SkillStat, DamageRange, MissileCtor,
  RootCtor, UnholyAuraCtor, WeaponTrails, DialogTarget, ProcessDamage, Interact, LightLink2
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
  Address entry();
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
  Address pad = 0; // Explicit state-helper input, independent of choose_pad.
  std::uint32_t anim_step(std::uint32_t speed = ONE);
  std::uint32_t move(std::uint32_t mode, Argument ext = std::uint32_t(0));
  std::int32_t frames() const;
  Address top_track(std::int64_t offset) const;
  void stop_loop_sound();
  void clear_extra_tracks();
  std::uint32_t slow_scale(std::uint32_t speed) const;
  std::uint32_t haste_of(Address item) const;
  std::uint32_t anim_name(std::int64_t offset) const;
  std::optional<std::uint32_t> selected_ability() const;
  void sound(std::uint32_t entry, std::uint32_t obj);
  Vec3 unit_sphere_shared();
  std::uint32_t instant = 0; // Neutral-state input, populated by top or supplied independently.
  void old_arrow_notices();
  void state_neutral();
  bool neutral_start_ability(std::uint32_t ability, std::uint32_t cost, Address record);
  bool neutral_attack(std::uint32_t mask, std::uint32_t slot);
  void state_melee();
  std::uint32_t melee_next_swing(Address record, std::int32_t steps, std::uint32_t started);
  std::uint32_t draw3();
  // Independently callable complete ranged family; does not advance run().
  void state_ranged();
  void ranged_throw(std::uint32_t weapon, std::int32_t release);
  void ranged_shoot();
  void missile(const Vec3 &spawn, const Vec3 &velocity, std::int64_t facing);
  Vec3 aim_at_target(std::optional<std::uint32_t> speed_divisor,
                     std::optional<std::uint32_t> scale = std::nullopt);
  std::uint32_t obj_rand(std::optional<std::uint32_t> object = std::nullopt);
  std::uint32_t strstr(Address haystack, Address needle) const;
  // Complete casting/legacy family; services stay explicit callbacks.
  // Binary strcmp returns zero for equality and one otherwise (256-byte cap).
  std::uint32_t strcmp(Address a, Address b) const;
  void state_cast();
  void cast_old_style(std::uint32_t ability);
  void light_effect(std::uint32_t height, std::uint32_t owner,
                    const std::vector<std::uint32_t> &colours);
  void old_fire_nova(std::uint32_t ability);
  // Complete action dispatch with explicit pad/R0/instant supplied by caller.
  // This does not invoke top() or the ordinary tail.
  void dispatch_state();
  // Complete pre-switch body. Scripted control throws Leave("scripted").
  // Ordinary completion populates pad, R0, weapon0 and instant; no state/tail.
  void top();
  void instant_abilities();
  // Complete sections 7–8, with caller-supplied pad for standalone use.
  void tail();
  std::uint32_t weapon0 = 0;
  void state_hit();
  void state_dead();
  void state_jump();
  void state_rush();
  void state_block_move(); // Leave on recall, matching the reference control exit.
  void state_warcry();
  void state_vehicle();
  void state_pound();
  void state_skill();
  void state_block();
  std::int32_t slot() const;
  std::uint32_t record() const; // This accessor alone wraps like reference R.
  bool is_copy() const;
  // Complete top -> dispatch -> tail. Leave returns its reason; other failures
  // propagate with their writes/calls retained. Services stay callback-bound.
  std::string run();
  // Historical entry/scripted subset, retained for frozen command-0 vectors.
  std::string run_legacy_entry();
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
