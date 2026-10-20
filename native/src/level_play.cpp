#include "champions/level_play.hpp"
#include "champions/session_level_flow.hpp"
#include "champions/level_flow_water_adapter.hpp"
#include "champions/objects.hpp"
#include "champions/terrain_file.hpp"
#include <algorithm>
#include <set>
namespace champions::simulation::rta::level_play {
namespace L = level_flow;
namespace {
using L::Blob;
using L::Record;
void put(Blob &b, std::size_t p, std::uint32_t x, unsigned n = 4) {
  if (b.size() < p + n)
    b.resize(p + n);
  for (unsigned i = 0; i < n; ++i)
    b[p + i] = std::uint8_t(x >> (i * 8));
}
Blob objects(const std::vector<Record> &rs) {
  Blob b(8), strings{0};
  put(b, 0, std::uint32_t(rs.size()), 2);
  auto intern = [&](const std::string &s) {
    auto p = std::uint32_t(strings.size());
    strings.insert(strings.end(), s.begin(), s.end());
    strings.push_back(0);
    return p;
  };
  for (auto &r : rs) {
    auto p = b.size();
    b.resize(p + 24 + r.props.size() * 4);
    put(b, p, intern(r.cls));
    put(b, p + 4, std::uint32_t(24 + r.props.size() * 4), 2);
    put(b, p + 6, r.angle, 2);
    for (unsigned i = 0; i < 3; ++i)
      put(b, p + 8 + 4 * i, r.position[i]);
    for (std::size_t i = 0; i < r.props.size(); ++i)
      put(b, p + 20 + 4 * i, intern(r.props[i]));
  }
  put(b, 4, std::uint32_t(b.size()));
  b.insert(b.end(), strings.begin(), strings.end());
  return b;
}
Blob lump(const std::vector<std::pair<std::string, Blob>> &entries) {
  Blob b(4 + 12 * entries.size());
  put(b, 0, std::uint32_t(entries.size()));
  for (std::size_t i = 0; i < entries.size(); ++i) {
    put(b, 4 + 12 * i, std::uint32_t(b.size()));
    b.insert(b.end(), entries[i].first.begin(), entries[i].first.end());
    b.push_back(0);
    put(b, 8 + 12 * i, std::uint32_t(b.size()));
    put(b, 12 + 12 * i, std::uint32_t(entries[i].second.size()));
    b.insert(b.end(), entries[i].second.begin(), entries[i].second.end());
  }
  return b;
}
Blob gob(const std::vector<std::pair<std::string, Blob>> &entries) {
  Blob b(0x800);
  for (std::size_t i = 0; i < entries.size(); ++i) {
    if (entries[i].first.size() >= 32) throw L::Error("fixture name");
    std::copy(entries[i].first.begin(), entries[i].first.end(),
              b.begin() + 40 * i);
    put(b, 40 * i + 32, 0xdeadbeef);
    put(b, 40 * i + 36, std::uint32_t(entries[i].second.size()));
    b.insert(b.end(), entries[i].second.begin(), entries[i].second.end());
    b.resize((b.size() + 0x7ff) & ~std::size_t(0x7ff));
  }
  return b;
}

Blob flat_world() {
  Blob b(0x841c);
  put(b,0,1); put(b,8,0xffffffff); put(b,12,0xffffffff);
  put(b,0x10,2); put(b,0x14,2); put(b,0x18,0x8400);
  put(b,0x1c,1); put(b,0x20,0xc0); put(b,0x24,0x84);
  put(b,0x64,0x8414); put(b,0x78,0x8410);
  put(b,0xc0,0xff80,2); put(b,0xc2,0xff80,2);
  put(b,0xc4,128,2); put(b,0xc6,128,2);
  put(b,0xcc,0x100); put(b,0xe0,32767,2);
  put(b,0x100,0xffffffc0); put(b,0x104,0xffffffc0);
  put(b,0x108,129); put(b,0x10c,129); put(b,0x8412,65535,2);
  return b;
}
Vec3 marker(const std::string &name) { return {from_int(name == "aa" ? 0 : 64),0,0}; }
std::vector<Record> records(const std::string &name) {
  return {{"Player1",0,marker(name),{}},{"Portal",0,marker(name),{}}};
}
}
struct Session::State {
  L::State flow;
  session_control::Observation status;
  objects::World world{objects::CallbackPolicy::ReferenceBase};
  world_queries::Level ground;
  std::unique_ptr<L::Pieces> pieces;
  creature_water::Memory water_memory;
  creature_water::Service water{water_memory};
  std::unique_ptr<L::Trigger> trigger;
  std::map<std::string,Blob> disc;
  std::unique_ptr<L::Loader> loader;
  std::unique_ptr<L::SessionAdapter> adapter;
  netwire::Machine network{1,0};
  objects::TaskList tasks;
  unsigned generation=0, restored=0;
  std::size_t restored_bytes=0;
  bool gate_found=false, in_scheduler=false;
  objects::GameObject &player() const {
    auto *p=world.names.find_first("fixture-player");
    if (!p) throw L::Error("fixture player missing");
    return *p;
  }
  // All accepted notifications apply only to this fixed empty-resource domain.
  // No scripts, equipment, audio, particles, decals, paths, shops or models exist.
  // Unknown work must refuse rather than masquerade as a production service.
  void event(const std::string &e) {
    if (in_scheduler) throw L::Error("level change inside scheduler");
    if (e=="clear_level" || e=="clear_level_after_player_reload") {
      world.delete_all_objects(); world.flush_delete_list();
      gate_found=false; trigger.reset(); pieces.reset(); return;
    }
    if(e=="flush_constructor_deletions") {world.flush_delete_list();return;}
    if(e=="water_setup") {
      water_memory.poke(0x1004,4,0x2000);
      water_memory.poke(0x1024,4,0x3000);
      water_memory.poke(0x2000,2,0); // Authored empty water tree.
      if(L::WaterAdapter{water,0x1000}.test(from_int(12),marker(flow.level)))
        throw L::Error("dry fixture water mismatch");
      return;
    }
    static const std::set<std::string> empty_services{
      "before_load_network_flush","restore_medal_player_if_saved","act_reset",
      "player_selection_apply_pending","loading_begin","unload_to_keep_9",
      "item_resources","real_level_loaded","release_equipment_cache",
      "world_fixup","copy_cosmetic_seed","entry_music","ambient_parse",
      "apply_level_row","allocate_water_pool","world_prepare",
      "reset_level_counters_and_sound","level_bounds","objects_level_reset",
      "restore_saved_decals_and_map","paths_join_segments",
      "loading_end_light_autosave_dialog_immunity30","warmup","deferred_calls",
      "network_barrier","shop_stock","loading_screen_off","environment_upload_if_resident"};
    if(!empty_services.count(e)) throw L::Unsupported("fixture service: "+e);
  }
  Blob save_level() const {
    if (!trigger) return {};
    auto body=objects(records(flow.level));
    auto ts=trigger->save();
    Blob b{'O','C','L','F',1,0,0,0};
    put(b,8,std::uint32_t(ts.delay)); put(b,12,ts.inside?1:0);
    b.insert(b.end(),body.begin(),body.end()); return b;
  }
  void change() {
    L::ChangeOptions opts; opts.old_blob=save_level();
    L::Placement placement; placement.players=1;
    adapter->change(opts,{},placement);
    ++generation;
  }
  State() {
    ScopedFpu fixed{Fpu{Fpu::Model::Ps2,false,false}};
    const auto empty=lump({});
    for (const auto *name:{"FX.LMP;1","HUD.LMP;1","MAPICONS.LMP;1","ICONS.LMP;1",
                          "LOADMISC.LMP;1","GOODDOG.LMP;1","SKELE_B.LMP;1"}) disc[name]=empty;
    auto world_bytes=flat_world();
    for(const auto *name:{"aa","aab"}) {
      auto level=lump({{"floor.world",world_bytes},{"objects.ob",objects(records(name))}});
      disc[L::cd_file_name(std::string(name)+".gob")]=gob({{std::string(name)+".lmp",level},
        {"waterenv.lmp",lump({{"one.tex",{}}})}});
    }
    L::LoadServices services;
    services.disc=[this](const std::string &name)->std::optional<Blob>{
      auto it=disc.find(name); if(it==disc.end())return {};return it->second;};
    services.range=[](const std::string &name,std::size_t off,std::size_t size){
      if((name!="AA.TEX;1"&&name!="AAB.TEX;1")||off||size!=0x5000)
        throw L::Unsupported("fixture texture range");
      return Blob(size);};
    services.player_lumps=[] {return false;}; // Base-object avatar has no model.
    services.model_lumps=[this,world_bytes](Bytes b){
      Blob bytes(b.data,b.data+b.size);
      if(bytes!=world_bytes)throw L::Unsupported("fixture WORLD differs");
      ground=world_queries::Level(terrain_file::decode(bytes));
      pieces=std::make_unique<L::Pieces>(std::vector<L::Piece>{},ground);
      return std::vector<std::string>{};};
    services.event=[this](const std::string &e){event(e);};
    loader=std::make_unique<L::Loader>(services);
    L::SessionHooks hooks;
    hooks.event=services.event;
    hooks.find_free_spot=[this](const Vec3 &p,Raw spread,Raw radius,bool any){
      return world.find_free_spot(p,to_int(spread),to_int(radius),any,
        [this](Raw r,const Vec3 &v){return Raw(world_queries::terrain_test_flag1(ground,r,v));},
        [this](Raw r,const Vec3 &v){return world_queries::terrain_height(ground,r,v);}).first;};
    hooks.shared_draw=[this]{return std::uint16_t(world.shared.draw());};
    hooks.terrain_height=[this](Raw r,const Vec3 &p){return world_queries::terrain_height(ground,r,p);};
    hooks.script=[](const std::string &entry,const Blob &b){
      if(!b.empty()||(entry!="preStart"&&entry!="start"))throw L::Unsupported("fixture script");};
    hooks.construct=[this](const Record &r){
      if(!r.props.empty()||(r.cls!="Player1"&&r.cls!="Portal"))throw L::Unsupported("fixture object");
      if(r.cls=="Portal"&&!trigger) {
        trigger=std::make_unique<L::Trigger>(sub(r.position[0],from_int(48)),add(r.position[1],from_int(48)),96,96,"fixture-gate");
      }
    };
    hooks.restore_level_state=[this](const Blob &b){
      if(b.size()<24 || !std::equal(b.begin(),b.begin()+8,Blob{'O','C','L','F',1,0,0,0}.begin()))
        throw L::Error("fixture level state header");
      auto u32=[&](unsigned o){Raw v=0;for(unsigned i=0;i<4;++i)v|=Raw(b[o+i])<<(8*i);return v;};
      if(u32(12)>1)throw L::Error("fixture trigger state");
      auto m=marker(flow.level);
      trigger=std::make_unique<L::Trigger>(sub(m[0],from_int(48)),add(m[1],from_int(48)),96,96,"fixture-gate");
      trigger->restore({std::int32_t(u32(8)),bool(u32(12))});
      ++restored;restored_bytes=b.size();
      return L::parse_objects({b.data()+16,b.size()-16});};
    hooks.construct_player=[this](unsigned slot,const Vec3 &p,std::uint16_t facing){
      if(slot)throw L::Unsupported("fixture party size");
      auto *actor=world.new_object(0x4c,p);
      world.names.add("fixture-player",actor);actor->angle=facing;
    };
    hooks.remove_flag39_items=[]{}; // Fixture has no inventory objects.
    hooks.disable_unclaimed_pieces=[this]{pieces->disable_unclaimed();};
    flow.acts={{{"hub",4096},{"aa",4096},{"aab",4096}}};
    flow.players[0].active=true;flow.players[0].hp=flow.players[0].max_hp=100;
    adapter=std::make_unique<L::SessionAdapter>(flow,*loader,hooks);
    flow.pending.request("aa");change();flow.gates.add_current("aab","hub");
    tasks.add(1,10,"levelFlowReady",0);
    network.num_players=1;
    network.run_tasks=[this](netwire::Machine &net){
      in_scheduler=true;
      try {tasks.call(1,-1,[this,&net](Raw){
        ++status.ready_ticks;++flow.sim_frame;
        if(flow.sim_frame%15==0)trigger->run({{0,player().pos}});
        adapter->advance_ready();
        net.sim_frame=flow.sim_frame;net.positions[0]=player().pos;
      });}catch(...){in_scheduler=false;throw;}
      in_scheduler=false;
    };
    status.ready_tick_limit=4096;
  }
};
Session::Session():s_(std::make_unique<State>()){}
Session::~Session()=default;
session_control::Observation Session::observation() const {
  auto o=s_->status; auto &p=s_->player();
  o.position=p.pos;o.radius=p.radius;o.facing=p.angle;o.hp=from_int(s_->flow.players[0].hp);
  o.max_health=from_int(s_->flow.players[0].max_hp);o.frame=s_->flow.sim_frame;
  o.paused=o.paused||s_->flow.travel.paused;return o;
}
session_control::Observation Session::advance(std::uint32_t command) {
  if(command)throw L::Unsupported("level fixture refuses movement/combat commands");
  auto &s=*s_;if(observation().paused||s.status.stopped)return observation();
  ScopedFpu fixed{Fpu{Fpu::Model::Ps2,false,false}};
  try {
    if(!s.flow.pending.name.empty())s.change();
    ++s.status.submissions;s.network.net_main({0,0,128,128,128,128,0,0});
    s.network.log.clear();s.network.outbox.clear();
    if(!s.flow.pending.name.empty())s.change(); // Outside run_tasks.
    if(s.status.ready_ticks>=s.status.ready_tick_limit){s.status.stopped=true;s.status.error="level fixture tick budget";}
  }catch(const std::exception &e){s.status.failed=s.status.stopped=true;s.status.error=e.what();}
  return observation();
}
Flow Session::flow() const {
  auto &s=*s_;return {s.flow.level,s.generation,s.flow.travel.ticks,s.restored,
    s.trigger->contains(s.player().pos),s.trigger->inside,s.flow.travel.menu_open,
    marker(s.flow.level),s.flow.spawn.pos[0],s.flow.states.entries.size(),s.restored_bytes};
}
world_queries::Snapshot Session::terrain() const{return s_->ground.snapshot();}
void Session::open_gate(){
  if(s_->status.stopped||s_->status.paused||!flow().gate_contact)throw L::Error("gate unavailable");
  if(!s_->adapter->prompt_gate(s_->gate_found,true,{},"hub").usable)throw L::Error("gate prompt refused");
  s_->flow.travel.open(false);
}
void Session::confirm_gate(const std::string &name){
  if(s_->status.stopped||s_->status.paused||name!="aab"||
     !s_->flow.travel.confirm(name,s_->flow.level,s_->flow.gates))throw L::Error("gate destination refused");
}
void Session::request_level(const std::string &name){
  if(name!="aa"||s_->flow.level!="aab"||s_->flow.travel.ticks||s_->status.stopped||observation().paused)
    throw L::Unsupported("fixture return requires arrived aab");
  if(!s_->adapter->request_script(name,{},false,0))throw L::Error("level request refused");
}
void Session::release_input(){}
void Session::set_paused(bool value){s_->status.paused=value;}
void Session::stop(){s_->status.stopped=true;}
Raw Session::shared_seed() const{return s_->world.shared.state;}
}
