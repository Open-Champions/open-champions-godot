#include "champions/native_audio.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <utility>

// Original implementation of the t3/research-audio contracts (formats/audio.md,
// audio-playback.md, tools/ref_audio.py, tools/ref_audio_playback.py).
// No game bytes or decompiler output.
namespace champions::audio {
namespace {
void need(Bytes b, std::size_t off, std::size_t len) {
  if (off > b.size || len > b.size - off || (len && !b.data))
    throw std::invalid_argument("truncated audio data");
}
std::uint32_t be32(Bytes b, std::size_t off) {
  need(b, off, 4);
  return (std::uint32_t(b.data[off]) << 24) | (std::uint32_t(b.data[off + 1]) << 16) |
         (std::uint32_t(b.data[off + 2]) << 8) | b.data[off + 3];
}
std::uint32_t le32(Bytes b, std::size_t off) {
  need(b, off, 4);
  return std::uint32_t(b.data[off]) | (std::uint32_t(b.data[off + 1]) << 8) |
         (std::uint32_t(b.data[off + 2]) << 16) | (std::uint32_t(b.data[off + 3]) << 24);
}
std::uint16_t le16(Bytes b, std::size_t off) {
  need(b, off, 2);
  return std::uint16_t(b.data[off] | (b.data[off + 1] << 8));
}
std::string ascii_name(Bytes b, std::size_t off, std::size_t len) {
  need(b, off, len);
  const auto *p = b.data + off;
  const auto *end = std::find(p, p + len, 0);
  for (auto q = p; q != end; ++q)
    if (*q > 127)
      throw std::invalid_argument("non-ASCII audio name");
  return std::string(p, end);
}
std::string lower(std::string s) {
  for (char &c : s)
    if (c >= 'A' && c <= 'Z')
      c = char(c - 'A' + 'a');
  return s;
}
// Arithmetic right shift (floor division by 2^shift), defined for negatives.
int floor_shift(int value, int shift) {
  const int divisor = 1 << shift;
  return value >= 0 ? value / divisor : -((-value + divisor - 1) / divisor);
}
constexpr int filter_c1[] = {0, 60, 115, 98, 122};
constexpr int filter_c2[] = {0, 0, -52, -55, -60};
void check_stream(std::size_t stream) {
  if (stream >= Playback::stream_limit)
    throw std::out_of_range("stream index");
}
int bit_length(std::uint64_t mag) {
  int count = 0;
  while (mag) {
    mag >>= 1;
    ++count;
  }
  return count;
}
// Floor division by 2^shift. A negative value uses the same direction as the
// R5900 description in tools/ee_emu.py (bits dropped toward minus infinity).
std::int64_t floor_shr(std::int64_t value, unsigned shift) {
  if (!shift)
    return value;
  if (value >= 0)
    return value >> shift;
  const auto mag = std::uint64_t(-value);
  auto quot = mag >> shift;
  if (mag & ((std::uint64_t{1} << shift) - 1))
    ++quot;
  return -std::int64_t(quot);
}
struct Unpacked {
  std::uint32_t raw, sign, man, exp;
};
Unpacked unpack(std::uint32_t raw) {
  const auto exp = (raw >> 23) & 0xff;
  if (!exp)
    return {raw, raw >> 31, 0, 0};
  return {raw, raw >> 31, (raw & 0x7fffffu) | 0x800000u, exp};
}
std::uint32_t pack_trunc(std::uint32_t sign, std::uint64_t mag, int e2) {
  if (!mag)
    return sign << 31;
  const int shift = bit_length(mag) - 24;
  if (shift > 0)
    mag >>= unsigned(shift);
  else
    mag <<= unsigned(-shift);
  const int exp = e2 + shift + 150;
  if (exp > 255)
    return (sign << 31) | 0x7fffffffu;
  if (exp < 1)
    return sign << 31;
  return (sign << 31) | (std::uint32_t(exp) << 23) | (std::uint32_t(mag) & 0x7fffffu);
}
std::size_t samples_at(std::uint32_t bytes, bool stereo) {
  if (bytes % 16)
    return std::size_t(-1);
  auto samples = std::size_t(bytes / 16) * 28;
  if (stereo)
    samples /= 2;
  return samples;
}
} // namespace

std::uint32_t ps2_add(std::uint32_t a, std::uint32_t b) {
  auto ua = unpack(a), ub = unpack(b);
  if (!ua.man && !ub.man)
    return (ua.sign & ub.sign) << 31;
  if (!ua.man)
    return b;
  if (!ub.man)
    return a;
  if (ua.exp < ub.exp)
    std::swap(ua, ub);
  const int diff = int(ua.exp) - int(ub.exp);
  if (diff >= 25)
    return ua.raw;
  const std::int64_t big = ua.sign ? -std::int64_t(ua.man) : std::int64_t(ua.man);
  const std::int64_t small = ub.sign ? -std::int64_t(ub.man) : std::int64_t(ub.man);
  const std::int64_t man = big * 64 + floor_shr(small * 64, unsigned(diff));
  if (!man)
    return 0;
  auto mag = std::uint64_t(man < 0 ? -man : man);
  int exp = int(ua.exp) - 6;
  const int shift = bit_length(mag) - 24;
  if (shift > 0)
    mag >>= unsigned(shift);
  else
    mag <<= unsigned(-shift);
  exp += shift;
  const auto sign = man < 0 ? 1u : 0u;
  if (exp > 255)
    return (sign << 31) | 0x7fffffffu;
  if (exp < 1)
    return sign << 31;
  return (sign << 31) | (std::uint32_t(exp) << 23) | (std::uint32_t(mag) & 0x7fffffu);
}
std::uint32_t ps2_sub(std::uint32_t a, std::uint32_t b) { return ps2_add(a, b ^ 0x80000000u); }
std::uint32_t ps2_mul(std::uint32_t a, std::uint32_t b) {
  const auto ua = unpack(a), ub = unpack(b);
  const auto sign = ua.sign ^ ub.sign;
  if (!ua.man || !ub.man)
    return sign << 31;
  return pack_trunc(sign, std::uint64_t(ua.man) * ub.man, int(ua.exp) + int(ub.exp) - 300);
}
int ps2_cmp(std::uint32_t a, std::uint32_t b) {
  const auto order = [](std::uint32_t w) -> std::int64_t {
    return (w & 0x80000000u) ? -std::int64_t(w & 0x7fffffffu) : std::int64_t(w & 0x7fffffffu);
  };
  const auto x = order(a), y = order(b);
  return (x > y) - (x < y);
}

std::uint32_t loop_restart_span(std::uint32_t value) {
  return (value & 0x7ffffc00u) << 1;
}
StreamCursor after_completed_read(bool named, std::uint32_t flags, std::uint32_t position,
                                  std::uint32_t remaining, std::uint32_t offset,
                                  std::uint32_t length, std::uint32_t restart,
                                  std::uint32_t nbytes) {
  position += nbytes;
  remaining -= nbytes;
  if (named && !remaining && (flags & stream_flag::loop)) {
    const auto span = loop_restart_span(restart);
    position = offset + span;
    remaining = length - span;
  }
  return {position, remaining};
}
bool music_gate_open(std::uint32_t volume_bits) {
  return ps2_cmp(volume_bits, music_volume_gate) >= 0;
}
std::uint32_t ducked_channel(std::uint32_t current, std::uint32_t music_volume) {
  return ps2_mul(ps2_mul(current, music_volume), music_volume_scale);
}
std::uint32_t set_duck_target(std::uint32_t argument) { return ps2_sub(float_one, argument); }
DuckStep duck_step(std::uint32_t current, std::uint32_t target, std::uint32_t voice_status,
                   std::uint32_t stream2_status, bool actintro_other_language,
                   std::uint32_t music_volume) {
  const auto saved = current;
  if (voice_status & stream_flag::suppress)
    target = duck_voice_level;
  target = ps2_add(target, duck_creep);
  if (ps2_cmp(float_one, target) < 0)
    target = float_one;
  if (ps2_cmp(target, saved) < 0) {
    const auto stepped = ps2_sub(saved, duck_ease);
    current = ps2_cmp(stepped, target) < 0 ? target : stepped;
  } else if (ps2_cmp(saved, target) < 0) {
    const auto stepped = ps2_add(saved, duck_ease);
    current = ps2_cmp(target, stepped) < 0 ? target : stepped;
  } else {
    current = saved;
  }
  if (actintro_other_language)
    current = 0;
  DuckStep step;
  step.current = current;
  step.target = target;
  step.sent = ps2_cmp(saved, current) != 0 && (stream2_status & stream_flag::suppress);
  step.channel = step.sent ? ducked_channel(current, music_volume) : 0;
  return step;
}
std::uint32_t stream_status_bits(std::uint32_t write_pos, std::uint32_t read_pos,
                                 std::uint32_t remaining, std::uint32_t active_flags,
                                 std::uint32_t queued_count, std::uint32_t first_flags) {
  const bool playing = write_pos != read_pos || (remaining > 0 && remaining <= 0x7fffffffu);
  if (playing)
    return active_flags | 1u;
  if (queued_count > 0) {
    if (first_flags & stream_flag::hold)
      return 0;
    return first_flags | 1u;
  }
  return 0;
}
bool recall_stops_music(std::uint32_t status) { return (status & stream_flag::recall) != 0; }
StreamDecision cut_run_stream2(bool gate, std::uint32_t status) {
  if (gate && (status & stream_flag::ambience) && !(status & stream_flag::cutscene_keep))
    return StreamDecision::stop;
  return StreamDecision::leave;
}
StreamDecision cut_end_stream2(bool gate, std::uint32_t status) {
  if (!gate || (status & stream_flag::cutscene_keep))
    return StreamDecision::leave;
  if (status & stream_flag::ambience)
    return StreamDecision::stop;
  return StreamDecision::clear_queue;
}
SoundCadence sound_cadence(std::uint32_t frame) {
  return {(frame & 3u) == 0, (frame & 7u) == 0, ((frame + 1u) & 0xfu) == 0, frame + 1u};
}
bool draw_cadence(std::uint32_t frame) { return (frame & 0xfu) == 0; }
BossAction boss_action(bool has_name, std::uint32_t volume_bits, std::uint32_t length) {
  if (!has_name)
    return BossAction::noop;
  if (!music_gate_open(volume_bits) || length == 0)
    return BossAction::silence;
  return BossAction::replace;
}

VagHeader parse_vag_header(Bytes b) {
  need(b, 0, 0x30);
  VagHeader h;
  std::copy_n(b.data, 4, h.magic.begin());
  h.version = be32(b, 4);
  h.data_size = be32(b, 0x0c);
  h.rate = be32(b, 0x10);
  h.name = ascii_name(b, 0x20, 16);
  if (h.data_size % 16 || !h.rate)
    throw std::invalid_argument("invalid VAG data size or rate");
  return h;
}

std::uint32_t base_pitch(std::uint32_t rate) {
  return std::uint32_t(std::uint64_t(rate) * 4096 / spu_rate);
}

Pcm decode_adpcm(Bytes b, bool honor_flags) {
  if (b.size % 16)
    throw std::invalid_argument("ADPCM body is not block aligned");
  need(b, 0, b.size);
  Pcm out;
  int prev = 0, older = 0;
  for (std::size_t off = 0; off < b.size; off += 16) {
    const int filter = b.data[off] >> 4, shift = b.data[off] & 15;
    const int flags = b.data[off + 1];
    if (filter > 4 || shift > 12 || (honor_flags && flags > 7))
      throw std::invalid_argument("invalid ADPCM block header");
    if (honor_flags && flags == 7)
      break; // terminal marker, never heard
    if (honor_flags && (flags & 4))
      out.loop_start = out.left.size();
    for (std::size_t j = 2; j < 16; ++j)
      for (int high = 0; high < 2; ++high) {
        const int nibble = high ? b.data[off + j] >> 4 : b.data[off + j] & 15;
        const int signed_nibble = nibble < 8 ? nibble : nibble - 16;
        const int scaled = floor_shift(signed_nibble * 4096, shift);
        const int predicted = floor_shift(
            filter_c1[filter] * prev + filter_c2[filter] * older + 32, 6);
        const int sample = std::clamp(scaled + predicted, -32768, 32767);
        out.left.push_back(std::int16_t(sample));
        older = prev;
        prev = sample;
      }
    if (honor_flags && (flags & 1)) {
      if (flags & 2)
        out.loop_end = out.left.size();
      break;
    }
  }
  return out;
}

Pcm decode_vag(Bytes b) {
  const auto h = parse_vag_header(b);
  need(b, 0x30, h.data_size);
  auto out = decode_adpcm({b.data + 0x30, h.data_size});
  out.rate = h.rate;
  return out;
}

std::vector<VatEntry> parse_vat_directory(Bytes dir, std::size_t vat_size) {
  if (!dir.size || dir.size % 0x50)
    throw std::invalid_argument("VAT directory is not record aligned");
  std::vector<VatEntry> entries;
  for (std::size_t off = 0; off < dir.size; off += 0x50) {
    auto name = ascii_name(dir, off, 0x40);
    const auto start = le32(dir, off + 0x40);
    if (name.empty()) {
      if (start != vat_size)
        throw std::invalid_argument("VAT terminal offset differs from VAT size");
      return entries;
    }
    if (off + 0xa0 > dir.size)
      throw std::invalid_argument("VAT terminal record missing");
    const auto end = le32(dir, off + 0x50 + 0x40);
    const auto channels = le16(dir, off + 0x48);
    if (start % 0x800 || end % 0x800 || start > end || end > vat_size)
      throw std::invalid_argument("VAT offsets are unaligned or out of order");
    if (channels > 1)
      throw std::invalid_argument("unsupported VAT channel flag");
    VatEntry e;
    e.name = std::move(name);
    e.offset = start;
    e.length = end - start;
    e.declared_rate = le32(dir, off + 0x44);
    e.stereo = channels == 1;
    e.language = std::int16_t(le16(dir, off + 0x4a));
    e.loop_restart = le32(dir, off + 0x4c);
    entries.push_back(std::move(e));
  }
  throw std::invalid_argument("VAT terminal record missing");
}

std::optional<VatEntry> find_vat_entry(const std::vector<VatEntry> &entries,
                                       const std::string &name, std::int16_t language) {
  std::optional<VatEntry> current, fallback;
  for (const auto &e : entries) {
    if (e.name != name)
      continue;
    if (e.language == language)
      current = e;
    else if (e.language == 0)
      fallback = e;
  }
  return current ? current : fallback;
}

Pcm decode_vat_entry(Bytes vat, const VatEntry &e) {
  need(vat, e.offset, e.length);
  if (e.length % 16)
    throw std::invalid_argument("VAT entry is not block aligned");
  // Loop restart is a playback cursor, applied when a looping read hits
  // remaining 0. It does not change the decoded samples.
  // The driver overwrites block flags and plays the whole directory length at
  // the fixed stream pitch; the declared rate is informational.
  if (!e.stereo) {
    auto out = decode_adpcm({vat.data + e.offset, e.length}, false);
    out.rate = e.declared_rate;
    return out;
  }
  if (e.length % 0x800)
    throw std::invalid_argument("VAT stereo entry is not whole 0x400 pairs");
  std::vector<std::uint8_t> left, right;
  left.reserve(e.length / 2);
  right.reserve(e.length / 2);
  for (std::size_t off = e.offset; off < std::size_t(e.offset) + e.length; off += 0x800) {
    left.insert(left.end(), vat.data + off, vat.data + off + 0x400);
    right.insert(right.end(), vat.data + off + 0x400, vat.data + off + 0x800);
  }
  auto out = decode_adpcm({left.data(), left.size()}, false);
  out.right = decode_adpcm({right.data(), right.size()}, false).left;
  out.rate = e.declared_rate;
  return out;
}

double stream_duration(std::size_t bytes, bool stereo, double rate) {
  if (bytes % 16 || !std::isfinite(rate) || rate <= 0)
    throw std::invalid_argument("invalid stream byte count or rate");
  return double(bytes / 16) * 28.0 / rate / (stereo ? 2 : 1);
}

Gains pan_atten(Vec3 p, Vec3 c, Vec3 r, double near, double far) {
  if (!std::isfinite(near) || !std::isfinite(far) || near < 0 || far <= near)
    throw std::invalid_argument("expected 0 <= near < far");
  const double dx = p.x - c.x, dy = p.y - c.y, dz = p.z - c.z;
  const double distance = std::sqrt(dx * dx + dy * dy + dz * dz);
  const double atten = std::clamp((far - distance) / (far - near), 0.0, 1.0);
  const double pan = std::clamp((dx * r.x + dy * r.y + dz * r.z) / far, -1.0, 1.0);
  return {atten * (1.0 - pan), atten * (1.0 + pan)};
}

GameplayCue classify_animation_event(std::uint32_t mask, ClipRole role) {
  if (role == ClipRole::attack && (mask & 0x1))
    return GameplayCue::hit;
  if (role == ClipRole::locomotion && (mask & (0x4 | 0x8)))
    return GameplayCue::footstep;
  return GameplayCue::none;
}

// --- registration -----------------------------------------------------------

// mix() moves a voice back by one loop (or one sample) length after a pitch step
// of at most 0x3fff (< 4 samples). Shorter loops or samples would leave the phase
// outside the data; VAG loops are whole 28-sample blocks, so such PCM is refused
// here rather than wrapped in an undocumented way.
constexpr std::size_t MIN_STEP_SAMPLES = (0x3fff >> 12) + 1;
bool Playback::add_effect(const std::string &name, Pcm pcm) {
  if (!pcm.rate || pcm.left.empty() || pcm.stereo())
    throw std::invalid_argument("effect sample must be mono PCM with a rate");
  if (pcm.left.size() < MIN_STEP_SAMPLES)
    throw std::invalid_argument("effect sample shorter than the largest pitch step");
  if (pcm.loop_start || pcm.loop_end) {
    if (!pcm.loop_start || !pcm.loop_end || *pcm.loop_end > pcm.left.size() ||
        *pcm.loop_start >= *pcm.loop_end)
      throw std::invalid_argument("effect loop outside the sample");
    if (*pcm.loop_end - *pcm.loop_start < MIN_STEP_SAMPLES)
      throw std::invalid_argument("effect loop shorter than the largest pitch step");
  }
  return effects_.emplace(lower(name), std::move(pcm)).second;
}

void Playback::add_stream(const std::string &name, Pcm pcm) {
  if (name.empty() || pcm.left.empty() ||
      (pcm.stereo() && pcm.left.size() != pcm.right.size()))
    throw std::invalid_argument("invalid stream PCM");
  registered_[name] = std::move(pcm);
}

void Playback::set_global_vat(std::vector<std::uint8_t> vat,
                              std::vector<VatEntry> entries, std::int16_t language) {
  global_ = {std::move(vat), std::move(entries), language};
}

void Playback::set_level_vat(std::vector<std::uint8_t> vat, std::vector<VatEntry> entries,
                             std::int16_t language, std::uint32_t initial_countdown) {
  // The process image of the countdown is 0x2a30. A level load's store of that
  // word was not re-executed, so installing a level still takes the caller's
  // countdown. The language byte is the same word VAT lookup prefers.
  if (!initial_countdown)
    throw std::invalid_argument("level poll countdown must start above zero");
  level_ = {std::move(vat), std::move(entries), language};
  level_active_ = true;
  language_ = language;
  countdown_ = initial_countdown;
}

const Pcm *Playback::effect(const std::string &name) const {
  const auto it = effects_.find(lower(name));
  return it == effects_.end() ? nullptr : &it->second;
}

bool Playback::stream_exists(const std::string &name, StreamSource source) const {
  if (source == StreamSource::registered)
    return registered_.count(name) != 0;
  const auto &vat = source == StreamSource::level ? level_ : global_;
  return find_vat_entry(vat.entries, name, vat.language).has_value();
}

// An entry is checked when it is queued; its directory may be replaced before the
// stream service reaches it. A missing entry is invalid media (dropped by the
// caller as a failed entry), never a dereference of nothing.
Pcm Playback::load_stream(const QueueEntry &q) const {
  if (!q.from_vat) {
    const auto it = registered_.find(q.name);
    if (it == registered_.end())
      throw std::invalid_argument("queued stream is no longer registered");
    return it->second;
  }
  const auto &vat = q.source == StreamSource::level ? level_ : global_;
  const auto found = find_vat_entry(vat.entries, q.name, vat.language);
  if (!found)
    throw std::invalid_argument("queued stream is not in the current VAT directory");
  VatEntry entry;
  entry.name = q.name;
  entry.offset = q.offset;
  entry.length = q.length;
  entry.loop_restart = q.loop_restart;
  entry.stereo = q.stereo;
  entry.declared_rate = found->declared_rate;
  return decode_vat_entry({vat.bytes.data(), vat.bytes.size()}, entry);
}

// --- effects ----------------------------------------------------------------

int Playback::group_limit(int group) {
  if (group < 0 || group > 98)
    throw std::invalid_argument("sound group outside documented RTA range 0..98");
  if (group == 0)
    return int(sequence_limit); // the category check is bypassed
  if (group == 14)
    return 9;
  if (group == 50 || group == 51)
    return 8;
  return 2;
}

std::size_t Playback::active_group(int group) const {
  return std::size_t(std::count_if(sequences_.begin(), sequences_.end(),
                                   [&](const Sequence &s) {
                                     return s.active && s.group == group;
                                   }));
}

void Playback::refresh_gains(Voice &v) {
  v.gains = v.position ? pan_atten(*v.position, camera_, right_, v.near, v.far)
                       : Gains{1, 1};
}

void Playback::set_sfx_volume(double volume) {
  if (!std::isfinite(volume) || volume < 0)
    throw std::invalid_argument("invalid effects volume");
  sfx_volume_ = volume;
}

int Playback::play_sample(const std::string &name, double pitch_scale, double volume,
                          std::optional<Vec3> position, bool plays_while_frozen) {
  if (!std::isfinite(pitch_scale) || pitch_scale < 0 || !std::isfinite(volume) ||
      volume < 0)
    throw std::invalid_argument("invalid pitch scale or volume");
  const Pcm *pcm = effect(name);
  if (!pcm)
    return -1; // unresolved names are silent
  Voice candidate;
  candidate.sample = lower(name);
  candidate.pcm = pcm;
  candidate.position = position;
  candidate.volume = volume;
  candidate.plays_while_frozen = plays_while_frozen;
  refresh_gains(candidate);
  // A one-shot whose scaled sides are both zero is not started.
  if (!pcm->loops() && candidate.gains.left * volume == 0 &&
      candidate.gains.right * volume == 0)
    return -1;
  const double pitch = double(base_pitch(pcm->rate)) * pitch_scale;
  candidate.pitch = std::uint32_t(std::min(pitch, double(0x3fff)));
  candidate.active = true;
  // voice_alloc 0x2169f8: round robin among free voices; no stealing.
  for (std::size_t probe = 0; probe < effect_voice_limit; ++probe) {
    const auto i = (next_voice_ + probe) % effect_voice_limit;
    if (voices_[i].active)
      continue;
    voices_[i] = std::move(candidate);
    next_voice_ = (i + 1) % effect_voice_limit;
    return int(i);
  }
  return -1;
}

int Playback::sequence_slot(int handle) const {
  if (handle < 0 || !(handle & 0x8000))
    return -1;
  const auto slot = std::size_t(handle & 0xff);
  if (slot >= sequence_limit)
    return -1;
  const auto &s = sequences_[slot];
  if (!s.active || s.generation != std::uint8_t((handle >> 8) & 0x7f))
    return -1;
  return int(slot);
}

int Playback::start_sequence(const std::string &name, double volume, Vec3 position,
                             int group, bool plays_while_frozen) {
  if (group && active_group(group) >= std::size_t(group_limit(group)))
    return -1;
  std::size_t slot = sequence_limit;
  for (std::size_t i = 0; i < sequence_limit; ++i)
    if (!sequences_[i].active) {
      slot = i;
      break;
    }
  if (slot == sequence_limit)
    return -1;
  const int voice = play_sample(name, 1.0, volume, position, plays_while_frozen);
  if (voice < 0)
    return -1;
  auto &s = sequences_[slot];
  s.active = true;
  s.repeat = false;
  s.group = group;
  s.voice = voice;
  s.generation = std::uint8_t((s.generation + 1) & 0x7f);
  voices_[std::size_t(voice)].sequence = int(slot);
  return 0x8000 | (int(s.generation) << 8) | int(slot);
}

bool Playback::stop_sequence(int handle) {
  const int slot = sequence_slot(handle);
  if (slot < 0)
    return false;
  auto &s = sequences_[std::size_t(slot)];
  if (s.voice >= 0)
    voices_[std::size_t(s.voice)] = Voice{};
  s.active = false;
  s.voice = -1;
  return true;
}

bool Playback::release_sequence(int handle) {
  const int slot = sequence_slot(handle);
  if (slot < 0)
    return false;
  sequences_[std::size_t(slot)].repeat = true; // repeats until stopped
  return true;
}

bool Playback::set_sequence_position(int handle, Vec3 position) {
  const int slot = sequence_slot(handle);
  if (slot < 0)
    return false;
  auto &v = voices_[std::size_t(sequences_[std::size_t(slot)].voice)];
  v.position = position;
  refresh_gains(v);
  return true;
}

int Playback::animation_event(std::uint32_t mask, ClipRole role, const std::string &sample,
                              Vec3 position) {
  if (classify_animation_event(mask, role) == GameplayCue::none)
    return -1;
  return play_sample(sample, 1.0, 1.0, position);
}

void Playback::set_camera(Vec3 position, Vec3 right) {
  camera_ = position;
  right_ = right;
  for (auto &v : voices_)
    if (v.active)
      refresh_gains(v);
}

// --- streams ----------------------------------------------------------------

bool Playback::queue_stream(std::size_t stream, const std::string &name,
                            std::uint32_t flags, StreamSource source) {
  check_stream(stream);
  if (flags & ~stream_flag::known)
    throw std::invalid_argument("stream queue flags outside the documented set");
  if (name.empty() && flags != stream_flag::fade_current)
    throw std::invalid_argument("an unnamed queue entry must be the flag 2 stop marker");
  if (!name.empty() && !stream_exists(name, source))
    return false;
  if (queues_[stream].size() >= stream_queue_limit)
    return false;
  QueueEntry entry{name, flags, source};
  if (source != StreamSource::registered) {
    const auto &vat = source == StreamSource::level ? level_ : global_;
    const auto found = find_vat_entry(vat.entries, name, vat.language);
    if (found) {
      entry.offset = found->offset;
      entry.length = found->length;
      entry.loop_restart = found->loop_restart;
      entry.stereo = found->stereo;
      entry.from_vat = true;
    }
  }
  queues_[stream].push_back(std::move(entry));
  return true;
}

bool Playback::queue_stop(std::size_t stream) {
  return queue_stream(stream, {}, stream_flag::fade_current);
}

void Playback::release_stream(std::size_t stream) {
  check_stream(stream);
  auto &s = streams_[stream];
  if (s.active && s.held) {
    s.held = false;
    return;
  }
  for (auto &q : queues_[stream])
    if (q.flags & stream_flag::hold) {
      q.flags &= ~stream_flag::hold;
      return;
    }
}

void Playback::set_stream_scale(std::size_t stream, std::uint16_t scale) {
  check_stream(stream);
  streams_[stream].scale = scale;
}

std::size_t Playback::queued_count(std::size_t stream) const {
  check_stream(stream);
  return queues_[stream].size();
}

std::uint64_t Playback::stream_samples_played(std::size_t stream) const {
  check_stream(stream);
  const auto &s = streams_[stream];
  return s.active ? s.phase >> 12 : 0;
}

double Playback::stream_time(std::size_t stream) const {
  return double(stream_samples_played(stream)) / game_stream_rate;
}

void Playback::promote(std::size_t i) {
  auto &s = streams_[i];
  auto &q = queues_[i];
  const auto front = q.front();
  q.pop_front();
  if (front.name.empty())
    return;
  try {
    s.pcm = load_stream(front);
  } catch (const std::invalid_argument &) {
    s.pcm = Pcm{};
  }
  if (s.pcm.left.empty() || (s.pcm.stereo() && s.pcm.right.size() != s.pcm.left.size())) {
    ++failed_stream_entries_;
    s.pcm = Pcm{};
    s.active = false;
    s.audible = false;
    s.bytes_remaining = 0;
    return;
  }
  s.name = front.name;
  s.flags = front.flags;
  s.held = false;
  s.phase = 0;
  s.active = true;
  s.audible = true;
  s.from_vat = front.from_vat;
  if (front.from_vat) {
    s.file_offset = front.offset;
    s.file_length = front.length;
    s.file_position = front.offset;
    s.bytes_remaining = front.length;
    s.loop_restart = front.loop_restart;
  } else {
    s.file_offset = s.file_length = s.file_position = s.loop_restart = 0;
    s.bytes_remaining = 1;
  }
}

// stream_serviceQueues / stream_startQueued. A held entry stays queued.
// A fade bit waits until the status volume word is 0, then command 0x60 and a
// full-volume 0x70 promote it. Any other entry waits while one is active.
void Playback::service_streams() {
  for (std::size_t i = 0; i < stream_limit; ++i) {
    auto &s = streams_[i];
    auto &q = queues_[i];
    if (q.empty())
      continue;
    const auto &front = q.front();
    if (front.flags & stream_flag::hold)
      continue;
    if ((front.flags & stream_flag::fade_current) && s.volume) {
      s.target = 0;
      continue;
    }
    if (s.active && !(front.flags & stream_flag::fade_current))
      continue;
    if (front.flags & stream_flag::fade_current) {
      s.active = false;
      s.held = false;
      s.audible = false;
      s.pcm = Pcm{};
      s.name.clear();
      s.bytes_remaining = 0;
      s.target = full_volume;
    }
    promote(i);
  }
}

// Stream driver timer: first 10 ms after init, then every 6 ms.
void Playback::step_stream_volumes() {
  for (std::size_t i = 0; i < stream_limit; ++i) {
    auto &s = streams_[i];
    if (s.volume > s.target)
      s.volume = std::uint16_t(std::max<int>(s.target, s.volume - 0x20));
    else if (s.volume < s.target)
      s.volume = std::uint16_t(std::min<int>(s.target, s.volume + (i == 0 ? 0x40 : 0xc0)));
  }
}

// --- music policy -----------------------------------------------------------

bool Playback::queue_music(const std::string &name, std::uint32_t flags, StreamSource source) {
  if (!music_gate_open(music_volume_))
    return false;
  const auto &vat = source == StreamSource::level ? level_ : global_;
  const auto entry = find_vat_entry(vat.entries, name, vat.language);
  if (!entry || entry->length == 0)
    return false;
  return queue_stream(2, name, flags | 0x22, source);
}

bool Playback::music_play_global(const std::string &name, std::uint32_t flags) {
  return queue_music(name, flags, StreamSource::global);
}

bool Playback::music_play_level(const std::string &name, std::uint32_t flags) {
  return queue_music(name, flags, StreamSource::level);
}

bool Playback::play_menu_music(bool wait_for_current) {
  // frontend_run's first request reaches music_playGlobal's volume gate
  // (research-menus-layout 8f528ce); keep its original queue flags.
  if (!music_gate_open(music_volume_))
    return false;
  return queue_stream(2, "mx_maintheme_l.vag", wait_for_current ? 5 : 1,
                      StreamSource::global);
}

std::optional<std::string> Playback::boss_music_name(const std::string &type,
                                                     const std::string &object) {
  // creature_bossMusicName 0x10dbe0: type compared without case. The object
  // name comparison is treated the same way.
  const auto t = lower(type), o = lower(object);
  const char *n = nullptr;
  if (t == "cazicavatar")
    n = "avataroffear";
  else if (t == "fatcyclops" && o == "broon")
    n = "broon";
  else if (t == "fatcyclops" && o == "droon")
    n = "droon";
  else if (t == "cyclops" && o == "proon")
    n = "proon";
  else if (t == "cazicthule")
    n = "cazicthule";
  else if (t == "reddragon")
    n = "dozekar";
  else if (t == "ironguardian")
    n = "ironguardian";
  else if (t == "mattmarr")
    n = "matanielmarr";
  else if (t == "raloszek")
    n = "ralloszek";
  else if (t == "vampirelord")
    n = "vanerhost";
  else if (t == "snowboss")
    n = "whitetormentor";
  if (!n)
    return std::nullopt;
  return std::string("mx_ba_") + n + "_l.vag";
}

bool Playback::play_boss_music(const std::string &type, const std::string &object) {
  const auto name = boss_music_name(type, object);
  if (!name)
    return false;
  // audio_streamStop clears the queue and the active name, then one new span
  // is queued. A second boss therefore replaces the first. The volume word is
  // left alone, so the fade bit inside 0x123 waits until that word reads 0.
  stream_stop(2);
  const auto entry = find_vat_entry(global_.entries, *name, global_.language);
  const auto length = entry ? entry->length : 0u;
  if (boss_action(true, music_volume_, length) != BossAction::replace)
    return false;
  return music_play_global(*name, 0x103);
}

bool Playback::stop_boss_music(const std::string &type, const std::string &object) {
  if (!boss_music_name(type, object))
    return false;
  return queue_stop(2);
}

std::uint32_t Playback::set_duck(std::uint32_t argument) {
  duck_target_ = set_duck_target(argument);
  return duck_target_;
}

DuckStep Playback::step_duck() {
  const auto step = duck_step(duck_current_, duck_target_, stream_status(1), stream_status(2),
                              level_name_ == "actintro" && language_ != 0,
                              music_volume_);
  duck_current_ = step.current;
  duck_target_ = step.target;
  duck_sent_ = step.sent;
  if (step.sent)
    music_channel_ = step.channel;
  return step;
}

void Playback::stream_stop(std::size_t stream) {
  check_stream(stream);
  queues_[stream].clear();
  auto &s = streams_[stream];
  s.active = false;
  s.held = false;
  s.audible = false;
  s.from_vat = false;
  s.pcm = Pcm{};
  s.name.clear();
  s.flags = 0;
  s.bytes_remaining = 0;
}

void Playback::clear_queue(std::size_t stream) {
  check_stream(stream);
  queues_[stream].clear();
}

bool Playback::apply_recall_stop() {
  if (!recall_stops_music(stream_status(2)))
    return false;
  return queue_stop(2);
}

StreamDecision Playback::cutscene_run(bool gate) {
  stream_stop(1);
  const auto decision = cut_run_stream2(gate, stream_status(2));
  if (decision == StreamDecision::stop)
    stream_stop(2);
  return decision;
}

StreamDecision Playback::cutscene_end(bool gate) {
  const auto decision = cut_end_stream2(gate, stream_status(2));
  if (decision == StreamDecision::stop)
    stream_stop(2);
  else if (decision == StreamDecision::clear_queue)
    clear_queue(2);
  return decision;
}

std::uint32_t Playback::stream_status(std::size_t stream) const {
  check_stream(stream);
  const auto &s = streams_[stream];
  const auto &q = queues_[stream];
  const bool playing = s.active && s.bytes_remaining > 0 && s.bytes_remaining <= 0x7fffffffu;
  if (playing)
    return s.flags | 1u;
  if (!q.empty()) {
    if (q.front().flags & stream_flag::hold)
      return 0;
    return q.front().flags | 1u;
  }
  return 0;
}

bool Playback::poll_level(std::uint32_t draw) {
  if (draw >= 7200)
    throw std::invalid_argument("cosmetic draw outside 0..7199");
  if (!level_active_)
    return false;
  bool queued = false;
  if (--countdown_ == 0) {
    countdown_ = 14400 + draw;
    std::uint32_t pending = streams_[2].active ? streams_[2].flags : 0;
    for (const auto &q : queues_[2])
      pending |= q.flags;
    if (!(pending & stream_flag::suppress))
      for (const auto &e : level_.entries)
        if (e.name.find("_env_") != std::string::npos ||
            e.name.find("mx_theme") != std::string::npos) {
          queued = music_play_level(e.name, 0x102) || queued;
          break;
        }
  }
  if (draw_cadence(draw_frame_) && !streams_[0].active && queues_[0].empty())
    for (const auto &e : level_.entries)
      if (e.name.find("amb_") != std::string::npos) {
        queued = queue_stream(0, e.name, 3, StreamSource::level) || queued;
        break;
      }
  return queued;
}

// --- tick and mix -----------------------------------------------------------

void Playback::mix(std::array<float, 2> &out) {
  double left = 0, right = 0;
  for (auto &v : voices_) {
    if (!v.active || v.ended)
      continue;
    const Pcm &p = *v.pcm;
    const double sample = p.left[std::size_t(v.phase >> 12)] / 32768.0;
    const double gain = v.volume * sfx_volume_;
    left += sample * gain * v.gains.left;
    right += sample * gain * v.gains.right;
    // Freeze zeros the pitch of voices that do not play while frozen.
    if (frozen_ && !v.plays_while_frozen)
      continue;
    v.phase += v.pitch;
    const auto index = v.phase >> 12;
    if (p.loops() && index >= *p.loop_end)
      v.phase -= std::uint64_t(*p.loop_end - *p.loop_start) << 12;
    else if (index >= p.left.size()) {
      const bool repeat = v.sequence >= 0 && sequences_[std::size_t(v.sequence)].repeat;
      if (repeat)
        v.phase -= std::uint64_t(p.left.size()) << 12;
      else
        v.ended = true; // end flag; the voice is reclaimed on an 8th frame
    }
  }
  for (std::size_t i = 0; i < stream_limit; ++i) {
    auto &s = streams_[i];
    if (!s.active || s.held)
      continue;
    const auto index = std::size_t(s.phase >> 12);
    if (!s.audible || index >= s.pcm.left.size())
      continue;
    // The duck's audio_setChannelVolume(2, ...) word is recorded, not applied:
    // how the driver's volume RPC 0x80 maps it onto the output is not
    // documented (docs/audio-playback.md 222-224, docs/formats/anim-audio.md).
    const double gain = double((std::uint32_t(s.volume) * s.scale) >> 8) / full_volume;
    const double l = s.pcm.left[index] / 32768.0;
    const double r = s.pcm.stereo() ? s.pcm.right[index] / 32768.0 : l;
    if (!s.pcm.stereo() || two_channel_) {
      left += l * gain;
      right += r * gain;
    } else {
      left += (l + r) * gain;
      right += (l + r) * gain;
    }
    s.phase += stream_pitch;
    if ((s.phase >> 12) < s.pcm.left.size())
      continue;
    if (s.from_vat) {
      const auto next = after_completed_read(
          !s.name.empty(), s.flags, s.file_position, s.bytes_remaining, s.file_offset,
          s.file_length, s.loop_restart, s.bytes_remaining);
      s.file_position = next.position;
      s.bytes_remaining = next.remaining;
      if (!(s.flags & stream_flag::loop)) {
        s.active = false;
        s.audible = false;
        s.pcm = Pcm{};
        s.bytes_remaining = 0;
        continue;
      }
      const auto inside = s.file_position >= s.file_offset &&
                          s.file_length > s.file_position - s.file_offset;
      const auto at = inside ? samples_at(s.file_position - s.file_offset, s.pcm.stereo())
                             : std::size_t(-1);
      if (inside && at < s.pcm.left.size()) {
        s.phase = std::uint64_t(at) << 12;
        s.audible = true;
      } else {
        // The remaining word wrapped, or the span is outside the entry.
        // Keep the cursor and do not invent samples past the entry.
        s.audible = false;
        s.phase = 0;
      }
    } else if (s.flags & stream_flag::loop) {
      s.phase -= std::uint64_t(s.pcm.left.size()) << 12;
    } else {
      s.active = false;
      s.audible = false;
      s.pcm = Pcm{};
      s.bytes_remaining = 0;
    }
  }
  out = {float(std::clamp(left, -1.0, 1.0)), float(std::clamp(right, -1.0, 1.0))};
}

void Playback::reclaim_ended() {
  for (std::size_t i = 0; i < effect_voice_limit; ++i) {
    auto &v = voices_[i];
    if (!v.active || !v.ended)
      continue;
    if (v.sequence >= 0) {
      sequences_[std::size_t(v.sequence)].active = false;
      sequences_[std::size_t(v.sequence)].voice = -1;
    }
    v = Voice{};
  }
}

void Playback::tick() {
  // sound_update reads the counters before it increments g_soundFrame.
  // Queue service: low two bits clear. End flags: low three bits clear.
  // audio_sendVolumes runs after the increment, when the new value's low four
  // bits are clear. g_drawFrame is a separate counter. music_updateDucking is
  // not run here: one call is executed, but its call site, cadence and order
  // against the queue service are not documented (docs/audio-playback.md 210),
  // so the owner calls step_duck() explicitly.
  const auto entering = sound_frame_;
  if ((entering & 3u) == 0)
    service_streams();
  if ((entering & 7u) == 0)
    reclaim_ended();
  sound_frame_ = (sound_frame_ + 1u) & 0xffffffffu;
  if ((sound_frame_ & 0xfu) == 0)
    ++volume_sends_;
  for (std::size_t n = 0; n < frames_per_tick; ++n, ++output_samples_) {
    while (output_samples_ >= next_timer_) {
      step_stream_volumes();
      next_timer_ += 288; // 6 ms at 48 kHz
    }
    std::array<float, 2> frame{};
    mix(frame);
    fifo_.push_back(frame);
  }
  while (fifo_.size() > fifo_limit) {
    fifo_.pop_front();
    ++dropped_frames_;
  }
}

void Playback::end_session() {
  for (std::size_t i = 0; i < stream_limit; ++i)
    stream_stop(i);
  for (auto &v : voices_)
    v = Voice{};
  for (auto &s : sequences_) {
    s.active = false;
    s.voice = -1;
  }
  level_ = Vat{};
  level_active_ = false;
  fifo_.clear();
}

std::vector<std::array<float, 2>> Playback::drain(std::size_t max_frames) {
  const auto count = std::min(max_frames, fifo_.size());
  std::vector<std::array<float, 2>> out(fifo_.begin(), fifo_.begin() + std::ptrdiff_t(count));
  fifo_.erase(fifo_.begin(), fifo_.begin() + std::ptrdiff_t(count));
  return out;
}
} // namespace champions::audio
