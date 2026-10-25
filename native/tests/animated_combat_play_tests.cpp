#include "champions/combat_play.hpp"
#include <iostream>
using namespace champions::simulation::rta;
void check(bool c,const char *m) { if(!c) throw std::runtime_error(m); }
int main() { try {
  combat_play::Session fight(session_control::Controller().setup().initial_terrain,true);
  auto initial=fight.animation_frame(1).vertices;
  check(fight.animated() && fight.actors().size()==3,"three animated actors");
  fight.advance(0);
  check(fight.observation().ready_ticks==0 && fight.animation_frame(1).tick==0,"no ready no animation");
  for(unsigned tick=1;tick<=36;++tick) {
    auto o=fight.advance(0);
    if(o.failed) throw std::runtime_error(o.error);
    check(o.hp==from_int(100) && o.damage_events==0,"no damage before tick 37");
    check(fight.animation_frame(1).tick==tick,"one animation step per ready tick");
  }
  check(initial!=fight.animation_frame(1).vertices,"real skinned pose changes");
  auto pose=fight.animation_frame(1).vertices;
  fight.set_paused(true); fight.advance(0);
  check(fight.animation_frame(1).tick==36 && fight.animation_frame(1).vertices==pose,"pause freezes poses");
  fight.set_paused(false);
  auto hit=fight.advance(0);
  if(hit.failed) throw std::runtime_error(hit.error);
  std::cout<<"tick="<<hit.ready_ticks<<" hp="<<hit.hp<<" damage_events="<<hit.damage_events<<'\n';
  check(hit.ready_ticks==37 && hit.hp==from_int(84) && hit.damage_events==2,"two exact hits at tick 37");
  for(unsigned slot=1;slot<3;++slot) {
    const auto &events=fight.animation_frame(slot).events;
    check(events.size()==1 && events[0].kind=="hit_test" && events[0].mask==1,"typed native hit event");
  }
  fight.advance(0);
  check(fight.observation().hp==hit.hp && fight.observation().damage_events==2,"no duplicate hit");
  bool refused=false;try { fight.hit_event(0,1); } catch(const std::exception &) { refused=true; }
  check(refused,"unknown player action timing refuses");
  for(unsigned tick=39;tick<=90;++tick) {
    auto o=fight.advance(0);
    if(o.failed) throw std::runtime_error(o.error);
    check(o.hp==from_int(tick<67 ? 84 : 74),"second table event exactly tick 67");
  }
  check(fight.observation().stopped && fight.animation_frame(1).tick==90,"explicit bounded fixture");
  std::cout<<"animated combat exact-frame, poses, pause, refusal and bounded retention passed\n";
  return 0;
} catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;} }
