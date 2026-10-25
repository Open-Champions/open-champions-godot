#include "champions/creature_anim/dispatch.hpp"
#include "champions/native_audio.hpp"
#include "creature_dispatch_fixture.hpp"
#include <iostream>
using namespace champions::simulation::rta;
namespace A=champions::simulation::rta::creature_anim;
namespace S=champions::audio;
unsigned checks=0;
void check(bool value,const char *why) { ++checks; if(!value) throw std::runtime_error(why); }
int main() try {
  ScopedFpu fixed{Fpu{Fpu::Model::Ps2,false,false}};
  A::Runtime runtime;
  auto state=runtime.add_state();
  std::map<Raw,A::ClipId> clips;
  A::DispatchContext context{runtime,state,[&](Raw entry) {
    auto &id=clips[entry]; if(!id) id=runtime.add_clip(dispatch_fixture::clip_bytes(120)); return id;
  },{}};
  A::install_dispatch_events(context,"FatCyclops");
  A::apply_dispatch(context,A::select_dispatch("FatCyclops",1)); // documented walk, entry 16
  S::Playback audio;
  S::Pcm pcm; pcm.rate=48000; pcm.left.assign(1600,1024);
  check(audio.add_effect("synthetic-step",pcm),"synthetic effect");
  // Actual VAT parsing and level policy, with independently authored silent blocks.
  std::vector<std::uint8_t> dir(0xa0),vat(0x800);
  const std::string name="mx_theme_synthetic.vag";
  std::copy(name.begin(),name.end(),dir.begin());
  dir[0x91]=8; // terminal offset 0x800 at record 1 + 0x40
  auto entries=S::parse_vat_directory({dir.data(),dir.size()},vat.size());
  // g_musicVolume stays at save_init's 0.5 (no options file); music reads it.
  audio.set_level_vat(vat,entries,0,1); // authored initial countdown
  unsigned count=0;
  for(unsigned tick=1;tick<=50;++tick) {
    runtime.set_sim_frame(tick); runtime.set_draw_frame(tick);
    auto motion=runtime.advance(ONE,state,false);
    auto events=A::dispatch_events("FatCyclops",1,motion.events);
    for(const auto &event:events) if(event.kind=="footstep") {
      check((tick==13 && event.mask==4) || (tick==49 && event.mask==8),"documented walk event tick");
      check(audio.frame()==tick-1,"event delivered before mixing its tick");
      auto voice=audio.animation_event(event.mask,S::ClipRole::locomotion,"synthetic-step",{});
      check(voice>=0 && audio.voices()[voice].sample=="synthetic-step","footstep voice allocated");
      ++count;
    }
    check(bool(motion.events&12)==(tick==13 || tick==49),"no early or duplicate steps");
    audio.poll_level(0); audio.tick();
    check(audio.frame()==tick,"one audio tick per animation tick");
    if(tick==1) check(audio.streams()[2].name==name && audio.queued_count(2)==0,
      "g_soundFrame image 0 starts the selected music on the first tick");
    auto samples=audio.drain(800);
    check(samples.size()==800,"exact 800 frames per tick");
    if(tick==13 || tick==49) check(samples[0][0]>0,"step audible PCM on exact event tick");
  }
  check(count==2,"both documented walk masks delivered once");
  check(audio.animation_event(4,S::ClipRole::attack,"synthetic-step",{})==-1,"attack combo is not footstep");
  check(audio.failed_stream_entries()==0 && audio.dropped_frames()==0,"no dropped/invalid audio");
  std::cout<<"playable audio: "<<checks<<" checks; FatCyclops walk steps ticks 13/49; level music starts tick 1\n";
  return 0;
} catch(const std::exception &e) { std::cerr<<e.what()<<'\n';return 1; }
