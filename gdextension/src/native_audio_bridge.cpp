#include "native_audio_bridge.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <algorithm>
#include <exception>
#include <stdexcept>
#include <string>
using namespace godot;
namespace {
std::string utf8(const String &value) {const auto bytes=value.utf8();return std::string(bytes.get_data(),std::size_t(bytes.length()));}
champions::audio::Vec3 vec(const Vector3 &v) {return {v.x,v.y,v.z};}
champions::audio::Bytes view(const PackedByteArray &b) {return {b.ptr(),std::size_t(b.size())};}
}
void ChampionsNativeAudio::_bind_methods() {
    ClassDB::bind_method(D_METHOD("register_vag","name","bytes"),&ChampionsNativeAudio::register_vag);
    ClassDB::bind_method(D_METHOD("register_vat","name","directory","vat","language"),&ChampionsNativeAudio::register_vat);
    ClassDB::bind_method(D_METHOD("set_level_directory","directory","vat_size"),&ChampionsNativeAudio::set_level_directory);
    ClassDB::bind_method(D_METHOD("attach_playback","playback"),&ChampionsNativeAudio::attach_playback);
    ClassDB::bind_method(D_METHOD("set_camera","position","right"),&ChampionsNativeAudio::set_camera);
    ClassDB::bind_method(D_METHOD("play_effect","name","position","volume","group","looping"),&ChampionsNativeAudio::play_effect);
    ClassDB::bind_method(D_METHOD("animation_event","mask","sample_name","position"),&ChampionsNativeAudio::animation_event);
    ClassDB::bind_method(D_METHOD("queue_stream","stream","name","flags"),&ChampionsNativeAudio::queue_stream);
    ClassDB::bind_method(D_METHOD("play_menu_music","wait_for_current"),&ChampionsNativeAudio::play_menu_music);
    ClassDB::bind_method(D_METHOD("play_boss_music","type","object_name"),&ChampionsNativeAudio::play_boss_music);
    ClassDB::bind_method(D_METHOD("stop_boss_music"),&ChampionsNativeAudio::stop_boss_music);
    ClassDB::bind_method(D_METHOD("release_stream","stream"),&ChampionsNativeAudio::release_stream);
    ClassDB::bind_method(D_METHOD("sound_tick","cosmetic_draw"),&ChampionsNativeAudio::sound_tick);
    ClassDB::bind_method(D_METHOD("pump_frames","max_frames"),&ChampionsNativeAudio::pump_frames);
    ClassDB::bind_method(D_METHOD("get_last_error"),&ChampionsNativeAudio::get_last_error);
}
bool ChampionsNativeAudio::register_vag(const String &name,const PackedByteArray &bytes) {
    try {model_.add_effect(utf8(name),champions::audio::decode_vag(view(bytes)));last_error_="";return true;}
    catch(const std::exception &e) {last_error_=String::utf8(e.what());return false;}
}
bool ChampionsNativeAudio::register_vat(const String &name,const PackedByteArray &directory,const PackedByteArray &vat,int32_t language) {
    try {auto entries=champions::audio::parse_vat_directory(view(directory),std::size_t(vat.size()));auto entry=champions::audio::find_vat_entry(entries,utf8(name),std::int16_t(language));
        if(!entry){last_error_="VAT entry or language not found";return false;} model_.add_stream(entry->name,champions::audio::decode_vat_entry(view(vat),*entry));last_error_="";return true;}
    catch(const std::exception &e){last_error_=String::utf8(e.what());return false;}
}
bool ChampionsNativeAudio::set_level_directory(const PackedByteArray &directory,int64_t vat_size) {
    try {if(vat_size<0)throw std::invalid_argument("negative VAT size");level_entries_=champions::audio::parse_vat_directory(view(directory),std::size_t(vat_size));last_error_="";return true;}
    catch(const std::exception &e){last_error_=String::utf8(e.what());return false;}
}
void ChampionsNativeAudio::attach_playback(const Ref<AudioStreamGeneratorPlayback> &p){playback_=p;}
void ChampionsNativeAudio::set_camera(const Vector3 &p,const Vector3 &r){model_.set_camera(vec(p),vec(r));}
bool ChampionsNativeAudio::play_effect(const String &n,const Vector3 &p,double v,int32_t g,bool l){try{return model_.play_effect(utf8(n),vec(p),v,g,l);}catch(const std::exception&e){last_error_=String::utf8(e.what());return false;}}
bool ChampionsNativeAudio::animation_event(int64_t m,const String &n,const Vector3 &p){return model_.animation_event(std::uint32_t(m),utf8(n),vec(p));}
bool ChampionsNativeAudio::queue_stream(int32_t i,const String &n,int64_t f){try{return model_.queue_stream(std::size_t(i),utf8(n),std::uint32_t(f));}catch(const std::exception&e){last_error_=String::utf8(e.what());return false;}}
bool ChampionsNativeAudio::play_menu_music(bool wait){return model_.play_menu_music(wait);}
bool ChampionsNativeAudio::play_boss_music(const String &t,const String &o){return model_.play_boss_music(utf8(t),utf8(o));}
void ChampionsNativeAudio::stop_boss_music(){model_.stop_boss_music();}
void ChampionsNativeAudio::release_stream(int32_t i){if(i>=0&&i<3)model_.release_stream(std::size_t(i));}
bool ChampionsNativeAudio::sound_tick(int32_t draw){
    try{model_.tick();return level_entries_.empty()?false:model_.poll_level(level_entries_,std::uint32_t(draw));}
    catch(const std::exception &e){last_error_=String::utf8(e.what());return false;}
}
int32_t ChampionsNativeAudio::pump_frames(int32_t maximum){
    if(playback_.is_null()||maximum<=0)return 0;
    const auto count=std::min(maximum,playback_->get_frames_available());if(count<=0)return 0;
    auto frames=model_.render(std::size_t(count));PackedVector2Array buffer;buffer.resize(count);
    for(int32_t i=0;i<count;++i)buffer.set(i,Vector2(frames[std::size_t(i)][0],frames[std::size_t(i)][1]));
    if(!playback_->push_buffer(buffer)){last_error_="AudioStreamGenerator buffer push failed";return 0;}return count;
}
