#include "champions/native_audio.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// Hand-built fixtures only: no disc files, game bytes or captured sound. The
// decoder and gain vectors were produced by t3/research-audio tools/ref_audio.py
// (834806e) from the synthetic bytes built below.
using namespace champions::audio;
namespace {
int checks = 0;
void check(bool ok, const char *msg) {
  ++checks;
  if (!ok)
    throw std::runtime_error(msg);
}
template <class F> bool throws(F &&f) {
  try {
    f();
  } catch (const std::invalid_argument &) {
    return true;
  } catch (const std::out_of_range &) {
    return true;
  }
  return false;
}
using Buffer = std::vector<std::uint8_t>;
void put_be(Buffer &b, std::size_t o, std::uint32_t v) {
  for (int i = 3; i >= 0; --i, v >>= 8)
    b[o + std::size_t(i)] = std::uint8_t(v);
}
void put_le(Buffer &b, std::size_t o, std::uint32_t v) {
  for (int i = 0; i < 4; ++i, v >>= 8)
    b[o + std::size_t(i)] = std::uint8_t(v);
}
void add_block(Buffer &b, int filter, int shift, int flags, std::vector<std::uint8_t> payload) {
  payload.resize(14);
  b.push_back(std::uint8_t((filter << 4) | shift));
  b.push_back(std::uint8_t(flags));
  b.insert(b.end(), payload.begin(), payload.end());
}
Bytes view(const Buffer &b) { return {b.data(), b.size()}; }
struct Record {
  const char *name;
  std::uint32_t offset;
  int stereo = 0, language = 0;
  std::uint32_t restart = 0;
};
Buffer directory(const std::vector<Record> &records) {
  Buffer d(records.size() * 0x50);
  for (std::size_t i = 0; i < records.size(); ++i) {
    const auto p = i * 0x50;
    std::memcpy(d.data() + p, records[i].name, std::strlen(records[i].name));
    put_le(d, p + 0x40, records[i].offset);
    put_le(d, p + 0x44, 24000);
    d[p + 0x48] = std::uint8_t(records[i].stereo);
    d[p + 0x4a] = std::uint8_t(records[i].language);
    put_le(d, p + 0x4c, records[i].restart);
  }
  return d;
}
Pcm effect_pcm(std::size_t n, std::uint32_t rate = 48000, bool looping = false) {
  Pcm p;
  p.rate = rate;
  p.left.assign(n, 8192);
  if (looping) {
    p.loop_start = 0;
    p.loop_end = n;
  }
  return p;
}
Pcm stream_pcm(std::size_t n) {
  Pcm p;
  p.left.assign(n, 4096);
  p.right.assign(n, -4096);
  return p;
}
void ticks(Playback &p, int n) {
  for (int i = 0; i < n; ++i)
    p.tick();
}

void formats() {
  Buffer h(0x30);
  put_be(h, 4, 4);
  put_be(h, 0x0c, 0x20);
  put_be(h, 0x10, 16000);
  std::memcpy(h.data() + 0x20, "test", 4);
  add_block(h, 0, 12, 1, {0x1f});
  add_block(h, 0, 12, 7, {});
  const auto vh = parse_vag_header(view(h));
  check(vh.magic == std::array<std::uint8_t, 4>{} && vh.version == 4 &&
            vh.data_size == 0x20 && vh.rate == 16000 && vh.name == "test",
        "VAG header without magic requirement");
  check(base_pitch(vh.rate) == 1365 && base_pitch(48000) == 0x1000, "base pitch");
  auto d = decode_vag(view(h));
  check(d.left.size() == 28 && d.left[0] == -1 && d.left[1] == 1 && d.left[2] == 0 &&
            !d.loop_start && d.rate == 16000,
        "nibble order, sign and end marker");

  Buffer pair;
  add_block(pair, 0, 12, 6, {0x01});
  add_block(pair, 1, 12, 3, {});
  d = decode_adpcm(view(pair));
  check(d.left.size() == 56 && d.left[0] == 1 && d.left[1] == 0 && d.left[28] == 0 &&
            d.loop_start == 0u && d.loop_end == 56u && d.loops(),
        "filter history and loop markers");

  // Saturation, every filter, negative floor shifts and history across blocks.
  Buffer mixed;
  add_block(mixed, 0, 0, 0, std::vector<std::uint8_t>(14, 0x77));
  add_block(mixed, 4, 0, 0, std::vector<std::uint8_t>(14, 0x77));
  std::vector<std::uint8_t> third = {0x9a, 0x1f, 0x80, 0x08};
  third.resize(14, 0xc3);
  add_block(mixed, 3, 3, 0, third);
  std::vector<std::uint8_t> fourth(7, 0xf8);
  fourth.resize(14, 0);
  add_block(mixed, 2, 12, 1, fourth);
  const std::vector<int> expected = {
      18943, -2737, -20982, -29265, -26781, -19955, -11637, -670, 10511, 14623,
      14895, 8193,  1281,   -7127,  -10478, -11968, -7785,  -3684, 2585, 5076,
      7087,  4442,  2247,   -2425,  -4108,  -6254,  -4510,  -3579, -2775, -2079,
      -1489, -987,  -572,   -227,   49,     271,    439,    568,   656,  716,
      746,   758,   756,    743,    721,    692,    658,    620,   579,  537,
      494,   451,   409,    368,    329,    292};
  d = decode_adpcm(view(mixed));
  bool same = d.left.size() == 112 && !d.loop_start && !d.loop_end;
  for (std::size_t i = 0; same && i < 56; ++i)
    same = d.left[i] == (i < 28 ? 28672 : 32767) && d.left[56 + i] == expected[i];
  check(same, "reference ADPCM vector across filters 0, 2, 3, 4");
  check(throws([] { decode_adpcm({nullptr, 1}); }), "unaligned ADPCM");
  Buffer bad;
  add_block(bad, 5, 0, 0, {});
  check(throws([&] { decode_adpcm(view(bad)); }), "filter 5 rejected");
  bad[0] = 0x0d;
  check(throws([&] { decode_adpcm(view(bad)); }), "shift 13 rejected");
  bad[0] = 0;
  bad[1] = 8;
  check(throws([&] { decode_adpcm(view(bad)); }) && decode_adpcm(view(bad), false).left.size() == 28,
        "flag byte checked for samples, ignored for streams");

  const auto dir = directory({{"mx_test.vag", 0, 1}, {"line.vag", 0x800, 0, 3}, {"", 0x1000}});
  const auto entries = parse_vat_directory(view(dir), 0x1000);
  check(entries.size() == 2 && entries[0].name == "mx_test.vag" && entries[0].length == 0x800 &&
            entries[0].stereo && entries[1].offset == 0x800 && entries[1].length == 0x800 &&
            entries[1].language == 3 && entries[0].declared_rate == 24000,
        "VAT directory uses the next offset");
  check(throws([&] { parse_vat_directory(view(dir), 0x1800); }), "VAT terminal size");
  const auto unaligned = directory({{"a.vag", 0x10}, {"", 0x800}});
  check(throws([&] { parse_vat_directory(view(unaligned), 0x800); }), "VAT sector alignment");

  // Separate decoder history per stereo channel across the 0x400 interleave.
  Buffer vat;
  for (int pass = 0; pass < 2; ++pass) {
    for (int i = 0; i < 64; ++i)
      add_block(vat, 1, 11, 0, std::vector<std::uint8_t>(14, 0x33));
    for (int i = 0; i < 64; ++i)
      add_block(vat, 2, 10, 0, std::vector<std::uint8_t>(14, 0xdd));
  }
  VatEntry stereo_entry;
  stereo_entry.length = 0x1000;
  stereo_entry.stereo = true;
  const auto st = decode_vat_entry(view(vat), stereo_entry);
  long long left_sum = 0, right_sum = 0;
  for (std::size_t i = 0; i < st.left.size(); ++i) {
    left_sum += st.left[i];
    right_sum += st.right[i];
  }
  check(st.left.size() == 3584 && st.right.size() == 3584 && st.left[0] == 6 &&
            st.left[1] == 12 && st.left[2] == 17 && st.left[1792] == 89 &&
            st.right[0] == -12 && st.right[1] == -34 && st.right[2] == -63 &&
            st.right[1792] == -788 && left_sum == 317941 && right_sum == -2814930,
        "reference stereo deinterleave vector");
  stereo_entry.loop_restart = 0x400;
  check(throws([&] { decode_vat_entry(view(vat), stereo_entry); }), "nonzero loop restart gated");

  const std::vector<VatEntry> langs = {
      {"a", 0, 0, 0, 0, false, 0}, {"a", 0x800, 0, 0, 0, false, 3}, {"a", 0x1000, 0, 0, 0, false, 0}};
  check(find_vat_entry(langs, "a", 3)->offset == 0x800 &&
            find_vat_entry(langs, "a", 5)->offset == 0x1000 && !find_vat_entry(langs, "A", 0),
        "VAT language preference, last duplicate, exact case");

  check(std::abs(stream_duration(0x800, true) - 64.0 * 28 / 24000) < 1e-12 &&
            std::abs(stream_duration(0x800, false) - 128.0 * 28 / 24000) < 1e-12 &&
            spu_rate * double(stream_pitch) / 4096 == 23976.5625,
        "stream duration and fixed stream pitch");
  const Vec3 origin{}, x_right{1, 0, 0};
  check(pan_atten(origin, origin, x_right).left == 1.0 &&
            pan_atten(origin, origin, x_right).right == 1.0,
        "inside the near sphere");
  auto g = pan_atten({600, 0, 0}, origin, x_right);
  check(std::abs(g.left - 0.5 * (1 - 600.0 / 840)) < 1e-12 &&
            std::abs(g.right - 0.5 * (1 + 600.0 / 840)) < 1e-12,
        "falloff and pan");
  g = pan_atten({840, 0, 0}, origin, x_right);
  check(g.left == 0 && g.right == 0, "far radius is silent");
  g = pan_atten({300, 400, 0}, origin, {0, 1, 0});
  check(g.left == 0x1.7befbefbefbf0p-2 && g.right == 0x1.0baebaebaebafp+0, "reference gain vector");
  g = pan_atten({-100, 50, 700}, {10, -20, 30}, {0.6, 0, 0.8});
  check(g.left == 0x1.27def537ade34p-3 && g.right == 0x1.05e2c0d59ba4dp-1, "reference gain vector 2");
  check(throws([] { pan_atten({}, {}, {}, 840, 360); }), "radii order");
}

void effects() {
  Playback p;
  check(p.add_effect("FX.vag", effect_pcm(100)) && !p.add_effect("fx.VAG", effect_pcm(5)),
        "case-insensitive duplicate keeps the first");
  check(p.play_sample("Fx.Vag", 1, 1, std::nullopt) == 0 && p.voices()[0].pitch == 0x1000,
        "first voice, 48 kHz pitch");
  check(p.play_sample("missing.vag", 1, 1, std::nullopt) == -1, "unresolved name is silent");
  p.tick();
  check(p.voices()[0].active && p.voices()[0].ended, "end flag before reclaim");
  ticks(p, 6);
  check(p.voices()[0].active, "voice held until an 8th frame");
  p.tick();
  check(!p.voices()[0].active && p.frame() == 8, "reclaimed on frame 8");
  check(p.play_sample("fx.vag", 1, 1, std::nullopt) == 1, "round robin continues after last voice");

  Playback cap;
  cap.add_effect("loop", effect_pcm(100, 48000, true));
  for (int i = 0; i < 43; ++i)
    check(cap.play_sample("loop", 1, 1, std::nullopt) == i, "43 effect voices");
  check(cap.play_sample("loop", 1, 1, std::nullopt) == -1, "no voice stealing");
  ticks(cap, 16);
  check(cap.voices()[42].active && !cap.voices()[42].ended, "looping voice persists");

  Playback pitch;
  pitch.add_effect("slow", effect_pcm(10, 16000));
  const int a = pitch.play_sample("slow", 1, 1, std::nullopt);
  const int b = pitch.play_sample("slow", 2, 1, std::nullopt);
  const int c = pitch.play_sample("slow", 0.5, 1, std::nullopt);
  const int d = pitch.play_sample("slow", 100, 1, std::nullopt);
  check(pitch.voices()[std::size_t(a)].pitch == 1365 && pitch.voices()[std::size_t(b)].pitch == 2730 &&
            pitch.voices()[std::size_t(c)].pitch == 682 && pitch.voices()[std::size_t(d)].pitch == 0x3fff,
        "pitch scale and 0x3fff clamp");
  check(throws([&] { pitch.play_sample("slow", -1, 1, std::nullopt); }), "negative pitch scale");

  Playback space;
  space.add_effect("one", effect_pcm(4000));
  space.add_effect("loop", effect_pcm(100, 48000, true));
  check(space.play_sample("one", 1, 1, Vec3{840, 0, 0}) == -1, "silent one-shot not started");
  const int loop = space.play_sample("loop", 1, 1, Vec3{840, 0, 0});
  check(loop >= 0, "silent loop still starts");
  space.set_camera({840, 0, 0}, {1, 0, 0});
  check(space.voices()[std::size_t(loop)].gains.left == 1 && space.voices()[std::size_t(loop)].gains.right == 1,
        "camera movement refreshes gains");
  const int center = space.play_sample("one", 1, 1, std::nullopt);
  check(space.voices()[std::size_t(center)].gains.left == 1, "non-positional unit gains");

  Playback freeze;
  freeze.add_effect("one", effect_pcm(4000));
  const int still = freeze.play_sample("one", 1, 1, std::nullopt);
  const int moving = freeze.play_sample("one", 1, 1, std::nullopt, true);
  freeze.set_frozen(true);
  freeze.tick();
  check(freeze.voices()[std::size_t(still)].phase == 0 &&
            freeze.voices()[std::size_t(moving)].phase == 800u << 12,
        "freeze zeros pitch unless plays-while-frozen");
}

void sequences() {
  Playback p;
  p.add_effect("loop", effect_pcm(100, 48000, true));
  p.add_effect("one", effect_pcm(100));
  check(p.start_sequence("loop", 1, {}, 7) >= 0 && p.start_sequence("loop", 1, {}, 7) >= 0 &&
            p.start_sequence("loop", 1, {}, 7) == -1,
        "default group cap 2");
  for (int i = 0; i < 9; ++i)
    check(p.start_sequence("loop", 1, {}, 14) >= 0, "group 14 cap 9");
  check(p.start_sequence("loop", 1, {}, 14) == -1, "group 14 rejects the tenth");
  for (int i = 0; i < 8; ++i)
    check(p.start_sequence("loop", 1, {}, 50) >= 0, "group 50 cap 8");
  check(p.start_sequence("loop", 1, {}, 50) == -1 && Playback::group_limit(51) == 8,
        "groups 50/51 cap 8");
  check(throws([&] { p.start_sequence("loop", 1, {}, 99); }), "group outside 0..98");
  for (int i = 0; i < 13; ++i)
    check(p.start_sequence("loop", 1, {}, 0) >= 0, "group 0 uncapped");
  check(p.start_sequence("loop", 1, {}, 0) == -1, "32 sequence slots");

  Playback h;
  h.add_effect("one", effect_pcm(100));
  const int first = h.start_sequence("one", 1, {}, 3);
  check(first == (0x8000 | (1 << 8) | 0), "handle bit 15, generation, slot");
  check(h.stop_sequence(first) && !h.stop_sequence(first) && !h.voices()[0].active, "stop once");
  const int second = h.start_sequence("one", 1, {}, 3);
  check(second == (0x8000 | (2 << 8) | 0) && !h.release_sequence(first) && h.release_sequence(second),
        "stale generation rejected");
  const int ends = h.start_sequence("one", 1, {}, 3);
  check(h.set_sequence_position(ends, {300, 0, 0}) &&
            h.voices()[std::size_t(h.sequences()[std::size_t(ends & 0xff)].voice)].gains.right > 1,
        "sequence follows its object");
  ticks(h, 16);
  check(h.sequences()[0].active && !h.voices()[std::size_t(h.sequences()[0].voice)].ended,
        "released sequence repeats until stopped");
  check(!h.sequences()[1].active && h.active_group(3) == 1, "ended sequence frees its slot");
}

void hooks() {
  check(classify_animation_event(0x1, ClipRole::attack) == GameplayCue::hit &&
            classify_animation_event(0x4, ClipRole::attack) == GameplayCue::none &&
            classify_animation_event(0x2, ClipRole::attack) == GameplayCue::none &&
            classify_animation_event(0x4, ClipRole::locomotion) == GameplayCue::footstep &&
            classify_animation_event(0x8, ClipRole::locomotion) == GameplayCue::footstep &&
            classify_animation_event(0x1, ClipRole::locomotion) == GameplayCue::none &&
            classify_animation_event(0x84, ClipRole::other) == GameplayCue::none,
        "dispatch masks by clip role");
  Playback p;
  p.add_effect("fx", effect_pcm(100));
  check(p.animation_event(1, ClipRole::attack, "fx", {}) == 0 &&
            p.animation_event(8, ClipRole::locomotion, "fx", {}) == 1 &&
            p.animation_event(4, ClipRole::attack, "fx", {}) == -1,
        "hit and footstep hooks");
}

void streams() {
  Playback p;
  p.add_stream("a", stream_pcm(50000));
  p.add_stream("b", stream_pcm(50000));
  p.add_stream("short", stream_pcm(1000));
  p.tick();
  check(p.streams()[0].volume == 0x80 && p.streams()[1].volume == 0x180 &&
            p.streams()[2].volume == 0x180,
        "driver timer: 10 ms then 6 ms steps from volume 0");
  ticks(p, 30);
  check(p.streams()[2].volume == 0x3fc0, "85 rising steps by tick 31");
  p.tick();
  check(p.streams()[2].volume == 0x3fff && p.streams()[0].volume == 88 * 0x40, "full at tick 32");

  Playback f;
  f.add_stream("a", stream_pcm(50000));
  f.add_stream("b", stream_pcm(50000));
  check(f.queue_stream(2, "a", 1), "queue loop");
  ticks(f, 3);
  check(!f.streams()[2].active && f.queued_count(2) == 1, "queue serviced on 4th frames only");
  f.tick();
  check(f.streams()[2].active && f.streams()[2].name == "a" && f.stream_samples_played(2) == 399 &&
            f.stream_time(2) == 399 / 24000.0,
        "stream starts on frame 4 at pitch 0x7fe");
  ticks(f, 36);
  check(f.queue_stream(2, "b", 0x3), "queue replacement");
  ticks(f, 4);
  check(f.streams()[2].target == 0 && f.streams()[2].name == "a", "flag 2 fades current first");
  ticks(f, 187);
  check(f.frame() == 231 && f.streams()[2].name == "a" && f.streams()[2].volume == 0,
        "512 falling steps, about 3.07 s");
  f.tick();
  check(f.streams()[2].name == "b" && f.streams()[2].target == 0x3fff && f.streams()[2].volume < 0x3fff,
        "next service starts the entry and fades in");
  ticks(f, 400);
  check(f.streams()[2].name == "b" && f.streams()[2].active, "flag 1 loops");

  Playback w;
  w.add_stream("short", stream_pcm(1000));
  w.add_stream("b", stream_pcm(50000));
  w.queue_stream(1, "short", 0);
  w.queue_stream(1, "b", 0x4);
  ticks(w, 4);
  check(w.streams()[1].name == "short", "first entry plays");
  ticks(w, 4);
  check(w.streams()[1].name == "b", "flag 4 waits for the end");

  Playback hold;
  hold.add_stream("line", stream_pcm(50000));
  hold.queue_stream(1, "line", 0x28);
  ticks(hold, 8);
  check(hold.streams()[1].active && hold.streams()[1].held && hold.stream_samples_played(1) == 0,
        "held entry waits");
  hold.release_stream(1);
  hold.tick();
  check(hold.stream_samples_played(1) == 399, "release starts playback");
  check(hold.queue_stop(1), "stop marker");
  ticks(hold, 120);
  check(!hold.streams()[1].active && hold.queued_count(1) == 0 && hold.streams()[1].target == 0x3fff,
        "stop marker fades and clears");

  Playback q;
  q.add_stream("a", stream_pcm(100));
  for (int i = 0; i < 8; ++i)
    check(q.queue_stream(0, "a", 0), "eight queue entries");
  check(!q.queue_stream(0, "a", 0) && !q.queue_stream(0, "missing", 0), "queue full, unknown name");
  check(q.queue_stream(1, "a", 0x40) && q.queue_stream(1, "a", 0x100) && q.gated_flag_entries() == 2,
        "flags 0x40/0x100 kept but uninterpreted");
  check(throws([&] { q.queue_stream(1, "a", 0x200); }) && throws([&] { q.queue_stream(1, "", 0); }) &&
            throws([&] { q.queue_stream(3, "a", 0); }),
        "undocumented flags, bad marker, bad stream");

  Playback two;
  two.add_stream("lr", stream_pcm(50000));
  two.queue_stream(2, "lr", 0);
  ticks(two, 40);
  auto out = two.drain(Playback::fifo_limit);
  const auto last = out.back();
  check(last[0] > 0 && last[1] < 0, "two-channel output keeps sides");
  two.set_two_channel_output(false);
  two.tick();
  out = two.drain(800);
  check(out.back()[0] == 0 && out.back()[1] == 0, "without two-channel output both voices mix");
  two.set_stream_scale(2, 0x80);
  two.set_two_channel_output(true);
  two.tick();
  out = two.drain(800);
  check(std::abs(out.back()[0] - last[0] / 2) < 1e-4, "stream scale 0x100 = 1");
}

Buffer zero_vat(std::size_t size) { return Buffer(size, 0); }

void music() {
  check(Playback::boss_music_name("CazicAvatar", "") == std::string("mx_ba_avataroffear_l.vag") &&
            Playback::boss_music_name("fatcyclops", "Broon") == std::string("mx_ba_broon_l.vag") &&
            Playback::boss_music_name("fatcyclops", "droon") == std::string("mx_ba_droon_l.vag") &&
            Playback::boss_music_name("cyclops", "proon") == std::string("mx_ba_proon_l.vag") &&
            Playback::boss_music_name("cazicthule", "") == std::string("mx_ba_cazicthule_l.vag") &&
            Playback::boss_music_name("reddragon", "") == std::string("mx_ba_dozekar_l.vag") &&
            Playback::boss_music_name("ironguardian", "") == std::string("mx_ba_ironguardian_l.vag") &&
            Playback::boss_music_name("mattmarr", "") == std::string("mx_ba_matanielmarr_l.vag") &&
            Playback::boss_music_name("raloszek", "") == std::string("mx_ba_ralloszek_l.vag") &&
            Playback::boss_music_name("vampirelord", "") == std::string("mx_ba_vanerhost_l.vag") &&
            Playback::boss_music_name("snowboss", "") == std::string("mx_ba_whitetormentor_l.vag"),
        "boss music table");
  check(!Playback::boss_music_name("fatcyclops", "proon") && !Playback::boss_music_name("cyclops", "") &&
            !Playback::boss_music_name("orc", ""),
        "no boss match");

  const auto gdir = directory({{"mx_maintheme_l.vag", 0, 1},
                               {"mx_ba_broon_l.vag", 0x800, 1},
                               {"mx_ba_droon_l.vag", 0x1000, 1},
                               {"", 0x1800}});
  Playback p;
  p.set_global_vat(zero_vat(0x1800), parse_vat_directory(view(gdir), 0x1800), 0);
  check(p.play_menu_music(true) && p.play_boss_music("fatcyclops", "broon") &&
            !p.play_boss_music("fatcyclops", "droon") && p.queued_count(2) == 2 &&
            p.gated_flag_entries() == 1,
        "menu then boss; second boss refused while ordering is unknown");
  ticks(p, 4);
  check(p.streams()[2].name == "mx_maintheme_l.vag" && p.streams()[2].flags == 5 &&
            p.streams()[2].pcm.left.size() == 1792,
        "menu theme from the global VAT");
  ticks(p, 4);
  check(p.streams()[2].volume == 0 || p.streams()[2].target == 0, "boss entry fades the theme");
  ticks(p, 200);
  check(p.streams()[2].name == "mx_ba_broon_l.vag" && p.streams()[2].flags == 0x123,
        "boss flags 0x103 | 0x22");
  check(p.stop_boss_music() && p.queued_count(2) == 1, "boss stop marker");

  const auto ldir = directory({{"line.vag", 0},
                               {"mx_env_a_l.vag", 0x800},
                               {"amb_x_l.vag", 0x1000},
                               {"mx_theme_b.vag", 0x1800},
                               {"", 0x2000}});
  const auto level_entries = parse_vat_directory(view(ldir), 0x2000);
  Playback l;
  check(!l.poll_level(0), "no level, no poll");
  check(throws([&] { l.set_level_vat(zero_vat(0x2000), level_entries, 0, 0); }), "countdown above zero");
  l.set_level_vat(zero_vat(0x2000), level_entries, 0, 3);
  check(!l.poll_level(5) && !l.poll_level(5) && l.level_countdown() == 1, "countdown");
  check(l.poll_level(5) && l.level_countdown() == 14405 && l.queued_count(2) == 1 &&
            l.gated_flag_entries() == 1,
        "first _env_/mx_theme entry via music_playLevel(0x102)");
  for (int i = 0; i < 12; ++i)
    check(!l.poll_level(0), "no ambience before the 16th frame");
  check(l.poll_level(0) && l.queued_count(0) == 1, "amb_ entry every 16th frame on stream 0");
  ticks(l, 4);
  check(l.streams()[2].name == "mx_env_a_l.vag" && l.streams()[2].flags == 0x122 &&
            l.streams()[0].name == "amb_x_l.vag" && l.streams()[0].flags == 3,
        "level streams start");
  check(throws([&] { l.poll_level(7200); }), "draw range");

  Playback s;
  s.add_stream("held", stream_pcm(100));
  s.set_level_vat(zero_vat(0x2000), level_entries, 0, 1);
  s.queue_stream(2, "held", 0x28);
  check(!s.poll_level(0) && s.queued_count(2) == 1 && s.level_countdown() == 14400,
        "0x20 on stream 2 suppresses the level track");
}

void determinism() {
  auto run = [] {
    Playback p;
    p.add_effect("fx", effect_pcm(3000, 22050));
    p.add_stream("m", stream_pcm(30000));
    p.queue_stream(2, "m", 1);
    std::vector<std::array<float, 2>> all;
    for (int i = 0; i < 64; ++i) {
      if (i % 5 == 0)
        p.play_sample("fx", 1.0 + i / 64.0, 0.5, Vec3{double(i * 10), 0, 0});
      p.set_camera({double(i), 0, 0}, {1, 0, 0});
      p.tick();
      const auto chunk = p.drain(std::size_t(300 + i * 13));
      all.insert(all.end(), chunk.begin(), chunk.end());
    }
    const auto rest = p.drain(Playback::fifo_limit);
    all.insert(all.end(), rest.begin(), rest.end());
    return all;
  };
  const auto a = run(), b = run();
  check(a.size() == 64 * 800 && a.size() == b.size() &&
            std::memcmp(a.data(), b.data(), a.size() * sizeof(a[0])) == 0,
        "identical calls produce identical PCM regardless of drain sizes");

  Playback fifo;
  fifo.tick();
  check(fifo.buffered_frames() == 800 && fifo.drain(500).size() == 500 && fifo.buffered_frames() == 300,
        "FIFO drain");
  ticks(fifo, 20);
  check(fifo.buffered_frames() == Playback::fifo_limit &&
            fifo.dropped_frames() == 300 + 20 * 800 - Playback::fifo_limit,
        "FIFO bounded");
}
} // namespace

int main() {
  try {
    formats();
    effects();
    sequences();
    hooks();
    streams();
    music();
    determinism();
    std::cout << "native audio: " << checks << " checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "native audio failed after " << checks << " checks: " << e.what() << '\n';
    return 1;
  }
}
