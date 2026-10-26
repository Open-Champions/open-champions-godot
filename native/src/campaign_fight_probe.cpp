// Counts-only fight probe over an explicitly selected, read-only RTA directory.
// Loads one level the way the play session does (script host with the level
// factory, boar-family class port, the session Player in the shared World) and
// ticks it. Prints aggregate counts and refusal reasons only: never paths,
// level or entry names, script text or bytes.
#include "champions/archive.hpp"
#include "champions/campaign_fight.hpp"
#include "champions/level_flow.hpp"
#include "champions/session_control.hpp"
#include "champions/terrain_file.hpp"
#include <cctype>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>

namespace fs = std::filesystem;
namespace R = champions::simulation::rta;
namespace CF = champions::simulation::rta::campaign_fight;

namespace {
std::string lower(std::string t) {
  for (auto &c : t) c = char(std::tolower(static_cast<unsigned char>(c)));
  return t;
}
std::vector<std::uint8_t> read_file(const fs::path &p) {
  std::ifstream in(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
void scan(const fs::path &dir, int depth, std::map<std::string, fs::path> &files) {
  if (depth > 3) return;
  std::error_code ec;
  for (auto it = fs::directory_iterator(dir, ec); !ec && it != fs::directory_iterator(); it.increment(ec)) {
    if (it->is_directory(ec)) scan(it->path(), depth + 1, files);
    else if (it->is_regular_file(ec)) files[lower(it->path().filename().string())] = it->path();
  }
}
const champions::Entry *find_entry(const std::vector<champions::Entry> &entries, const std::string &name) {
  for (const auto &e : entries)
    if (lower(e.name) == name || lower(e.name).substr(lower(e.name).rfind('.') == std::string::npos ? 0 : lower(e.name).rfind('.')) == name)
      return &e;
  return nullptr;
}
float as_float(R::Raw raw) {
  float f;
  std::memcpy(&f, &raw, 4);
  return f;
}
R::Raw raw_of(float f) {
  R::Raw r;
  std::memcpy(&r, &f, 4);
  return r;
}
} // namespace

int main(int argc, char **argv) {
  std::string root, level_name = "valor";
  unsigned ticks = 600, attack_every = 30;
  bool walk = false, near_boar = false, weak = false, near_weakest = false, near_minotaur = false, ai_check = false, death_check = false;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--game" && i + 1 < argc) root = argv[++i];
    else if (a == "--level" && i + 1 < argc) level_name = argv[++i];
    else if (a == "--ticks" && i + 1 < argc) ticks = unsigned(std::stoul(argv[++i]));
    else if (a == "--attack-every" && i + 1 < argc) attack_every = unsigned(std::stoul(argv[++i]));
    else if (a == "--walk") walk = true;
    else if (a == "--near") near_boar = true;
    else if (a == "--weak") weak = true;
    else if (a == "--near-weakest") near_weakest = true;
    else if (a == "--near-minotaur") near_minotaur = true;
    else if (a == "--ai-check") ai_check = true;
    else if (a == "--death-check") death_check = true;
    else { std::cerr << "usage: champions-campaign-fight-probe --game DIR [--level NAME] [--ticks N] [--attack-every K] [--walk] [--near (DIAGNOSTIC: start 40 units from a boar)] [--near-weakest (DIAGNOSTIC: like --near, at the boar with the lowest hit points)] [--weak (DIAGNOSTIC: set HP 1.0 on boars within 400 units to exercise the death path)]\n"; return 2; }
  }
  if (root.empty() || !fs::is_directory(root)) { std::cerr << "need --game DIR\n"; return 2; }
  std::map<std::string, fs::path> files;
  scan(root, 0, files);
  auto exe = std::make_shared<R::arena_modes::Executable>();
  if (!files.count("slus_209.73") || !exe->load(read_file(files["slus_209.73"]))) { std::cerr << "no executable\n"; return 1; }
  const auto gob = read_file(files.at(level_name + ".gob"));
  const auto top = champions::readGob({gob.data(), gob.size()});
  const auto *lump_entry = find_entry(top, level_name + ".lmp");
  if (!lump_entry) { std::cerr << "no level lump\n"; return 1; }
  const auto lump = champions::payload({gob.data(), gob.size()}, *lump_entry);
  const auto inner = champions::readLmp(lump);
  auto entry_bytes = [&](const std::string &name) {
    const auto *e = find_entry(inner, name);
    if (!e) return std::vector<std::uint8_t>{};
    const auto b = champions::payload(lump, *e);
    return std::vector<std::uint8_t>(b.data, b.data + b.size);
  };
  const auto script = entry_bytes("script.scr"), objects = entry_bytes("objects.ob"),
             world_bytes = entry_bytes(".world");
  std::vector<std::uint8_t> balance;
  {
    auto archive = read_file(files.at("balance.lmp"));
    const auto rows = champions::readLmp({archive.data(), archive.size()});
    if (const auto *e = find_entry(rows, "monsters.bal")) {
      const auto b = champions::payload({archive.data(), archive.size()}, *e);
      balance.assign(b.data, b.data + b.size);
    }
  }
  // Terrain, services, authority.
  R::world_queries::Level level(R::terrain_file::decode(world_bytes));
  R::player_services::Bundle bundle(level);
  auto authority = std::make_shared<R::WorldAuthority>();
  bool dialog = false;
  CF::Fight fight(authority, exe, bundle, [&] { return dialog; });
  R::player_services::Camera camera;
  camera.yaw = 0;
  camera.position = {0, 0, raw_of(500.f)};
  const R::Raw one = R::ONE, zero = 0;
  camera.side_normals = {{{one, zero, zero}, {R::MINUS_ONE, zero, zero}, {zero, one, zero}, {zero, R::MINUS_ONE, zero}}};
  bundle.set_camera(0, camera);
  bundle.set_local_slot(0);
  bundle.bind_water_chunks({}); // probe: no water image (the session binds the level's)
  // Host as the play session builds it.
  R::script_commands::Host host(authority);
  R::SharedRandom seed(17);
  host.bind_seed(&seed, R::script_commands::SeedPolicy::OfflineSession);
  host.start_single_player();
  host.set_executable(R::script_commands::executable_view(exe));
  const unsigned model_index = 11;
  std::vector<std::string> lumps{exe->string(exe->word(0x502f80)),
      exe->string(exe->word(0x501200)), CF::Fight::player_animation_lump(*exe, model_index)};
  for (const auto &name : lumps)
    if (files.count(lower(name))) {
      const auto bytes = read_file(files.at(lower(name)));
      host.load_resource_lmp(name, {bytes.data(), bytes.size()});
    }
  host.set_level_stats(level_name, balance.data(), balance.size());
  host.set_level_world(world_bytes.data(), world_bytes.size());
  host.advance_first_level_items(level_name);
  // The character record the session binder writes (probe-only values for the
  // fields a real session takes from its created character).
  {
    auto &mem = host.variables().memory;
    const auto rec = host.variables().player_record(0);
    mem.word(rec + 0x50, 6);
    mem.word(rec + 0x54, model_index);
    mem.byte(rec + 0x58, 1);
    mem.byte(rec + 0x59, 1);
    for (unsigned n = 0; n < 4; ++n) {
      mem.word(rec + 0x78 + 4 * n, 20);
      mem.word(rec + 0x90 + 4 * n, 20);
    }
    mem.word(rec + 0xac, raw_of(100.f));
    mem.word(rec + 0xb0, raw_of(100.f));
    mem.word(rec + 0xe4, 500);
  }
  fight.install(host);
  R::amx::ByteVector image(script.begin(), script.end());
  if (!host.load(image)) { std::cerr << "script refused\n"; return 1; }
  host.run_prestart();
  host.adopt_objects(objects.data(), objects.size());
  host.run_start();
  const auto objs = R::level_flow::parse_objects({objects.data(), objects.size()});
  CF::Fight::PlayerStart start;
  start.model_index = model_index;
  start.level_name = level_name;
  start.hp = raw_of(100.f);
  for (const auto &o : objs)
    if (o.cls == "Player1") { start.position = o.position; start.facing = o.angle; break; }
  if (near_boar || near_minotaur)
    for (const auto &o : objs)
      if ((near_boar && o.cls.compare(0, 7, "WarBoar") == 0) ||
          (near_minotaur && o.cls.compare(0,8,"Minotaur")==0)) {
        start.position = o.position;
        start.position[0] = R::sub(start.position[0], R::from_int(40));
        start.facing = 0;
        break;
      }
  const auto start_error = fight.start(start);
  if (near_weakest) {
    const CF::EnemyView *weakest = nullptr;
    const auto all = fight.enemies();
    for (const auto &e : all)
      if (!weakest || as_float(e.hp) < as_float(weakest->hp)) weakest = &e;
    if (weakest) {
      auto spot = weakest->position;
      spot[0] = R::sub(spot[0], R::from_int(40));
      fight.place_player(spot, 0);
      start.position = spot;
    }
  }
  if (ai_check) {
    // Counts only. Move the diagnostic player near a real level boar after
    // 120 ticks, without writing enemy state, target, stats or animation.
    // This proves native integration, not an unassisted campaign route.
    const auto initial = fight.enemies();
    const auto selected = std::min_element(initial.begin(),initial.end(),[](const auto &a,const auto &b) {
      return R::lt(a.hp,b.hp);
    });
    std::map<CF::Address, R::Vec3> positions;
    for (const auto &e : initial) positions[e.address] = e.position;
    std::set<CF::Address> wander, chase, attack, moved, reaction, retreat;
    unsigned near_ticks = 0, active_ticks = 0, target_ticks = 0, visible_ticks = 0;
    std::size_t inert = 0, no_target = 0, player_enemies = 0;
    const auto player_address = CF::Address(fight.world().read(R::combat_runtime::PLAYERS + 0x43d8));
    for (const auto &e : initial) {
      inert += fight.world().read(e.address + 0x231,1) != 0;
      no_target += (fight.world().read(e.address + 0xa8,8) & UINT64_C(0x4000000)) != 0;
      if (player_address) player_enemies += fight.world().enemy(e.address,player_address) != 0;
    }
    bool placed = false;
    for (unsigned t = 0; t < ticks && !fight.failed(); ++t) {
      if (t == 120 && !initial.empty()) {
        auto spot = selected->position;
        spot[0] = R::sub(spot[0], R::from_int(40));
        placed = fight.place_player(spot, 0).empty();
      }
      fight.tick(0);
      if (placed && !initial.empty() && player_address) {
        const auto a = selected->address;
        const auto p = fight.player().position;
        const R::Vec3 enemy_pos{R::Raw(fight.world().read(a + 0x10)),R::Raw(fight.world().read(a + 0x14)),R::Raw(fight.world().read(a + 0x18))};
        const auto dx = R::sub(p[0],enemy_pos[0]), dy = R::sub(p[1],enemy_pos[1]), dz = R::sub(p[2],enemy_pos[2]);
        near_ticks += R::lt(R::add(R::add(R::mul(dx,dx),R::mul(dy,dy)),R::mul(dz,dz)),R::from_int(250000));
        active_ticks += (fight.world().read(a + 0xa8,8) >> 63) != 0;
        target_ticks += fight.world().target(a + 0x234) != 0;
        visible_ticks += fight.world().can_see(a,player_address,0,0x7fff,0,0,R::from_int(300)) != 0;
      }
      for (const auto &e : fight.enemies()) {
        if (e.state == 1) wander.insert(e.address);
        if (e.state == 3) chase.insert(e.address);
        if (e.state >= 6 && e.state <= 10) attack.insert(e.address);
        if (e.state == 18) reaction.insert(e.address);
        if (e.state == 16) retreat.insert(e.address);
        if (e.position[0] != positions[e.address][0] || e.position[1] != positions[e.address][1]) moved.insert(e.address);
      }
    }
    const auto &n = fight.counts();
    std::cout << "AI_CHECK diagnostic_placement=" << placed << " boars=" << initial.size()
              << " ticks=" << n.ticks << " wandered=" << wander.size() << " chased=" << chase.size()
              << " attacked=" << attack.size() << " moved=" << moved.size()
              << " reactions=" << reaction.size() << " retreats=" << retreat.size()
              << " state_changes=" << n.boar_state_changes << " attack_entries=" << n.boar_attack_entries
              << " hit_events=" << n.boar_hit_events << " failed=" << fight.failed() << "\n";
    std::cout << "AI_INPUTS inert=" << inert << " no_target=" << no_target << " player_enemies=" << player_enemies
              << " near_ticks=" << near_ticks << " active_ticks=" << active_ticks << " target_ticks=" << target_ticks
              << " visible_ticks=" << visible_ticks << "\n";
    for (const auto &kv : fight.refusals()) std::cout << "refusal count=" << kv.second << ": " << kv.first << "\n";
    return !fight.failed() && placed && !chase.empty() && !attack.empty() ? 0 : 1;
  }
  if (std::getenv("FIGHT_DEBUG")) {
    auto &w = fight.world();
    std::cout << "debug flags=" << std::hex << w.read(0x4d8af0 + 0x43d8) << " numplayers=" << std::dec << w.read(0x4d8ae0)
              << " alive=" << w.read(0x4f8420) << " c8=" << w.read(0x6c88c0 + 0xc8) << " 2dc=" << w.read(0x6c88c0 + 0x2dc)
              << " rec_ac=" << w.read(0x4d8af0 + 0xac) << " obj228=" << w.read(0x6c88c0 + 0x228) << " obj_flags=" << w.read(0x6c88c0 + 12) << "\n";
  }
  std::cout << "authority memory backing bytes=" << authority->memory.bytes().size() << "\n";
  const auto &c = host.counts();
  std::cout << "records built=" << c.objects_built << " unbuilt=" << c.objects_unbuilt << " not_created=" << c.objects_not_created
            << " fight_actors=" << fight.counts().actors_built
            << " minotaurs=" << fight.counts().minotaurs_built
            << " start_error=" << (start_error.empty() ? "none" : start_error) << "\n";
  if (!death_check)
    for (const auto &kv : c.build_refusals) std::cout << "  build refusal x" << kv.second << ": " << kv.first << "\n";
  // Ticks: J pressed for one tick every attack_every ticks.
  if (weak) {  // probe-only: exercise the death path on boars the player can reach
    for (const auto &e : fight.enemies()) {
      const float dx = as_float(e.position[0]) - as_float(start.position[0]);
      const float dy = as_float(e.position[1]) - as_float(start.position[1]);
      if (dx * dx + dy * dy < 400.f * 400.f) fight.world().write(e.address + 0x228, 4, raw_of(1.f));
    }
  }
  const auto before = fight.enemies();
  std::map<CF::Address, float> hp_before;
  for (const auto &e : before) hp_before[e.address] = as_float(e.hp);
  double hp0 = 0;
  for (const auto &e : before) hp0 += as_float(e.hp);
  if (!death_check) std::cout << "enemies=" << before.size() << " total_hp=" << hp0 << "\n";
  const auto original_player = fight.world().read(R::combat_runtime::PLAYERS + 0x43d8);
  auto retained_record = [&] {
    // Documented bags/equipment ranges, XP, Gold and mana. Compare only;
    // private record bytes and hashes are never printed.
    std::vector<std::uint64_t> out;
    for (unsigned off : {0x5cu, 0xc0u, 0xe4u})
      out.push_back(fight.world().read(R::combat_runtime::PLAYERS + off));
    for (unsigned off = 0x130; off < 0x130 + 3 * 0x1208; ++off)
      out.push_back(fight.world().read(R::combat_runtime::PLAYERS + off, 1));
    for (unsigned off = 0x43f0; off < 0x4440; ++off)
      out.push_back(fight.world().read(R::combat_runtime::PLAYERS + off, 1));
    return out;
  };
  std::vector<std::uint64_t> retained_before;
  bool diagnostic_queued = false;
  for (unsigned t = 0; t < ticks && !fight.failed(); ++t) {
    if (death_check && t == 90 && !before.empty()) {
      retained_before = retained_record();
      diagnostic_queued = fight.diagnostic_lethal_hit().empty();
    }
    std::uint32_t cmd = 0;
    if (walk && t < ticks / 2) cmd |= R::session_control::POS_X;
    if (attack_every && (t % attack_every) < 6) cmd |= R::session_control::DAMAGE;
    fight.tick(cmd);
    if (std::getenv("FIGHT_TRACE") && t < 12) {
      auto &w = fight.world();
      std::cout << "tick " << t << " cmd=" << cmd << " action=" << w.read(0x6c88c0 + 0x58c) << " pad_held=" << w.read(0x41bfd8 + 8) << " pad_pressed=" << w.read(0x41bfd8 + 0xc) << " btn=" << w.read(0x41b980) << " events:";
      for (const auto &e : w.events()) if (e.name != "visibility" && e.name.rfind("presentation", 0) != 0 && e.name != "run") std::cout << " " << e.name << (e.text.empty() ? "" : ":" + e.text);
      std::cout << "\n";
    }
  }
  if (std::getenv("FIGHT_DEBUG")) {
    auto &w = fight.world();
    std::cout << "debug after: player flags=" << std::hex << w.read(0x6c88c0 + 12) << " action=" << std::dec << w.read(0x6c88c0 + 0x58c)
              << " numplayers=" << w.read(0x4d8ae0) << " alive=" << w.read(0x4f8420) << " rec hp=" << as_float(R::Raw(w.read(0x4d8af0 + 0xac))) << "\n";
    std::size_t shown = 0;
    for (const auto &e : w.events()) { if (shown++ > 60) break; std::cout << "  ev " << e.name << " " << e.text; for (auto a : e.args) std::cout << " " << std::hex << a; std::cout << std::dec << "\n"; }
  }
  const auto after = fight.enemies();
  double hp1 = 0;
  std::size_t dead = 0;
  for (const auto &e : after) { hp1 += as_float(e.hp); dead += e.dead; }
  {
    std::size_t damaged = 0;
    std::size_t minotaur_damaged=0,minotaur_dead=0,minotaur_active=0,minotaur_targeted=0;
    std::map<unsigned,std::size_t> minotaur_states;
    float lowest = 1e30f, most_lost = 0;
    for (const auto &e : after) {
      const float now = as_float(e.hp), was = hp_before[e.address];
      if(e.minotaur){
        minotaur_damaged+=now<was;minotaur_dead+=e.dead;minotaur_states[e.state]+=1;
        minotaur_active += fight.world().read(e.address+0xa8,8)>>63;
        minotaur_targeted += fight.world().target(e.address+0x234)!=0;
      }
      if (now < was && !death_check) {
        ++damaged;
        if(!e.minotaur)std::cout << "damaged boar: " << (e.small ? "small" : "large") << " hp " << was << " -> " << now << " max " << as_float(e.max_hp) << " dead=" << e.dead << "\n";
      }
      lowest = std::min(lowest, now);
      most_lost = std::max(most_lost, was - now);
    }
    if (!death_check) {
    std::cout << "boars damaged=" << damaged << " lowest_hp=" << lowest << " most_hp_lost_by_one=" << most_lost << "\n";
    std::cout << "minotaurs damaged=" << minotaur_damaged << " dead=" << minotaur_dead
              << " active=" << minotaur_active << " targeted=" << minotaur_targeted
              << " hit_requests=" << fight.counts().minotaur_hit_requests
              << " held_part_requests=" << fight.counts().held_part_requests << "\n";
    for(const auto &state:minotaur_states)
      std::cout << "minotaur_state=" << state.first << " count=" << state.second << "\n";
    }
  }
  const auto player = fight.player();
  const auto &n = fight.counts();
  if (death_check) {
    const auto death = fight.death_view();
    const bool retained = retained_before == retained_record();
    const bool same_body = original_player == fight.world().read(R::combat_runtime::PLAYERS + 0x43d8);
    std::cout << "death_check actors=" << after.size() << " ticks=" << n.ticks
              << " failed=" << fight.failed() << " dead=" << (player.action_state == 4)
              << " zero_hp=" << (player.hp == 0) << " wipe_timer=" << death.wipe_countdown
              << " slot_timer=" << death.dead_countdown
              << " task=" << death.running << " phase=" << death.phase << " fade=" << death.fade
              << " native_clip_started=" << death.animation_started << " native_clip_steps=" << death.animation_steps
              << " diagnostic_hit=" << diagnostic_queued << " retained_record_fields=" << retained
              << " same_dead_body=" << same_body << " reload_refused=" << !death.refusal.empty() << '\n';
    // This is a bounded death/refusal check, not successful respawn evidence.
    return fight.failed() || !diagnostic_queued || player.action_state != 4 || player.hp != 0 ||
        death.wipe_countdown != 0 || !death.running || death.phase != 1 || death.fade != 128 ||
        !death.animation_started || !death.animation_steps || !retained || !same_body || death.refusal.empty();
  }
  std::cout << "ticks=" << n.ticks << " failed=" << fight.failed() << " error=" << fight.error() << "\n"
            << "player present=" << player.present << " pos=(" << as_float(player.position[0]) << "," << as_float(player.position[1]) << "," << as_float(player.position[2]) << ")"
            << " hp=" << as_float(player.hp) << " action=" << player.action_state << " class=" << player.weapon_class << " gold=" << player.gold << "\n"
            << "attack_edges=" << n.attack_edges << " swings=" << n.player_swings << " hits=" << n.player_hits << " damage_events=" << n.damage_events
            << " deaths=" << n.deaths << " loot_calls=" << n.loot_calls << " dead=" << dead << " enemy_hp " << hp0 << " -> " << hp1
            << " factory_runs_skipped=" << n.factory_runs_skipped << "\n";
  for (const auto &a : fight.awards())
    std::cout << "xp award: slot " << a.slot << " amount " << a.amount << " level " << a.level_before << " -> " << a.level_after
              << " exp " << a.experience_before << " -> " << a.experience_after << " levels_gained " << a.levels_gained << "\n";
  std::cout << "player level=" << player.level << " exp=" << player.experience << " skill_points=" << player.skill_points
            << " stat_points=" << player.stat_points << " max_hp=" << as_float(player.max_hp) << " mana=" << as_float(player.mana)
            << "/" << as_float(player.max_mana) << " loot_items=" << n.loot_items << " loot_shared_draws=" << n.loot_shared_draws << "\n";
  for (const auto &g : fight.ground_drops())
    std::cout << "ground drop: kind " << g.kind << " category " << g.category << " count " << g.count << " spawned " << g.spawned << "\n";
  for (const auto &kv : fight.refusals()) std::cout << "  refusal x" << kv.second << ": " << kv.first << "\n";
  return 0;
}
