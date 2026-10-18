#include "champions/play_animation.hpp"
namespace champions::simulation::rta::play_animation {
namespace A = creature_anim;
namespace {
// Independently authored two-joint .anm fixture. Root XY is zero; the child
// breathes vertically. No commercial bytes, guessed resource names or root AI.
std::vector<std::uint8_t> clip() {
  std::vector<std::uint8_t> out(0x84, 0);
  auto word = [&](unsigned at, unsigned v) {
    for (unsigned i=0;i<4;++i) out[at+i]=std::uint8_t(v>>(8*i));
  };
  word(0,2); word(4,120); word(8,0x84); word(12,0x74); word(16,0x70);
  out[0x71]=1;
  std::vector<unsigned> bits;
  auto put = [&](unsigned v,unsigned n) { for(unsigned i=n;i;--i) bits.push_back((v>>(i-1))&1); };
  for(unsigned j=0;j<2;++j) {
    put(15,4); for(int k=0;k<3;++k) put(0,16);
    put(15,4); put(4096,16); for(int k=0;k<3;++k) put(0,16);
  }
  for(unsigned k=0;k<12;++k) {
    put(k ? 10 : 0,8); put(1,1); put(1,6); put(15,4);
    put(0,16); put(0,16); put(k%2 ? unsigned(-32) : 32,16);
  }
  put(255,8); while(bits.size()%32) bits.push_back(0);
  for(unsigned i=0;i<bits.size();i+=16) {
    unsigned v=0; for(unsigned j=0;j<16;++j) v=(v<<1)|bits[i+j];
    out.push_back(v&255); out.push_back(v>>8);
  }
  return out;
}
}
Actors::Actors() {
  ScopedFpu fixed{Fpu{Fpu::Model::Ps2,false,false}};
  // Four faces, skinned diamond: base vertices root-bound, crown child-bound.
  const std::array<std::array<std::int16_t,3>,5> points{{{-64,-64,16},{64,-64,16},{64,64,16},{-64,64,16},{0,0,192}}};
  for(unsigned side=0;side<4;++side)
    for(unsigned index : {side,(side+1)%4,4u})
      mesh_.vertices.push_back({points[index],{0,0,127},{{std::uint8_t(index==4),255}}});
  auto bytes=clip();
  for(unsigned slot=0;slot<3;++slot) {
    states_[slot]=runtime_.add_state();
    std::map<Raw,A::ClipId> resources;
    A::DispatchContext ctx{runtime_,states_[slot],[&](Raw entry) {
      auto &id=resources[entry]; if(!id) id=runtime_.add_clip(bytes); return id;
    },{}};
    if(slot) {
      A::install_dispatch_events(ctx,"Agony");
      A::apply_dispatch(ctx,A::select_dispatch("Agony",0));
      A::apply_dispatch(ctx,A::select_dispatch("Agony",6));
    } else {
      A::install_player_events(ctx);
      A::apply_dispatch(ctx,A::player_action("script_idle",{},{}));
    }
    frames_[slot].flags=runtime_.state(states_[slot]).flags;
    pose(slot);
  }
}
void Actors::pose(unsigned slot) {
  const auto &p=runtime_.draw_pose(states_.at(slot));
  auto &vertices=frames_.at(slot).vertices;
  vertices.clear();
  for(const auto &v:mesh_.vertices) vertices.push_back(A::skin_point(p,v));
}
void Actors::advance(unsigned slot,std::uint32_t tick) {
  auto &frame=frames_.at(slot);
  if(tick!=frame.tick+1) throw A::Unsupported("animation requires the next ready tick");
  runtime_.set_sim_frame(tick); runtime_.set_draw_frame(tick);
  runtime_.sound_log.clear();
  auto motion=runtime_.advance(ONE,states_[slot],false);
  if(motion.x || motion.y || motion.yaw) throw A::Unsupported("stationary fixture refuses root displacement");
  frame.events=slot ? A::dispatch_events("Agony",6,motion.events) : A::player_events(0,motion.events);
  frame.flags=runtime_.state(states_[slot]).flags;
  frame.tick=tick;
  for(const auto &event:frame.events)
    if(event.kind=="footstep" && (event.mask==4 || event.mask==8) && footstep_) footstep_(slot,event);
  pose(slot);
}
const Frame &Actors::frame(unsigned slot) const { return frames_.at(slot); }
void Actors::set_footstep_hook(std::function<void(unsigned,const A::DispatchEvent &)> hook) { footstep_=std::move(hook); }
}
