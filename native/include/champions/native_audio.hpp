#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace champions::audio {
struct Bytes { const std::uint8_t *data; std::size_t size; };
struct VagHeader { std::array<std::uint8_t,4> magic{}; std::uint32_t version=0, data_size=0, rate=0; std::string name; };
struct Pcm { std::vector<std::int16_t> left, right; std::optional<std::size_t> loop_start, loop_end; std::uint32_t rate=0; bool stereo() const { return !right.empty(); } };
struct VatEntry { std::string name; std::uint32_t offset=0, length=0, declared_rate=0, loop_restart=0; bool stereo=false; std::int16_t language=0; };
VagHeader parse_vag_header(Bytes bytes);
Pcm decode_adpcm(Bytes bytes, bool honor_flags=true);
Pcm decode_vag(Bytes bytes);
std::vector<VatEntry> parse_vat_directory(Bytes directory, std::size_t vat_size);
Pcm decode_vat_entry(Bytes vat, const VatEntry &entry);
std::optional<VatEntry> find_vat_entry(const std::vector<VatEntry>& entries, const std::string& name, std::int16_t language);
std::uint32_t base_pitch(std::uint32_t rate);
double stream_duration(std::size_t bytes, bool stereo, double rate=24000.0);
struct Vec3 { double x=0,y=0,z=0; };
struct Gains { double left=0,right=0; };
Gains pan_atten(Vec3 position, Vec3 camera, Vec3 right, double near=360.0, double far=840.0);

// Local presentation state only. One tick is one displayed 60 Hz sound frame.
class Playback {
public:
    static constexpr std::size_t effect_voice_limit=43, sequence_limit=32, stream_limit=3, stream_queue_limit=8;
    struct Voice { bool active=false, looping=false; std::string name; std::size_t cursor=0; double phase=0, step=1; Vec3 position{}; double volume=1, near=360, far=840; Gains gains{}; int group=0, sequence_slot=-1; };
    struct Stream { std::string name; bool active=false, held=false; std::uint16_t volume=0; std::uint16_t target=0; std::uint32_t flags=0; std::size_t cursor=0; double phase=0; };
    struct Queued { std::string name; std::uint32_t flags=0; };
    struct Sequence { bool active=false, repeat=false; int group=0, voice=-1; std::uint8_t generation=0; };
    void add_effect(std::string name, Pcm pcm);
    void add_stream(std::string name, Pcm pcm);
    bool play_effect(const std::string& name, Vec3 position, double volume=1, int group=0, bool looping=false);
    bool play_sequence(const std::string& name, Vec3 position, int group, bool looping=false);
    int start_sequence(const std::string& name, Vec3 position, int group);
    bool stop_sequence(int handle);
    bool release_sequence(int handle);
    bool queue_stream(std::size_t stream, std::string name, std::uint32_t flags);
    void release_stream(std::size_t stream);
    void stop_stream(std::size_t stream);
    void set_camera(Vec3 position, Vec3 right);
    void set_voice_position(std::size_t index, Vec3 position);
    void tick();
    std::vector<std::array<float,2>> render(std::size_t frames);
    const std::array<Voice,effect_voice_limit>& voices() const { return voices_; }
    const std::array<Stream,stream_limit>& streams() const { return streams_; }
    const std::array<Sequence,sequence_limit>& sequences() const { return sequences_; }
    std::size_t queued_count(std::size_t stream) const;
    std::size_t active_group(int group) const;
    static int group_limit(int group);
    // Caller supplies directory order, and cosmetic RNG's 0..7199 draw.
    bool poll_level(const std::vector<VatEntry>& entries, std::uint32_t cosmetic_draw);
    bool play_menu_music(bool wait_for_current);
    bool play_boss_music(const std::string& creature_type, const std::string& object_name={});
    void stop_boss_music();
    // Event names are integration hook points; sample names are supplied by caller.
    bool animation_event(std::uint32_t mask, const std::string& sample_name, Vec3 position);
private:
    std::map<std::string,Pcm> effects_, stream_pcm_;
    std::array<Voice,effect_voice_limit> voices_{};
    std::array<Stream,stream_limit> streams_{};
    std::array<Sequence,sequence_limit> sequences_{};
    std::array<std::vector<Queued>,stream_limit> queues_{};
    Vec3 camera_{}, right_{1,0,0};
    std::size_t next_voice_=0;
    std::uint32_t sound_frames_=0, level_poll_remaining_=14400;
    std::size_t fade_sample_remainder_=0;
    void service_queue(std::size_t index);
};
}
