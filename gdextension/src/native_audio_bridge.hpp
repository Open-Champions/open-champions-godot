#pragma once
#include "champions/native_audio.hpp"
#include <godot_cpp/classes/audio_stream_generator_playback.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector3.hpp>
namespace godot {
class ChampionsNativeAudio : public RefCounted {
    GDCLASS(ChampionsNativeAudio, RefCounted)
    champions::audio::Playback model_;
    std::vector<champions::audio::VatEntry> level_entries_;
    Ref<AudioStreamGeneratorPlayback> playback_;
    String last_error_;
    static void _bind_methods();
public:
    bool register_vag(const String &name,const PackedByteArray &bytes);
    bool register_vat(const String &name,const PackedByteArray &directory,const PackedByteArray &vat,int32_t language);
    bool set_level_directory(const PackedByteArray &directory,int64_t vat_size);
    void attach_playback(const Ref<AudioStreamGeneratorPlayback> &playback);
    void set_camera(const Vector3 &position,const Vector3 &right);
    bool play_effect(const String &name,const Vector3 &position,double volume,int32_t group,bool looping);
    bool animation_event(int64_t mask,const String &sample_name,const Vector3 &position);
    bool queue_stream(int32_t stream,const String &name,int64_t flags);
    bool play_menu_music(bool wait_for_current);
    bool play_boss_music(const String &type,const String &object_name);
    void stop_boss_music();
    void release_stream(int32_t stream);
    bool sound_tick(int32_t cosmetic_draw);
    int32_t pump_frames(int32_t max_frames);
    String get_last_error() const {return last_error_;}
};
}
