#include "champions/campaign_fight.hpp"
#include "champions/campaign_actors.hpp"
#include "champions/minotaur.hpp"
#include "champions/combat_ctors.hpp"
#include "champions/campaign_rewards.hpp"
#include "champions/player_state.hpp"
#include "champions/player_name_format.hpp"
#include "champions/script_items.hpp"
#include "champions/script_effects.hpp"
#include "champions/creature_anim/runtime.hpp"
#include "champions/model.hpp"
#include "champions/session_combat_runtime_adapter.hpp"
#include "champions/session_control.hpp"
#include <algorithm>
#include <stdexcept>

namespace champions::simulation::rta::campaign_fight {
namespace C = combat_runtime;
namespace {
// The pad mapping the Full composition fixture uses for the client (combat_play):
// the attack button mask. A host choice, not a recorded original word.
constexpr Raw ATTACK_MASK = ATTACK, BLOCK_MASK = GUARD, SWAP_MASK = GUARD_MOVE;
// Player_ctor's 12 model definitions (research-player-model 7f13899,
// docs/spec/player-model-resources.md): word 0 names the animation LMP, words
// 1..111 (8 bytes apart) name the slot entries.
constexpr std::array<std::uint32_t, 12> PLAYER_DEFINITIONS{
    0x4ecc58, 0x4ec8d0, 0x4ee188, 0x4ede00, 0x4eda78, 0x4ed6f0,
    0x4ee898, 0x4ee510, 0x4ecfe0, 0x4ed368, 0x4eec20, 0x4eefa8};
// Unarmed weapon animation class (docs/player.md: 5 = unarmed). Rows are 0x68
// apart; idle/walk/run layers at +0x44, +0x4c, +0x54, +0x5c
// (research-player-anim cda95dc, player-animation-selection.md section 1-2).
constexpr unsigned UNARMED_CLASS = 5, ROW_SIZE = 0x68;
constexpr std::array<unsigned, 4> BASE_TRACK_OFFSETS{0x44, 0x4c, 0x54, 0x5c};
constexpr std::uint32_t BASE_TRACK_FLAGS = 0x1205;
constexpr Address PLAYER_LUMP_TOKEN = 0xa0000000;
constexpr Raw PLAYER_TOKEN_BASE = 0xb0000000;
constexpr Address NAME_SCRATCH = 0x01f20000;
// Executable string ranges the player tick names by address (every literal
// player.cpp passes to a resource lookup lies in these two ranges).
constexpr std::array<std::pair<std::uint32_t, std::uint32_t>, 2> NAME_RANGES{
    {{0x535800, 0x536300}, {0x5375c0, 0x537d00}}};
constexpr Address PLAYERS = C::PLAYERS, RECORD = C::RECORD_SIZE;

Raw bits_of(std::int32_t v) { return Raw(v); }
} // namespace

void bind_minotaur_boundaries(arena_monsters::Model &m, C::World &world, Counts &counts,
    std::function<void(const std::string &)> refuse) {
  namespace A = arena_monsters;
  auto arg = [](const auto &args, unsigned index) {
    return Raw(std::get<std::int64_t>(args.at(index)));
  };
  m.callbacks["Creature_init"]=[refuse](auto &,const auto &)->A::Value {
      const std::string reason = "Minotaur Creature_init: model bounds height/draw byte unbound";
      refuse(reason);
      throw C::Unsupported(reason);
  };
  m.callbacks["Holder.v11"]=[&world,&counts,refuse,arg](auto &model,const auto &args)->A::Value {
      const std::vector<A::Value> rest(args.begin()+1,args.end());
      return std::int64_t(combat_ctors::holder_slot(model,arg(args,0),11,rest,
          [&world,&counts,refuse](Raw part,Address c,Address,Raw) {
            if (!part || !c || (world.read(c+0xc)&2)) return;
            counts.held_part_requests+=1;
            const std::string reason = "held-part drop: attachment pose and PhProp publication refused_unknown";
            refuse(reason);
            throw C::Unsupported(reason);
          }));
  };
  m.callbacks["Creature_hitTargetInArc"]=[&world,&counts,refuse,arg](auto &,const auto &args)->A::Value {
      // Preserve the executed hit helper's target/radius/terrain gate. Final
      // live descriptor contents and delivered player damage were not traced.
      const auto a=arg(args,0), target=world.target(a+C::TARGET);
      if (!target || !world.can_see(a,target,arg(args,1),arg(args,2),0,
                                      arg(args,8),arg(args,9),0)) return std::int64_t(0);
      counts.minotaur_hit_requests+=1;
      const std::string reason = "Minotaur delivered player damage: refused_unknown (class hit arguments only)";
      refuse(reason);
      throw C::Unsupported(reason);
  };
}

struct Fight::Impl {
  std::shared_ptr<WorldAuthority> authority;
  std::shared_ptr<const arena_modes::Executable> exe;
  player_services::Bundle &bundle;
  std::function<bool()> dialog_open;
  std::shared_ptr<creature_anim::Runtime> anim = std::make_shared<creature_anim::Runtime>();
  struct Dispatch {
    std::function<bool(Address)> run;
  };
  std::shared_ptr<Dispatch> dispatch = std::make_shared<Dispatch>();
  std::unique_ptr<C::World> world;
  std::unique_ptr<C::SessionAdapter> adapter;
  std::unique_ptr<player_death_flow::Flow> death;
  std::string death_error;
  script_commands::Host *host = nullptr;
  std::unique_ptr<campaign_actors::Runtime> actors;
  arena_monsters::Model *creatures = nullptr;
  Counts counts;
  std::map<std::string, std::size_t> refusals;
  bool started = false, failed = false;
  std::string error;
  // resource tokens
  std::map<std::string, Raw> name_token;
  std::map<Raw, std::string> token_name;
  std::map<Raw, creature_anim::ClipId> token_clip;
  std::map<creature_anim::ClipId, Raw> clip_token;
  std::string player_lump;
  Address format_scratch = 0;
  // actors
  std::map<Address, creature_anim::StateId> states;
  std::map<Address, bool> adopted;
  std::vector<Address> adopt_order;
  std::map<std::pair<Raw,Raw>, std::uint64_t> hide_masks;
  // player
  Address player = 0, player_handle = 0;
  creature_anim::StateId player_state = 0;
  bool player_ok = false;
  MovePad move_pad;
  std::string move_refusal;
  Raw death_clip = 0;
  std::size_t death_animation_steps = 0;
  std::uint32_t gold_start = 0;
  std::size_t base_events = 0;
  // rewards of a kill (campaign_rewards)
  std::optional<player_state::Tables> player_tables;
  std::unique_ptr<campaign_rewards::Loot> loot;
  std::vector<GroundDrop> ground_drops;
  std::vector<AwardView> awards;
  campaign_rewards::Memory shared_memory() {
    return {[this](Address a, unsigned n) { return std::uint64_t(world->read(a, n)); },
            [this](Address a, unsigned n, std::uint64_t v) { world->write(a, n, v); }};
  }

  Impl(std::shared_ptr<WorldAuthority> a, std::shared_ptr<const arena_modes::Executable> e,
       player_services::Bundle &b, std::function<bool()> d)
      : authority(std::move(a)), exe(std::move(e)), bundle(b), dialog_open(std::move(d)) {}

  void refuse(const std::string &label) { refusals[label] += 1; }
  void refuse_move(const std::string &label) { move_refusal = label; refuse(label); }
  Raw read(Address a, unsigned n = 4) const { return Raw(world->read(a, n)); }

  // ---- resources ---------------------------------------------------------
  Raw player_token(const std::string &name) {
    auto it = name_token.find(name);
    if (it != name_token.end()) return it->second;
    const Raw token = PLAYER_TOKEN_BASE + 4 * Raw(name_token.size() + 1);
    name_token[name] = token;
    token_name[token] = name;
    return token;
  }
  std::optional<Bytes> player_entry(const std::string &name) {
    if (!host || player_lump.empty()) return std::nullopt;
    auto handle = host->resources().find(player_lump, name);
    if (!handle) return std::nullopt;
    return host->resources().bytes(*handle);
  }
  creature_anim::ClipId clip_for(Raw token) {
    if (!token) return 0;
    auto cached = token_clip.find(token);
    if (cached != token_clip.end()) return cached->second;
    std::optional<Bytes> bytes;
    std::string name;
    if (token >= PLAYER_TOKEN_BASE) {
      auto it = token_name.find(token);
      if (it == token_name.end()) return 0;
      name = it->second;
      bytes = player_entry(name);
    } else if (actors) {
      auto pair = actors->resource_name(token);
      if (!pair) return 0;
      name = pair->second;
      bytes = actors->resource_bytes(token);
    }
    if (!bytes) return 0;
    const auto id = anim->add_clip(std::vector<std::uint8_t>(bytes->data, bytes->data + bytes->size), name);
    token_clip[token] = id;
    clip_token[id] = token;
    return id;
  }

  // ---- animation state <-> memory cells (animation.md 2.1) ----------------
  void store_cells(Address actor, creature_anim::StateId state) {
    const auto &st = anim->state(state);
    world->write(actor + 0x118, 1, std::uint8_t(st.count));
    world->write(actor + 0x119, 1, std::uint8_t(st.capacity));
    world->write(actor + 0x11a, 2, st.flags & 0xffff);
    for (std::int32_t i = 0; i < st.count && std::size_t(i) < st.tracks.size(); ++i) {
      const auto &t = anim->track(st.tracks[std::size_t(i)]);
      const Address cell = actor + 0x11c + 0x20 * Address(i);
      const auto token = clip_token.find(t.clip);
      world->write(cell + 0x00, 4, token == clip_token.end() ? 0 : token->second);
      world->write(cell + 0x04, 4, 0);
      world->write(cell + 0x08, 4, t.weight);
      world->write(cell + 0x0c, 4, t.rate);
      world->write(cell + 0x10, 4, t.life);
      world->write(cell + 0x14, 4, t.life_rate);
      world->write(cell + 0x18, 4, t.time);
      world->write(cell + 0x1c, 2, t.flags & 0xffff);
      world->write(cell + 0x1e, 2, std::uint16_t(t.sound_idx));
    }
  }
  // Player code writes weights, lives and times straight into the cells
  // (Player_move); the native engine owns the clip and slot of each track.
  void load_cells(Address actor, creature_anim::StateId state) {
    const auto &st = anim->state(state);
    for (std::int32_t i = 0; i < st.count && std::size_t(i) < st.tracks.size(); ++i) {
      auto &t = anim->track(st.tracks[std::size_t(i)]);
      const Address cell = actor + 0x11c + 0x20 * Address(i);
      t.weight = read(cell + 0x08);
      t.rate = read(cell + 0x0c);
      t.life = read(cell + 0x10);
      t.life_rate = read(cell + 0x14);
      t.time = read(cell + 0x18);
      t.flags = (t.flags & ~0xffffu) | read(cell + 0x1c, 2);
      t.sound_idx = std::int16_t(read(cell + 0x1e, 2));
    }
  }
  creature_anim::StateId state_of(Address actor) const {
    auto it = states.find(actor);
    return it == states.end() ? 0 : it->second;
  }

  // ---- rewards -------------------------------------------------------------
  // player_giveExp for the session player's record in the shared bytes.
  void award_exp(Raw slot, Raw amount) {
    counts.xp_awards += 1;
    if (!player_tables) throw C::Unsupported("player_giveExp: experience table not bound");
    auto memory = shared_memory();
    const auto r = campaign_rewards::give_exp(*player_tables, memory, std::int64_t(std::int32_t(slot)),
                                              std::int64_t(std::int32_t(amount)));
    counts.xp_gained += std::uint32_t(std::int64_t(std::int32_t(amount)));
    counts.levels_gained += std::size_t(r.levels_gained);
    awards.push_back({std::int32_t(slot), std::int32_t(amount), r.levels_gained, r.level_before, r.level_after,
                      r.experience_before, r.experience_after});
    refuse("player_recalcStats after a level-up: only the maximum hit points and mana (level and effective statistics) are recomputed; the other derived fields are not modelled");
  }
  // loot_generateDrops over the shared seed with the executable's item tables.
  void roll_loot(const C::LootDrop &d) {
    counts.loot_calls += 1;
    // g_levelMonsterLevel 0x4f714c and g_difficultyIdx 0x4f719c are game_loadLevel's
    // stores (research-pickups data-tables.md section 5); nothing in this World
    // sets them (they hold a 1.0 constructor default and zero). They come from
    // the script host's level load, and without them the roll is refused: the
    // shared seed is left untouched and no item is generated.
    const auto globals = host ? host->level_load_globals() : std::nullopt;
    if (!globals) {
      refuse("loot_generateDrops: the level's monster level and the difficulty are not bound (needs the level load at g_difficultyIdx 1); the roll is not made and the shared seed is untouched");
      return;
    }
    if (!loot) {
      const auto reader = [e = exe](std::uint32_t a, std::size_t n) {
        std::vector<std::uint8_t> out;
        out.reserve(n);
        for (std::size_t i = 0; i < n; ++i) out.push_back(e->byte(a + std::uint32_t(i)));
        return out;
      };
      loot = std::make_unique<campaign_rewards::Loot>(script_items::image_tables(reader), shared_memory());
    }
    loot->bind_level_globals(globals->monster_level, globals->difficulty);
    // The reagent filter is a picture-name string address; the boar passes none.
    if (d.filter) throw C::Unsupported("loot_generateDrops: a reagent filter string is not bound");
    const auto out = loot->roll(d, {});
    counts.loot_items += out.items.size();
    counts.loot_shared_draws += out.shared_draws;
    for (const auto &g : out.items) {
      ground_drops.push_back({g.kind, g.category, g.count, false});
      refuse("loot item prop (" + g.kind + "): the ground object is not built; the loot PhProp_ctor's lump/model/texture, part mask, launch flags and scale, and PhProp_initPhysics' inputs are not in the research (the item record and the constructor's one shared draw are kept)");
    }
  }

  // ---- services ----------------------------------------------------------
  C::Services build_services() {
    C::Services base;
    base.damage_permitted = [this](Address victim, Address attacker) {
      if (victim == player && player && actors && actors->find(attacker)) {
        refuse("WarBoar outgoing player damage: normal-route min/max/rating and victim equipment are not established; damage dispatch refused before shared RNG or HP writes");
        return false;
      }
      return true;
    };
    base.death_task = [this] {
      if (death && death_error.empty()) {
        try { death->task_step(); }
        catch (const C::Unsupported &e) {
          death_error = e.what();
          refuse(death_error);
        }
      }
    };
    base.check_all_dead = [this] {
      if (death && death_error.empty()) {
        try { death->check_all_dead(); }
        catch (const C::Unsupported &e) {
          death_error = e.what();
          refuse(death_error);
        }
      }
    };
    base.dialog_is_open = [this] { return dialog_open ? dialog_open() : false; };
    // No gate travel in this session's World (the level script owns gates).
    base.jumpgate_travel_state = [] { return Raw(0); };
    base.skill_stat = [](const std::string &name, Raw, Raw rank) -> Raw {
      // SkillLifeTap_getStat0Fraction 0x2f8e78 = statistic 0 at (slot 14 rank,
      // 1) x 0.01 (research-campaign-combat skills-35-68.md, skill 59), and the
      // statistic getter (slot 10) returns 0 for rank 0 (tools/ref_skills_b.py
      // skill_get_stat): a player without the skill has a 0.0 chance. The
      // session player has no skill ranks, which is the only case answered.
      if (name == "SkillLifeTap_getStat0Fraction" && rank == 0) return 0;
      throw C::Unsupported("skill statistic: skill tables for a nonzero rank are not bound (" + name + ")");
    };
    base.give_exp = [this](Raw slot, Raw amount) { award_exp(slot, amount); };
    base.attach_point = [this](Address actor, Raw, const Vec3 &position, Raw, Raw) {
      refuse("model_getAttachPoint: pose transform not ported (spark origin is the actor position)");
      (void)actor;
      return position;
    };
    base.sample_point_count = [this](Raw) -> Raw {
      refuse("model_getSamplePointCount: sample points not bound (zero reported)");
      return 0;
    };
    base.mesh_joint = [](Raw, Raw) -> Raw {
      throw C::Unsupported("model_getMeshJoint: model data not bound");
    };
    base.resref = [](Raw) -> Raw {
      throw C::Unsupported("resref_get: resource references not bound");
    };
    base.sample_point = [](Raw, Address, const Vec3 &, Raw, Raw, Raw) -> Vec3 {
      throw C::Unsupported("model_getSamplePoint: model geometry not bound");
    };
    base.hide_mask = [this](Raw model, Raw variant) -> std::uint64_t {
      return boar_hide_mask(model,variant);
    };
    base.resource = [this](const C::ResourceQuery &q) -> Raw {
      if (q.lump_is_text || q.lump != PLAYER_LUMP_TOKEN) return 0;
      if (q.entry.empty() || !player_entry(q.entry)) return 0;
      return player_token(q.entry);
    };
    base.format_name = [this](Address format, std::optional<Raw> value) -> Raw {
      if (!format_scratch) throw C::Unsupported("player name format: scratch storage not bound");
      std::string text;
      for (unsigned i = 0; i < 64; ++i) {
        const auto c = read(format + i, 1);
        if (!c) break;
        text += char(c);
      }
      const auto name = player::format_name(text, value
          ? std::optional<std::int32_t>{std::int32_t(*value)} : std::optional<std::int32_t>{});
      for (std::size_t i = 0; i <= name.size(); ++i)
        world->write(format_scratch + Address(i), 1, i < name.size() ? std::uint8_t(name[i]) : 0);
      return format_scratch;
    };
    base.is_class = [](Address, const std::string &) { return false; };
    base.loot_generate = [this](const C::LootDrop &d) { roll_loot(d); };
    base.spawn_named_item = [](const Vec3 &, Raw) -> void {
      throw C::Unsupported("loot_spawnNamedItem: ground items not bound");
    };
    base.particle_group = []() -> Raw {
      throw C::Unsupported("particleGroup_create: not bound");
    };
    base.external_run = [d = dispatch](Address a) { return d->run ? d->run(a) : false; };
    auto services = bundle.services(base);
    // The bundle drives the native animation engine; the player and the
    // creatures read their state back from the memory cells.
    auto start = services.animation_start;
    auto advance = services.animation_advance;
    services.animation_start = [this, start](Address a, Raw clip, Raw flags, Raw blend, Raw out) {
      if (a == player && flags == 0x206 && !clip)
        throw C::Unsupported("player death animation: model/weapon-class clip unavailable");
      if (!clip) return; // res_find returned null: anim_start ignores a null clip
      start(a, clip, flags, blend, out);
      if (a == player && flags == 0x206) death_clip = clip;
      if (auto s = state_of(a)) store_cells(a, s);
      if (a == player) counts.player_animation_starts += 1;
    };
    services.animation_advance = [this, advance](Address a, Raw speed) -> Raw {
      auto s = state_of(a);
      if (s) load_cells(a, s);
      const Raw events = advance(a, speed);
      // The existing Full fixture supplies XY only. This class uses the real
      // native clip advance's yaw too, before Creature_stepAnim's slow scale.
      if (actors && actors->find(a))
        world->write(a + 0xc4, 2, std::uint16_t(bundle.animation_motion(a).yaw));
      if (a == player && read(a + 0x58c) == 4) ++death_animation_steps;
      if (s) store_cells(a, s);
      if (a == player) counts.player_animation_advances += 1;
      return events;
    };
    return services;
  }

  // ---- boar family ---------------------------------------------------------
  std::uint64_t boar_hide_mask(std::uint32_t token, std::uint32_t variant) {
    auto cached = hide_masks.find({token,variant});
    if (cached != hide_masks.end()) return cached->second;
    auto bytes = actors->resource_bytes(token);
    if (!bytes) throw campaign_actors::Unsupported("model_buildHideMask: model resource unavailable");
    // research docs/formats/model-texture.md (model_buildHideMask 0x179550):
    // the show mask starts as the complement of the default-hidden word.
    const auto mask = minotaur::hide_mask(bytes->data,bytes->size,variant);
    hide_masks[{token,variant}] = mask;
    return mask;
  }
  campaign_actors::Services boar_services() {
    campaign_actors::Services s;
    auto W = [this]() -> C::World & {
      if (!world) throw campaign_actors::Unsupported("combat World is not attached");
      return *world;
    };
    s.model_hide_mask = [this](std::uint32_t token, std::uint32_t variant) {
      return boar_hide_mask(token, variant);
    };
    s.minotaur_construct = [this](Raw a,const std::string &type) {
      minotaur::finish_constructor(*creatures,a,type);
    };
    s.minotaur_set_state = [this](Raw a,Raw state) { creatures->vcall(a,27,{std::int64_t(state)}); };
    s.minotaur_run = [this](Raw a) {
      if (auto *actor=actors->find(a); actor && actor->ai_refusal) {
        return;
      }
      minotaur::run(*creatures,a);
    };
    s.animation_init = [this](std::uint32_t addr, bool) {
      // WarBoar_ctor: anim_clear, then anim_push of definition slot 16 (idle1,
      // "idle track def+0x84", flags 1) on the state at +0x114.
      const auto state = anim->add_state(8);
      anim->anim_clear(state);
      const auto token = actors->definition_entry(campaign_actors::Kind::WarBoar, 16);
      const auto clip = clip_for(token);
      if (!clip) throw campaign_actors::Unsupported("anim_push: idle clip resource unavailable");
      anim->anim_push(state, clip, 1);
      states[addr] = state;
      store_cells(addr, state);
    };
    s.set_def_entry_params = [this](std::uint32_t, const campaign_actors::EventParams &p) {
      // creature_setDefEntryParams 0x118378: four (mask, time) event slots of
      // the entry's loaded clip (arena-monsters.md 2.2).
      const auto clip = clip_for(actors->definition_entry(campaign_actors::Kind::WarBoar, p.entry));
      if (!clip) throw campaign_actors::Unsupported("creature_setDefEntryParams: clip unavailable");
      auto &events = anim->clip(clip).events;
      for (unsigned k = 0; k < 4; ++k) {
        events[k].mask = std::uint8_t(p.masks[k]);
        events[k].seconds = p.times[k];
      }
    };
    s.creature_init = [this](std::uint32_t addr) {
      // Creature_init 0x10a230 for a record with no dead/initScript/change/
      // weapon property (the host refuses those). Step 2 (height and draw byte
      // from model_getBounds) is not ported: the base-constructor defaults stay.
      (void)addr;
      refuse("Creature_init step 2: model_getBounds height/draw byte not ported");
    };
    s.handle_clear = [W](std::uint32_t head) { W().clear_handle(head); };
    s.handle_link = [W](std::uint32_t head, std::uint32_t target) { W().relink(head, target); };
    s.process_damage = [W](std::uint32_t a) { W().process_damage(a); };
    s.creature_run = [W](std::uint32_t a) { return W().creature_run(a); };
    s.update_active = [W](std::uint32_t a) { return W().creature_update_active(a); };
    s.animation_step = [W](std::uint32_t a) { return W().creature_step_anim(a, ONE); };
    s.knockback = [W](std::uint32_t a) { W().apply_knockback(a, 0x3e4ccccc); };
    s.pick_target = [W](std::uint32_t a) { W().pick_target(a); };
    s.can_see_target = [W](Address a, Raw force) { return W().creature_can_see_target(a, force); };
    s.target_in_arc = [W](Address a, Raw f, Raw arc, Raw near, Raw far) {
      const auto target = W().target(a + 0x234);
      return target && W().can_see(a, target, f, arc, 0, near, far, 0);
    };
    s.enemy_in_cone = [W](Address a, Raw f, Raw arc, Raw near, Raw far) {
      return W().find_enemy_in_cone(a, f, arc, near, far) != 0;
    };
    s.away_from_home = [W](Address a) { return W().creature_away_from_home(a); };
    s.clear_path = [W](Address a) { W().creature_clear_path(a); };
    s.pick_wander_dest = [W](Address a) { W().creature_pick_wander_dest(a); };
    // A missing path service refuses the affected move, retaining its exact
    // mutation prefix. Keep sensing/reactions running so an actor can still
    // attack or recover on a later tick; never substitute a straight-line path.
    s.move_to_dest = [this, W](Address a) {
      try { W().creature_move_to_dest(a); }
      catch (const C::Unsupported &e) { refuse(std::string("WarBoar wander: ") + e.what()); }
    };
    s.move_to_target = [this, W](Address a) {
      try { W().creature_move_to_target(a); }
      catch (const C::Unsupported &e) { refuse(std::string("WarBoar chase: ") + e.what()); }
    };
    s.flee_from_target = [this, W](Address a, Raw kind) {
      try { W().creature_flee_from_target(a, kind); }
      catch (const C::Unsupported &e) { refuse(std::string("WarBoar retreat: ") + e.what()); }
    };
    s.turn_to_target = [W](Address a) { W().creature_turn_to_target(a); };
    s.move_zero = [W](Address a) { W().move_creature(a, {}); };
    s.hit_all_in_arc = [this, W](std::uint32_t a, std::uint32_t flags) {
      counts.boar_hit_events += 1;
      C::Arc arc;
      arc.facing = Raw(W().read(a + 0x38, 2));
      arc.angle = 3000;
      arc.minimum = Raw(std::int16_t(W().read(a + 0xd0, 2)));
      arc.maximum = Raw(std::int16_t(W().read(a + 0xd2, 2)));
      arc.attack = Raw(std::int16_t(W().read(a + 0xd4, 2)));
      arc.flags = flags;
      arc.near = 0;
      arc.far = 0x41c80000; // 25.0
      W().hit_all_in_arc(a, arc);
    };
    s.filter_state = [W](std::uint32_t a, std::uint32_t state) {
      return W().creature_filter_state_result(a, state);
    };
    s.animation_start = [this, W](Address a, const campaign_actors::AnimationRequest &r) {
      const auto state = state_of(a);
      if (!state) throw campaign_actors::Unsupported("WarBoar animation state is not bound");
      const auto token = actors->definition_entry(campaign_actors::Kind::WarBoar,
                                                 (r.definition_offset - 4) / 8);
      const auto clip = clip_for(token);
      if (!clip) throw campaign_actors::Unsupported("WarBoar animation clip unavailable");
      anim->set_sim_frame(Raw(W().read(C::FRAME)));
      if (r.blend_out) anim->anim_start_blend_in_out(r.blend_in, r.blend_out, state, clip, r.flags);
      else anim->anim_start(r.blend_in, state, clip, r.flags);
      if (r.sync) anim->anim_sync_to_sim_frame(state);
      store_cells(a, state);
      counts.boar_state_changes += 1;
      if (r.blend_out && (r.flags & 8)) counts.boar_attack_entries += 1;
    };
    s.dead_update = [W](Address a, Raw events) { W().dead_update(a, events, 0, 0x41f00000); };
    s.death_animation = [this](std::uint32_t) {
      refuse("WarBoar_setState(0x14): death-animation clip choice from the own draw not documented");
    };
    s.on_death = [W](std::uint32_t a) { W().creature_on_death(a, 0); };
    s.creature_drop_loot = [this, W](std::uint32_t a, const campaign_actors::LootArgs &args) {
      const Vec3 position{Raw(W().read(a + 0x10)), Raw(W().read(a + 0x14)), Raw(W().read(a + 0x18))};
      W().drop_loot(a, position, Raw(W().read(a + 0x38, 2)), args.filter, args.z,
                    args.throw_speed, args.gold_scale, args.gold_chance, args.quality_row,
                    args.item_chance);
      counts.deaths += 1;
    };
    return s;
  }

  void bind_minotaur_services() {
    namespace A=arena_monsters;
    creatures=&world->creature_services();
    auto &m=*creatures;
    auto arg=[](const auto &args,unsigned k) { return Raw(std::get<std::int64_t>(args.at(k))); };
    m.callbacks["anim_clear"]=[this,arg](auto &,const auto &args)->A::Value {
      const auto a=arg(args,0)-0x114;
      const auto st=anim->add_state(8);
      states[a]=st;
      anim->anim_clear(st);
      bundle.bind_actor(a,st);
      store_cells(a,st);
      return std::int64_t(0);
    };
    m.callbacks["anim_syncToSimFrame"]=[this,arg](auto &,const auto &args)->A::Value {
      const auto a=arg(args,0)-0x114;
      const auto st=state_of(a);
      if (!st) throw C::Unsupported("animation sync: actor state missing");
      anim->set_sim_frame(read(C::FRAME));
      anim->anim_sync_to_sim_frame(st);
      store_cells(a,st);
      return std::int64_t(0);
    };
    m.callbacks["anim_findTrack"]=[this,arg](auto &,const auto &args)->A::Value {
      const auto st=state_of(arg(args,0)-0x114);
      const auto clip=clip_for(arg(args,1));
      if (!st || !clip) return std::int64_t(0);
      return std::int64_t(anim->anim_find_track(st,clip));
    };
    m.callbacks["creature_setDefEntryParams"]=[this,arg](auto &,const auto &args)->A::Value {
      const auto a=arg(args,0), entry=arg(args,1);
      const auto clip=clip_for(read(read(a+0x24)+8*entry+4));
      if (!clip) throw C::Unsupported("Minotaur event clip unavailable");
      auto &events=anim->clip(clip).events;
      for (unsigned k=0;k<4;++k) {
        events[k].mask=std::uint8_t(arg(args,2+k));
        events[k].seconds=arg(args,6+k);
      }
      return std::int64_t(0);
    };
    bind_minotaur_boundaries(m, *world, counts, [this](const auto &label) { refuse(label); });
    m.callbacks["Creature_dropLoot"]=[this,arg](auto &,const auto &args)->A::Value {
      world->drop_loot(arg(args,0),std::get<Vec3>(args.at(1)),arg(args,2),arg(args,3),
          arg(args,4),arg(args,5),arg(args,6),arg(args,7),arg(args,8),arg(args,9));
      return std::int64_t(0);
    };

    world->define_creature_table(minotaur::VTABLE,{{4,minotaur::RUN},{5,minotaur::DRAW},{27,minotaur::SET_STATE}});
    // Preserve the selected executable's table adjustments and unported
    // slots too. Unknown function bodies still refuse on dispatch.
    for (Raw table:{minotaur::VTABLE,0x50cda0u}) {
      const unsigned count=table==minotaur::VTABLE?39:25;
      for(unsigned slot=0;slot<count;++slot) {
        world->write(table+8*slot,2,exe->word(table+8*slot)&0xffff);
        world->write(table+8*slot+4,4,exe->word(table+8*slot+4));
      }
    }
    world->set_class_body(minotaur::SET_STATE,[this](Address a,const auto &args) {
      const auto before=read(a+0x232,2);
      minotaur::set_state(*creatures,a,args.empty()?0:Raw(args[0]));
      if (before!=20 && read(a+0x232,2)==20) counts.deaths+=1;
    });
  }

  void build_boar(const script_commands::Host::CreatureRequest &request) {
    if (!actors) throw campaign_actors::Unsupported("boar runtime is not installed");
    const bool mino=request.class_name.compare(0,8,"Minotaur")==0;
    const auto kind = mino ? campaign_actors::Kind::Minotaur : request.small ? campaign_actors::Kind::WarBoarSmall
                                    : campaign_actors::Kind::WarBoar;
    // Rank: the record carries no boss rank in this host (Creature_ctor's
    // bossRank is 0 for an unnamed creature); death loot refuses otherwise.
    if (mino) {
      // This unbound final Creature_ctor service must fail before the class
      // constructor writes definition, holder or runtime registration state.
      creatures->callbacks.at("Creature_init")(*creatures, {std::int64_t(request.address)});
      world->adopt_creature(request.address,true);
      try { actors->adopt(kind,*request.object,request.angle,std::int64_t(0)); }
      catch (...) { world->unadopt_creature(request.address); throw; }
      adopted[request.address]=true;
      counts.minotaurs_built+=1;
    } else actors->adopt(kind, *request.object, request.angle, std::int64_t(0));
    counts.actors_built += 1;
  }

  // game_loadLevel 0x1e55c0 stores row fields of g_levelTable 0x4f7210 (0x30
  // bytes per row, null name ends it and is the default row): +0x08 kill plane
  // height -> 0x4f7188, +0x20 level flag word -> 0x44fe08 (research
  // docs/data-tables.md 7.1, Observed). The three-pass lookup is 7.1's: a row
  // equal to the name, then a row starting with it, then a row inside it.
  void level_globals(const std::string &name) {
    if (!exe || name.empty()) {
      refuse("g_levelTable row unavailable: kill plane and level flags stay unset");
      return;
    }
    std::vector<std::pair<std::uint32_t, std::string>> rows;
    std::uint32_t at = 0x4f7210;
    for (unsigned i = 0; i <= 90; ++i, at += 0x30) {
      const auto pointer = exe->word(at);
      rows.emplace_back(at, pointer ? exe->string(pointer) : std::string());
      if (!pointer) break;
    }
    std::uint32_t row = rows.back().first; // the terminator is the default row
    bool found = false;
    for (const auto &r : rows)
      if (!r.second.empty() && r.second == name) { row = r.first; found = true; break; }
    if (!found)
      for (const auto &r : rows)
        if (!r.second.empty() && r.second.compare(0, name.size(), name) == 0) { row = r.first; found = true; break; }
    if (!found)
      for (const auto &r : rows)
        if (!r.second.empty() && name.find(r.second) != std::string::npos) { row = r.first; found = true; break; }
    world->write(0x4f7188, 4, exe->word(row + 0x08));
    world->write(0x44fe08, 4, exe->word(row + 0x20));
  }

  // ---- player ----------------------------------------------------------------
  std::string setup_player(const PlayerStart &start) {
    if (!exe) return "player: executable not bound";
    const auto lump = player_animation_lump(*exe, start.model_index);
    if (lump.empty()) return "player: model definition has no animation lump";
    player_lump = lump;
    // The session's character record is the real one (script host binder).
    C::Spawn spawn;
    spawn.position = start.position;
    spawn.facing = start.facing & 0xffff;
    spawn.hp = start.hp;
    spawn.team = 0;
    spawn.session_record = true;
    level_globals(start.level_name);
    if (!lt(0, read(PLAYERS + 0xac))) {
      // No character record bound (the script host's binder wrote none): the
      // scene's health stands in so the player is alive.
      world->write(PLAYERS + 0xac, 4, start.hp);
      world->write(PLAYERS + 0xb0, 4, start.hp);
      refuse("no character record: player hit points taken from the scene");
    }
    player = world->spawn_player(0, spawn);
    player_handle = world->retain(player);
    refuse("net_getCamFlag: host-sourced (1 only while a script dialog is open; inventory, shop and cutscene camera modes not covered)");
    refuse("net_getCamYaw: host-sourced (the Godot sandbox camera's yaw, not Camera_getYaw; movement is relative to the host camera)");
    // The initializer's definition addresses are executed for bodies 0-11 at
    // seeds 7 and 20973 and kinds -1 and 1
    // (366b65a:docs/starting-equipment-r4.md:43-60). This fight does not run
    // that initializer, and the route's seed, kind and saved inventory are
    // not those controls. Weapon class 5 stays the unarmed stand-in. Damage
    // or kill results in this mode are not campaign evidence.
    refuse("starting_equipment: refused_unknown (player_giveStartingItems not ported; weapon class 5 unarmed stand-in, not campaign evidence)");
    const Address def = read(player + 0x24);
    const auto table = PLAYER_DEFINITIONS.at(start.model_index);
    // The block the player's slot words are read from: lump word, then for
    // slot k (1..111): name pointer and the resolved clip identity.
    world->write(def, 4, PLAYER_LUMP_TOKEN);
    Address scratch = NAME_SCRATCH;
    for (unsigned slot = 1; slot <= 111; ++slot) {
      const auto name = exe->string(exe->word(table + slot * 8));
      for (std::size_t i = 0; i <= name.size(); ++i)
        world->write(scratch + Address(i), 1, i < name.size() ? std::uint8_t(name[i]) : 0);
      world->write(def + 8 * slot, 4, scratch);
      world->write(def + 8 * slot + 4, 4, player_entry(name) ? player_token(name) : 0);
      scratch += Address((name.size() + 16) & ~std::size_t(15));
    }
    // Reuse the end of this host-owned string area, after all definition names.
    // Each Player sprintf result is consumed immediately by res_find/sound.
    format_scratch = scratch;
    // The strings the player tick names by executable address.
    for (const auto &range : NAME_RANGES)
      for (std::uint32_t a = range.first; a < range.second; ++a) {
        std::uint8_t byte = 0;
        try {
          byte = exe->byte(a);
        } catch (const std::exception &) {
        }
        if (byte) world->write(a, 1, byte);
      }
    player_state = anim->add_state(8);
    bundle.bind_actor(player, player_state);
    states[player] = player_state;
    reset_idle_anim();
    player_ok = true;
    gold_start = read(PLAYERS + 0xe4);
    return {};
  }
  // player_resetIdleAnim 0x1b2470 (research-player-anim cda95dc,
  // player-animation-selection.md section 2): idle, walk, run 1 and run 2 of
  // the weapon-class row into cells 0..3.
  void reset_idle_anim() {
    if (!player || (std::int32_t)read(PLAYERS + 0xac) <= 0 && !lt(0, read(PLAYERS + 0xac))) return;
    const Address def = read(player + 0x24);
    anim->anim_clear(player_state);
    for (unsigned i = 0; i < BASE_TRACK_OFFSETS.size(); ++i) {
      const auto token = read(def + UNARMED_CLASS * ROW_SIZE + BASE_TRACK_OFFSETS[i]);
      const auto clip = clip_for(token);
      if (!clip) {
        refuse("player_resetIdleAnim: base clip resource unavailable");
        continue;
      }
      anim->anim_push_with_weights(player_state, clip, BASE_TRACK_FLAGS, i == 0 ? ONE : 0, 0,
                                   i == 0 ? ONE : 0, 0);
    }
    anim->anim_release_caches(player_state);
    store_cells(player, player_state);
  }
};

Fight::Fight(std::shared_ptr<WorldAuthority> authority,
             std::shared_ptr<const arena_modes::Executable> exe, player_services::Bundle &bundle,
             std::function<bool()> dialog_open)
    : p_(std::make_unique<Impl>(std::move(authority), std::move(exe), bundle, std::move(dialog_open))) {
  auto &p = *p_;
  p.bundle.bind_animation(*p.anim, [impl = p_.get()](Address, Raw clip) { return impl->clip_for(clip); });
  // The campaign host supplies live player positions to updateActive. Focus
  // mode 0 instead retains the fixture's spawn-time focus after diagnostic
  // placement or ordinary movement, leaving distant level creatures asleep.
  // Mode 1 is the existing per-player visibility-input path, not an AI radius.
  p.world = std::make_unique<C::World>(p.authority, p.build_services(), 1, C::Composition::Full);
  p.adapter = std::make_unique<C::SessionAdapter>(*p.world);
  p.adapter->bind_button(0, C::World::Button::Attack, ATTACK_MASK);
  p.adapter->bind_button(0, C::World::Button::Block, BLOCK_MASK);
  p.adapter->bind_button(0, C::World::Button::Swap, SWAP_MASK);
  p.bundle.set_local_slot(0);
  p.dispatch->run = [impl = p_.get()](Address a) -> bool {
    auto &q = *impl;
    if (q.actors)
      if (auto *actor = q.actors->find(a)) {
        (void)actor;
        try {
          q.actors->tick(a);
        } catch (const std::exception &e) {
          q.refuse(std::string("WarBoar tick: ") + e.what());
        }
        return true;
      }
    if (q.authority->is_factory(a)) {
      // A factory creature of a class this fight has no tick port for.
      q.counts.factory_runs_skipped += 1;
      return true;
    }
    return false;
  };
}
Fight::~Fight() {
  p_->dispatch->run = {};
}
void Fight::install(script_commands::Host &host) {
  auto &p = *p_;
  p.host = &host;
  p.actors = std::make_unique<campaign_actors::Runtime>(*p.authority->objects, host.resources(),
                                                       p.boar_services(), &p.authority->memory);
  p.actors->set_contain_refusals(true);
  if (!p.exe) throw C::Unsupported("Minotaur definition: executable unavailable");
  std::vector<std::string> entries;
  for (unsigned index=0;index<26;++index)
    entries.push_back(p.exe->string(p.exe->word(minotaur::DEFINITION+8+8*index)));
  if (p.exe->word(minotaur::DEFINITION+8+8*26))
    throw C::Unsupported("Minotaur definition: unsupported resource count");
  p.actors->bind_minotaur_definition(p.exe->string(p.exe->word(minotaur::DEFINITION)),entries);
  p.bind_minotaur_services();
  host.set_creature_builder([impl = p_.get()](const script_commands::Host::CreatureRequest &r) {
    impl->build_boar(r);
  });
}
std::string Fight::player_animation_lump(const arena_modes::Executable &exe, unsigned model_index) {
  if (model_index >= PLAYER_DEFINITIONS.size()) return {};
  try {
    return exe.string(exe.word(PLAYER_DEFINITIONS[model_index]));
  } catch (const std::exception &) {
    return {};
  }
}
std::string Fight::start(const PlayerStart &start) {
  auto &p = *p_;
  if (p.started) return "fight already started";
  p.refuse("WarBoar live event spacing/root trajectory parity: no normal-route capture; only native clip advancement is executed");
  p.refuse("WarBoar script/event-driven group activation: Unknown; no pack activation is synthesized");
  try {
    // Documented WarBoar vtable (handoff round 2, monsters-2.md): the Creature
    // base slots with run (slot 4) and setState (slot 27) overridden; slot 2
    // is the inherited Creature_takeDamage 0x116638.
    p.world->define_creature_table(0x517cf0, {{4, 0x2ba3a0}, {11, 0x2bac58}, {27, 0x2bac60}});
    p.world->set_class_body(0x2bac58, [](Address, const std::vector<std::uint64_t> &) {});
    p.world->set_class_body(0x2bac60, [impl = p_.get()](Address self, const std::vector<std::uint64_t> &args) {
      auto &q = *impl;
      const auto state = args.empty() ? 0u : std::uint32_t(args[0]);
      try {
        q.actors->set_state(self, state);
      } catch (const std::exception &e) {
        q.refuse("WarBoar_setState(" + std::to_string(state) + "): " + e.what());
      }
    });
    if (p.actors)
      for (const auto &entry : p.actors->actors()) {
        if (!p.adopted[entry.first]) p.world->adopt_creature(entry.first);
        if (p.states.count(entry.first)) p.bundle.bind_actor(entry.first, p.states[entry.first]);
      }
  } catch (const std::exception &e) {
    p.error = e.what();
    p.failed = true;
    return e.what();
  }
  // The experience thresholds and class formulas come from the executable at
  // run time. The 81 table words are also put where Creature_processDamage
  // reads the per-kill cap (0x41b558).
  try {
    if (!p.exe) throw std::runtime_error("executable not bound");
    p.player_tables = player_state::read_tables(*p.exe);
    for (std::size_t level = 0; level < p.player_tables->experience.size(); ++level)
      p.world->write(0x41b558 + 4 * Address(level), 4, std::uint32_t(p.player_tables->experience[level]));
  } catch (const std::exception &e) {
    p.refuse(std::string("experience: tables unavailable (") + e.what() + "); kills award nothing");
  }
  const auto player_error = [&]() -> std::string {
    try {
      return p.setup_player(start);
    } catch (const std::exception &e) {
      return std::string("player: ") + e.what();
    }
  }();
  p.started = true;
  player_death_flow::Services death_services;
  death_services.frozen = [&p] {
    if (p.read(0x4f6708) || p.read(0x506048))
      throw C::Unsupported("death flow: offline controller-out/warm-up freeze unsupported; task-name freeze service is not bound");
    return p.read(0x41b4f4) != 0;
  };
  death_services.close_menus = [&p] {
    ++p.counts.close_menu_requests;
  };
  death_services.clear_fade = [&p] {
    if (!p.host) throw C::Unsupported("death flow: script HUD host unavailable");
    p.host->set_fade_level(0);
  };
  death_services.step_fade = [&p](Raw delta) {
    if (!p.host) throw C::Unsupported("death flow: script HUD host unavailable");
    script_effects::Effects effects(p.host->variables().memory);
    effects.discarded.insert("ui_drawRect"); // renderer consumes the native fade observation
    return effects.step_fade(delta);
  };
  death_services.find_act = [&p, level = start.level_name] {
    if (!p.exe) throw C::Unsupported("death flow: save_findAct executable unavailable");
    const auto count = std::int32_t(p.exe->word(0x4f8e7c));
    if (count < 0 || count > 64) throw C::Unsupported("death flow: invalid act table count");
    for (int act = 0; act < count; ++act) {
      const auto list = p.exe->word(0x4f8e48 + 4 * act);
      const auto rows = std::int32_t(p.exe->word(0x4f8e10 + 4 * act));
      if (rows < 0 || rows > 256) throw C::Unsupported("death flow: invalid act row count");
      for (int row = 0; row < rows; ++row) {
        const auto name = p.exe->word(list + 8 * row);
        if (name && p.exe->string(name) == level) return act;
      }
    }
    return -1;
  };
  death_services.reload_checkpoint = [] {
    throw C::Unsupported("save_reload refused: no valid in-memory checkpoint (complete saved player records, level, pose, quests and gates); populated world/decal restore and camera load unsupported; invalid-buffer behavior Unknown");
  };
  death_services.request_current_level = [] {
    throw C::Unsupported("same-level restart refused: death level-loader/object restoration is not bound; current records must be retained and HP restored only with a valid level request");
  };
  p.death = std::make_unique<player_death_flow::Flow>(
      player_death_flow::Memory{
          [&p](Address address, unsigned size) { return p.world->read(address, size); },
          [&p](Address address, unsigned size, std::uint64_t value) { p.world->write(address, size, value); }},
      std::move(death_services));
  p.death->level_enter(p.player_ok ? 1 : 0);
  p.world->write(0x4f85b0, 4, 1);
  if (!player_error.empty()) p.refuse(player_error);
  return player_error;
}
void Fight::tick(std::uint32_t command) {
  auto &p = *p_;
  if (!p.started || p.failed) return;
  try {
    const auto frame = Raw(p.world->read(C::FRAME));
    if (p.player_ok) {
      auto axis = [&](std::uint32_t pos, std::uint32_t neg) {
        return int((command & pos) ? 1 : 0) - int((command & neg) ? 1 : 0);
      };
      const int x = axis(session_control::POS_X, session_control::NEG_X);
      const int y = axis(session_control::POS_Y, session_control::NEG_Y);
      // Client stick mapping of the Full fixture: one axis is +-1.0, a diagonal
      // +-0.70710677 per axis (authored host choice, not an original value).
      const Raw unit = (x && y) ? Raw(0x3f3504f3) : ONE;
      auto value = [&](int v) { return v > 0 ? unit : v < 0 ? (unit | 0x80000000u) : Raw(0); };
      const auto moves = p.move_pad.update(command);
      if (moves.requests & ~MOVEMENT) p.move_refusal.clear();
      C::SessionAdapter::PadInput in;
      in.stick_x = value(x);
      in.stick_y = value(y);
      in.held = moves.held;
      in.pressed = moves.pressed;
      if (in.pressed & ATTACK_MASK) p.counts.attack_edges += 1;
      if (moves.requests & HEAVY)
        p.refuse_move("heavy_attack: refused_unknown (old inline ability 0x25 requires a usable learned record and documented mana cost; not a universal attack button)");
      // Player_run reads the two selected ability records (+0x4a84/+0x4a85)
      // before the action switch. Their campaign handoff is not bound. Do not
      // borrow a fixture skill or conflate an old-inline id with a Skill id.
      if (moves.requests & ABILITY1)
        p.refuse_move("ability_slot_1: refused_unknown (selected learned record, rank, cost and campaign Skill binding are not established)");
      if (moves.requests & ABILITY2)
        p.refuse_move("ability_slot_2: refused_unknown (selected learned record, rank, cost and campaign Skill binding are not established)");
      if (moves.requests & GUARD) {
        const auto two = p.read(PLAYERS + 0x43f8);
        if ((two && p.read(p.read(two) + 0x1e, 1) == 4) ||
            (!two && !p.read(PLAYERS + 0x43f0) && !p.read(PLAYERS + 0x43fc)))
          p.refuse_move("guard: unavailable (Player_canBlock requires equipped weapon or shield; unarmed stand-in cannot guard)");
        if (p.read(0x44fe08) & 0x10000000)
          p.refuse_move("guard: unavailable (level veto 0x10000000)");
      }
      if (moves.requests & GUARD_MOVE) {
        if (p.read(0x44fe08) & 0x10000000)
          p.refuse_move("guard_move: unavailable (level veto 0x10000000)");
        else if (!(moves.held & GUARD) || !p.read(p.player + 0x47f, 1))
          p.refuse_move("guard_move: unavailable (hold an established guard before pressing swap; no universal dodge)");
      }
      p.adapter->pad_input(0, in);
      // net_getCamFlag offline is "the camera is not in follow mode"
      // (docs/spec/movement-camera.md 7.1, `mode != 1`). Mode 1 holds except
      // while this machine's inventory, shop, dialog camera or a cutscene is up
      // (research-ui docs/monsters-2.md M6). HOST-SOURCED: only the dialog case
      // is covered, so the flag is 1 only while a dialog is open here; the
      // inventory, shop and cutscene modes do not set it (labelled at start).
      p.bundle.set_camera_flag(0, p.dialog_open && p.dialog_open() ? 1 : 0);
    }
    p.world->clear_history();
    p.adapter->ready_tick(frame);
    p.counts.ticks += 1;
    for (const auto &event : p.world->events()) {
      if (event.name == "player_dead") {
        ++p.counts.player_deaths;
        ++p.counts.close_menu_requests; // Player_processDamage closes local menus immediately
      }
      if (event.name == "damage_apply" && event.args.size() >= 2 && event.args[1] == p.player &&
          p.player)
        p.counts.player_hits += 1;
      if (event.name == "damage_result" && event.args.size() == 2 && event.args[1] != 0)
        p.counts.damage_events += 1;
    }
    if (p.player_ok) {
      const auto state = std::int32_t(p.read(p.player + 0x58c));
      if (state == 1) p.counts.player_swings += 1;
    }
  } catch (const std::exception &e) {
    p.failed = true;
    p.error = e.what();
    p.refuse(std::string("World tick stopped: ") + e.what());
  }
}
void Fight::release_input() {
  auto &p = *p_;
  p.move_pad = MovePad{};
  if (p.player_ok && !p.failed) p.adapter->pad_input(0, {});
}

// DIAGNOSTIC ONLY: unchecked write (no terrain or bounds test); see the header.
std::string Fight::place_player(const Vec3 &position, Raw facing) {
  auto &p = *p_;
  if (!p.player_ok) return "no player";
  for (unsigned k = 0; k < 3; ++k) p.world->write(p.player + 0x10 + 4 * k, 4, position[k]);
  p.world->write(p.player + 0x38, 2, facing & 0xffff);
  p.world->update_grid(p.player);
  return {};
}
std::string Fight::restore_health(Raw hp) {
  auto &p = *p_;
  if (!p.player_ok) return "no player";
  if ((p.read(p.player + 12) & 8) || p.read(p.player + 0x58c) == 4)
    return "dead player cannot be healed in place: player_respawn/object replacement is required";
  p.world->write(p.player + 0x228, 4, hp);
  p.world->write(PLAYERS + 0xac, 4, hp);
  return {};
}
bool Fight::started() const { return p_->started; }
bool Fight::player_ready() const { return p_->player_ok; }
bool Fight::failed() const { return p_->failed; }
const std::string &Fight::error() const { return p_->error; }
PlayerView Fight::player() const {
  PlayerView v;
  auto &p = *p_;
  if (!p.player_ok) return v;
  v.present = true;
  v.position = {p.read(p.player + 0x10), p.read(p.player + 0x14), p.read(p.player + 0x18)};
  v.facing = p.read(p.player + 0x38, 2);
  v.hp = p.read(PLAYERS + 0xac);
  v.max_hp = p.read(PLAYERS + 0xb0);
  v.action_state = std::int32_t(p.read(p.player + 0x58c));
  v.weapon_class = std::int8_t(p.read(PLAYERS + 0x4a86, 1));
  v.combo_step = std::int8_t(p.read(p.player + 0x460, 1));
  v.attack_variant = std::int8_t(p.read(p.player + 0x461, 1));
  v.attack_kind = std::int32_t(p.read(p.player + 0x44c));
  v.blocking = p.read(p.player + 0x47f, 1) != 0;
  v.move_refusal = p.move_refusal;
  v.gold = p.read(PLAYERS + 0xe4);
  v.level = std::int8_t(p.read(PLAYERS + 0x59, 1));
  v.experience = std::int32_t(p.read(PLAYERS + 0x5c));
  v.skill_points = std::int32_t(p.read(PLAYERS + 0xe8));
  v.stat_points = std::int32_t(p.read(PLAYERS + 0xec, 1));
  v.mana = p.read(PLAYERS + 0xc0);
  v.max_mana = p.read(PLAYERS + 0xc4);
  return v;
}
std::vector<EnemyView> Fight::enemies() const {
  std::vector<EnemyView> out;
  auto &p = *p_;
  if (!p.actors || !p.world) return out;
  for (const auto &entry : p.actors->actors()) {
    const auto &actor = *entry.second;
    if (!actor.object->alive) continue;
    EnemyView v;
    v.address = entry.first;
    v.small = actor.kind == campaign_actors::Kind::WarBoarSmall;
    v.minotaur = actor.kind == campaign_actors::Kind::Minotaur;
    v.scale = p.read(entry.first+0xdc);
    v.model_mask = actor.hp.mem.load(entry.first+0x220,8);
    if (auto name=p.actors->resource_name(p.actors->definition_entry(actor.kind,1))) {
      v.model_lump=name->first;
      v.model_entry=name->second;
    }
    if (auto name=p.actors->resource_name(p.actors->definition_entry(actor.kind,2)))
      v.texture_entry=name->second;
    v.hp = p.read(entry.first + 0x228);
    v.max_hp = p.read(entry.first + 0x22c);
    v.dead = actor.dead();
    v.ai_frozen = actor.ai_refusal.has_value();
    v.state = p.read(entry.first + 0x232, 2);
    v.position = {p.read(entry.first + 0x10), p.read(entry.first + 0x14), p.read(entry.first + 0x18)};
    v.facing = p.read(entry.first + 0x38, 2);
    out.push_back(v);
  }
  return out;
}
const Counts &Fight::counts() const { return p_->counts; }
const std::vector<AwardView> &Fight::awards() const { return p_->awards; }
const std::vector<GroundDrop> &Fight::ground_drops() const { return p_->ground_drops; }
const std::map<std::string, std::size_t> &Fight::refusals() const {
  static std::map<std::string, std::size_t> merged;
  merged = p_->refusals;
  if (p_->world && p_->world->stub_box_skips())
    merged["unconstructed host stub left out of a collision/target box (class not ported)"] += p_->world->stub_box_skips();
  if (p_->actors)
    for (const auto &entry : p_->actors->refusal_counts()) merged["Creature AI: " + entry.first] += entry.second;
  return merged;
}
combat_runtime::World &Fight::world() { return *p_->world; }
const player_death_flow::Flow &Fight::death_flow() const {
  if (!p_->death) throw C::Unsupported("death flow: fight not started");
  return *p_->death;
}
const std::string &Fight::death_error() const { return p_->death_error; }
DeathView Fight::death_view() const {
  const auto &p = *p_;
  DeathView out;
  if (!p.death) return out;
  out.running = p.death->running();
  out.phase = p.death->phase();
  out.wipe_countdown = unsigned(p.read(player_death_flow::WIPE_COUNTDOWN, 2));
  out.dead_countdown = unsigned(p.read(player_death_flow::DEAD_COUNTDOWNS, 2));
  out.fade = p.host ? p.host->fade_level() : 0;
  out.refusal = p.death_error;
  out.animation_started = p.death_clip != 0;
  out.animation_steps = p.death_animation_steps;
  if (p.death_clip && p.player_state) {
    const auto track = p.anim->anim_find_track(p.player_state, p.token_clip.at(p.death_clip));
    if (track) {
      out.animation_time = p.anim->track(track).time;
      out.animation_flags = p.anim->track(track).flags;
    }
  }
  return out;
}
std::string Fight::diagnostic_lethal_hit() {
  auto &p = *p_;
  if (!p.player_ok || p.failed || p.read(p.player + 0x58c) == 4)
    return "diagnostic lethal hit requires a living shared-World Player";
  for (const auto &enemy : enemies()) {
    if (enemy.dead || !lt(0, enemy.hp)) continue;
    // The call queues damage; Player_run owns the HP change, ward check,
    // death clip/state and alive rebuild on its next ready tick.
    p.world->take_damage(p.player, enemy.address, 1, p.read(PLAYERS + 0xb0));
    return {};
  }
  return "diagnostic lethal hit requires a live campaign enemy";
}
} // namespace champions::simulation::rta::campaign_fight
