#include "champions/combat_play.hpp"
#include <iostream>
using namespace champions::simulation::rta;
void check(bool c, const char *m) { if (!c) throw std::runtime_error(m); }
template<class F> void refuse(F f) { bool caught=false; try { f(); } catch (const std::exception &) { caught=true; } check(caught,"expected refusal"); }
int main() {
  try {
    auto terrain = session_control::Controller().setup().initial_terrain;
    combat_play::Session fight(terrain);
    check(fight.actors().size()==3,"three actors");
    for (auto a:fight.actors()) check(a.present && !a.dead,"all alive");
    refuse([&]{fight.advance(1);});
    refuse([&]{fight.hit_event(0,2);});
    refuse([&]{fight.hit_event(1,1);});
    fight.set_paused(true);
    fight.advance(0);
    check(fight.observation().submissions==0,"paused no submission");
    fight.set_paused(false);
    fight.advance(0);
    check(fight.observation().ready_ticks==0,"first submission no ready");
    for (int i=0;i<100 && fight.observation().hp==from_int(100);++i) {
      auto o=fight.advance(0);
      if(o.failed) throw std::runtime_error(o.error);
    }
    auto hit=fight.observation();
    check(fight.visual_events().size()==2 && !fight.visual_events()[0].death && fight.visual_events()[0].slot==0,"two owned hit observations");
    check(hit.hp == from_int(84) && hit.ready_ticks == 12, "exact monster damage and timing");
    std::cout<<"damage frame="<<hit.frame<<" hp="<<hit.hp<<" state="<<fight.actors()[0].state<<'\n';
    fight.hit_event(0,1);
    fight.advance(0);
    auto lethal=fight.actors()[1];
    check(fight.visual_events().size()==1 && fight.visual_events()[0].slot==1 && !fight.visual_events()[0].death,"lethal hit observation");
    check(lethal.present && le(lethal.hp,0),"player lethal hit immediate, actor retained");
    fight.advance(0);
    check(fight.visual_events().size()==1 && fight.visual_events()[0].death && fight.visual_events()[0].slot==1,"native death queue observation");
    check(fight.actors()[1].present && fight.actors()[1].dead,"death queues next flush");
    fight.advance(0);
    check(fight.visual_events().empty(),"removal does not duplicate death observation");
    check(!fight.actors()[1].present,"next tick removal");
    check(fight.actors()[2].present,"second creature retained");
    check(fight.shared_seed()==1140036074,"exact fight RNG");
    auto before=fight.observation();
    fight.hit_event(0,1);
    fight.set_paused(true); // Discard pending presentation input.
    fight.advance(0);
    check(fight.observation().frame==before.frame,"pause freezes");
    terrain.data.heightfields[0].samples[0]=1;
    refuse([&]{combat_play::Session unsupported(terrain);});
    for(auto a:fight.actors()) std::cout<<a.slot<<' '<<a.handle<<' '<<a.present<<' '<<a.hp<<' '<<a.state<<'\n';
    std::cout<<"combat fixture fight and refusals passed seed="<<fight.shared_seed()<<'\n';
    return 0;
  } catch(const std::exception &e) { std::cerr<<e.what()<<'\n';return 1; }
}
