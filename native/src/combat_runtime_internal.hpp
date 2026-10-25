#pragma once
#include "champions/arena_monsters.hpp"
#include "champions/combat_ctors.hpp"
#include "champions/combat_damage.hpp"
#include "champions/combat_runtime.hpp"
#include "champions/combat_stats.hpp"
#include "champions/core_state.hpp"
#include "champions/creature_movement.hpp"
#include "champions/player.hpp"
#include <algorithm>
#include <map>
#include <set>

namespace champions::simulation::rta::combat_runtime {
namespace A = arena_monsters;
using Value = A::Value;
inline Value iv(std::int64_t v) { return v; }
inline Raw number(const Value &v) {
  if (auto p = std::get_if<std::int64_t>(&v))
    return Raw(*p);
  throw Error("integer service result required");
}
inline int byte(std::uint64_t n) {
  return n & 128 ? int(n & 255) - 256 : int(n & 255);
}
// Full composition addresses (research ref_combat_* modules).
inline constexpr Address G_WORLD = 0x44fdc8, G_EXCLUDE = 0x4039b8,
                         G_NETWORK = 0x402791, G_PVP_A = 0x4f711c,
                         G_PVP_B = 0x4f7120, G_LEVEL_NAME = 0x41b721,
                         G_LEVEL_FLAGS = 0x44fe08, G_RUN_SPEED = 0x4f8418,
                         G_ALIVE_COUNT = 0x4f8420, G_GHOST_REBASE = 0x402797,
                         G_SKILL_TABLE = 0x41b170, G_BTN_ATTACK = 0x41b980,
                         G_TIMED_MODIFIERS = 0x537c08, FIXTURE_BASE = 0x10000,
                         STACK_HEAD = 0x8000, VT_GAMEOBJECT = 0x5122a8,
                         VT_PROJECTILE = 0x5120b8, VT_DISEASEBOLT = 0x5183d8,
                         VT_MISSILE = 0x50e5a0,
                         VT_THROWN = 0x50e470, VT_HOLYSTRIKE = 0x51a078,
                         VT_SPARKS = 0x50e998, VT_LIGHTEFFECT = 0x50e198,
                         VT_SPELL_WAVE = 0x51c330, MISSILE_SIZE = 0x1c8,
                         THROWN_SIZE = 0x1d0, VT_DISEASE_CLOUD = 0x5186f0,
                         VT_DISEASE_SPELL = 0x518db0, VT_GUTCHUNK = 0x50f3b8,
                         VT_PHANTOM = 0x50f448, DISEASE_GETTER = 0x3cdd40,
                         DISEASE_RUN = 0x2d5854, DISEASE_CLOUD_RUN = 0x2ce6e0,
                         DISEASE_CLOUD_DTOR = 0x3cd480, PHANTOM_RUN = 0x161a54,
                         GUTCHUNK_SIZE = 0x268, PHANTOM_SIZE = 0x3d8,
                         DISEASE_EFFECT_SIZE = 0x1b0, DISEASE_CLOUD_SIZE = 0x64,
                         // Thrown potion endings (potion-ctors.md).
                         VT_LOOSEFIRE = 0x50e2b8, VT_FIRE = 0x50f208,
                         VT_POISON_GAS = 0x50d500, LOOSEFIRE_SIZE = 0x54,
                         FIRE_SIZE = 0x50, POISON_GAS_SIZE = 0x70,
                         LOOSEFIRE_DTOR = 0x3c6778, LOOSEFIRE_RUN = 0x202c48,
                         LOOSEFIRE_TOUCH = 0x1fcb70, FIRE_DTOR = 0x3c5050,
                         FIRE_RUN = 0x1f34e0, POISON_GAS_DTOR = 0x3c7878,
                         POISON_GAS_RUN = 0x1fd0f0;
// Spell type-info getters. A spell lookup compares the getter's address, as
// with HolyShield's 0x3cf124 (slot 0 of table 0x519ce0). Research
// t3/research-combat faa7033: tools/ref_player.py:184 (AncestralCall
// 0x3ce9bc) and :186 (BattleCry 0x3d08dc); tools/difftest_combat_runtime.py:58
// and :275 (HarmTouch 0x3cdb08). That branch's ref_combat_runtime.py svc stubs
// (lines 2041-2045, 2151) answer 0x3d0001..0x3d0003, which are not getter
// addresses; codex/combat-runtime 188bd74 had copied them.
inline constexpr Raw SPELL_ANCESTRAL_CALL_GETTER = 0x3ce9bc,
                     SPELL_BATTLE_CRY_GETTER = 0x3d08dc,
                     SPELL_HARM_TOUCH_GETTER = 0x3cdb08;
// DiseaseCloud_dtor 0x3cd480: research symbols/runtime.tsv names it (slot 1 of
// 0x5186f0, 108 bytes); no doc, ref method or difftest reads the body.
inline constexpr const char *DISEASE_CLOUD_DTOR_REFUSAL =
    "DiseaseCloud_dtor 0x3cd480 (slot 1 of 0x5186f0): body not read";
struct World::State {
  enum class Kind { Creature, Player, Missile, Thrown, Effect, Spell };
  struct Actor : Object {
    bool player = false;
    bool alive = true;
    Kind kind = Kind::Creature;
  };
  struct Overlay;
  struct Cast {
    int skill = 0, level = 1;
    std::optional<Vec3> point;
    std::optional<Raw> facing;
  };
  std::shared_ptr<WorldAuthority> authority;
  Pool &pool;
  HandlePool &handles;
  SharedRandom shared; // borrowed by damage_apply; synchronised with memory
  std::map<Address, Cell *> heads;
  std::map<Address, std::unique_ptr<Cell>> owned_heads;
  std::map<const Link *, Address> links;
  std::map<Address, std::unique_ptr<Actor>> objects;
  std::map<Address, std::unique_ptr<Actor>> spells; // spell records (no object)
  std::vector<std::unique_ptr<Actor>> retired;
  std::shared_ptr<Overlay> overlay;
  creature_hp::Memory &mem;
  A::Model model;
  creature_movement::Model movement;
  Services services;
  Composition composition = Composition::Round1;
  // Round-3 and round-4 bodies are on for Full. The pinned round-2 corpus
  // still records the refusals those bodies replace, so that replay turns
  // both off.
  bool round3 = true, round4 = true;
  std::vector<Event> trace;
  std::map<Address, std::pair<Address, Raw>>
      queued_events; // key is external handle, not actor
  std::map<std::string, Address> class_tables;
  Address fixture_next = FIXTURE_BASE, external_next = 0x90000000;
  int camera_mode = 1;
  bool running = false, failed = false;
  // Iterators are retained after end() because stale node references can read
  // their prev links. The retention is a host choice, not a game rule: an
  // ended iterator that no list, node or live iterator refers to is released
  // (prune_iterators), and only a runaway count of referenced ones is refused.
  std::vector<std::unique_ptr<Iterator>>
      iterators; // preserve retired prev links
  std::set<const Link *> ended_iterators;
  std::size_t prune_at = 4096;
  IteratorRetention retention = IteratorRetention::Lazy;
  void reserve_iterator();
  void end_iterator(Iterator &);
  void prune_iterators();
  std::vector<Iterator *> spell_iterators; // iter_begin/next/end stack
  std::map<Address, std::pair<Raw, Raw>> animations;
  std::array<std::uint8_t, 32> music{};
  std::map<Address, std::vector<Cast>> queued_casts;
  std::map<int, Address> skill_objects;
  // Constructor-verified PhProp model pointers. Binding replaces the current
  // pointer with a World-owned fixture extent, never writes to a resource hint.
  std::map<Address, Address> prop_model_pointers;
  std::map<Address, unsigned> fixture_extents;
  std::vector<std::pair<Raw, Raw>> exp_awards;
  std::size_t stub_box_skips = 0;
  // Class virtual bodies ported outside the arena model, by function address
  // (World::set_class_body). Consulted before any inherited body.
  std::map<Raw, std::function<void(Address, const std::vector<std::uint64_t> &)>> class_hooks;
  bool run_class_hook(Raw fn, Address self, const std::vector<Value> &args);
  Address typeid_target = 0, pending_dtor = 0;
  bool has_typeid_target = false;
  Kind kind_hint = Kind::Missile;
  Address b_stack = 0x01e10000; // ref_skills_b model stack (alloca)
  std::vector<std::unique_ptr<Cell>> retired_cells;
  std::vector<std::pair<Address, unsigned>> arena_stack;
  std::shared_ptr<player::Tick::NamedCallbacks> tick_callbacks;
  explicit State(std::shared_ptr<WorldAuthority>, Services, int, Composition);
  explicit State(Services, int, Composition);
  ~State();
  bool full() const { return composition == Composition::Full; }
  Raw u(Address a) const { return Raw(mem.load(a, 4)); }
  std::uint64_t q(Address a) const { return mem.load(a, 8); }
  Raw h(Address a) const { return Raw(mem.load(a, 2)); }
  Raw b(Address a) const { return Raw(mem.load(a, 1)); }
  std::int32_t s8m(Address a) const { return byte(b(a)); }
  std::int32_t s16m(Address a) const { return s16(h(a)); }
  std::int32_t s32m(Address a) const { return s32(u(a)); }
  void w(Address a, unsigned n, std::uint64_t v) { mem.store(a, n, v); }
  void w(Address a, Raw v) { w(a, 4, v); }
  Vec3 vec(Address a) const { return {u(a), u(a + 4), u(a + 8)}; }
  void vec(Address a, const Vec3 &v) {
    for (unsigned k = 0; k < 3; ++k)
      w(a + 4 * k, v[k]);
  }
  // The twelve-byte copy the compiler emits for a vector assignment.
  void wvec(Address a, const Vec3 &v) {
    w(a, 8, std::uint64_t(v[0]) | (std::uint64_t(v[1]) << 32));
    w(a + 8, 4, v[2]);
  }
  void poke(Address a, unsigned n, std::uint64_t v) { mem.poke(a, n, v); }
  std::string cstr(Address a, unsigned limit = 64) const;
  Actor &actor(Address a) const;
  Actor *find(Address a) const;
  Kind kind_of(Address a) const { return actor(a).kind; }
  Cell &head(Address);
  Address ptr(const Link *) const;
  Address target(Address) const;
  Address alloc_fixture(unsigned);
  Address spawn(const Spawn &, bool, unsigned, int, int);
  Address spawn_class(const std::string &, const Spawn &);
  std::array<Address, 2> spawn_mounted_horseman(unsigned, const Spawn &,
                                                const Spawn &);
  void write_table(Address table, const std::map<unsigned, Raw> *extra = nullptr);
  Address construct(Address addr, const Vec3 &pos, Kind kind);
  void link(Address, Address, Address);
  void clear(Address);
  void relink(Address, Address);
  void remove(Address);
  void grid(Address);
  Address home(Address, bool) const;
  void schedule(Address, unsigned);
  void destroy(Address);
  void destroy_round1(Address);
  void free_block(Address);
  void forget_heads(Address, Address);
  void erase_later(Address);
  void walk(Address, const std::function<void(Address)> &);
  std::vector<Address> box(const Vec3 &, const Vec3 &, int);
  std::pair<Vec3, int> find_free_spot(const Vec3 &, Raw, Raw, Raw);
  void run_objects();
  void tick();
  void run(Address);
  void skelight_run(Address);
  bool own_run(Address, Raw fn);
  void own_class_run(Address, const char *name);
  void ctor_class_run(Address, const std::string &name);
  Address spawn_ctor_class(const std::string &, const Spawn &);
  void plant_ctor_fixtures(const std::string &);
  bool ctor_bodies = false, skeleton_fixture = false, soul_fixture = false;
  Address skeleton_type_name = 0;
  void apply_own_state(Address, const char *name, Raw requested);
  const char *own_set_class(Raw fn) const;
  void player_run(Address);
  void player_tick(Address);
  void alive();
  void domain() const;
  void actor_domain(Address) const;
  void note(std::string name, std::vector<std::uint64_t> args = {});
  void install_services();
  void install_ctor_services();
  // The AntQueen, Skeleton, Soul, Fire, LooseFire and poison-gas destructors
  // (research-combat-ctors-2 combat-ctors-2.md section 6; CT::class_dtor) with
  // the World's handle, iterator and pool services behind their call-outs.
  void class_dtor(Address, Raw fn, Raw flags);
  bool dtor_services = false;
  // The stack descriptor of the hit in progress (AntQueen_run states 7, 9 and
  // 10 pass (0x17, v, v, 0)); 0 for none.
  const combat::Descriptor *hit_descriptor = nullptr;
  // Thrown potions 1 and 2 (combat_runtime_potions.cpp).
  void potion_table(Address table, Raw getter, Address desc, const char *name,
                    std::initializer_list<std::pair<unsigned, Raw>> slots);
  void fx_spawn_fire(const Vec3 &);
  void fire_ctor(Address, const Vec3 &);
  void fire_run(Address);
  void loosefire_ctor(Address, const Vec3 &, Address owner);
  void loosefire_run(Address);
  void loosefire_on_touch(Address, Address other);
  void poison_gas_ctor(Address, const Vec3 &, Raw life, Raw radius, Raw word,
                       Address owner, Raw damage);
  Value service(const std::string &, const std::vector<Value> &);
  Value vcall(Address, unsigned, std::vector<Value> = {});
  Value full_vcall(Address, unsigned, std::vector<Value>);
  Value dispatch_virtual(Address, unsigned, std::vector<Value>);
  bool presentation(const std::string &) const;
  int enemy(Address, Address);
  Raw record(Address) const;
  Raw rec_of(Address) const; // G_PLAYERS + s32(+0x3c8) * size, unchecked
  Raw skill_rank(Raw, Raw) const;
  Raw draw();
  Raw own(Address);
  Raw seed() const { return u(SHARED_SEED); }
  void set_seed(Raw v) { w(SHARED_SEED, v); }
  void take_damage(Address, Address, Raw, Raw);
  int damage(Address, Address, Raw, Raw, Raw, Raw);
  int damage_full(Address, Address, Raw, Raw, Raw, Raw, Raw, Raw, Raw);
  void process_damage(Address);
  void player_take_damage(Address, Address, Raw, Raw);
  void player_process_damage(Address);
  void player_find_target(Address, Raw);
  void player_melee_hit(Address, Address, Raw, Raw);
  void recalc(Address, Raw flag = 0);
  void recalc_weapon(Address, Raw weapon, Raw flag = 0);
  void state3(Address);
  void track_fixup(Address);
  void reaction(Address);
  int can_see(Address, Address, Raw, Raw, Raw, Raw, Raw, Raw);
  int hit_arc(Address, const Arc &, bool);
  Address cone(Address, Raw, Raw, Raw, Raw);
  Raw ray(const Vec3 &, const Vec3 &, Raw, Raw);
  Raw move(Address, const Vec3 &);
  void knockback(Address, Raw);
  void dead_update(Address, Raw, Raw, Raw, const Vec3 &);
  void start_anim(Address, Raw, Raw, Raw, Raw = 0);
  Raw advance_anim(Address, Raw);
  void tables();
  void full_fixtures();
  // ---- movement (combat_runtime_move.cpp) --------------------------------
  Raw move_with_collision(Address, const Vec3 &, Address, Raw);
  Raw player_move(Address, Raw, Address, const Vec3 &);
  Raw speed_scale(Address);
  void player_water(Address, Raw);
  Raw locomotion(Address, Raw);
  Raw player_anim_step(Address, Raw);
  Raw anim_advance_root(Address, Raw, Address);
  struct Clamp {
    Raw result;
    Vec3 velocity;
    Raw mask;
  };
  Clamp clamp_on_image(Address, const Vec3 &, Address);
  Raw clamp_to_screen(Address, Address, Address);
  void screen_planes(Address);
  std::optional<Raw> slow_factor(Address) const;
  Raw camera_yaw(Raw slot);
  Raw camera_flag(Raw slot);
  bool dialog_open();
  // ---- melee (combat_runtime_melee.cpp) ---------------------------------
  void pick_attack_anim(Address, Raw);
  void hit_sparks(Address, Address);
  void melee_hit_full(Address, Address, Raw, Raw, Raw);
  std::int32_t passive_level(Raw slot, std::uint64_t mask) const;
  Raw item_rating(Address item);
  Raw model_of(Address obj);
  Raw type_of(Address obj);
  bool same_type(Raw a, Raw b) const;
  Raw static_type(const std::string &name);
  // ---- defence (combat_runtime_defence.cpp) -----------------------------
  void take_damage_full(Address, Address, Raw, Raw);
  void block_sparks(Address, Raw);
  void process_damage_full(Address);
  void player_death(Address);
  void player_stagger(Address);
  Raw has_anim(Address, const std::string &);
  void interact(Address, Address, Raw);
  Raw resource(Raw lump, const std::string &entry);
  Raw resource_text(const std::string &lump, const std::string &entry);
  Raw resource_at(Raw lump, Address entry);
  // ---- ranged (combat_runtime_ranged.cpp) -------------------------------
  Raw move_and_collide(Address, const Vec3 &);
  void projectile_ctor(Address, const Vec3 &, Address owner);
  void missile_ctor(Address, const Missile &, Address owner);
  void thrown_ctor(Address, const Thrown &, Address owner);
  void missile_run(Address);
  void missile_arrow_run(Address, std::int32_t kind);
  void thrown_run(Address);
  void missile_on_touch(Address, Address);
  Raw missile_nearest_sample(Address, Address);
  void projectile_destroy(Address, Raw);
  Raw obj_update_active(Address, Raw, Raw);
  // ---- monsters (combat_runtime_monsters.cpp) ---------------------------
  int can_see_point(Address, const Vec3 &, Raw, Raw, Raw, Raw);
  int can_see_edge(Address, Address, Raw, Raw, Raw, Raw, Raw, Raw);
  Address find_player_in_cone(Address, Raw, Raw, Raw, Raw);
  int hit_target_at_reach(Address, Raw, Raw, Raw, Raw, Raw, Raw);
  int hit_target_in_arc_edge(Address, Raw, Raw, Raw, Raw, Raw, Raw, Raw, Raw,
                             Raw);
  int hit_target_or_player(Address, Raw, Raw, Raw, Raw, Raw, Raw, Raw, Raw,
                           Raw);
  void knockback_target(Address, Raw);
  void set_cooldown(Address, Raw);
  void set_cooldown_on(Address, Raw, Address);
  void notify_objects_near(Raw, const Vec3 &, Raw, Raw);
  void drop_loot(Address, const Vec3 &, Raw, Raw, Raw, Raw, Raw, Raw, Raw, Raw);
  void goblin_set_state(Address, Raw);
  // ---- spells (combat_runtime_spells.cpp) -------------------------------
  std::vector<Address> collect_in_arc(const Vec3 &, Raw, Address, Address,
                                      Raw, Raw, Raw);
  std::vector<Address> collect_in_sphere(const Vec3 &, Address, Raw);
  std::vector<Address> damage_in_radius(Address, const Vec3 &, Raw, Raw, Raw,
                                        Raw, Raw, Raw);
  std::vector<Address> list(Address head) const;
  Cell &list_head(Address head);
  Address handle_link(Address head, Address back, Address obj);
  Address handle_insert(Address head, Address after, Address obj);
  Address handle_add(Address head, Address obj);
  Object *link_target(Address obj);
  Actor &spell_record(Address);
  std::vector<Address> attached(Address creature) const;
  Address cast(Address caster, const Cast &);
  void run_queued_casts(Address);
  Value skill_virtual(Address skill, unsigned slot, std::vector<Value>);
  Value spell_virtual(Address spell, unsigned slot, std::vector<Value>);
  bool skill_body(Raw fn) const;
  Value object_body(Address obj, Raw fn, unsigned slot, std::vector<Value>);
  void effect_dtor(Raw fn, Address obj, Raw flags);
  Address pool_new(Raw size);
  void pool_free(Address);
  void gameobject_dtor_call(Address obj, Raw flags);
  Address light_effect(const Vec3 &pos, Address follow, Raw life);
  void light_effect_ctor(Address block, const Vec3 &pos, Address follow,
                         Raw life);
  Vec3 obj_ball(Address);
  void puff(Address, Raw definition, const Vec3 &centre, Raw spread, Raw speed);
  Raw holystrike_cast(Address skill, Raw level, Address caster, const Vec3 &,
                      Raw facing);
  void holystrike_run(Address);
  void holystrike_hit(Address, Address);
  Raw diseasebolt_cast(Address skill, Raw level, Address caster, const Vec3 &,
                       Raw facing);
  void diseasebolt_run(Address);
  void diseasebolt_hit(Address, Address);
  Address b_alloca(Raw size);
  Address b_local_vec(const Vec3 &);
  Raw shockoffire_cast(Address skill, Raw level, Address caster, Address pos,
                       Raw facing);
  void sparks_ctor(Address, Address pos, Address velocity, Address owner,
                   Raw tag, Raw low, Raw high, Raw splash, Raw radius);
  void sparks_run(Address);
  void sparks_on_touch(Address, Address);
  Raw waveofflame_cast(Address skill, Raw level, Address caster);
  void waveofflame_spell_run(Address);
  Raw spell_rand(Address);
  Vec3 spell_ball(Address);
  void light_effect_run(Address);
  Raw find_spell_by_type(Address creature, Raw type);
  void update_spells(Address creature, const Vec3 &);
  // Round 3 (research ref_combat_round3, 22649cd).
  Address diseasecloud_spawn(const Vec3 &, Raw level, Address owner);
  void diseasecloud_ctor(Address obj, const Vec3 &, Raw level, Address owner);
  void diseasecloud_run(Address);
  void make_disease_effect(Address block, Address victim, Address attacker,
                           Raw seed_value);
  Address disease_attach(Address victim, Address attacker);
  void gutchunk_ctor(Address, const Vec3 &, Raw kind, Raw bits);
  Address gutchunk_spawn(const Vec3 &, Raw kind, Raw bits);
  void phantom_ctor(Address, Address target, const std::string &name, Raw speed,
                    Raw flag);
  Address phantom_spawn(Address target, const std::string &name, Raw speed,
                        Raw flag);
  void phantom_run(Address);
  // Round 4 (research ref_combat_round4, 95c17ae).
  Vec3 shared_ball_round4();
  Vec3 spell_ball_add(Address spell);
  void potion_run(Address);
  Raw holy_shield_absorb(Address spell);
  Address attach_holy_shield(Address caster, Address skill, Raw level);
  void block_sparks_silent(Address, Raw silent);
  // Round 5 (research ref_combat_round5, 2980af1).
  void fade_bank_tracks(Address creature);
  void update_bank(Address creature);
  int is_demon_near(Address creature);
  Raw holy_shield_sample(Address spell);
  void holy_shield_run(Address spell);
  void multifire_run(Address obj);
  Address arm_fixture_holy_shield(Address player);
  // Round 6 (research ref_combat_round6 at faa7033).
  int is_near_view(Address pos, Raw radius);
  Address spell_block();
  Address spawn_effect_fixture(Raw run, Raw size, const Vec3 &, Address owner);
  void poison_gas_run(Address);
  void disease_corpse_run(Address);
  void multifire_on_hit(Address, Address);
  void multifire_end(Address, Raw reason);
  void smoke_gen_run(Address);
  void pustule_run(Address);
  void arena_run(Address, const Vec3 &);
  Address hornomen_ctor(Raw kind, Address owner, Raw extra);
  void hornomen_run(Address, const Vec3 &);
  Address planarsteel_ctor(Raw kind, Address owner, Raw extra);
  void planarsteel_run(Address, const Vec3 &);
};
// Shared helpers of the round-2 bodies.
Raw dbl_track_weight(Raw w, Raw s, std::uint64_t knee);
int damage_with_descriptor(World::State &, Address victim, Address attacker,
                           Raw attack, Raw flags, const combat::Descriptor &,
                           Raw lo, Raw hi);
void construct_effect(World::State &, Address block, const Vec3 &pos);
} // namespace champions::simulation::rta::combat_runtime
