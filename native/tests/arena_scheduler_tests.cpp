#include "champions/combat_runtime.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <tuple>

using namespace champions::simulation::rta;
namespace A = arena_match;
namespace C = combat_runtime;
namespace {
unsigned checks = 0;
void check(bool ok, const std::string &label) {
  ++checks;
  if (!ok) throw std::runtime_error(label);
}
template<class F> void refused(F fn, const std::string &part) {
  std::string error;
  try { fn(); } catch (const A::Unsupported &e) { error = e.what(); }
  check(error.find(part) != std::string::npos, "refusal " + part + ": " + error);
}
// Authored tables/pointers and service boundaries. No original data or actor
// success is supplied. Tests run actual World arena entrypoints/shared memory.
struct Fixture {
  C::World world{{}, 1, C::Composition::Full};
  A::Services services;
  std::shared_ptr<A::RequestServices> requests = std::make_shared<A::RequestServices>();
  std::vector<Raw> preloads, spawns;
  std::vector<std::pair<Raw, Raw>> rings, copies;
  std::vector<std::string> calls;
  unsigned active = 1;
  Fixture(unsigned slots = 1) : active(slots) {
    requests->tables.recipes = {{0x34,0,0,1,0,0}, {0x31,2,3,2,0,0},
                               {0x36,4,5,6,0,0}, {0x30,7,7,7,0,0}, {0x32,8,1,8,0,0}};
    for (unsigned i=0; i<9; ++i) requests->tables.classes.push_back(0x100000 + 0x100*i);
    for (unsigned i=0; i<7; ++i) requests->tables.levels.push_back(0x200000 + 0x100*i);
    requests->active_count = [&] { return int(active); };
    requests->preload_factory = [&](Raw pointer) { preloads.push_back(pointer); calls.push_back("preload"); return Raw(0); };
    requests->close_menus = [&](Raw arg) { check(arg == 0, "close(0)"); calls.push_back("close"); };
    requests->prepare_players = [&](const A::WaveRequest &r) {
      check(get(A::WAVE,2) == r.wave && get(A::WAVES_LEFT,2) == r.waves_left, "native wave counters before player boundary");
      calls.push_back("players");
    };
    requests->spawn = [&](Raw pointer, Raw saved) {
      check(saved == 0, "recipe spawn has null saved object");
      spawns.push_back(pointer); rings.emplace_back(get(0x8e7364),get(0x8e7360,2)); calls.push_back("spawn");
    };
    requests->camera_reset = [&] { calls.push_back("camera"); };
    requests->copy_level = [&](Raw destination, Raw source) { copies.emplace_back(destination,source); calls.push_back("copy"); };
    services.requests = requests;
    services.active = [&](unsigned slot) { return slot < active; };
    services.prepare = [&] { calls.push_back("prepare"); };
    services.destroy = [](Raw) { throw std::runtime_error("fixture must not destroy an actor"); };
    services.screen_effect = [&](Raw effect) { check(effect == 15, "next wave effect 15"); calls.push_back("effect"); };
    services.intro_cue = [&] { calls.push_back("cue"); };
    services.saved_record = [&](unsigned slot) { calls.push_back("saved"); return std::vector<std::uint8_t>(A::RECORD_SIZE, std::uint8_t(0x40+slot)); };
    services.shutdown = [&] { calls.push_back("shutdown"); };
    put(A::NUM_PLAYERS,slots); put(A::MODE_B,1); put(A::INTRO,0xffff,2);
    world.arena_reset_scores();
    for (unsigned i=0; i<slots; ++i) {
      put(A::PLAYERS + i*A::RECORD_SIZE + 0xac, TWO);
      put(A::SCORE + i*A::SCORE_STRIDE, i, 2); // distinct scores: qsort ties stay refused
    }
  }
  void put(Raw a, Raw v, unsigned size=4) { world.write(a,size,v); }
  Raw get(Raw a,unsigned size=4) const { return Raw(world.read(a,size)); }
  void visit(Raw pressed=0, Raw confirm=0, Raw cancel=0) { world.arena_task_visit(services,pressed,confirm,cancel); }
};

void oracle(const char *path) {
  // research-arena-live@9a0c26a5 tools/ref_arena_modes.py:325-410;
  // docs/spec/arena-live-scheduler.md:10-47. Independent immutable oracle.
  std::ifstream file(path); check(bool(file), "open synthetic scheduler oracle");
  std::string line; unsigned cases=0;
  while (std::getline(file,line)) {
    if (line.empty() || line.front()=='#') continue;
    std::istringstream row(line); Raw seed,slots,wave,level,final,n;
    row >> seed >> slots >> wave >> level >> final >> n;
    std::vector<Raw> expected(n); for (auto &v:expected) row >> v;
    unsigned steps; row >> steps;
    std::vector<std::pair<Raw,Raw>> wanted_rings;
    for (unsigned s=0; s<steps; ++s) {
      unsigned count; Raw angle,draw; row >> count >> angle >> draw;
      for (unsigned i=0; i<count; ++i) wanted_rings.emplace_back(angle,draw);
    }
    check(bool(row), "complete oracle row");
    for (unsigned network=0; network<2; ++network) {
      Fixture f(slots); f.put(A::NETWORK,network,1); f.put(0x404464,seed);
      f.put(A::WAVE,wave-1,2); f.put(A::WAVES_LEFT,3-wave%3,2);
      if (wave%3==0) f.world.arena_begin(f.services);
      else f.world.arena_start_wave(f.services,1);
      check(f.get(A::WAVE,2)==wave && f.get(A::WAVES_LEFT,2)==2-wave%3, "actual native start counters");
      check(f.get(0x4f714c)==level && f.get(0x404464)==final, "reference raw level and shared seed");
      check(f.get(A::INTRO,2)==180 && f.get(A::INTERMISSION,2)==0 && f.get(0x503890)==1, "start intro and lock");
      check(f.spawns.size()==expected.size() && f.rings==wanted_rings, "all spawn groups retain ring draws and count");
      for (unsigned i=0; i<expected.size(); ++i)
        check(f.spawns.at(i)==0x100000+0x100*expected[i], "ordered reference class pointer");
      const std::vector<std::vector<Raw>> wanted_preloads={{0,1},{2,3},{4,5,6},{7},{8,1}};
      std::vector<Raw> preloads;
      if (wave%3==0) for (auto i:wanted_preloads[(wave/3)%5]) preloads.push_back(0x100000+0x100*i);
      check(f.preloads==preloads, "preload deduplication independent of spawn grouping");
      for (unsigned i=0;i<slots;++i) check(f.get(A::SCORE+i*A::SCORE_STRIDE+4,2)==2,"native life snapshot");
      check(!f.get(A::END) && !f.get(A::EXIT),"request stages do not invent victory");
      ++cases;
    }
  }
  check(cases==480,"480 independently reconstructed request cases");
}

void timing_and_grid() {
  // research-arena-live@9a0c26a5 tools/difftest_arena_live_scheduler.py:91-163.
  for (unsigned network=0;network<2;++network) {
    Fixture f; f.put(A::NETWORK,network,1); f.put(A::WAVE,0xffff,2);
    f.world.arena_begin(f.services); f.calls.clear();
    for (unsigned i=0;i<180;++i) { f.put(A::FRAME,i+1); f.visit(); check(f.get(A::INTRO,2)==179-i,"every intro visit"); }
    f.visit(); check(f.get(A::INTRO,2)==0xffff && !f.get(0x41b500) && !f.get(0x41b4fc),"181st visit unlocks");
    check(f.calls==std::vector<std::string>{"cue"},"one intro cue, no premature body");
    const auto actor=f.world.spawn_skelight({});
    f.put(A::SCORE,32767,2); f.put(A::KILL_LIMIT,1,1);
    f.put(A::FRAME,255); f.put(A::LIVE_COUNT,17,2); f.visit();
    check(f.get(A::LIVE_COUNT,2)==17 && !f.get(A::INTERMISSION,2),"255 does not walk");
    f.put(A::FRAME,256); f.visit();
    check(f.get(A::LIVE_COUNT,2)==1 && !f.get(A::INTERMISSION,2),"World occupied grid ignores high kills in B");
    // Explicit synthetic grid state, not a kill claim.
    f.put(actor+12,f.get(actor+12)|8); f.visit();
    check(f.get(A::INTERMISSION,2)==1 && !f.get(A::END),"World empty scan starts intermission only");
    f.world.destroy(actor); f.calls.clear(); f.spawns.clear();
    const unsigned limit=network?960:480;
    for(unsigned i=1;i<limit;++i) f.visit();
    check(f.get(A::INTERMISSION,2)==limit && f.calls.empty(),"all old-counter visits before expiry");
    f.visit();
    check(f.get(A::WAVE,2)==1 && f.get(A::WAVES_LEFT,2)==1 && f.get(A::INTRO,2)==180 && !f.get(A::INTERMISSION,2),"World expiry executes native start request stages");
    check(f.calls[0]=="effect" && f.calls[1]=="prepare" && f.calls[2]=="close", "same-level service order");
    bool live_refused=false;
    try { f.world.tick(); } catch (const C::Unsupported &e) { live_refused=std::string(e.what()).find("PvP")!=std::string::npos; }
    check(live_refused,"normal live frame acceptance remains refused");
  }
}

void levels_and_terminal() {
  // research-arena-live@9a0c26a5 tools/difftest_arena_terminal.py:10-79.
  for (unsigned network=0;network<2;++network) {
    const std::vector<unsigned> waves={2,5,8,11,14,17,20,23,26,29,44,149,299,2999,29999,32765};
    const std::vector<unsigned> wanted={1,6,0,2,4,1,6,0,2,4,4,4,4,4,4,6};
    for(unsigned i=0;i<waves.size();++i) {
      Fixture f; f.put(A::NETWORK,network,1); f.put(A::WAVE,waves[i],2);
      f.put(A::WAVES_LEFT,0,2); f.put(A::INTERMISSION,network?960:480,2); f.visit();
      check(f.copies==std::vector<std::pair<Raw,Raw>>{{0x41b6d0,0x200000+0x100*wanted[i]}},"concrete modulo destination including later samples");
      check(f.get(0x41b720,1)==0xff && f.calls==std::vector<std::string>({"copy","close"}),"orientation and close after request");
      check(f.get(A::WAVE,2)==waves[i] && !f.get(A::END) && !f.get(A::EXIT) && f.get(A::MODE_B)==1 && f.spawns.empty(),"continuation never becomes victory or wave spawn");
    }
    for(unsigned slots=1;slots<=4;++slots) for(bool confirm:{false,true}) {
      Fixture f(slots); f.put(A::NETWORK,network,1); f.put(A::FRAME,1); f.put(A::WAVE,149,2);
      for(unsigned i=0;i<slots;++i) {
        f.put(A::PLAYERS+i*A::RECORD_SIZE+0xac,0); f.put(A::SCORE+i*A::SCORE_STRIDE+2,0xffff,2);
        f.put(A::SCORE+i*A::SCORE_STRIDE+4,0,2); f.put(A::COUNTDOWN+2*i,0,2);
      }
      f.visit(); check(f.get(A::END)==1 && !f.get(A::INTERMISSION,2) && !f.get(A::FADE) && f.calls.empty(),"terminal defeat without retry");
      for(Raw fade:{0u,0x3f7fffffu}) { f.put(A::FADE,fade); f.visit(0x100,confirm?0x100:0,confirm?0:0x100); check(f.calls.empty() && f.get(A::MODE_B)==1 && !f.get(A::EXIT),"full-fade predecessor rejects held button"); }
      f.put(A::FADE,ONE); f.visit(0x100,confirm?0x100:0,confirm?0:0x100);
      if(confirm) {
        check(f.copies==std::vector<std::pair<Raw,Raw>>{{0x41b6d0,0x200400}} && f.get(A::MODE_B)==1 && !f.get(A::EXIT),"restart recipe zero regardless of current wave");
        check(f.calls.size()==slots+2 && f.calls[slots]=="shutdown" && f.calls.back()=="copy","restore/shutdown/request order");
        for(unsigned i=0;i<slots;++i) check(f.get(A::PLAYERS+i*A::RECORD_SIZE+A::RECORD_SIZE-1,1)==0x40+i,"full saved record tail restored");
      } else check(!f.get(A::MODE_B) && f.get(A::EXIT)==1 && f.copies.empty() && f.calls.empty(),"cancel without shutdown/load request");
    }
  }
}

void refusals() {
  { Fixture f; f.put(A::MODE_B,0); f.services.requests.reset(); f.world.arena_begin(f.services); check(f.calls.empty(),"disabled B begin no-op"); }
  { Fixture f; f.put(A::WAVE,0xffff,2); f.requests->prepare_players={}; refused([&]{f.world.arena_begin(f.services);},"player/resource"); check(f.calls.empty() && f.get(A::WAVE,2)==0xffff,"missing body refuses before preload/mutation"); }
  { Fixture f; f.put(A::WAVE,0xffff,2); f.requests->tables.recipes[0][1]=0xff; refused([&]{f.world.arena_begin(f.services);},"class index"); check(f.calls.empty(),"invalid signed class preflight"); }
  { Fixture f; f.put(A::WAVE,0xffff,2); f.requests->tables.recipes.clear(); refused([&]{f.world.arena_begin(f.services);},"recipe count"); }
  { Fixture f; f.put(A::WAVE,0xffff,2); f.requests->preload_factory=[](Raw){return Raw(0x10000);}; refused([&]{f.world.arena_begin(f.services);},"non-null"); check(f.get(A::WAVE,2)==0xffff && f.spawns.empty(),"unexecuted destruction stops begin"); }
  { Fixture f; f.put(A::WAVE,32767,2); f.put(A::WAVES_LEFT,1,2); refused([&]{f.world.arena_start_wave(f.services,1);},"signed-wave overflow"); check(f.calls.empty() && f.get(A::WAVE,2)==32767,"no signed overflow mutations"); }
  { Fixture f; f.put(A::WAVE,0,2); f.put(A::WAVES_LEFT,2,2); f.put(A::SAVED_COUNT,1,2); refused([&]{f.world.arena_start_wave(f.services,1);},"saved-creature"); check(f.calls.empty(),"saved creature construction is explicit refusal"); }
  { Fixture f; f.put(A::WAVE,14,2); f.put(A::INTERMISSION,480,2); f.requests->tables.recipes[0][0]=0x3f; refused([&]{f.visit();},"level index"); check(f.calls.empty() && f.get(A::INTERMISSION,2)==480,"bad level destination before counter write"); }
  { Fixture f; f.put(A::WAVE,14,2); f.put(A::INTERMISSION,480,2); f.requests->copy_level={}; refused([&]{f.visit();},"copy service"); check(f.get(A::INTERMISSION,2)==480,"missing loader never succeeds"); }
  { Fixture f(2); f.put(A::SCORE+12,0,2); f.put(A::FRAME,128); refused([&]{f.visit();},"qsort tie"); check(!f.get(A::INTERMISSION,2) && !f.get(A::END),"ties retain refusal"); }
  { Fixture f; f.put(0x4f8420,1); refused([&]{f.visit();},"gold-purchase"); check(f.calls.empty(),"no invented rewards"); }
  { Fixture f; f.put(A::FRAME,1); f.put(A::WAVE,0,2); f.put(A::WAVES_LEFT,2,2); f.put(A::PLAYERS+0xac,0); f.put(A::SCORE+2,0xffff,2); f.put(A::SCORE+4,2,2); f.visit(); check(!f.get(A::END) && f.get(A::WAVE,2)==0 && f.get(A::WAVES_LEFT,2)==2 && f.get(A::INTRO,2)==180 && f.get(A::SCORE+4,2)==2,"positive snapshot retries same wave and preserves negative-life snapshot"); }
}
} // namespace
int main(int argc,char **argv) {
  try {
    set_fpu(Fpu::Model::Ps2);
    if(argc!=2) throw std::runtime_error("scheduler oracle path required");
    oracle(argv[1]); timing_and_grid(); levels_and_terminal(); refusals();
    std::cout << checks << " arena scheduler runtime checks passed\n";
    return 0;
  } catch(const std::exception &e) { std::cerr << "FAIL after " << checks << ": " << e.what() << '\n'; return 1; }
}
