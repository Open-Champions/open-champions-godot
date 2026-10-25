#pragma once
#include "champions/core_math.hpp"
#include "champions/creature_hp.hpp"
#include "champions/archive.hpp"
#include "champions/world_queries.hpp"
#include "champions/world_authority.hpp"
#include <array>
#include <map>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// RTA US v2.00 only. Authored composition of pinned executed contracts.
// No Session, Godot, save, controller or original-player-loop dependency.
namespace champions::simulation::rta::combat_runtime {
using Address = std::uint32_t;
struct Error : std::runtime_error {
  using std::runtime_error::runtime_error;
};
struct Unsupported : Error {
  using Error::Error;
};
inline constexpr Address SHARED_SEED = 0x404464, FRAME = 0x44fdcc,
                         PLAYERS = 0x4d8af0, RECORD_SIZE = 0x4c30,
                         NUM_PLAYERS = 0x4d8ae0, TARGET = 0x234,
                         LAST_ATTACKER = 0x2e4, VT_CREATURE = 0x50cc58,
                         VT_PLAYER = 0x511308, VT_SKELIGHT = 0x5174b0,
                         PADS = 0x41bfd8, PAD_SIZE = 0x58;
// Round1 keeps the round-1 set-up and its combat-only player phase. Full is
// the round-2 composition (research ref_combat_full.FullRuntime): the real
// player tick, movement, armed melee, defence, projectiles, three spells and
// the monster helpers in one world.
enum class Composition { Round1, Full };
struct Event {
  std::string name;
  std::vector<std::uint64_t> args;
  std::string text; // e.g. the player tick's Leave reason
};
struct Spawn {
  Vec3 position{};
  Raw hp = 0x42c80000, home_range = 0x44160000;
  std::uint32_t facing = 0;
  std::int32_t team = -1, armor = 0, minimum = 4, maximum = 8, attack = 100;
  std::uint64_t creature_flags = 0x100000;
  // Full composition players only (authored set-up, not Player_ctor).
  Raw idle_voice_limit = 0x7fff, speed_mul = 0x3f800000;
  // A session player: the record at PLAYERS already holds the real character
  // (class, level, statistics, hit points). Only the object back-pointer, the
  // documented unarmed weapon class and the team are written; the class
  // definition block is left blank (0x400 bytes) and no authored base cells
  // are planted, so the caller builds both from the real resources.
  bool session_record = false;
};
struct Slide {
  Vec3 position{};
  Raw hit = 0;
  Vec3 normal{};
};
struct Plane {
  Vec3 point{}, normal{};
};
// res_find(lump, entry). `lump_text` is set when the caller passes the lump as
// text (the projectile bodies); otherwise `lump` is the lump word.
// One loot_generateDrops call of Creature_dropLoot (ref_items owns the
// generator; the caller answers).
struct LootDrop {
  Vec3 position{};
  Raw angle = 0, filter = 0, force = 0, z = 0, throw_speed = 0, gold_scale = 0,
      gold_chance = 0, quality_row = 0, item_chance = 0;
};
struct ResourceQuery {
  Raw lump = 0;
  bool lump_is_text = false;
  std::string lump_text, entry;
};
struct Services {
  std::function<bool()> dialog_is_open;
  std::function<Raw()> jumpgate_travel_state;
  // Required for reachable terrain/animation services. No silent zero defaults.
  std::function<Raw(const Vec3 &, const Vec3 &, Raw, Raw)> raycast;
  std::function<Slide(const Vec3 &, const Vec3 &, Raw, Raw)> slide;
  std::function<Raw(const Vec3 &, Raw)> height_below;
  std::function<Raw(const Vec3 &, Raw)> water_test;
  // water_disturb 0x189514 (point, strength, radius) -> chunks touched, for the
  // player's wading ripples (docs/PLAYER_WADING.md). A missing function refuses
  // when a wading step needs it. The flat fixture is a dry world (touches 0).
  std::function<Raw(const Vec3 &, Raw, Raw)> water_disturb;
  // Optional: the minimum z (raw32) of water_test's first chunk, the height of
  // the airborne entry's splash burst. Presentation only.
  std::function<Raw(const Vec3 &, Raw)> water_height;
  // world_findFreeSpot 0x14acf8's terrain tests (point, radius):
  // terrain_testFlag1 0x183b48 and terrain_height 0x183820. Full only.
  std::function<Raw(const Vec3 &, Raw)> terrain_blocked;
  std::function<Raw(const Vec3 &, Raw)> terrain_height;
  std::function<Raw(Address, Raw)>
      animation_advance; // actor, speed -> mask (hit=1)
  std::function<void(Address, Raw, Raw, Raw, Raw)> animation_start;
  std::function<Raw(const std::string &, Raw, Raw)> skill_stat;
  // player_giveExp 0x1d3150 (slot, amount), called where Creature_processDamage
  // awards a kill. The award is always recorded (World::exp_awards); a host that
  // owns a real player record applies it here. Optional.
  std::function<void(Raw slot, Raw amount)> give_exp;
  // Full composition. Each is called where the executed bodies call it.
  std::function<Raw(Raw slot)> camera_yaw;  // net_getCamYaw
  std::function<Raw(Raw slot)> camera_flag; // net_getCamFlag
  std::function<std::array<Plane, 4>(const Vec3 &)> screen_planes;
  // Root motion (x, y) written by the advance just made for this actor.
  std::function<std::array<Raw, 2>(Address)> animation_root;
  // model_getAttachPoint for the actor whose animation state is used.
  std::function<Vec3(Address actor, Raw model, const Vec3 &, Raw facing,
                     Raw index)>
      attach_point;
  std::function<Raw(Raw model)> sample_point_count;
  // Nightmare charge contact excludes players in knockdown animation.
  std::function<bool(Address)> in_knockdown_anim;
  // model_getMeshJoint 0x179358 (model, mesh index) -> the u8 at +6 of the
  // 10-byte mesh record (model-texture.md, mesh record). The record is loaded
  // data (the user's disc at run time); the research models the call as an
  // unmodelled call-out. A missing function refuses. Not defaulted: there is
  // no joint to invent for a synthetic model.
  std::function<Raw(Raw model, Raw mesh)> mesh_joint;
  // resref_get 0x17377c (resource reference address) -> the cached data
  // pointer (engine.md: it looks the entry up again when g_lumpGeneration
  // changes). Also loaded data and an unmodelled call-out in the research.
  // A missing function refuses; the first word at the pointer is not it.
  std::function<Raw(Raw ref)> resref;
  // model_buildHideMask (model, variant bits): the 64-bit show mask the
  // AntQueen and Skeleton constructors store at +0x220. A missing function
  // refuses.
  std::function<std::uint64_t(Raw model, Raw variant)> hide_mask;
  // model_getSamplePoint. The weight out-parameter is not retained. A missing
  // function refuses; a count of 0 never calls it.
  std::function<Vec3(Raw model, Address anim, const Vec3 &position, Raw facing,
                     Raw index, Raw scale)>
      sample_point;
  std::function<Raw(const ResourceQuery &)> resource;
  // Creature_dropLoot / Goblin death call-outs: rtti_typeInfoEquals(typeid(*c),
  // <class>_typeinfo) by the getter's name, loot_generateDrops and
  // loot_spawnNamedItem (the item name is an executable string address).
  std::function<bool(Address, const std::string &)> is_class;
  std::function<void(const LootDrop &)> loot_generate;
  std::function<void(const Vec3 &, Raw)> spawn_named_item;
  // terrain_heightAtPoint 0x182f28 (world-queries.md §4.3). This is not
  // height_below. A missing function refuses. The flat fixture returns 0,
  // the fixture ground, not an original terrain sample.
  std::function<Raw(const Vec3 &)> height_at_point;
  // particleGroup_create (SpellHornOmenEffect_ctor). A missing function
  // refuses. The flat fixture returns 0, which is not an original group id.
  std::function<Raw()> particle_group;
  // Slot 4 of an adopted campaign creature (World::adopt_creature). Called
  // from the run list before any table lookup; return true when the class
  // body ran. A false return falls through to the executed dispatch.
  std::function<bool(Address actor)> external_run;
};
// Install the original terrain query answers from one selected, post-fixup
// WORLD. The caller owns Level for as long as the resulting World runs.
Services with_terrain(Services base, world_queries::Level &level);
struct Arc {
  Raw facing = 0, angle = 6000, minimum = 4, maximum = 8, attack = 100,
      flags = 8;
  Raw near = 0, far = 0x42400000;
};
// Authored weapon item (research MeleeRuntime.give_weapon).
struct Weapon {
  std::int32_t minimum = 5, maximum = 9, attack = 10, subtype = 0, cls = 0;
  Raw damage_type = 1;
  std::vector<std::array<std::int32_t, 4>> mods;
};
// Missile and thrown construction arguments (the call sites' arguments).
struct Missile {
  Vec3 position{}, velocity{};
  Raw facing = 0, minimum = 1, maximum = 2, attack = 100, damage_flags = 4,
      item = 0, kind = 0, launch = 0, impact = 0, rating = 0, item_flags = 0;
  std::array<Raw, 2> models{};
};
struct Thrown {
  Vec3 position{}, velocity{};
  Raw facing = 0, minimum = 1, maximum = 2, attack = 100000, kind = 4,
      item = 0;
  std::array<Raw, 2> models{};
};
// Authored Skill statistics: index -> (first level, base, step pattern).
struct SkillStat {
  std::int32_t first = 1, base = 1;
  Raw step = 0;
};
struct SkillSetup {
  std::array<SkillStat, 5> stats{};
  Raw kind = 0, recast = 0, flags = 0;
};
// Owns one pool, one handle graph and one memory. Address results are
// transient; keep a handle for anything retained across calls/ticks or
// allocation reuse.
// When finished iterators are released (a host policy, not a game rule): Lazy
// (default) at 4,096 and then at twice what survives; Always before every walk;
// Never keeps all until the 65,536 retention refusal. The simulated state does
// not depend on it; the differential test compares the three.
enum class IteratorRetention { Lazy, Always, Never };
class World {
public:
  struct State;

private:
  std::unique_ptr<State> s_;

public:
  explicit World(Services services, int camera_mode = 1,
                 Composition composition = Composition::Round1);
  World(std::shared_ptr<WorldAuthority> authority, Services services,
        int camera_mode = 1, Composition composition = Composition::Round1);
  ~World();
  World(const World &) = delete;
  World &operator=(const World &) = delete;
  Composition composition() const;
  const creature_hp::Memory &memory() const;
  std::uint64_t read(Address, unsigned size = 4) const;
  // Explicit authored setup, not an original constructor. Raw handle/node
  // writes refuse.
  void write(Address, unsigned size, std::uint64_t);
  Address spawn_skelight(const Spawn & = {});
  // Own-machine classes. The vtable is allocated on the first spawn, so an
  // untouched world keeps the Skelight corpus image.
  Address spawn_class(const std::string &name, const Spawn & = {});
  // Authored synthetic placement of one documented mounted Horsemen pair.
  // The two Spawn records supply fixture stats; the native run bodies own the
  // subsequent AI, contact, damage and lifecycle. Returns {rider, mount}.
  std::array<Address, 2> spawn_mounted_horseman(unsigned variant,
                                               const Spawn &rider,
                                               const Spawn &mount);
  void set_mounted_state(Address mount, Raw state);
  Address spawn_player(unsigned slot, const Spawn &, int strength = 20,
                       int level = 1);
  bool contains(Address) const;
  std::vector<Address> actors() const;
  Address retain(Address); // creates an owned external handle head
  void relink(Address head, Address target);
  void clear_handle(Address head);
  Address target(Address head) const;
  // A level-factory object remains owned by the script host. Its handle can
  // be resolved here without claiming an unported combat constructor or run.
  const objects::GameObject *factory_object(Address head) const;
  // A script-host factory creature that a campaign class port has finished
  // building: its bytes already live in the shared memory and its object is
  // registered with the authority. Gives it a combat record so executed
  // damage, targeting and knockback bodies (damage_apply, Creature_takeDamage,
  // grid queries) accept it as a victim or target. The vtable at +0x48 must be
  // defined (define_creature_table) first. Never allocates, draws or links.
  void adopt_creature(Address factory_object);
  // Writes a creature vtable image: the executed Creature base slots, then the
  // class's own overrides (slot -> function address).
  void define_creature_table(Address table,
                             const std::map<unsigned, Raw> &overrides);
  // A class virtual body ported outside the arena model, selected by function
  // address (for example a WarBoar_setState at slot 27). Runs before any
  // inherited body; arguments are the call's integer arguments.
  void set_class_body(Raw function,
                      std::function<void(Address self,
                                         const std::vector<std::uint64_t> &args)> body);
  // Base Creature steps a class run makes over the shared memory
  // (Creature_run, Creature_updateActive, Creature_stepAnim).
  bool creature_run(Address);
  bool creature_update_active(Address);
  Raw creature_step_anim(Address, Raw speed);
  bool creature_away_from_home(Address);
  // Creature_filterState 0x10a9c0: true when the requested state is allowed.
  bool creature_filter_state(Address, std::uint32_t requested);
  // Creature_onDeath 0x10beb8 (virtual slot 28).
  void creature_on_death(Address, Raw quiet = 0);
  // Grid nodes left out of box queries because they are unconstructed host
  // stubs (their class constructor is not ported).
  std::size_t stub_box_skips() const;
  void destroy(Address);       // slot 1 with 3 through the executed chain
  void delete_object(Address); // unlink run list now; grid/handles persist
  void update_grid(Address);
  void run_every_frame(Address);
  void run_every(Address, unsigned period); // 15 or 60
  std::vector<Address> collect_box(const Vec3 &, const Vec3 &, int limit);
  void run_objects(); // flush, active list, frame%15, frame%60; does not
                      // increment frame
  void tick(); // visibility, run_objects, frame advance, alive list; party wipe
               // refuses
  void take_damage(Address victim, Address attacker, Raw flags, Raw amount);
  int damage(Address victim, Address attacker, Raw minimum, Raw maximum,
             Raw attack, Raw flags);
  int enemy(Address, Address);
  void pick_target(Address, bool force = false);
  int can_see(Address, Address, Raw facing, Raw arc, Raw ray_mode, Raw near,
              Raw far, Raw lead = 0);
  int hit_target_in_arc(Address, const Arc &);
  int hit_all_in_arc(Address, const Arc &);
  Address find_enemy_in_cone(Address, Raw facing, Raw arc, Raw near, Raw far);
  void process_damage(Address);
  void player_find_target(Address, Raw reach);
  void player_melee_hit(Address, Address, Raw extra = 0, Raw scale = 0);
  // Mode-B arena_startWave's bounded intro state and arena_runTask's first
  // 181 visits. The wave builder and later empty-wave branch are not implied.
  void begin_arena_intro();
  void arena_intro_visit();
  // Bind the owner-supplied .vif hulls at a PhProp's resource pointer. Checks
  // every active mesh bit before publishing bytes to the owned World memory.
  void bind_prop_model(Address prop, champions::Bytes vif);
  void apply_knockback(Address, Raw scale);
  Raw move_creature(Address, const Vec3 &);
  void dead_update(Address, Raw events, Raw hold, Raw size,
                   const Vec3 &offset = {});
  // One supplied event batch is consumed only when this actor animates in its
  // run. Persistent pending events are held through handles and discarded on
  // inactivity.
  void animation_events(Address, Raw mask);
  const std::vector<Event> &events() const;
  void clear_history();
  void set_iterator_retention(IteratorRetention);
  Raw slot_function(Address, unsigned slot) const;
  [[noreturn]] void refuse(const std::string &feature) const;

  // ---- Full composition (round 2) -----------------------------------------
  // Lockstep pad input of a slot (research Runtime.press_attack, stick).
  void stick(unsigned slot, Raw x, Raw y);
  void press_attack(unsigned slot, bool held = true);
  // Explicit button-mask word and pad edge/held words for a slot. Masks are
  // the caller's controller mapping; none is invented here.
  enum class Button { Attack, AbilityA, AbilityB, Block, PotionA, PotionB, Swap };
  void bind_button(unsigned slot, Button, Raw mask);
  void pad(unsigned slot, Raw held, Raw pressed);
  Address give_weapon(unsigned slot, const Weapon &);
  Address spawn_missile(Address owner, const Missile &);
  Address spawn_thrown(Address owner, const Thrown &);
  Address give_skill(int skill_id, const SkillSetup &);
  // Opt-in authored Full-fixture record. The caller supplies mana and a
  // researched player rank (1..20); no character value is inferred.
  void select_disease_bolt(unsigned slot, Raw mana, Raw max_mana,
                           unsigned rank = 1);
  // The cast is made where Player_run state 5 would call Player_castSkill:
  // a player's before its tick tail, a creature's at the end of its run.
  void queue_cast(Address caster, int skill_id, int level = 1,
                  std::optional<Vec3> point = std::nullopt,
                  std::optional<Raw> facing = std::nullopt);
  // Executed bodies, callable directly on world actors.
  Raw move_with_collision(Address, const Vec3 &velocity, Address ignore = 0,
                          Raw skip_creatures = 0);
  Raw player_move(Address player, Raw mode, Address pad, const Vec3 &ext = {});
  Raw player_anim_step(Address player, Raw speed);
  Raw clamp_to_screen(Address player, Address velocity, Address mask);
  void pick_attack_anim(Address player, Raw weapon_class);
  void hit_sparks(Address player, Address target);
  void block_sparks(Address player, Raw silent = 0);
  void interact(Address player, Address pad, Raw display_only = 0);
  int can_see_point(Address, const Vec3 &, Raw facing, Raw arc, Raw near,
                    Raw far);
  int can_see_edge(Address, Address, Raw facing, Raw arc, Raw ray_mode,
                   Raw near, Raw far, Raw lead = 0);
  Address find_player_in_cone(Address, Raw facing, Raw arc, Raw near, Raw far);
  int hit_target_at_reach(Address, Raw minimum, Raw maximum, Raw attack,
                          Raw flags, Raw reach);
  int hit_target_in_arc_edge(Address, const Arc &);
  int hit_target_or_player_in_arc(Address, const Arc &);
  void knockback_target(Address, Raw speed);
  void set_cooldown(Address, Raw frames);
  void set_cooldown_on(Address, Raw frames, Address object);
  void notify_objects_near(Raw a, const Vec3 &, Raw b, Raw radius);
  std::vector<Address> collect_in_arc(const Vec3 &, Raw facing, Address out,
                                      Address filter, Raw radius, Raw arc,
                                      Raw width);
  std::vector<Address> collect_in_sphere(const Vec3 &, Address out,
                                         Raw radius);
  std::vector<Address> damage_in_radius(Address attacker, const Vec3 &,
                                        Raw most, Raw flags, Raw least,
                                        Raw skip, Raw inner, Raw outer);
  // world_findFreeSpot 0x14acf8: up to 50 shared-seed tries; (point, ok).
  std::pair<Vec3, int> find_free_spot(const Vec3 &centre, Raw spread,
                                      Raw radius, Raw any_height);
  std::vector<Address> attached_spells(Address creature) const;
  void drop_loot(Address, const Vec3 &, Raw angle, Raw filter, Raw z,
                 Raw throw_speed, Raw gold_scale, Raw gold_chance,
                 Raw quality_row, Raw item_chance);
  void goblin_set_state(Address, Raw state); // death branch only
  // Observation only: every registered handle word whose list is non-empty
  // (address, first node) and the first free handle node.
  std::vector<std::pair<Address, Address>> handle_words() const;
  Address free_handle_node() const;
  // Node pool words (next, prev, partner, object, tag); a link to an iterator
  // reads 0xffffffff.
  std::vector<std::array<Address, 5>> handle_nodes() const;
  // Experience delivered by damage_apply (slot, amount), in order.
  const std::vector<std::pair<Raw, Raw>> &experience() const;

  // ---- Effect hooks (playable integration, wave 11) -------------------------
  // damage_apply 0x1e8230 with the explicit descriptor an effect class passes
  // (native-fx-2 GameHooks: DiseaseCloud, StaticFire). It forwards to the same
  // executed descriptor glue the composed spells use; no new arithmetic. Rows
  // are (type, a, b, c) as s16 values, unused rows type -1.
  int damage_descriptor(Address victim, Address attacker, Raw attack, Raw flags,
                        const std::vector<std::array<std::int64_t, 4>> &rows,
                        Raw minimum, Raw maximum);

  // The pinned round-2 corpus was generated before the round-3 and round-4
  // bodies. It still records a refusal for a disease allocation, arrow kinds
  // 1, 2, 5 and 6, thrown potions, and silent block sparks. This restores
  // that surface for the corpus replay. The runtime's own Full composition
  // leaves the bodies on.
  void retain_round2_refusals();

  // Round 3. These build the executed objects; they do not run a skill body
  // this round left refused.
  Address spawn_disease_cloud(const Vec3 &, Raw level, Address owner);
  Address attach_disease(Address victim, Address attacker = 0);
  Address spawn_gut_chunk(const Vec3 &, Raw kind, Raw bits);
  // Thrown-potion endings (research-combat-ctors 98c081a, potion-ctors.md):
  // LooseFire_ctor and poisonGasSpell_ctor on a pool block, and slot 6.
  Address spawn_loose_fire(const Vec3 &, Address owner);
  Address spawn_poison_gas(const Vec3 &, Raw life, Raw radius, Raw word,
                           Address owner, Raw damage);
  void touch(Address object, Address other);
  Address spawn_phantom(Address target, const std::string &name, Raw speed,
                        Raw flag);
  void run_actor(Address);

  // Round 4. absorb is SpellHolyShield_absorb. attach_holy_shield plants the
  // words that function reads and the type-info lookup; it is not
  // SpellHolyShield_ctor, which stays a refused skill path.
  Raw holy_shield_absorb(Address spell);
  Address attach_holy_shield(Address caster, Address skill, Raw level = 1);

  // Round 5. These are the executed bodies. PlanarSteel, HornOmen, SpellArena,
  // Pustule and SmokeGen are round 6.
  void fade_bank_tracks(Address creature);
  void update_bank(Address creature);
  int is_demon_near(Address creature);
  Raw holy_shield_sample(Address spell);
  void multifire_run(Address obj);
  Address arm_fixture_holy_shield(Address player);

  // Round 6 (research ref_combat_round6 at faa7033). multifire_run now runs
  // multifire_end's body after it records the reason. spell_block is a fixture
  // block (0x80 bytes, the difftest's); the built-ins that allocate these
  // spells were not read. The ctors allocate one and return it. A class whose
  // destructor is not in the research refuses at "slot 1 with 3".
  Address spell_block();
  // A fixture object for the round-6 object runs: GameObject_ctor on a pool
  // block of `size`, a fixture table (not the class's) whose slot 4 is `run`
  // and whose other slots are 0, and the owner handle at +0x4c when `owner`
  // is set. The classes' constructors are not in the research.
  Address spawn_effect_fixture(Raw run, Raw size, const Vec3 &pos,
                               Address owner = 0);
  void poison_gas_run(Address obj);
  void disease_corpse_run(Address obj);
  void multifire_on_hit(Address obj, Address other);
  void multifire_end(Address obj, Raw reason);
  void smoke_gen_run(Address obj);
  void pustule_run(Address obj);
  void arena_run(Address spell, const Vec3 &pos);
  Address hornomen_ctor(Raw kind, Address owner, Raw extra);
  void hornomen_run(Address spell, const Vec3 &pos);
  Address planarsteel_ctor(Raw kind, Address owner, Raw extra);
  void planarsteel_run(Address spell, const Vec3 &pos);
};
// Explicit synthetic fixtures, never the production default.
Services flat_fixture_services();
} // namespace champions::simulation::rta::combat_runtime
