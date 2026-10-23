#include "champions/campaign_fight.hpp"
#include "champions/campaign_actors.hpp"
#include "champions/campaign_rewards.hpp"
#include "champions/player_state.hpp"
#include "champions/script_items.hpp"
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
constexpr Raw ATTACK_MASK = 0x10, BLOCK_MASK = 0x20;
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
  script_commands::Host *host = nullptr;
  std::unique_ptr<campaign_actors::Runtime> actors;
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
  // actors
  std::map<Address, creature_anim::StateId> states;
  std::map<Address, bool> stopped;
  std::map<Address, Raw> step_events;
  std::map<Address, bool> adopted;
  std::vector<Address> adopt_order;
  std::map<std::uint32_t, Raw> hide_masks;
  // player
  Address player = 0, player_handle = 0;
  creature_anim::StateId player_state = 0;
  bool player_ok = false;
  bool attack_was_held = false;
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
    base.hide_mask = [this](Raw model, Raw) -> std::uint64_t {
      refuse("model_buildHideMask for a combat-constructed class: not bound in the session World");
      (void)model;
      return UINT64_MAX;
    };
    base.resource = [this](const C::ResourceQuery &q) -> Raw {
      if (q.lump_is_text || q.lump != PLAYER_LUMP_TOKEN) return 0;
      if (q.entry.empty() || !player_entry(q.entry)) return 0;
      return player_token(q.entry);
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
      if (!clip) return; // res_find returned null: anim_start ignores a null clip
      start(a, clip, flags, blend, out);
      if (auto s = state_of(a)) store_cells(a, s);
    };
    services.animation_advance = [this, advance](Address a, Raw speed) -> Raw {
      auto s = state_of(a);
      if (s) load_cells(a, s);
      const Raw events = advance(a, speed);
      if (s) store_cells(a, s);
      return events;
    };
    return services;
  }

  // ---- boar family ---------------------------------------------------------
  std::uint64_t boar_hide_mask(std::uint32_t token, std::uint32_t variant) {
    if (variant) throw campaign_actors::Unsupported("model_buildHideMask: variant bits not bound");
    auto cached = hide_masks.find(token);
    if (cached != hide_masks.end()) return cached->second;
    auto bytes = actors->resource_bytes(token);
    if (!bytes) throw campaign_actors::Unsupported("model_buildHideMask: model resource unavailable");
    // research docs/formats/model-texture.md (model_buildHideMask 0x179550):
    // the show mask starts as the complement of the default-hidden word.
    const auto model = decodeRtaModel(*bytes);
    const auto mask = modelVisibilityMask(model, -1);
    hide_masks[token] = mask;
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
    s.handle_clear = [this, W](std::uint32_t head) { W().clear_handle(head); };
    s.handle_link = [this, W](std::uint32_t head, std::uint32_t target) { W().relink(head, target); };
    s.process_damage = [this, W](std::uint32_t a) { W().process_damage(a); };
    s.creature_run = [this, W](std::uint32_t a) { stopped[a] = W().creature_run(a); };
    s.tick_input = [this, W](std::uint32_t a) {
      campaign_actors::TickInput in;
      // Creature_updateActive, then Creature_stepAnim(1.0) in the original
      // order; the events are this tick's (animation_step below is the same
      // call already made).
      if (stopped[a] || !W().creature_update_active(a)) return in;
      in.active = true;
      step_events[a] = W().creature_step_anim(a, ONE);
      if (auto st = state_of(a)) store_cells(a, st);
      const auto events = step_events[a];
      in.hit_event = events & 1;
      in.next_event = events & 2;
      in.animation_over = !(read(a + 0x11a, 2) & 8);
      // Slot 25's `force` argument is not documented for WarBoar, so the
      // sight call is refused (state 0 then refuses) rather than guessed.
      in.sight_refused = true;
      in.away_home = W().creature_away_from_home(a);
      // LABEL: the state-0 entry age is Unknown. The research gives
      // +0xf4 = entry + 180 (monsters-2.md WarBoar) but not what sets +0xf4 or
      // the entry age in the constructor. The frame the class registered on
      // the 60-frame list (0, "as asked" in the round-1 report) stands in; it
      // is used only to decide when state 0 refuses (campaign_actors
      // tick_boar), never to produce a state-1 transition.
      in.state_entry_frame = registered_frame[a];
      return in;
    };
    s.animation_step = [](std::uint32_t) {}; // done in tick_input, in order
    s.knockback = [W](std::uint32_t a) { W().apply_knockback(a, 0x3e4ccccc); };
    s.pick_target = [W](std::uint32_t a) { W().pick_target(a); };
    s.hit_all_in_arc = [W](std::uint32_t a, std::uint32_t flags) {
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
      return W().creature_filter_state(a, state);
    };
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
  std::map<Address, std::int64_t> registered_frame;

  void build_boar(const script_commands::Host::CreatureRequest &request) {
    if (!actors) throw campaign_actors::Unsupported("boar runtime is not installed");
    const auto kind = request.small ? campaign_actors::Kind::WarBoarSmall
                                    : campaign_actors::Kind::WarBoar;
    // Rank: the record carries no boss rank in this host (Creature_ctor's
    // bossRank is 0 for an unnamed creature); death loot refuses otherwise.
    // LABEL: entry age of state 0 is Unknown (see tick_input); 0 is the
    // registration frame stand-in, not a documented value.
    registered_frame[request.address] = 0;
    actors->adopt(kind, *request.object, request.angle, std::int64_t(0));
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
    // VF-1: player_giveStartingItems 0x1acc78 is not ported and the research
    // does not establish which item the class 6 / model 11 character has
    // equipped on this route (campaign-combat-handoff.md: "the normal campaign
    // route's equipped player remains Unknown"). The record keeps weapon class
    // 5 (unarmed, docs/player.md) with no equipment as a host stand-in; damage
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
  p.world = std::make_unique<C::World>(p.authority, p.build_services(), 0, C::Composition::Full);
  p.adapter = std::make_unique<C::SessionAdapter>(*p.world);
  p.adapter->bind_button(0, C::World::Button::Attack, ATTACK_MASK);
  p.adapter->bind_button(0, C::World::Button::Block, BLOCK_MASK);
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
  try {
    // Documented WarBoar vtable (handoff round 2, monsters-2.md): the Creature
    // base slots with run (slot 4) and setState (slot 27) overridden; slot 2
    // is the inherited Creature_takeDamage 0x116638.
    p.world->define_creature_table(0x517cf0, {{4, 0x2ba3a0}, {27, 0x2bac60}});
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
        p.world->adopt_creature(entry.first);
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
      const bool held = (command & session_control::DAMAGE) != 0;
      C::SessionAdapter::PadInput in;
      in.stick_x = value(x);
      in.stick_y = value(y);
      in.held = held ? ATTACK_MASK : 0;
      in.pressed = (held && !p.attack_was_held) ? ATTACK_MASK : 0;
      if (in.pressed) p.counts.attack_edges += 1;
      p.attack_was_held = held;
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
  v.hp = p.read(p.player + 0x228);
  v.max_hp = p.read(PLAYERS + 0xb0);
  v.action_state = std::int32_t(p.read(p.player + 0x58c));
  v.weapon_class = std::int8_t(p.read(PLAYERS + 0x4a86, 1));
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
    for (const auto &entry : p_->actors->refusal_counts()) merged["WarBoar AI: " + entry.first] += entry.second;
  return merged;
}
combat_runtime::World &Fight::world() { return *p_->world; }
} // namespace champions::simulation::rta::campaign_fight
