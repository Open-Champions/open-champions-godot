#include "champions/native_audio.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace champions::audio;
namespace {
void check(bool ok,const char* msg) {if(!ok)throw std::runtime_error(msg);}
void put_be(std::vector<std::uint8_t>& b,std::size_t o,std::uint32_t v) {for(int i=3;i>=0;--i){b[o+i]=std::uint8_t(v);v>>=8;}}
void put_le(std::vector<std::uint8_t>& b,std::size_t o,std::uint32_t v) {for(int i=0;i<4;++i){b[o+i]=std::uint8_t(v);v>>=8;}}
std::array<std::uint8_t,16> block(int filt=0,int shift=12,int flags=0,std::uint8_t first=0) {std::array<std::uint8_t,16> b{};b[0]=std::uint8_t((filt<<4)|shift);b[1]=std::uint8_t(flags);b[2]=first;return b;}
void record(std::vector<std::uint8_t>& d,std::size_t i,const char* name,std::uint32_t offset,int stereo=0,int lang=0) {const std::size_t p=i*0x50;for(std::size_t j=0;name[j];++j)d[p+j]=std::uint8_t(name[j]);put_le(d,p+0x40,offset);put_le(d,p+0x44,24000);d[p+0x48]=std::uint8_t(stereo);d[p+0x4a]=std::uint8_t(lang);}
Pcm sample(std::size_t n=2000) {Pcm p;p.rate=48000;p.left.assign(n,16384);return p;}
void vectors() {
    std::vector<std::uint8_t> h(0x50); put_be(h,4,4);put_be(h,0x0c,0x20);put_be(h,0x10,16000);h[0x20]='t';h[0x21]='e';h[0x22]='s';h[0x23]='t';
    auto vh=parse_vag_header({h.data(),h.size()});check(vh.magic==std::array<std::uint8_t,4>{0,0,0,0}&&vh.version==4&&vh.data_size==32&&vh.rate==16000&&vh.name=="test","VAG header exact vector");check(base_pitch(vh.rate)==1365,"base pitch exact vector");
    auto a=block(0,12,1,0x1f),end=block(0,12,7);std::copy(a.begin(),a.end(),h.begin()+0x30);std::copy(end.begin(),end.end(),h.begin()+0x40);
    auto d=decode_vag({h.data(),h.size()});check(d.left.size()==28&&d.left[0]==-1&&d.left[1]==1&&d.left[2]==0&& !d.loop_start,"nibble order and terminal exact vector");
    auto b=block(0,12,6,0x01),c=block(1,12,3);std::vector<std::uint8_t> pair;pair.insert(pair.end(),b.begin(),b.end());pair.insert(pair.end(),c.begin(),c.end());d=decode_adpcm({pair.data(),pair.size()});check(d.left.size()==56&&d.left[0]==1&&d.left[1]==0&&d.left[28]==0&&d.loop_start==0&&d.loop_end==56,"history and loop exact vector");
    bool bad=false;try{auto x=block(5);decode_adpcm({x.data(),x.size()});}catch(const std::invalid_argument&){bad=true;}check(bad,"invalid ADPCM filter");
    std::vector<std::uint8_t> dir(3*0x50);record(dir,0,"mx_test.vag",0,1);record(dir,1,"line.vag",0x800,0,3);record(dir,2,"",0x1000);auto e=parse_vat_directory({dir.data(),dir.size()},0x1000);
    check(e.size()==2&&e[0].name=="mx_test.vag"&&e[0].length==0x800&&e[0].stereo&&e[1].offset==0x800&&e[1].language==3,"VAT directory exact vector");
    bad=false;try{parse_vat_directory({dir.data(),dir.size()},0x1800);}catch(const std::invalid_argument&){bad=true;}check(bad,"VAT terminal size validation");
    std::vector<std::uint8_t> vat(0x1000);for(std::size_t off=0;off<0x1000;off+=16){vat[off]=12;vat[off+2]=std::uint8_t((off/0x400)%4+1);}auto stereo=decode_vat_entry({vat.data(),vat.size()},e[0]);check(stereo.stereo()&&stereo.left.size()==1792&&stereo.right.size()==1792&&stereo.left[0]==1&&stereo.right[0]==2,"0x400 stereo interleave");
    check(std::abs(stream_duration(0x800,true)-64.0*28/24000)<1e-12&&std::abs(stream_duration(0x800,false)-128.0*28/24000)<1e-12,"stream duration vectors");
    auto g=pan_atten({600,0,0},{0,0,0},{1,0,0});check(std::abs(g.left-0.5*(1-600.0/840))<1e-12&&std::abs(g.right-0.5*(1+600.0/840))<1e-12,"camera gain vector");check(pan_atten({840,0,0},{},{1,0,0}).left==0,"far mute vector");
}
void playback() {
    Playback p;p.add_effect("fx",sample());check(p.play_effect("fx",{}),"effect start");check(p.voices()[0].active,"first voice");
    for(int i=0;i<43;++i)p.play_effect("fx",{},1,0,true);check(!p.play_effect("fx",{},1,0,true),"43 voice cap");
    Playback groups;groups.add_effect("fx",sample());check(groups.play_sequence("fx",{},7,true)&&groups.play_sequence("fx",{},7,true)&&!groups.play_sequence("fx",{},7,true),"group 2 cap");
    for(int i=0;i<9;++i)check(groups.play_sequence("fx",{},14,true),"group 14 cap");check(!groups.play_sequence("fx",{},14,true),"group 14 reject");
    groups.set_camera({-600,0,0},{1,0,0});check(groups.voices()[0].gains.right>groups.voices()[0].gains.left,"moving camera pan");
    Playback streams;auto s=sample(50000);streams.add_stream("mx_maintheme_l.vag",s);streams.add_stream("mx_ba_broon_l.vag",s);streams.add_stream("mx_theme_test",s);
    check(streams.play_menu_music(false),"menu track");for(int i=0;i<20;++i)streams.tick();check(streams.streams()[2].volume>0,"fade in");
    check(streams.play_boss_music("fatcyclops","broon"),"boss mapping");check(streams.queued_count(2)==1,"boss sequential queue");
    for(int i=0;i<200;++i)streams.tick();check(streams.streams()[2].name=="mx_ba_broon_l.vag","boss starts after fade");check(!streams.play_boss_music("fatcyclops","broon"),"simultaneous boss gated");
    bool rejected=false;try{streams.queue_stream(2,"mx_theme_test",0x40);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"unknown flag gate");
    Playback level;level.add_stream("mx_theme_test",s);std::vector<VatEntry> list={{"voice",0,0,0,0,false,0},{"mx_theme_test",0,0,0,0,false,0}};
    for(int i=0;i<14400;++i)level.poll_level(list,0);check(level.streams()[2].name=="mx_theme_test","level first match timer");
    Playback events;events.add_effect("fx",sample());check(events.animation_event(1,"fx",{}),"hit hook");check(events.animation_event(4,"fx",{}),"left foot hook");check(events.animation_event(8,"fx",{}),"right foot hook");check(!events.animation_event(2,"fx",{}),"unmapped event");
    auto frames=events.render(800);check(frames.size()==800&&frames[0][0]>0,"PCM render");
}
}
int main(){try{vectors();playback();std::cout<<"native audio synthetic vectors and playback passed\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
