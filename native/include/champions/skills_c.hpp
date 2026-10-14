#pragma once
#include "champions/creature_hp.hpp"
#include <set>

// Standalone Return to Arms contract subset, not a Norrath profile.
namespace champions::simulation::rta::skills_c {
using Memory = creature_hp::Memory;
using Address = creature_hp::Address;
using Write = creature_hp::Write;
struct Unsupported : std::runtime_error {
  using std::runtime_error::runtime_error;
};
struct Call {
  unsigned slot;
  std::vector<std::uint32_t> args;
};
// Operation arguments mirror the pinned reference's method arguments, with the
// target address first (skill, spell, object or handle head/node). Return
// integers and floats are unsigned raw32 patterns. HandleItems flattens pairs;
// void runtime operations return an empty vector. Stack-local handles are not
// representable in this raw32 interface and are unsupported.
enum class Operation : unsigned {
  Stat = 0,
  Tier2,
  ForCaster,
  ForHandle,
  Adjust,
  ManaFromStat4,
  Range,
  CanUseDefault,
  CastDefault,
  ManaDefault,
  ManaCost,
  StatisticSpread,
  RageSwing,
  RageDamage,
  Complement,
  Throwing,
  BattleMage,
  PenetratingArrows,
  SpellBase, SpellRand, ObjectRand, SpellUnit, ObjectUnit,
  SpellBall, ObjectBall, VisualPosition, HandleLink, HandleRemove,
  HandleClear, HandleItems,
  RetributionCanUse, RetributionCast, RetributionCtor, RetributionRun, RetributionHeal,
  SpiritShieldCanUse, SpiritShieldCast, SpiritShieldCtor, SpiritShieldRun,
  SpiritShieldParticles, SpiritShieldAbsorb,
  FrostArmorCanUse, FrostArmorCast, FrostArmorCtor, FrostArmorRun, FrostArmorHit,
  FindSpell, IsPlayer, PlayerRecord, Flash, LightEffect, ModelSample, SoundAt, SpellDestroy,
  BloodLustCanUse, BloodLustCast, GhostHands, BloodLustCtor, HandStreamers,
  BloodLustRun, BloodLustGlow, BloodLustComplement, BloodLustMultiplier,
  ShoutAnimation, ShoutInterrupted, CalleeStore,
  RiposteCast, RiposteCounterAttack, DodgeManaCost, DodgeFraction,
  HealingCast, HealingCtor, HealingAmount, HealingFlash, HealingRun,
  SummonArrowsCanUse, SummonAxeCast, SummonArrowsCast, SummonCast,
  SummonAxeBuildItem, SummonCtor, SummonAxeRun, SummonArrowsRun, SummonRun, ItemRecord,
  CriticalHitCanUse, CriticalHitCast, CriticalHitManaCost, CriticalHitCtor,
  CriticalHitTint, CriticalHitRun, CriticalHitBonus, LightRecord,
  FireAxeCanUse, ColdAxeCanUse, AxeCanUse, FireAxeCast, ColdAxeCast, AxeCast,
  BattleCryCanUse, BattleCryCast, BattleCryCtor, BattleCryStreamers,
  BattleCryTint, BattleCryRun, BattleCryAffects, BodyAttachPoint,
  LightningStrikeCast, PusStormCast, StormCast, StormCtor,
  LightningStrikeRun, PusStormRun, StormRun, PustuleCtor,
  DiseaseBladeCanUse, DiseaseBladeCast, PlayerWeapons, WriteDisease,
  DiseaseBladeCtor, DiseaseBladeRun,
  RaiseArmyCanUse, RaiseArmyCast, RaiseArmyIsRaisable, RaiseArmyCtor,
  RaiseArmyRun, RaiseArmyRaiseInRing, RaiseArmyDress,
  DrowsyCast, DrowsyCtor, DrowsyBurst, DrowsyRun, DrowsyFraction,
  WaveOfFlameCanUse, WaveOfFlameCast, WaveOfFlameCtor, WaveOfFlameRun, WaveOfFlameHitTest,
  RoarCanUse, RoarCast, RoarCtor, RoarBreath, RoarRun, RoarHitRing,
  UnspokenWordCast, UnspokenWordCtor, UnspokenWordPhase, UnspokenWordRun, UnspokenWordHitRing,
  PoisonBoltCast, PoisonBoltCtor, PoisonBoltRun, PoisonBoltOnHit, PoisonBoltBurst,
  SummonBadgerCanUse, SummonBadgerCast, SummonBadgerCtor, SummonBadgerRun,
  RootCanUse, RootCast, RootCtor, RootRun,
  ConeOfShockCast, ConeOfShockCtor, ConeOfShockRun
};
// Callback identifiers share the existing callback registry and ordered call log.
// Vector arguments are flattened in signature order; DamageApply carries the
// descriptor as 18 header bytes and four complete four-halfword slots.
// HandleClearStack carries [0] as the explicit stack sentinel, not an address.
enum Service : unsigned {
  TypeInfo = 16, FindSpellByType, PoolNew, LightLink, ResourceFind,
  SoundPlayAtPosition, ParticleGroupCreate, ParticleGroupBegin,
  EmitFromModelVertices, ParticleGroupEnd, LightRelink, SpellDestroyCallback,
  LightEffectConstructor, CreatureModel, SamplePointCount, SamplePoint,
  ParticleSpawn, DamageApply, EffectSpawn, NetObjectOrGhost, AttachPoint,
  StringFormat, AnimationBlend, NetIsLocalSlot, PlayerAnimationStep, GlowAdd, SoundStop,
  SoundPlayOnObject, GlowingShaftConstructor, CollectObjectsInBox, CreatureAlly,
  CreatureTeam, ParticleGroupSetMatrix, TerrainHeightAtPoint, ItemParseDescriptor, PlayerItemWeight,
  InventoryGiveItem, PlayerRecalcLoad, InventoryEquipItem, PlayerResetIdleAnimation,
  PlayerVoice, ParticleGroupFree, LightUnlink, PlayerGetSkillRank, TextLookup, HudShowMessage, ItemResourceIndex,
  ItemCopyMutableFields, PlayerProjectileConstructor, HudCountActiveNotices, InventoryRemoveItem, CreatureEnemy, CreatureStun,
  GameObjectConstructor, TerrainHeight, ObjectRunEveryFrame, ObjectUpdateGridCell,
  ObjectDestroyCallback, LightningBoltConstructor, HolderDisease, HolderClearElement, CreatureBossRank, WorldCreateObject,
  CreatureSetCharm, CreatureGetTeam, AnimationPushWithWeights, CreatureSetState,
  HolderGetModel, HolderGetElement, HolderSetModel, StringPrefixDiffers, ModelBuildHideMask,
  CollectObjectsInArc, CreatureAddSpell, HandleClearStack, CollectInSphere, CameraShake, WaterTest, WaterDisturb,
  TrailCreate, TrailSetLength, TrailSetColor, TrailSetPoints,
  ProjectileConstructor, ObjectMoveAndCollide, PoisonGasConstructor, CreatureSetEffectTimer,
  WorldFindObjectByName, WorldFindFreeSpot, CastBadgerConstructor, CreatureSetHomeRange, LightningBoltConstructor2
};
// Canonical item value, not a fabricated stack address or a 0x48-byte record.
// [definition, quantity16, 18 header bytes, slot-count, (length, halfwords)...].
// Unwritten bytes, including record +6, are absent. Slot lengths are preserved.
std::vector<std::uint32_t> item_record(std::uint32_t definition,
    std::uint32_t quantity, const std::vector<std::vector<std::uint32_t>> &slots = {});
class Model {
  std::uint32_t u32(Address) const;
  std::int32_t half(Address) const;
  std::uint32_t tier(Address, unsigned, unsigned, unsigned, std::uint32_t,
                     std::int32_t);
  std::uint32_t draw();
  void write(Address, unsigned, std::uint64_t);
  std::uint32_t own_draw(Address);
  std::uint32_t unit(Address);
  std::vector<std::uint32_t> ball(Address);
  std::uint32_t handle_take();
  void handle_unlink(Address);
  std::uint32_t scalar(Address, unsigned, std::vector<std::uint32_t>);
  std::vector<std::uint32_t> invoke(Address, unsigned,
                                    std::vector<std::uint32_t>);
  std::uint32_t rage(Address, std::uint32_t, Address, std::uint32_t, unsigned,
                     bool);
  std::vector<std::uint32_t> service(unsigned, std::vector<std::uint32_t>);
  std::uint32_t service_scalar(unsigned, std::vector<std::uint32_t>);
  std::vector<std::uint32_t> vec(Address) const;
  std::uint32_t level(Address) const;
  std::uint32_t record(Address) const;
  void flash(Address, unsigned, const std::vector<std::uint32_t> &);
  std::vector<std::uint32_t> sound(std::uint32_t, std::uint32_t, const std::vector<std::uint32_t> &);
  void destroy(Address);
  std::uint32_t creature_model(Address);
  std::vector<std::uint32_t> sample(Address, std::uint32_t,
                                  const std::vector<std::uint32_t> &, std::uint32_t);
  void protection_ctor(Operation, Address, std::uint32_t, Address, Address);
  void shield_particles(Address, std::uint32_t, bool);
  void write_vec(Address, const std::vector<std::uint32_t> &);
  std::vector<std::uint32_t> ghost_hands(Address);
  void hand_streamers(Address, std::uint32_t, const std::vector<std::uint32_t> &,
                      unsigned, unsigned);
  void bloodlust_glow(Address, const std::vector<std::uint32_t> &);
  std::vector<std::uint32_t> summon_build_item(std::uint32_t, std::uint32_t, bool);
  std::uint32_t summon_cast(bool, Address, std::uint32_t, Address,
                            const std::vector<std::uint32_t> &);
  void summon_ctor(bool, Address, std::uint32_t, Address, Address);
  void summon_run(bool, Address);
  std::vector<std::uint32_t> attach_point(Address, std::uint32_t, bool = false, std::uint32_t = 0);
  void battlecry_streamers(Address, bool, std::uint32_t);
  void write_disease(Address, std::uint32_t, std::int64_t, std::int64_t);
  void holder_call(Address, unsigned, std::vector<std::uint32_t>);
  std::size_t depth_ = 0;

public:
  Memory mem;
  std::vector<Write> writes;
  std::vector<Call> calls;
  std::uint64_t draws = 0;
  // Force a known slot to the callback, as Python's external set does. Unknown
  // bodies always delegate. Missing handlers refuse after recording the call.
  std::set<unsigned> external;
  using Callback = std::function<std::vector<std::uint32_t>(
      Model &, const std::vector<std::uint32_t> &)>;
  std::map<unsigned, Callback> callbacks;
  // Callbacks may mutate memory, replace handlers, or throw. No rollback.
  // Neither the model nor its memory may be destroyed during a callback.
  std::vector<std::uint32_t> execute(Operation,
                                     const std::vector<std::uint32_t> &);
  std::vector<std::uint32_t> skill_call(Address skill, unsigned slot,
                                        const std::vector<std::uint32_t> &args);
  [[noreturn]] void cast_or_run(std::uint32_t skill_id);
};
} // namespace champions::simulation::rta::skills_c
