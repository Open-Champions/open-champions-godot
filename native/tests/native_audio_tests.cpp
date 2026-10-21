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
  const auto restarted = decode_vat_entry(view(vat), stereo_entry);
  check(restarted.left.size() == st.left.size() && restarted.left[0] == st.left[0] &&
            loop_restart_span(0x400) == 0x800,
        "nonzero loop restart still decodes; playback applies the span");

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
  check(p.voices()[0].active && p.voices()[0].ended && p.frame() == 1,
        "end flag before reclaim; the entry-0 read is before the mix");
  ticks(p, 7);
  check(p.voices()[0].active && p.frame() == 8, "voice held until the counter on entry is 8");
  p.tick();
  check(!p.voices()[0].active && p.frame() == 9, "reclaimed when g_soundFrame on entry is 8");
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
  f.tick();
  check(f.streams()[2].active && f.streams()[2].name == "a" && f.frame() == 1 &&
            f.stream_samples_played(2) == 399 && f.stream_time(2) == 399 / 24000.0,
        "queue serviced when g_soundFrame on entry has its low two bits clear");
  while (f.frame() < 32)
    f.tick();
  check(f.streams()[2].volume == 0x3fff, "stream 2 is full before the fade");
  check(f.queue_stream(2, "b", 0x3), "queue replacement");
  f.tick();
  check(f.streams()[2].target == 0 && f.streams()[2].name == "a", "flag 2 fades current first");
  bool held_name = true;
  int fall = 0;
  while (f.streams()[2].volume && fall < 400) {
    f.tick();
    ++fall;
    held_name = held_name && f.streams()[2].name == "a";
  }
  check(held_name && f.streams()[2].volume == 0 && fall >= 170 && fall <= 200,
        "fade from full is about 3.07 s and holds the current entry");
  int wait = 0;
  while (f.streams()[2].name != "b" && wait < 8) {
    f.tick();
    ++wait;
  }
  check(f.streams()[2].name == "b" && f.streams()[2].target == 0x3fff && f.streams()[2].volume < 0x3fff,
        "next service starts the entry and fades in");
  ticks(f, 400);
  check(f.streams()[2].name == "b" && f.streams()[2].active, "flag 1 loops");

  Playback w;
  w.add_stream("short", stream_pcm(1000));
  w.add_stream("b", stream_pcm(50000));
  w.queue_stream(1, "short", 0);
  w.queue_stream(1, "b", 0x4);
  w.tick();
  check(w.streams()[1].name == "short", "first entry plays");
  bool cut_live = false;
  int steps = 0;
  while (w.streams()[1].name != "b" && steps < 16) {
    const bool live = w.streams()[1].active && w.streams()[1].name == "short";
    w.tick();
    ++steps;
    if (live && w.streams()[1].name == "b")
      cut_live = true;
  }
  check(w.streams()[1].name == "b" && !cut_live, "flag 4 waits for the end");

  Playback hold;
  hold.add_stream("line", stream_pcm(50000));
  hold.queue_stream(1, "line", 0x28);
  ticks(hold, 8);
  check(!hold.streams()[1].active && hold.queued_count(1) == 1 && hold.stream_status(1) == 0,
        "held entry stays queued and reports idle");
  hold.release_stream(1);
  hold.tick();
  check(hold.stream_samples_played(1) == 399 && !hold.streams()[1].held, "release starts playback");
  check(hold.queue_stop(1), "stop marker");
  ticks(hold, 120);
  check(!hold.streams()[1].active && hold.queued_count(1) == 0 && hold.streams()[1].target == 0x3fff,
        "stop marker fades and clears");

  Playback q;
  q.add_stream("a", stream_pcm(100));
  for (int i = 0; i < 8; ++i)
    check(q.queue_stream(0, "a", 0), "eight queue entries");
  check(!q.queue_stream(0, "a", 0) && !q.queue_stream(0, "missing", 0), "queue full, unknown name");
  check(q.queue_stream(1, "a", 0x40) && q.queue_stream(1, "a", 0x100) && q.gated_flag_entries() == 0 &&
            q.stream_status(1) == 0x41,
        "flags 0x40 and 0x100 are stored and returned, not refused");
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
  p.set_music_volume(float_one); // fixture option, as tools/difftest_audio.py sets G_MUSIC
  p.set_global_vat(zero_vat(0x1800), parse_vat_directory(view(gdir), 0x1800), 0);
  check(p.play_menu_music(true) && !p.play_boss_music("orc") && p.queued_count(2) == 1,
        "a creature with no boss name leaves stream 2 alone");
  Playback silent_menu;
  silent_menu.set_global_vat(zero_vat(0x1800), parse_vat_directory(view(gdir), 0x1800), 0);
  silent_menu.set_music_volume(music_volume_gate - 1);
  check(!silent_menu.play_menu_music(false) && silent_menu.queued_count(2) == 0,
        "the first menu theme is gated below the documented music volume");
  silent_menu.set_music_volume(music_volume_gate);
  check(silent_menu.play_menu_music(false) && silent_menu.queued_count(2) == 1,
        "the first menu theme queues at the gate value");
  check(p.play_boss_music("fatcyclops", "broon") && p.queued_count(2) == 1 &&
            p.streams()[2].file_offset == 0,
        "the stop clears the menu before the boss span is queued");
  check(p.play_boss_music("fatcyclops", "droon") && p.queued_count(2) == 1 &&
            !p.streams()[2].active,
        "a second boss replaces the queued span");
  p.tick();
  check(p.streams()[2].name == "mx_ba_droon_l.vag" && p.streams()[2].flags == 0x123 &&
            p.streams()[2].file_offset == 0x1000 && p.streams()[2].loop_restart == 0,
        "boss flags 0x103 | 0x22 and the new VAT offset");
  check(p.stop_boss_music("orc") == false && p.queued_count(2) == 0, "no-name stop does not append");
  check(p.stop_boss_music("fatcyclops", "broon") && p.queued_count(2) == 1,
        "a named stop appends the flag 2 marker even for a different boss");

  const auto ldir = directory({{"line.vag", 0},
                               {"mx_env_a_l.vag", 0x800},
                               {"amb_x_l.vag", 0x1000},
                               {"mx_theme_b.vag", 0x1800},
                               {"", 0x2000}});
  const auto level_entries = parse_vat_directory(view(ldir), 0x2000);
  Playback l;
  l.set_music_volume(float_one);
  check(l.draw_frame() == draw_frame_image && l.frame() == sound_frame_image &&
            l.level_countdown() == level_countdown_image,
        "process images of the two frame counters and the amb countdown");
  check(!l.poll_level(0), "no level, no poll");
  check(throws([&] { l.set_level_vat(zero_vat(0x2000), level_entries, 0, 0); }), "countdown above zero");
  l.set_level_vat(zero_vat(0x2000), level_entries, 0, 3);
  l.advance_draw_frame(); // 0x401 is not an amb frame
  check(!l.poll_level(5) && !l.poll_level(5) && l.level_countdown() == 1 && l.queued_count(0) == 0,
        "countdown");
  check(l.poll_level(5) && l.level_countdown() == 14405 && l.queued_count(2) == 1 &&
            l.gated_flag_entries() == 0,
        "first _env_/mx_theme entry via music_playLevel(0x102)");
  while (!draw_cadence(l.draw_frame()))
    l.advance_draw_frame();
  check(l.poll_level(0) && l.queued_count(0) == 1, "amb_ when g_drawFrame's low four bits are clear");
  l.tick();
  check(l.streams()[2].name == "mx_env_a_l.vag" && l.streams()[2].flags == 0x122 &&
            l.streams()[0].name == "amb_x_l.vag" && l.streams()[0].flags == 3,
        "level streams start on the sound frame whose low two bits are clear");
  check(throws([&] { l.poll_level(7200); }), "draw range");

  Playback s;
  s.add_stream("held", stream_pcm(100));
  s.set_level_vat(zero_vat(0x2000), level_entries, 0, 1);
  s.queue_stream(2, "held", 0x28);
  s.advance_draw_frame();
  check(!s.poll_level(0) && s.queued_count(2) == 1 && s.queued_count(0) == 0 &&
            s.level_countdown() == 14400,
        "0x20 on stream 2 suppresses the level track");
}

void playback_vectors() {
  const std::uint32_t spans[] = {0, 1, 0x3ff, 0x400, 0x401, 0x800, 0x123456, 0x7ffffc00u,
                                  0x80000000u, 0x40000000u};
  const std::uint32_t expect[] = {0, 0, 0, 0x800, 0x800, 0x1000, 0x246800, 0xfffff800u, 0,
                                   0x80000000u};
  bool spans_ok = true;
  for (int i = 0; i < 10; ++i)
    spans_ok = spans_ok && loop_restart_span(spans[i]) == expect[i];
  check(spans_ok, "loop span drops the low ten bits and bit 31");
  auto read = after_completed_read;
  check(read(true, 1, 0, 0, 0x2000, 0x9000, 0x400, 0).position == 0x2800 &&
            read(true, 1, 0, 0, 0x2000, 0x9000, 0x400, 0).remaining == 0x8800,
        "restart 0x400 skips one 0x800 step");
  check(read(true, 1, 0, 0, 0x2000, 0x9000, 0, 0).position == 0x2000 &&
            read(true, 1, 0, 0, 0x2000, 0x9000, 0, 0).remaining == 0x9000,
        "restart 0 rewinds to the entry offset");
  const auto wrapped = read(true, 1, 0, 0, 0x2000, 0x9000, 0x123456, 0);
  check(wrapped.position == 0x248800 && wrapped.remaining == 0xffdc2800u, "a long span wraps remaining");
  const auto exhausted = read(true, 1, 0x111, 0x20, 0x2000, 0x9000, 0x400, 0x20);
  check(exhausted.position == 0x2800 && exhausted.remaining == 0x8800, "the read must exhaust remaining");
  const auto short_read = read(true, 1, 0x111, 0x20, 0x2000, 0x9000, 0x400, 0);
  check(short_read.position == 0x111 && short_read.remaining == 0x20, "a short read does not rewind");
  const auto unlooped = read(true, 0, 0x111, 0, 0x2000, 0x9000, 0x400, 0);
  check(unlooped.position == 0x111 && unlooped.remaining == 0, "no loop bit, no rewind");
  check(read(false, 1, 0x111, 0x10, 0x2000, 0x9000, 0x400, 0x10).position == 0x121 &&
            read(false, 1, 0x111, 0x10, 0x2000, 0x9000, 0x400, 0x10).remaining == 0,
        "an empty name does not rewind");

  check(!music_gate_open(music_volume_gate - 1) && music_gate_open(music_volume_gate) &&
            music_gate_open(float_one) && !music_gate_open(0),
        "music gate is closed only below the constant");
  check(boss_action(false, float_one, 0x1000) == BossAction::noop &&
            boss_action(true, float_one, 0x1000) == BossAction::replace &&
            boss_action(true, music_volume_gate - 1, 0x1000) == BossAction::silence &&
            boss_action(true, float_one, 0) == BossAction::silence,
        "boss action");

  check(stream_status_bits(1, 2, 0, 0x140, 0, 0) == 0x141 &&
            stream_status_bits(0, 0, 1, 0x40, 0, 0) == 0x41 &&
            stream_status_bits(0, 0, 0, 0, 1, 0x100) == 0x101 &&
            stream_status_bits(0, 0, 0, 0, 1, 0x108) == 0 &&
            stream_status_bits(0, 0, 0, 0x40, 0, 0) == 0 &&
            stream_status_bits(0, 0, 0x80000000u, 0x40, 1, 0x100) == 0x101,
        "status surfaces caller bits and hides a held queue entry");
  check(recall_stops_music(0x41) && !recall_stops_music(0x21) &&
            cut_run_stream2(false, 0x80) == StreamDecision::leave &&
            cut_run_stream2(true, 0x80) == StreamDecision::stop &&
            cut_run_stream2(true, 0x180) == StreamDecision::leave &&
            cut_run_stream2(true, 0x20) == StreamDecision::leave &&
            cut_end_stream2(false, 0x80) == StreamDecision::leave &&
            cut_end_stream2(true, 0x180) == StreamDecision::leave &&
            cut_end_stream2(true, 0x80) == StreamDecision::stop &&
            cut_end_stream2(true, 0x20) == StreamDecision::clear_queue &&
            cut_end_stream2(true, 0) == StreamDecision::clear_queue,
        "recall fades and cutscene keep leaves stream 2");

  const auto voiced = duck_step(float_one, 0, 0x21, 0x21, false, float_one);
  check(voiced.current == 0x3f7d70a3u && voiced.target == 0x3e5ddde1u && voiced.sent &&
            voiced.channel == 0x3f45ffffu,
        "voice target is the constant plus one creep");
  check(duck_step(float_one, 0, 0x21, 0, false, float_one).sent == false, "no music bit, no send");
  check(duck_step(0x3e5ddde1u, voiced.target, 0x21, 0x21, false, float_one).sent == false,
        "an unchanged scalar does not send");
  const auto quiet = duck_step(float_one, 0, 0, 0x21, false, float_one);
  check(quiet.target == duck_creep && quiet.current == 0x3f7d70a3u, "without voice the target keeps creeping");
  const auto intro = duck_step(float_one, 0, 0x21, 0x21, true, float_one);
  check(intro.current == 0 && intro.target == 0x3e5ddde1u, "actintro in another language zeros the scalar");
  check(duck_step(float_one, 0, 0x21, 0x21, false, float_one).current != 0,
        "actintro in language 0 leaves the scalar");
  check(set_duck_target(0) == float_one && set_duck_target(float_one) == 0 &&
            set_duck_target(0x3e4ccccdu) == 0x3f4cccccu,
        "music_setDuck stores one minus the argument");

  const auto at0 = sound_cadence(0);
  check(sound_frame_image == 0 && draw_frame_image == 0x400 && at0.queues && at0.end_flags &&
            !at0.send_volumes && at0.next == 1 && sound_cadence(4).queues && !sound_cadence(4).end_flags &&
            sound_cadence(8).end_flags && sound_cadence(0xf).send_volumes &&
            !sound_cadence(0x10).send_volumes && draw_cadence(0x400) && !draw_cadence(0x401) &&
            draw_cadence(0) && !draw_cadence(15) && draw_cadence(16),
        "sound and draw cadences, including the image phases");
  Playback cadence;
  ticks(cadence, 15);
  check(cadence.frame() == 15 && cadence.volume_sends() == 0 && cadence.draw_frame() == draw_frame_image,
        "the volume send is after the increment, and draw is a separate counter");
  cadence.tick();
  check(cadence.frame() == 16 && cadence.volume_sends() == 1, "entering 0xF sends volumes");

  Playback duck;
  duck.set_music_volume(float_one);
  duck.add_stream("v", stream_pcm(1000));
  duck.queue_stream(1, "v", 0x20);
  duck.queue_stream(2, "v", 0x20);
  const auto live = duck.step_duck();
  check(live.current == 0x3f7d70a3u && live.target == 0x3e5ddde1u && duck.duck_sent() &&
            duck.music_channel() == 0x3f45ffffu,
        "Playback duck uses stream 1 status bit 0x20");
  check(duck.set_duck(0x3e4ccccdu) == 0x3f4cccccu && duck.duck_current() == live.current,
        "set_duck writes the target and leaves the current scalar");
  Playback intro_play;
  intro_play.set_music_volume(float_one);
  intro_play.add_stream("v", stream_pcm(1000));
  intro_play.set_level_name("actintro");
  intro_play.set_language(1);
  intro_play.queue_stream(1, "v", 0x20);
  intro_play.queue_stream(2, "v", 0x20);
  check(intro_play.step_duck().current == 0, "actintro with a nonzero language stores integer 0");

  Playback closed;
  closed.set_music_volume(music_volume_gate - 1);
  closed.set_global_vat(zero_vat(0x800),
                        parse_vat_directory(view(directory({{"mx_ba_broon_l.vag", 0, 1}, {"", 0x800}})), 0x800),
                        0);
  check(!closed.play_boss_music("fatcyclops", "broon") && !closed.streams()[2].active &&
            closed.queued_count(2) == 0,
        "a closed music gate stops stream 2 and queues nothing");

  Playback missing;
  missing.set_music_volume(float_one);
  missing.set_global_vat(zero_vat(0x800),
                         parse_vat_directory(view(directory({{"other.vag", 0, 1}, {"", 0x800}})), 0x800), 0);
  missing.queue_stream(2, "other.vag", 1);
  check(missing.play_boss_music("fatcyclops", "broon") == false && missing.queued_count(2) == 0 &&
            !missing.streams()[2].active,
        "a missing boss row stops stream 2 and queues nothing");
  Playback empty_row;
  empty_row.set_music_volume(float_one);
  empty_row.set_global_vat(
      {}, parse_vat_directory(view(directory({{"mx_ba_broon_l.vag", 0, 1}, {"pad.vag", 0}, {"", 0}})), 0),
      0);
  check(empty_row.queue_stream(2, "pad.vag", 1, StreamSource::global), "zero-length row can sit in the queue");
  check(!empty_row.play_boss_music("fatcyclops", "broon") && empty_row.queued_count(2) == 0,
        "a zero-length boss row stops stream 2 and queues nothing");

  Playback recall;
  recall.add_stream("theme", stream_pcm(1000));
  recall.queue_stream(2, "theme", 0x41);
  check(recall.stream_status(2) == 0x41 && recall.apply_recall_stop() && recall.queued_count(2) == 2,
        "bit 0x40 makes the recall slices append a stream-2 stop");
  Playback kept;
  kept.add_stream("theme", stream_pcm(1000));
  kept.queue_stream(2, "theme", 0x122);
  check(!kept.apply_recall_stop() && kept.queued_count(2) == 1, "boss and level flags do not carry 0x40");

  Playback cut;
  cut.add_stream("amb", stream_pcm(1000));
  cut.add_stream("line", stream_pcm(1000));
  cut.queue_stream(1, "line", 1);
  cut.queue_stream(2, "amb", 0x80);
  cut.tick();
  check(cut.cutscene_run(true) == StreamDecision::stop && !cut.streams()[1].active &&
            !cut.streams()[2].active,
        "cut_run hard-stops an ambience entry that lacks 0x100 and also stops stream 1");
  cut.queue_stream(2, "amb", 0x180);
  check(cut.cutscene_run(true) == StreamDecision::leave && cut.queued_count(2) == 1,
        "bit 0x100 keeps the stream-2 entry");
  cut.clear_queue(2);
  cut.queue_stream(2, "amb", 1);
  while (!cut.streams()[2].active)
    cut.tick();
  cut.queue_stream(2, "amb", 1);
  check(cut.cutscene_end(true) == StreamDecision::clear_queue && cut.streams()[2].active &&
            cut.queued_count(2) == 0,
        "cut_end clears the queue and keeps the active entry");

  // A looping VAT entry with restart 0x400 resumes at the second 0x800 sector.
  Buffer body(0x1000, 0);
  for (int i = 0; i < 256; ++i)
    body[std::size_t(i) * 16] = 0x0c;
  const auto vdir = directory({{"loop.vag", 0, 0, 0, 0x400}, {"", 0x1000}});
  Playback loop;
  loop.set_global_vat(body, parse_vat_directory(view(vdir), 0x1000), 0);
  check(loop.queue_stream(0, "loop.vag", 1, StreamSource::global), "looping VAT entry");
  int guard = 0;
  while (loop.streams()[0].file_position == 0 && guard < 40) {
    loop.tick();
    ++guard;
  }
  const auto &cursor = loop.streams()[0];
  const auto index = cursor.phase >> 12;
  // The crossing tick keeps mixing, so the sample index is a little past 3584.
  check(cursor.file_position == 0x800 && cursor.bytes_remaining == 0x800 && cursor.audible &&
            index >= 3584 && index < 4000,
        "loop restart 0x400 resumes 0x800 bytes into the entry");
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

// Review audit (GDExtension sound_tick, U1): a VAT entry queued from one level
// directory, then the directory replaced before the 4th-frame stream service
// reaches it. The entry is dropped as failed media; it is never decoded from an
// entry the directory no longer has (that was an empty std::optional dereference).
void replaced_directory() {
  const auto first = directory({{"mx_env_a_l.vag", 0}, {"", 0x800}});
  const auto second = directory({{"other_l.vag", 0}, {"", 0x800}});
  Playback p;
  p.set_level_vat(zero_vat(0x800), parse_vat_directory(view(first), 0x800), 0, 100);
  check(p.queue_stream(2, "mx_env_a_l.vag", 0, StreamSource::level) && p.queued_count(2) == 1,
        "entry queued from the first level directory");
  p.set_level_vat(zero_vat(0x800), parse_vat_directory(view(second), 0x800), 0, 100);
  ticks(p, 4);
  check(!p.streams()[2].active && p.failed_stream_entries() == 1 && p.queued_count(2) == 0,
        "an entry missing from the replaced directory is dropped as failed media");
}
// Review "Mix OOB": Playback::mix steps a voice by its pitch (at most 0x3fff, just
// under four samples) and moves it back by one loop length or one sample length.
// A loop or a repeating sample shorter than the step would leave the phase past
// the data and the next mix would read outside it. VAG loops are whole 28-sample
// blocks; shorter PCM is refused when it is registered.
void short_loops() {
  Playback p;
  Pcm tiny = effect_pcm(8, 48000);
  tiny.loop_start = 4;
  tiny.loop_end = 5; // a one-sample loop
  check(throws([&] { p.add_effect("tiny_loop", tiny); }), "a loop shorter than the largest pitch step is refused");
  Pcm past = effect_pcm(8, 48000);
  past.loop_start = 2;
  past.loop_end = 12;
  check(throws([&] { p.add_effect("past_end", past); }), "a loop past the sample is refused");
  check(throws([&] { p.add_effect("three", effect_pcm(3, 48000)); }), "a sample shorter than the largest pitch step is refused");
  Pcm ok = effect_pcm(64, 48000, true);
  check(p.add_effect("ok", ok), "a whole-sample loop is accepted");
  check(p.play_sample("ok", 100.0, 1.0, std::nullopt) >= 0, "plays at the clamped top pitch");
  for (int i = 0; i < 8; ++i) p.tick();
  check(!p.drain(4096).empty(), "mixed without leaving the data");
}
// Audit AUD-2: music_updateDucking's audio_setChannelVolume(2, ...) argument is
// executed (docs/audio-playback.md 222-224), but how the driver applies it to
// the output is not documented. The word is recorded and stream 2's gain stays
// the status volume times the stream scale.
void duck_channel_not_mixed() {
  Playback p;
  p.set_music_volume(float_one);
  Pcm silent;
  silent.left.assign(200000, 0);
  silent.right.assign(200000, 0);
  p.add_stream("voice", silent);
  p.add_stream("music", stream_pcm(200000));
  p.queue_stream(1, "voice", 0x20);
  p.queue_stream(2, "music", 0x20);
  ticks(p, 40); // promoted on frame 0; 86 driver steps of +0xc0 reach 0x3fff
  check(p.streams()[2].active && p.streams()[2].volume == Playback::full_volume,
        "music stream at full status volume");
  p.step_duck();
  check(p.duck_sent() && p.music_channel() != float_one, "the duck sent and recorded its channel word");
  p.drain(Playback::fifo_limit);
  p.tick();
  const auto out = p.drain(800);
  check(out.size() == 800 && out[0][0] == 0.125f && out[799][0] == 0.125f && out[0][1] == -0.125f,
        "the channel word does not scale stream 2's mix");
}
// Audit AUD-3: the research executes one music_updateDucking call; its call
// site, cadence and order inside sound_update are not documented, so tick()
// does not run it. Callers run step_duck() explicitly.
void duck_not_in_tick() {
  Playback p;
  p.add_stream("v", stream_pcm(100000));
  p.queue_stream(1, "v", 0x20);
  p.queue_stream(2, "v", 0x20);
  ticks(p, 8);
  check(p.duck_current() == duck_current_image && p.duck_target() == duck_target_image &&
            !p.duck_sent(),
        "tick() leaves the duck words at their images");
}
// Audit AUD-1, lifted: g_musicVolume 0x4f8944 starts at save_init's 0.5 and a
// session takes the options file's +0x08 unclamped (research 22408a1
// docs/spec/music-volume.md). Music reads the stored word; nothing refuses.
void music_volume_input() {
  const auto gdir = directory({{"mx_ba_broon_l.vag", 0, 1}, {"", 0x800}});
  Playback p;
  check(p.music_volume() == music_volume_default, "the default is save_init's 0.5, not the ELF 1.0");
  p.set_global_vat(zero_vat(0x800), parse_vat_directory(view(gdir), 0x800), 0);
  check(p.play_boss_music("fatcyclops", "broon") && p.queued_count(2) == 1,
        "boss music plays from the default volume");
  p.add_stream("theme", stream_pcm(100));
  p.queue_stream(2, "theme", 0x21);
  p.tick();
  const auto step = p.step_duck();
  check(step.sent && step.channel == ducked_channel(step.current, music_volume_default),
        "a duck send reads the default volume");
  const auto ldir = directory({{"mx_theme_b.vag", 0}, {"amb_x_l.vag", 0x800}, {"", 0x1000}});
  Playback l;
  l.set_level_vat(zero_vat(0x1000), parse_vat_directory(view(ldir), 0x1000), 0, 1);
  check(l.poll_level(0) && l.queued_count(2) == 1 && l.queued_count(0) == 1 &&
            l.level_countdown() == 14400,
        "the level poll queues its music_playLevel call at the default");
  Playback file;
  file.set_level_vat(zero_vat(0x1000), parse_vat_directory(view(ldir), 0x1000), 0, 1);
  file.set_music_volume(boot_music_volume(0u));
  check(file.music_volume() == 0 && file.poll_level(0) && file.queued_count(2) == 0 &&
            file.queued_count(0) == 1,
        "an options-file 0 closes the gate: no level music");
  Playback loud;
  loud.set_music_volume(boot_music_volume(0x3fc00000u));
  loud.set_level_vat(zero_vat(0x1000), parse_vat_directory(view(ldir), 0x1000), 0, 1);
  check(loud.music_volume() == 0x3fc00000u && loud.music_play_level("mx_theme_b.vag", 0x102),
        "an options-file 1.5 is stored unclamped and plays");
  l.set_music_volume(music_volume_gate);
  check(l.music_volume() == music_volume_gate && l.music_play_level("mx_theme_b.vag", 0x102) &&
            l.queued_count(2) == 2,
        "the gate pattern itself opens the gate");
}
// Audit AUD-4: g_soundFrame and g_drawFrame are process-lifetime words, and the
// duck words are process statics (docs/audio-playback.md 210-212, 260-265). A
// client session boundary keeps them instead of restoring the process images.
void session_boundary() {
  const auto ldir = directory({{"mx_theme_b.vag", 0}, {"amb_x_l.vag", 0x800}, {"", 0x1000}});
  Playback p;
  p.set_music_volume(float_one);
  p.add_effect("fx", effect_pcm(3000, 48000, true));
  p.add_stream("v", stream_pcm(100000));
  p.set_level_vat(zero_vat(0x1000), parse_vat_directory(view(ldir), 0x1000), 0, 1);
  p.queue_stream(1, "v", 0x20);
  p.queue_stream(2, "v", 0x20);
  ticks(p, 5);
  p.advance_draw_frame();
  p.step_duck();
  const auto seq = p.start_sequence("fx", 1.0, {}, 0);
  check(p.play_sample("fx", 1.0, 1.0, std::nullopt) >= 0 && seq >= 0 && p.queue_stream(2, "v", 1),
        "session has voices, a sequence and queued music");
  const auto duck = p.duck_current(), target = p.duck_target();
  const auto volume = p.streams()[2].volume;
  p.end_session();
  bool silent = true;
  for (const auto &v : p.voices())
    silent = silent && !v.active;
  for (std::size_t i = 0; i < Playback::stream_limit; ++i)
    silent = silent && !p.streams()[i].active && p.queued_count(i) == 0;
  check(silent && p.buffered_frames() == 0 && !p.poll_level(0) && !p.stop_sequence(seq),
        "the boundary ends streams, voices, sequences, the level poll and unread output");
  check(p.frame() == 5 && p.draw_frame() == draw_frame_image + 1 && p.duck_current() == duck &&
            p.duck_target() == target && p.music_volume() == float_one &&
            p.streams()[2].volume == volume && volume != 0,
        "process-lifetime words survive the boundary");
}
int main() {
  try {
    session_boundary();
    duck_channel_not_mixed();
    duck_not_in_tick();
    music_volume_input();
    replaced_directory();
    short_loops();
    formats();
    effects();
    sequences();
    hooks();
    streams();
    music();
    playback_vectors();
    determinism();
    std::cout << "native audio: " << checks << " checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "native audio failed after " << checks << " checks: " << e.what() << '\n';
    return 1;
  }
}
