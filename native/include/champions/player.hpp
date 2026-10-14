#pragma once
#include "champions/core_math.hpp"
#include "champions/creature_hp.hpp"
#include <variant>
#include <optional>
#include <set>
#include <memory>
#include <type_traits>

namespace champions::simulation::rta::player {
// Shared sparse storage also implements ref_player Memory's underlying-load
// callback and unlogged write_bytes. Its read-only overlay is a separate policy.
using Memory = creature_hp::Memory;
using Write = creature_hp::Write;
using Address = creature_hp::Address;
using Error = creature_hp::Error;
using MissingHandler = creature_hp::MissingHandler;
// Low-byte signed conversion, independent of memory or FPU configuration.
std::int32_t s8(std::uint64_t value);
using ResourceLimit = creature_hp::ResourceLimit;
struct Unsupported : Error {
  using Error::Error;
};
struct Leave : Error { using Error::Error; };
struct NullArgument {}; // Reference None argument, distinct from address zero.
struct StackHandle {}; // Reference callback argument "stack", never a host pointer.
using Argument = std::variant<std::uint32_t, Vec3, StackHandle, NullArgument, std::int64_t, std::uint64_t>;
using QueryResult = std::array<std::uint32_t, 2>; // (accepted, raw factor)
using AttachResult = std::array<Vec3, 2>; // Position and optional direction output.
struct BufferResult { std::vector<std::uint8_t> bytes; }; // Reference ("buf", bytes) output.
using Result = std::variant<std::uint32_t, Vec3, std::vector<std::uint32_t>, QueryResult, AttachResult, NullArgument, std::int64_t, std::uint64_t, BufferResult>;
// Stable service IDs retained for historical vectors; named calls are also supported.
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
  RootCtor, UnholyAuraCtor, WeaponTrails, DialogTarget, ProcessDamage, Interact, LightLink2,
  Named // Arbitrary callback key stored in Call.name; stable IDs remain 0..94.
};
struct Call {
  Service service;
  std::vector<Argument> args;
  std::string name; // Empty for stable Service calls; otherwise the exact callback key.
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
  void hud_notice(unsigned, std::uint32_t);
  template<class F> auto delegated(const std::string &name, std::vector<Argument> args, F body) -> decltype(body()) {
    using T = decltype(body());
    if (!external.count(name)) return body();
    auto value = call(name, std::move(args));
    if constexpr (!std::is_void_v<T>) {
      if (auto raw = std::get_if<std::uint32_t>(&value)) return T(*raw);
      if (auto signed_value = std::get_if<std::int64_t>(&value)) return T(*signed_value);
      throw Error("scalar helper must return a raw32 or signed integer");
    }
  }

public:
  // A callable is copied before invocation to preserve self-replacement. Keep
  // persistent callback state in memory or stable external captures; mutable
  // value-capture identity is a host boundary, not simulated player state.
  using Callback =
      std::function<Result(Tick &, const std::vector<Argument> &)>;
  using NamedCallbacks = std::map<std::string, Callback>;
  std::shared_ptr<NamedCallbacks> named_callbacks; // Shared identity, like reference cb.
  std::set<std::string> external;
  Result call(const std::string &, std::vector<Argument> = {});
  Result vcall(const std::string &cls, std::uint32_t obj, unsigned table_offset, unsigned slot, std::vector<Argument> = {});
  Result helper(const std::string &, Callback function, std::vector<Argument> = {});
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
                Policy = Policy::RequireHandlers,
                std::shared_ptr<NamedCallbacks> = {}, std::set<std::string> = {});
  Tick(const Tick &) = delete;
  Tick &operator=(const Tick &) = delete;
  std::uint8_t u8(Address a) const;
  std::uint16_t u16(Address a) const;
  std::uint32_t u32(Address a) const;
  std::uint64_t u64(Address a) const;
  // Values are reduced to the selected width; signed callers convert modulo 2^64.
  // Stores retain the existing writes/events/memory-log order and failure prefix.
  void w8(Address a, std::uint64_t value);
  void w16(Address a, std::uint64_t value);
  void w32(Address a, std::uint64_t value);
  // Binary bytes, excluding the first NUL. Nonpositive limits perform no reads.
  // Reads retain checked raw32 addressing and a 1 MiB output resource cap.
  std::string cstr(Address a, std::int64_t limit = 256) const;
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
  Result use_potion(std::uint32_t kind); // run's (P,kind,0,0,-1) form
  void use_potion_real(std::uint32_t player, std::uint32_t kind,
                       std::uint32_t list_bag, std::uint32_t list_cell,
                       std::uint32_t list_panel);
  std::int32_t passive_level(std::uint32_t slot, std::uint64_t mask);
private:
  void h_idle_fidget(std::uint32_t a, Address b) { return delegated("Player_idleFidget", {std::uint32_t(a), std::uint32_t(b)}, [&] { return idle_fidget(a, b); }); }
  bool h_can_block(std::uint32_t a, std::uint32_t b) { return delegated("player_canBlock", {std::uint32_t(a), std::uint32_t(b)}, [&] { return bool(can_block(a, b)); }); }
  void h_try_block_move(std::uint32_t a, Address b) { return delegated("Player_tryBlockMove", {std::uint32_t(a), std::uint32_t(b)}, [&] { return try_block_move(a, b); }); }
  bool h_has_anim(std::uint32_t a, std::uint32_t b) { return delegated("Player_hasAnim", {std::uint32_t(a), std::uint32_t(b)}, [&] { return bool(has_anim(a, b)); }); }
  std::int64_t h_skill_rank(std::uint32_t a, std::uint32_t b) { return delegated("player_getSkillRank", {std::uint32_t(a), std::uint32_t(b)}, [&] { return std::int64_t(skill_rank(a, b)); }); }
  std::int64_t h_passive_level(std::uint32_t a, std::uint64_t b) { return delegated("player_getPassiveLevel", {std::uint32_t(a), std::uint32_t(b)}, [&] { return std::int64_t(passive_level(a, b)); }); }
  void h_update_cast(std::uint32_t a, Address b) { return delegated("Player_updateCast", {std::uint32_t(a), std::uint32_t(b)}, [&] { return update_cast(a, b); }); }
  void h_regen_health(std::uint32_t a) { return delegated("Player_regenHealth", {std::uint32_t(a)}, [&] { return regen_health(a); }); }
  void h_regen_mana(std::uint32_t a) { return delegated("Player_regenMana", {std::uint32_t(a)}, [&] { return regen_mana(a); }); }
  void h_tick_enchant_timers(std::uint32_t a) { return delegated("Player_tickEnchantTimers", {std::uint32_t(a)}, [&] { return tick_enchant_timers(a); }); }
  void h_weapon_swap_input(std::uint32_t a, Address b, std::uint32_t c) { return delegated("Player_weaponSwapInput", {std::uint32_t(a), std::uint32_t(b), std::uint32_t(c)}, [&] { weapon_swap_input(a, b, c); }); }
  void h_cast_skill(std::uint32_t a, std::uint32_t b) { return delegated("Player_castSkill", {std::uint32_t(a), std::uint32_t(b)}, [&] { return cast_skill(a, b); }); }
  Result h_use_potion_real(std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d, std::uint32_t e) {
    return helper("Player_usePotion", [=](Tick &t, const auto &) -> Result { t.use_potion_real(a,b,c,d,e); return NullArgument{}; }, {a,b,c,d,e});
  }
};
const char *service_name(Service);
} // namespace champions::simulation::rta::player
