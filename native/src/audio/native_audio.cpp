#include "champions/native_audio.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

// Original implementation of the t3/research-audio contracts (formats/audio.md,
// audio-playback.md, tools/ref_audio.py). No game bytes or decompiler output.
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
} // namespace

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
  // Every observed restart value is zero; a nonzero one has no validated meaning.
  if (e.loop_restart)
    throw std::invalid_argument("nonzero VAT loop restart is not validated");
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

bool Playback::add_effect(const std::string &name, Pcm pcm) {
  if (!pcm.rate || pcm.left.empty() || pcm.stereo())
    throw std::invalid_argument("effect sample must be mono PCM with a rate");
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
  // The countdown's value on level entry is not documented; the caller owns it.
  if (!initial_countdown)
    throw std::invalid_argument("level poll countdown must start above zero");
  level_ = {std::move(vat), std::move(entries), language};
  level_active_ = true;
  countdown_ = initial_countdown;
  level_frames_ = 0;
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
  if (q.source == StreamSource::registered) {
    const auto it = registered_.find(q.name);
    if (it == registered_.end())
      throw std::invalid_argument("queued stream is no longer registered");
    return it->second;
  }
  const auto &vat = q.source == StreamSource::level ? level_ : global_;
  const auto entry = find_vat_entry(vat.entries, q.name, vat.language);
  if (!entry)
    throw std::invalid_argument("queued stream is not in the current VAT directory");
  return decode_vat_entry({vat.bytes.data(), vat.bytes.size()}, *entry);
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
  constexpr std::uint32_t known = stream_flag::loop | stream_flag::fade_current |
                                  stream_flag::wait_for_end | stream_flag::hold |
                                  stream_flag::suppress | stream_flag::ambience |
                                  stream_flag::gated;
  if (flags & ~known)
    throw std::invalid_argument("stream queue flags outside the documented set");
  if (name.empty() && flags != stream_flag::fade_current)
    throw std::invalid_argument("an unnamed queue entry must be the flag 2 stop marker");
  if (!name.empty() && !stream_exists(name, source))
    return false;
  if (queues_[stream].size() >= stream_queue_limit)
    return false;
  if (flags & stream_flag::gated)
    ++gated_flag_entries_;
  queues_[stream].push_back({name, flags, source});
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

// stream_serviceQueues 0x211df8, every 4th sound frame, streams in order 0, 1, 2.
void Playback::service_streams() {
  for (std::size_t i = 0; i < stream_limit; ++i) {
    auto &s = streams_[i];
    auto &q = queues_[i];
    if (q.empty())
      continue;
    const auto &front = q.front();
    if (s.active) {
      // Flags 4 and 1 wait for the current entry. Without flag 2 or 4 the
      // contract does not say whether the current entry is cut; this model waits.
      if (!(front.flags & stream_flag::fade_current))
        continue;
      s.target = 0;
      if (s.volume)
        continue;
      // Command 0x60 then 0x70 with full volume.
      s.active = false;
      s.held = false;
      s.pcm = Pcm{};
      s.target = full_volume;
    }
    if (front.name.empty()) {
      q.pop_front();
      continue;
    }
    try {
      s.pcm = load_stream(front);
    } catch (const std::invalid_argument &) {
      s.pcm = Pcm{};
    }
    if (s.pcm.left.empty() || (s.pcm.stereo() && s.pcm.right.size() != s.pcm.left.size())) {
      ++failed_stream_entries_; // malformed media is dropped, never played
      q.pop_front();
      continue;
    }
    s.name = front.name;
    s.flags = front.flags;
    s.held = (front.flags & stream_flag::hold) != 0;
    s.phase = 0;
    s.active = true;
    q.pop_front();
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

bool Playback::music_play_global(const std::string &name, std::uint32_t flags) {
  return queue_stream(2, name, flags | 0x22, StreamSource::global);
}

// The caller applies music_playLevel's volume gate; its condition is not traced.
bool Playback::music_play_level(const std::string &name, std::uint32_t flags) {
  return queue_stream(2, name, flags | 0x22, StreamSource::level);
}

bool Playback::play_menu_music(bool wait_for_current) {
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
  // Gated: the ordering of simultaneous boss overrides is unknown, so a second
  // boss track is refused while one plays or waits.
  const auto is_boss = [](const std::string &n) { return n.rfind("mx_ba_", 0) == 0; };
  if (streams_[2].active && is_boss(streams_[2].name))
    return false;
  for (const auto &q : queues_[2])
    if (is_boss(q.name))
      return false;
  return music_play_global(*name, 0x103);
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
  if (++level_frames_ % 16 == 0 && !streams_[0].active && queues_[0].empty())
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
    if ((s.phase >> 12) >= s.pcm.left.size()) {
      if (s.flags & stream_flag::loop)
        s.phase -= std::uint64_t(s.pcm.left.size()) << 12;
      else {
        s.active = false;
        s.pcm = Pcm{};
      }
    }
  }
  out = {float(std::clamp(left, -1.0, 1.0)), float(std::clamp(right, -1.0, 1.0))};
}

void Playback::tick() {
  ++frame_;
  // sound_update 0x212670: end flags every 8th frame reclaim non-looping voices.
  if (frame_ % 8 == 0)
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
  if (frame_ % 4 == 0)
    service_streams();
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

std::vector<std::array<float, 2>> Playback::drain(std::size_t max_frames) {
  const auto count = std::min(max_frames, fifo_.size());
  std::vector<std::array<float, 2>> out(fifo_.begin(), fifo_.begin() + std::ptrdiff_t(count));
  fifo_.erase(fifo_.begin(), fifo_.begin() + std::ptrdiff_t(count));
  return out;
}
} // namespace champions::audio
