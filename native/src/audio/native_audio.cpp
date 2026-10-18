#include "champions/native_audio.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace champions::audio {
namespace {
void need(Bytes b, std::size_t off, std::size_t len) { if (off>b.size || len>b.size-off || (len && !b.data)) throw std::invalid_argument("truncated audio data"); }
std::uint32_t be32(Bytes b, std::size_t off) { need(b,off,4); return (std::uint32_t(b.data[off])<<24)|(std::uint32_t(b.data[off+1])<<16)|(std::uint32_t(b.data[off+2])<<8)|b.data[off+3]; }
std::uint32_t le32(Bytes b, std::size_t off) { need(b,off,4); return std::uint32_t(b.data[off])|(std::uint32_t(b.data[off+1])<<8)|(std::uint32_t(b.data[off+2])<<16)|(std::uint32_t(b.data[off+3])<<24); }
std::uint16_t le16(Bytes b, std::size_t off) { need(b,off,2); return std::uint16_t(b.data[off])|(std::uint16_t(b.data[off+1])<<8); }
std::string name(Bytes b, std::size_t off, std::size_t len) { need(b,off,len); const auto *p=b.data+off; const auto *end=std::find(p,p+len,0); for(auto q=p;q!=end;++q) if(*q>127) throw std::invalid_argument("non-ASCII audio name"); return std::string(p,end); }
std::int16_t clamp16(int value) { return std::int16_t(std::max(-32768,std::min(32767,value))); }
std::string lower(std::string s) { for(char &c:s) if(c>='A'&&c<='Z') c=char(c-'A'+'a'); return s; }
}
VagHeader parse_vag_header(Bytes b) {
    need(b,0,0x30); VagHeader h; std::copy_n(b.data,4,h.magic.begin()); h.version=be32(b,4); h.data_size=be32(b,0x0c); h.rate=be32(b,0x10); h.name=name(b,0x20,16);
    if(h.data_size%16 || !h.rate) throw std::invalid_argument("invalid VAG size or rate"); return h;
}
std::uint32_t base_pitch(std::uint32_t rate) { return std::uint32_t(std::uint64_t(rate)*4096/48000); }
Pcm decode_adpcm(Bytes b, bool honor_flags) {
    if(b.size%16) throw std::invalid_argument("ADPCM body is not block aligned");
    Pcm out; int prev=0,older=0; constexpr int c1[]={0,60,115,98,122}, c2[]={0,0,-52,-55,-60};
    for(std::size_t off=0;off<b.size;off+=16) {
        need(b,off,16); const auto head=b.data[off], flags=b.data[off+1]; const int filter=head>>4, shift=head&15;
        if(filter>4 || shift>12 || (honor_flags && flags>7)) throw std::invalid_argument("invalid ADPCM block header");
        if(honor_flags && flags==7) break;
        if(honor_flags && (flags&4)) out.loop_start=out.left.size();
        for(std::size_t j=2;j<16;++j) for(int k=0;k<2;++k) {
            const int nibble=k ? b.data[off+j]>>4 : b.data[off+j]&15;
            const int signed_nibble=nibble<8?nibble:nibble-16;
            // Arithmetic right shift, specified explicitly for negative values.
            const int scaled=signed_nibble*4096; const int divisor=1<<shift;
            const int value=scaled>=0 ? scaled/divisor : -((-scaled+divisor-1)/divisor);
            const int predict=c1[filter]*prev+c2[filter]*older+32;
            const int filtered=predict>=0 ? predict/64 : -((-predict+63)/64);
            const int sample=clamp16(value+filtered);
            out.left.push_back(std::int16_t(sample)); older=prev; prev=sample;
        }
        if(honor_flags && (flags&1)) { if(flags&2) out.loop_end=out.left.size(); break; }
    }
    return out;
}
Pcm decode_vag(Bytes b) { auto h=parse_vag_header(b); need(b,0x30,h.data_size); auto out=decode_adpcm({b.data+0x30,h.data_size}); out.rate=h.rate; return out; }
std::vector<VatEntry> parse_vat_directory(Bytes dir, std::size_t vat_size) {
    if(!dir.size || dir.size%0x50) throw std::invalid_argument("VAT directory is not record aligned");
    std::vector<VatEntry> entries; bool terminal=false;
    for(std::size_t off=0;off<dir.size;off+=0x50) {
        auto n=name(dir,off,0x40); const auto start=le32(dir,off+0x40);
        if(n.empty()) { if(start!=vat_size) throw std::invalid_argument("VAT terminal offset mismatch"); terminal=true; break; }
        if(off+0xa0>dir.size) throw std::invalid_argument("VAT terminal missing");
        const auto end=le32(dir,off+0x90); const auto channels=le16(dir,off+0x48);
        if(start%0x800 || end%0x800 || start>end || end>vat_size || channels>1) throw std::invalid_argument("invalid VAT entry");
        entries.push_back({std::move(n),start,end-start,le32(dir,off+0x44),le32(dir,off+0x4c),channels==1,std::int16_t(le16(dir,off+0x4a))});
    }
    if(!terminal) throw std::invalid_argument("VAT terminal missing"); return entries;
}
std::optional<VatEntry> find_vat_entry(const std::vector<VatEntry>& entries,const std::string& n,std::int16_t lang) {
    std::optional<VatEntry> found; for(const auto &e:entries) if(e.name==n && (e.language==0 || e.language==lang)) found=e; return found;
}
Pcm decode_vat_entry(Bytes vat,const VatEntry &e) {
    need(vat,e.offset,e.length); if(e.length%16) throw std::invalid_argument("VAT entry not block aligned");
    if(e.loop_restart) throw std::invalid_argument("nonzero VAT loop restart is not validated");
    Pcm out; // Stream pitch is the fixed 0x7fe; declared rate is not used.
    if(!e.stereo) { out=decode_adpcm({vat.data+e.offset,e.length},false); return out; }
    if(e.length%0x800) throw std::invalid_argument("VAT stereo entry is not whole interleave pairs");
    std::vector<std::uint8_t> l,r; l.reserve(e.length/2); r.reserve(e.length/2);
    for(std::size_t off=e.offset;off<e.offset+e.length;off+=0x800) { l.insert(l.end(),vat.data+off,vat.data+off+0x400); r.insert(r.end(),vat.data+off+0x400,vat.data+off+0x800); }
    out=decode_adpcm({l.data(),l.size()},false); auto right=decode_adpcm({r.data(),r.size()},false); out.right=std::move(right.left); return out;
}
double stream_duration(std::size_t bytes,bool stereo,double rate) { if(bytes%16 || !std::isfinite(rate) || rate<=0) throw std::invalid_argument("invalid stream duration input"); return double(bytes/16)*28.0/rate/(stereo?2:1); }
Gains pan_atten(Vec3 p,Vec3 c,Vec3 r,double near,double far) {
    if(!std::isfinite(near)||!std::isfinite(far)||near<0||far<=near) throw std::invalid_argument("invalid audio radii");
    const double dx=p.x-c.x,dy=p.y-c.y,dz=p.z-c.z; const double dist=std::sqrt(dx*dx+dy*dy+dz*dz);
    const double atten=std::clamp((far-dist)/(far-near),0.0,1.0); const double pan=std::clamp((dx*r.x+dy*r.y+dz*r.z)/far,-1.0,1.0); return {atten*(1-pan),atten*(1+pan)};
}
void Playback::add_effect(std::string n,Pcm p) { if(!p.rate || p.left.empty() || p.stereo()) throw std::invalid_argument("effect must be mono PCM with rate"); effects_[std::move(n)]=std::move(p); }
void Playback::add_stream(std::string n,Pcm p) { if(p.left.empty() || (p.stereo() && p.left.size()!=p.right.size())) throw std::invalid_argument("invalid stream PCM"); stream_pcm_[std::move(n)]=std::move(p); }
int Playback::group_limit(int group) { if(group<0 || group>98) throw std::invalid_argument("group outside documented RTA range"); return group==0?32:group==14?9:(group==50||group==51)?8:2; }
std::size_t Playback::active_group(int group) const { std::size_t n=0; for(const auto &v:sequences_) if(v.active && v.group==group) ++n; return n; }
bool Playback::play_effect(const std::string& n,Vec3 pos,double vol,int group,bool looping) {
    auto it=effects_.find(n); if(it==effects_.end()) return false; if(!std::isfinite(vol)||vol<0) throw std::invalid_argument("invalid volume");
    (void)group;
    const auto gain=pan_atten(pos,camera_,right_); if(!looping && (gain.left*vol==0 && gain.right*vol==0)) return false;
    for(std::size_t probe=0;probe<effect_voice_limit;++probe) { const auto i=(next_voice_+probe)%effect_voice_limit; if(!voices_[i].active) {
        auto &v=voices_[i]; v={}; v.active=true; v.looping=looping; v.name=n; v.position=pos; v.volume=vol; v.gains=gain; v.group=0;
        v.step=double(std::min<std::uint32_t>(0x3fff,base_pitch(it->second.rate)))/4096.0; next_voice_=(i+1)%effect_voice_limit; return true;
    }} return false;
}
int Playback::start_sequence(const std::string& n,Vec3 pos,int group) {
    if(group<0 || group>98) throw std::invalid_argument("group outside documented RTA range");
    if(group && active_group(group)>=std::size_t(group_limit(group))) return -1;
    std::size_t slot=sequence_limit; for(std::size_t i=0;i<sequence_limit;++i) if(!sequences_[i].active) {slot=i;break;}
    if(slot==sequence_limit) return -1;
    if(!play_effect(n,pos,1,0,false)) return -1;
    const auto voice=(next_voice_+effect_voice_limit-1)%effect_voice_limit;
    auto &seq=sequences_[slot]; seq.active=true;seq.repeat=false;seq.group=group;seq.voice=int(voice);seq.generation=std::uint8_t((seq.generation+1)&0x7f);
    voices_[voice].group=group;voices_[voice].sequence_slot=int(slot);
    return 0x8000 | (int(seq.generation)<<8) | int(slot);
}
bool Playback::play_sequence(const std::string& n,Vec3 pos,int group,bool looping) {
    const int h=start_sequence(n,pos,group);if(h<0)return false;if(looping)release_sequence(h);return true;
}
bool Playback::stop_sequence(int h) {
    if((h&0x8000)==0)return false;const auto slot=std::size_t(h&0xff);if(slot>=sequence_limit)return false;
    auto &s=sequences_[slot];if(!s.active || s.generation!=std::uint8_t((h>>8)&0x7f))return false;
    if(s.voice>=0)voices_[std::size_t(s.voice)].active=false;s.active=false;return true;
}
bool Playback::release_sequence(int h) {
    if((h&0x8000)==0)return false;const auto slot=std::size_t(h&0xff);if(slot>=sequence_limit)return false;
    auto &s=sequences_[slot];if(!s.active || s.generation!=std::uint8_t((h>>8)&0x7f))return false;
    s.repeat=true;return true;
}
void Playback::set_camera(Vec3 p,Vec3 r) { camera_=p;right_=r;for(auto &v:voices_) if(v.active) v.gains=pan_atten(v.position,camera_,right_,v.near,v.far); }
void Playback::set_voice_position(std::size_t i,Vec3 p) { if(i>=effect_voice_limit) throw std::out_of_range("voice index"); auto &v=voices_[i];v.position=p;if(v.active)v.gains=pan_atten(p,camera_,right_,v.near,v.far); }
bool Playback::queue_stream(std::size_t i,std::string n,std::uint32_t flags) {
    if(i>=stream_limit) throw std::out_of_range("stream index"); if(flags&(0x40|0x100)) throw std::invalid_argument("undocumented stream queue flags");
    if(flags&~std::uint32_t(0xaf)) throw std::invalid_argument("unsupported stream queue flags");
    if(!n.empty() && !stream_pcm_.count(n)) return false; if(queues_[i].size()>=stream_queue_limit) return false;
    queues_[i].push_back({std::move(n),flags}); service_queue(i); return true;
}
void Playback::service_queue(std::size_t i) {
    auto &s=streams_[i]; auto &q=queues_[i]; if(q.empty())return;
    const auto &entry=q.front();
    if(s.active) { if(entry.flags&2) s.target=0; if((entry.flags&4) || (entry.flags&2)) return; if(entry.flags&1) return; }
    if(s.active) return;
    if(entry.name.empty()) { q.erase(q.begin()); return; }
    s={};s.name=entry.name;s.active=true;s.held=(entry.flags&8)!=0;s.flags=entry.flags;s.target=0x3fff;q.erase(q.begin());
}
void Playback::release_stream(std::size_t i) { if(i>=stream_limit) throw std::out_of_range("stream index");streams_[i].held=false; }
void Playback::stop_stream(std::size_t i) { if(i>=stream_limit) throw std::out_of_range("stream index");streams_[i].target=0; }
std::size_t Playback::queued_count(std::size_t i) const { if(i>=stream_limit) throw std::out_of_range("stream index");return queues_[i].size(); }
void Playback::tick() {
    ++sound_frames_; fade_sample_remainder_+=800; while(fade_sample_remainder_>=288) { fade_sample_remainder_-=288;
        for(std::size_t i=0;i<stream_limit;++i) { auto &s=streams_[i];if(!s.active) continue;
            if(s.volume>s.target) s.volume=std::uint16_t(std::max<int>(s.target,int(s.volume)-0x20));
            else if(s.volume<s.target) s.volume=std::uint16_t(std::min<int>(s.target,int(s.volume)+(i==0?0x40:0xc0)));
            if(s.volume==0 && s.target==0) { s={}; service_queue(i); }
        }
    }
    if(sound_frames_%16==0 && !streams_[0].active) service_queue(0);
    for(std::size_t i=0;i<stream_limit;++i) service_queue(i);
}
std::vector<std::array<float,2>> Playback::render(std::size_t frames) {
    std::vector<std::array<float,2>> out(frames,{0,0});
    for(auto &v:voices_) if(v.active) { const auto &p=effects_.at(v.name);for(auto &f:out) {
        if(v.cursor>=p.left.size()) { if(v.sequence_slot>=0 && sequences_[std::size_t(v.sequence_slot)].repeat) {v.phase=0;v.cursor=0;} else if(v.looping && p.loop_start && p.loop_end && *p.loop_start<*p.loop_end) { v.phase=double(*p.loop_start);v.cursor=*p.loop_start; } else {v.active=false;if(v.sequence_slot>=0)sequences_[std::size_t(v.sequence_slot)].active=false;break;} }
        const float sample=float(p.left[v.cursor])/32768.0f; f[0]+=sample*float(v.volume*v.gains.left);f[1]+=sample*float(v.volume*v.gains.right);
        v.phase+=v.step;v.cursor=std::size_t(v.phase);
        if(v.looping && p.loop_start && p.loop_end && v.cursor>=*p.loop_end) {v.phase=double(*p.loop_start)+(v.phase-double(*p.loop_end));v.cursor=std::size_t(v.phase);}
    }}
    for(std::size_t i=0;i<stream_limit;++i) {auto &s=streams_[i];if(!s.active||s.held)continue;const auto &p=stream_pcm_.at(s.name);for(auto &f:out) {
        if(s.cursor>=p.left.size()) {if(s.flags&1) {s.cursor=0;s.phase=0;} else {s={};service_queue(i);break;}}
        const float gain=float(s.volume)/16383.0f;f[0]+=float(p.left[s.cursor])/32768.0f*gain;f[1]+=float(p.stereo()?p.right[s.cursor]:p.left[s.cursor])/32768.0f*gain;
        s.phase+=2046.0/4096.0;s.cursor=std::size_t(s.phase);
    }}
    for(auto &f:out) {f[0]=std::clamp(f[0],-1.0f,1.0f);f[1]=std::clamp(f[1],-1.0f,1.0f);}return out;
}
bool Playback::poll_level(const std::vector<VatEntry>& entries,std::uint32_t draw) {
    if(draw>=7200) throw std::invalid_argument("cosmetic draw outside 0..7199");
    bool ambience=false;
    if(sound_frames_%16==0 && !streams_[0].active && queues_[0].empty()) {
        for(const auto &e:entries) if(e.name.find("amb_")!=std::string::npos) {ambience=queue_stream(0,e.name,3);break;}
    }
    if(level_poll_remaining_>0) --level_poll_remaining_;
    if(level_poll_remaining_)return ambience;
    level_poll_remaining_=14400+draw;
    if(streams_[2].active || !queues_[2].empty()) return ambience;
    for(const auto &e:entries) if(e.name.find("_env_")!=std::string::npos || e.name.find("mx_theme")!=std::string::npos) return queue_stream(2,e.name,0x22);
    return ambience;
}
bool Playback::play_menu_music(bool wait) { return queue_stream(2,"mx_maintheme_l.vag",wait?5:1); }
bool Playback::play_boss_music(const std::string& type,const std::string& object) {
    const auto t=lower(type),o=lower(object);std::string n;
    if(t=="cazicavatar")n="avataroffear";else if(t=="fatcyclops"&&(o=="broon"||o=="droon"))n=o;
    else if(t=="cyclops"&&o=="proon")n=o;else if(t=="cazicthule")n="cazicthule";else if(t=="reddragon")n="dozekar";
    else if(t=="ironguardian")n="ironguardian";else if(t=="mattmarr")n="matanielmarr";else if(t=="raloszek")n="ralloszek";
    else if(t=="vampirelord")n="vanerhost";else if(t=="snowboss")n="whitetormentor";
    if(n.empty())return false;
    // A second active boss is unresolved; refuse instead of inventing override order.
    if(streams_[2].active && streams_[2].name.find("mx_ba_")==0) return false;
    return queue_stream(2,"mx_ba_"+n+"_l.vag",0x23);
}
void Playback::stop_boss_music() { stop_stream(2); }
bool Playback::animation_event(std::uint32_t mask,const std::string& sample,Vec3 pos) {
    if(mask&1) return play_effect(sample,pos);
    if(mask&(4|8)) return play_effect(sample,pos);
    return false;
}
}
