#include "champions/level_play.hpp"
#include <iostream>
using namespace champions::simulation::rta;
void check(bool c,const char *m){if(!c)throw std::runtime_error(m);}
template<class F>void refuse(F f){bool caught=false;try{f();}catch(const std::exception &){caught=true;}check(caught,"expected refusal");}
int main(){try{
  level_play::Session s;
  check(s.flow().level=="aa"&&s.flow().gate_contact,"initial marker stands on gate");
  check(s.observation().facing==1769,"shared LCG fourth draw is already upper16 (43769), marker facing 1769");
  refuse([&]{s.advance(1);});refuse([&]{s.confirm_gate("aab");});
  s.advance(0);check(s.observation().ready_ticks==0,"first submission no ready");
  for(int i=0;i<15;++i)s.advance(0);
  check(s.flow().trigger_inside,"native trigger entered before save");
  s.open_gate();auto before=s.observation();s.advance(0);
  check(s.observation().paused&&s.observation().submissions==before.submissions,"menu freezes ready clock");
  refuse([&]{s.confirm_gate("aa");});s.confirm_gate("aab");
  s.set_paused(true);s.advance(0);check(s.flow().travel_ticks==1,"paused gate clock");s.set_paused(false);
  for(int i=0;i<179;++i){s.advance(0);check(s.flow().level=="aa","no early arrival");}
  check(s.flow().travel_ticks==180,"179 ready ticks");s.advance(0);
  if(s.observation().failed)throw std::runtime_error(s.observation().error);
  check(s.flow().level=="aab"&&s.flow().travel_ticks==181,"180-tick arrival");
  check(s.observation().ready_ticks==before.ready_ticks+180,"exact ready clock");
  check(s.flow().resolved_spawn==s.observation().position&&s.observation().position[2]==0,"native destination placement");
  check(s.flow().gate_contact&&s.flow().saved_levels==1&&!s.flow().trigger_inside,"fresh destination state");
  refuse([&]{s.request_level("aa");});s.advance(0);check(s.flow().travel_ticks==0,"arrival clears next ready");
  s.request_level("aa");s.advance(0);
  if(s.observation().failed)throw std::runtime_error(s.observation().error);
  check(s.flow().level=="aa"&&s.flow().restored==1&&s.flow().restored_bytes>16,"saved level decoded");
  check(s.flow().trigger_inside&&s.flow().saved_levels==2,"saved trigger state restored");
  refuse([&]{s.request_level("unknown");});
  s.stop();auto final=s.observation();s.advance(0);check(s.observation().submissions==final.submissions,"stop freezes");
  std::cout<<"PASS level fixture: real GOB/objects/terrain/water/World; gate 180 ready ticks; saved trigger re-entry bytes="<<s.flow().restored_bytes<<'\n';
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
