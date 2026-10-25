// Counts-only fight probe over an explicitly selected, read-only RTA directory.
// Loads one level the way the play session does (script host with the level
// factory, boar-family class port, the session Player in the shared World) and
// ticks it. Prints aggregate counts and refusal reasons only: never paths,
// level or entry names, script text or bytes.
#include "champions/archive.hpp"
#include "champions/campaign_fight.hpp"
#include "champions/pickups.hpp"
#include "champions/level_flow.hpp"
#include "champions/session_control.hpp"
#include "champions/terrain_file.hpp"
#include <cctype>
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
int verify_starting(const std::shared_ptr<R::arena_modes::Executable> &exe) {
  const auto reader = [exe](std::uint32_t a, std::size_t n) {
    std::vector<std::uint8_t> bytes(n,0);
    for(std::size_t k=0;k<n;++k) { try { bytes[k]=exe->byte(a+std::uint32_t(k)); } catch(const std::exception &) {} }
    return bytes;
  };
  const auto tables = R::script_items::image_tables(reader);
  unsigned passed=0;
  for(unsigned cls=0;cls<7;++cls) {
    R::builtins::Variables variables;
    variables.memory.under=reader;
    variables.local_player_slot=[] {return 0;};
    auto &m=variables.memory;
    m.word(CF::Address(0x4d8ae0),1);
    m.word(CF::Address(0x404464),20973);
    m.word(CF::Address(0x4d8af0+0x50),cls);
    m.w16(CF::Address(0x4d8af0+0x4b08),0xffff);
    R::script_items::Service service(variables); service.bind_tables(tables);
    // Same boundary as the research's seven parser/insertion cases: equip,
    // skill and complete stat callbacks are recorded, not certified here.
    service.delegates["inventory_equipItem"]=[](const std::vector<std::uint64_t>&){return std::uint64_t(0);};
    R::pickups::give_starting_items(service,0,cls,[exe](std::uint32_t a){return exe->string(a);});
    const std::array<unsigned,3> counts{
      m.word(R::script_items::bag_count_at(0,0)),m.word(R::script_items::bag_count_at(0,1)),m.word(R::script_items::bag_count_at(0,2))};
    bool ok=counts==std::array<unsigned,3>{cls==3?3u:cls==5?2u:1u,3,3};
    for(unsigned bag=1;bag<3;++bag) for(unsigned k=0;k<3;++k)
      ok=ok && m.s16(R::script_items::bag_entry(0,bag,k)+4)==(bag==1?1:2);
    if(cls==3) ok=ok && m.s16(R::script_items::bag_entry(0,0,2)+4)==40;
    ok=ok && service.counts.equipped==(cls==3||cls==5?5u:4u);
    passed+=ok;
  }
  std::cout<<"starting classes "<<passed<<"/7 parser/inventory checks; equip callbacks recorded\n";
  return passed==7?0:1;
}
} // namespace

int main(int argc, char **argv) {
  std::string root, level_name = "valor";
  unsigned ticks = 600, attack_every = 30, shared_seed = 17;
  bool walk = false, near_boar = false, weak = false, near_weakest = false, starting_only = false;
  bool near_chest = false, use = false;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--game" && i + 1 < argc) root = argv[++i];
    else if (a == "--level" && i + 1 < argc) level_name = argv[++i];
    else if (a == "--ticks" && i + 1 < argc) ticks = unsigned(std::stoul(argv[++i]));
    else if (a == "--attack-every" && i + 1 < argc) attack_every = unsigned(std::stoul(argv[++i]));
    else if (a == "--seed" && i + 1 < argc) {
      const auto value=std::stoull(argv[++i]);
      if(value>UINT32_MAX) return 2;
      shared_seed=unsigned(value);
    }
    else if (a == "--walk") walk = true;
    else if (a == "--near") near_boar = true;
    else if (a == "--weak") weak = true;
    else if (a == "--near-weakest") near_weakest = true;
    else if (a == "--near-chest") near_chest = true;
    else if (a == "--use") use = true;
    else if (a == "--verify-starting") starting_only = true;
    else { std::cerr << "usage: champions-campaign-fight-probe --game DIR [--level NAME] [--ticks N] [--attack-every K] [--seed N (authored offline context)] [--verify-starting] [--walk] [--use] [--near-chest (DIAGNOSTIC: start 40 units from a placed chest)] [--near (DIAGNOSTIC: start 40 units from a boar)] [--near-weakest (DIAGNOSTIC: like --near, at the boar with the lowest hit points)] [--weak (DIAGNOSTIC: set HP 1.0 on boars within 400 units to exercise the death path)]\n"; return 2; }
  }
  if (root.empty() || !fs::is_directory(root)) { std::cerr << "need --game DIR\n"; return 2; }
  std::map<std::string, fs::path> files;
  scan(root, 0, files);
  auto exe = std::make_shared<R::arena_modes::Executable>();
  if (!files.count("slus_209.73") || !exe->load(read_file(files["slus_209.73"]))) { std::cerr << "no executable\n"; return 1; }
  if (starting_only) return verify_starting(exe);
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
  R::SharedRandom seed(shared_seed);
  // Authored offline probe context, not a claim about the campaign loader's
  // unresolved original draw count. Seed the shared World before any objects.
  host.variables().memory.word(R::combat_runtime::SHARED_SEED, shared_seed);
  host.bind_seed(&seed, R::script_commands::SeedPolicy::OfflineSession);
  host.start_single_player();
  host.set_executable(R::script_commands::executable_view(exe));
  const unsigned model_index = 11;
  std::vector<std::string> lumps{"warboar.lmp", "minotaur.lmp", CF::Fight::player_animation_lump(*exe, model_index)};
  const auto pickup_lumps = CF::Fight::pickup_resource_lumps(*exe);
  lumps.insert(lumps.end(), pickup_lumps.begin(), pickup_lumps.end());
  for (const auto &name : lumps)
    if (files.count(lower(name))) {
      const auto bytes = read_file(files.at(lower(name)));
      host.load_resource_lmp(name, {bytes.data(), bytes.size()});
    } else if (const auto *entry = find_entry(top, lower(name))) {
      host.load_resource_lmp(name, champions::payload({gob.data(),gob.size()}, *entry));
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
  start.grant_starting_items = true;
  start.hp = raw_of(100.f);
  for (const auto &o : objs)
    if (o.cls == "Player1") { start.position = o.position; start.facing = o.angle; break; }
  if (near_boar)
    for (const auto &o : objs)
      if (o.cls.compare(0, 7, "WarBoar") == 0) {
        start.position = o.position;
        start.position[0] = R::sub(start.position[0], R::from_int(40));
        start.facing = 0;
        break;
      }
  if (near_chest)
    for (const auto &o : objs)
      if (o.cls.compare(0, exe->string(0x532f70).size(), exe->string(0x532f70)) == 0 ||
          o.cls == exe->string(0x533130)) {
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
  if (std::getenv("FIGHT_DEBUG")) {
    auto &w = fight.world();
    std::cout << "debug flags=" << std::hex << w.read(0x4d8af0 + 0x43d8) << " numplayers=" << std::dec << w.read(0x4d8ae0)
              << " alive=" << w.read(0x4f8420) << " c8=" << w.read(0x6c88c0 + 0xc8) << " 2dc=" << w.read(0x6c88c0 + 0x2dc)
              << " rec_ac=" << w.read(0x4d8af0 + 0xac) << " obj228=" << w.read(0x6c88c0 + 0x228) << " obj_flags=" << w.read(0x6c88c0 + 12) << "\n";
  }
  std::cout << "authority memory backing bytes=" << authority->memory.bytes().size() << "\n";
  const auto &c = host.counts();
  std::cout << "records built=" << c.objects_built << " unbuilt=" << c.objects_unbuilt << " not_created=" << c.objects_not_created
            << " fight_actors=" << fight.counts().actors_built << " start_error=" << (start_error.empty() ? "none" : start_error) << "\n";
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
  std::cout << "enemies=" << before.size() << " total_hp=" << hp0 << "\n";
  for (unsigned t = 0; t < ticks && !fight.failed(); ++t) {
    std::uint32_t cmd = 0;
    if (walk && t < ticks / 2) cmd |= R::session_control::POS_X;
    if (attack_every && (t % attack_every) < 6) cmd |= R::session_control::DAMAGE;
    if (use && t % 30 == 0) fight.request_use();
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
    float lowest = 1e30f, most_lost = 0;
    for (const auto &e : after) {
      const float now = as_float(e.hp), was = hp_before[e.address];
      if (now < was) {
        ++damaged;
        std::cout << "damaged boar: " << (e.small ? "small" : "large") << " hp " << was << " -> " << now << " max " << as_float(e.max_hp) << " dead=" << e.dead << "\n";
      }
      lowest = std::min(lowest, now);
      most_lost = std::max(most_lost, was - now);
    }
    std::cout << "boars damaged=" << damaged << " lowest_hp=" << lowest << " most_hp_lost_by_one=" << most_lost << "\n";
  }
  const auto player = fight.player();
  const auto &n = fight.counts();
  std::cout << "ticks=" << n.ticks << " failed=" << fight.failed() << " error=" << fight.error() << "\n"
            << "player present=" << player.present << " pos=(" << as_float(player.position[0]) << "," << as_float(player.position[1]) << "," << as_float(player.position[2]) << ")"
            << " hp=" << as_float(player.hp) << " action=" << player.action_state << " class=" << player.weapon_class << " gold=" << player.gold << "\n"
            << "attack_edges=" << n.attack_edges << " swings=" << n.player_swings << " hits=" << n.player_hits << " damage_events=" << n.damage_events
            << " deaths=" << n.deaths << " loot_calls=" << n.loot_calls << " dead=" << dead << " enemy_hp " << hp0 << " -> " << hp1
            << " factory_runs_skipped=" << n.factory_runs_skipped << "\n"
            << "chests_built=" << n.chests_built << " chests_opened=" << n.chests_opened
            << " pickups=" << n.pickups << " recipients=" << n.pickup_recipients << "\n";
  for (const auto &a : fight.awards())
    std::cout << "xp award: slot " << a.slot << " amount " << a.amount << " level " << a.level_before << " -> " << a.level_after
              << " exp " << a.experience_before << " -> " << a.experience_after << " levels_gained " << a.levels_gained << "\n";
  std::cout << "player level=" << player.level << " exp=" << player.experience << " skill_points=" << player.skill_points
            << " stat_points=" << player.stat_points << " max_hp=" << as_float(player.max_hp) << " mana=" << as_float(player.mana)
            << "/" << as_float(player.max_mana) << " loot_items=" << n.loot_items << " loot_shared_draws=" << n.loot_shared_draws << "\n";
  for (const auto &g : fight.ground_drops())
    std::cout << "ground drop: kind " << g.kind << " category " << g.category << " count " << g.count
              << " spawned " << g.spawned << " picked_up " << g.picked_up << " lost " << g.lost << "\n";
  for (const auto &kv : fight.refusals()) std::cout << "  refusal x" << kv.second << ": " << kv.first << "\n";
  return 0;
}
