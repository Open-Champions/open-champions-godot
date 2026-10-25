// Authored Session-boundary regressions. These test the declared scheduler and
// ownership policy; they do not assert original game animation ordering.
#include "champions/detail/checkpoint.hpp"
#include "champions/session.hpp"
#include "session_anim_cache_trace.hpp"
#include <array>
#include <functional>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace R = champions::simulation::rta;
namespace S = R::session;
namespace O = R::objects;
namespace A = R::animation_runtime;
namespace {
unsigned checks = 0;
void check(bool ok, const char *why) {
  ++checks;
  if (!ok)
    throw std::runtime_error(why);
}
template <class E = S::Error, class F> void rejects(F &&fn, const char *why) {
  bool caught = false;
  try {
    fn();
  } catch (const E &) {
    caught = true;
  }
  check(caught, why);
}
S::Config config(unsigned bits = 0) {
  S::Config c;
  c.network_mode = 1;
  c.peers = 1;
  c.local = 0;
  c.model = static_cast<R::Fpu::Model>(bits / 4);
  c.mul_bias = (bits & 2) != 0;
  c.div_nearest = (bits & 1) != 0;
  c.seed = 0xabcdef01;
  return c;
}
R::netwire::ByteVector sample() { return {0, 0, 128, 128, 128, 128, 0, 0}; }
std::vector<std::uint8_t> bytes() {
  std::vector<std::uint8_t> out(0x80, 0);
  auto word = [&](std::size_t at, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i)
      out[at + i] = std::uint8_t(value >> (8 * i));
  };
  word(0, 1);
  word(4, 20);
  word(8, 0x80);
  word(12, 0x78);
  word(16, 0x70);
  std::vector<unsigned> bits;
  auto put = [&](std::uint32_t value, unsigned n) {
    for (unsigned i = n; i > 0; --i)
      bits.push_back((value >> (i - 1)) & 1);
  };
  put(7, 4);
  put(64, 8);
  put(0, 8);
  put(0, 8);
  put(13, 4);
  put(4096, 14);
  put(0, 14);
  put(0, 14);
  put(0, 14);
  put(255, 8);
  while (bits.size() % 32)
    bits.push_back(0);
  for (std::size_t i = 0; i < bits.size(); i += 16) {
    std::uint16_t value = 0;
    for (unsigned j = 0; j < 16; ++j)
      value = std::uint16_t((value << 1) | bits[i + j]);
    out.push_back(std::uint8_t(value));
    out.push_back(std::uint8_t(value >> 8));
  }
  return out;
}
struct Fixture {
  S::Session session;
  std::vector<O::GameObject *> objects;
  std::vector<S::HpId> hp;
  unsigned runs = 0;
  explicit Fixture(unsigned bits = 0, unsigned actors = 1,
                   S::ObjectBindings bindings = {})
      : session(config(bits)) {
    bindings.policy = O::CallbackPolicy::ReferenceBase;
    if (!bindings.on_run.function)
      bindings.on_run = {"count_run", [this](auto &) { ++runs; }};
    session.enable_objects(17, std::move(bindings));
    session.with_world([&](auto &w) {
      for (unsigned i = 0; i < actors; ++i) {
        auto *o = w.new_object(0x400, {});
        o->flags |= 0x20;
        w.run_every_frame(*o);
        objects.push_back(o);
      }
    });
    session.enable_hp();
    for (auto *o : objects)
      hp.push_back(session.bind_hp(o->serial, o->addr, {0x42c80000, 0, {}}));
  }
  void task(S::Session::Task fn, std::int64_t priority = 20,
            std::uint32_t id = 0xf5000001) {
    session.register_object_task(id, "authored_animation", priority, 0,
                                 std::move(fn));
  }
  void prime() {
    check(session.submit(sample()).steps == 0,
          "one-peer initial submission has no ready tick");
  }
  void tick() { session.submit(sample()); }
};
const S::AnimationCacheBindingSnapshot &binding(const S::Snapshot &s,
                                                std::size_t i = 0) {
  return s.animation_cache->bindings.at(i);
}
void enable(Fixture &f, S::AnimationCacheLimits limits = {},
            std::int32_t capacity = 3) {
  f.session.enable_animation_cache(2, limits);
  for (auto hp : f.hp)
    f.session.bind_animation_cache(hp, capacity);
}
std::string observed(const S::Snapshot &s) {
  return champions::test::session_anim_cache_trace::observation(s);
}
void ready_and_owned() {
  for (unsigned cfg = 0; cfg < 16; ++cfg) {
    Fixture f(cfg, 2);
    enable(f);
    auto source = R::animation_decoder::parse_clip(bytes());
    source.events[0] = {0, 128};
    const auto one = f.session.add_animation_cache_clip(source),
               two = f.session.add_animation_cache_clip(source);
    source.events[0].mask = 1;
    source.words.clear();
    check(one != two, "equal clips preserve independent identities");
    A::TrackId track = 0;
    unsigned tick = 0;
    std::optional<S::Snapshot> saved;
    std::string original;
    f.task([&](auto &s) {
      if (tick++ == 0) {
        track = s.start_animation_cache(f.hp[0], one);
        s.start_animation_cache(f.hp[1], two);
        s.set_animation_cache_event(one, 0, {0, 255});
      }
      check(s.advance_animation_cache(f.hp[0], 0x3f000001) ==
                (tick == 1 ? -1 : 0),
            "shared event signed value");
      check(s.advance_animation_cache(f.hp[1]) == (tick == 1 ? -128 : 0),
            "distinct identity event stays unchanged");
      const auto decoded = s.decode_animation_cache(f.hp[0], track);
      auto repeat = s.decode_animation_cache(f.hp[0], track);
      check(decoded.joints.size() == 1 && repeat.slot == decoded.slot &&
                decoded.joints[0].position == repeat.joints[0].position,
            "explicit repeated decode reuses cache");
      check(decoded.time ==
                s.snapshot()
                    .animation_cache->runtime.decoder.tracks[track - 1]
                    .value.time,
            "decode samples current raw32 track time");
      if (tick == 1) {
        saved = s.snapshot();
        original = observed(*saved);
      }
      if (tick == 2) {
        const auto before = s.snapshot().animation_cache->runtime;
        s.stop_animation_cache(f.hp[0], one);
        const auto stopped = s.snapshot().animation_cache->runtime;
        check(stopped.states[0].value.count == 0 &&
                  stopped.decoder.tracks[track - 1].value.slot == 0,
              "stop releases live cache while retaining fixed cells");
        check(stopped.decoder.order == before.decoder.order &&
                  stopped.decoder.slots[decoded.slot - 1].owner == 0 &&
                  stopped.decoder.slots[decoded.slot - 1].cursor ==
                      before.decoder.slots[decoded.slot - 1].cursor,
              "release preserves LRU and stale decoder payload");
        s.start_animation_cache(f.hp[0], two);
        s.decode_animation_cache(f.hp[0], track);
      }
    });
    const auto initial = f.session.snapshot();
    f.prime();
    check(f.session.snapshot().animation_cache->operations.empty(),
          "warmup has no animation cache work");
    auto &ambient = R::set_fpu(R::Fpu::Model::Nearest, true, true);
    f.tick();
    f.tick();
    auto final = f.session.snapshot();
    check(observed(*saved) == original,
          "complete snapshot immutable after events tracks slots mutate");
    check(final.animation_cache->runtime.decoder.cosmetic_seed == 0 &&
              final.draws == initial.draws &&
              final.world->shared_seed == initial.world->shared_seed,
          "animation makes no random draws");
    check(ambient.model == R::Fpu::Model::Nearest && ambient.mul_bias &&
              ambient.div_nearest,
          "Session restores ambient arithmetic");
    check(binding(final).state == 1 && binding(final, 1).state == 2,
          "runtime state IDs bind existing HP identities");
    check(final.animation_cache->runtime.states[0].value.tracks.size() == 3,
          "inactive state cells retained");
    check(final.animation_cache->runtime.decoder.slots[0].joints.size() == 64,
          "all decoder records retained");
    check(final.animation_cache->runtime.decoder.sim_frame ==
              std::uint32_t(final.world->frame),
          "operation imports frame without increment");
    auto detached = final;
    detached.animation_cache->runtime.decoder.clips[0].value.events[0].mask =
        13;
    check(f.session.snapshot()
                  .animation_cache->runtime.decoder.clips[0]
                  .value.events[0]
                  .mask == 255,
          "snapshot mutation cannot reach Runtime");
  }
}
void setup_and_phase() {
  for (unsigned mode = 0; mode < 9; ++mode) {
    Fixture f;
    S::AnimationCacheLimits l;
    if (mode == 0)
      l.calls_per_tick = 65;
    if (mode == 1)
      l.history = 0;
    if (mode == 2)
      l.runtime.decoder.slots = 1;
    if (mode == 3)
      l.runtime.states = A::MAX_STATES + 1;
    if (mode == 4)
      l.runtime.state_cells = A::MAX_STATE_CELLS + 1;
    if (mode == 5)
      l.runtime.decoder.input_bytes = R::animation_decoder::MAX_INPUT_BYTES + 1;
    if (mode == 6)
      l.runtime.decoder.key_records = R::animation_decoder::MAX_KEY_RECORDS + 1;
    if (mode == 7)
      l.runtime.max_loop_subtractions = A::MAX_LOOP_SUBTRACTIONS + 1;
    if (mode == 8)
      f.session.enable_animation();
    rejects<std::exception>(
        [&] { f.session.enable_animation_cache(mode == 2 ? 1 : 2, l); },
        "invalid setup refuses before publication");
    check(!f.session.snapshot().animation_cache,
          "invalid cache setup is atomic");
  }
  {
    Fixture f;
    enable(f);
    rejects([&] { f.session.enable_animation(); },
            "reverse policy coexistence refuses");
    check(!f.session.snapshot().animation, "failed second policy unpublished");
  }
  for (int cap : {0, -1, 128}) {
    Fixture f;
    f.session.enable_animation_cache();
    rejects<A::ValueError>(
        [&] { f.session.bind_animation_cache(f.hp[0], cap); },
        "bounded signed track capacity");
    check(f.session.snapshot().animation_cache->runtime.states.empty() &&
              f.session.snapshot()
                  .animation_cache->runtime.decoder.tracks.empty(),
          "invalid state creates no cells");
  }
  {
    Fixture f;
    enable(f);
    rejects<std::exception>(
        [&] {
          f.session.add_animation_cache_clip(std::vector<std::uint8_t>(5));
        },
        "invalid bytes refuse");
    check(f.session.snapshot().animation_cache->runtime.decoder.clips.empty(),
          "invalid parse publishes no clip");
  }
  {
    Fixture f;
    enable(f);
    f.prime();
    rejects([&] { f.session.add_animation_cache_clip(bytes()); },
            "setup freezes at submit");
  }
  for (unsigned mode = 0; mode < 6; ++mode) {
    Fixture f;
    enable(f);
    auto c = f.session.add_animation_cache_clip(bytes());
    f.task(
        [&](auto &s) {
          if (mode == 0)
            s.with_world([&](auto &) { s.start_animation_cache(f.hp[0], c); });
          else if (mode == 3)
            s.start_animation_cache(f.hp[0], c, 0, A::ROOT);
          else if (mode == 4)
            s.decode_animation_cache(f.hp[0], 1);
          else if (mode == 5)
            s.start_animation_cache(999, c);
          else
            s.start_animation_cache(f.hp[0], c);
        },
        mode == 1   ? 5
        : mode == 2 ? 10
                    : 20);
    f.prime();
    rejects<std::exception>([&] { f.tick(); },
                            "phase identity service and inactive gates");
    check(f.session.snapshot().animation_cache->operations.empty(),
          "preflight publishes no operation");
  }
  {
    Fixture f;
    enable(f);
    rejects([&] { f.session.advance_animation_cache(f.hp[0]); },
            "outside task refuses");
    check(f.session.snapshot().failed, "outside operation latches");
  }
}
void prefixes_and_budgets() {
  for (unsigned mode = 0; mode < 5; ++mode) {
    Fixture f;
    S::AnimationCacheLimits l;
    if (mode == 0)
      l.calls_per_tick = 1;
    if (mode == 1)
      l.history = 1;
    if (mode == 2)
      l.runtime.max_loop_subtractions = 1;
    if (mode == 3)
      l.runtime.decoder.key_records = 0;
    enable(f, l);
    auto clip = R::animation_decoder::parse_clip(bytes());
    if (mode == 2)
      clip.length = 1;
    auto c = f.session.add_animation_cache_clip(clip);
    f.task([&](auto &s) {
      auto t = s.start_animation_cache(f.hp[0], c, 0, mode == 2 ? A::LOOP : 0);
      if (mode == 3)
        s.decode_animation_cache(f.hp[0], t);
      else {
        s.advance_animation_cache(f.hp[0], mode == 2 ? 0x40800000 : R::ONE);
        if (mode == 4)
          s.advance_animation_cache(f.hp[0]);
      }
    });
    f.prime();
    rejects<std::exception>([&] { f.tick(); }, "host budgets latch failures");
    auto failed = f.session.snapshot();
    const auto text = observed(failed);
    check(failed.failed && failed.animation_cache->depth == 0,
          "failure drops depth without rollback");
    check(failed.animation_cache->operations.front().completed,
          "successful start prefix retained");
    if (mode == 2 || mode == 3)
      check(!failed.animation_cache->operations.back().completed,
            "runtime failure has incomplete operation");
    if (mode == 3)
      check(failed.animation_cache->runtime.decoder.tracks[0].value.slot != 0 &&
                failed.animation_cache->runtime.decoder.slots[1].clip == c,
            "decoder initialization before key budget remains");
    rejects<S::Failed>([&] { f.tick(); }, "failed Session cannot retry");
    check(observed(f.session.snapshot()) == text,
          "retry leaves entire observation unchanged");
  }
  {
    Fixture f;
    enable(f);
    auto c = f.session.add_animation_cache_clip(bytes());
    f.task([&](auto &s) {
      auto t = s.start_animation_cache(f.hp[0], c);
      s.decode_animation_cache(f.hp[0], t);
      throw std::runtime_error("later authored consumer");
    });
    f.prime();
    rejects<std::exception>([&] { f.tick(); },
                            "later consumer failure latches");
    check(f.session.snapshot().animation_cache->operations.back().completed &&
              binding(f.session.snapshot()).last_decoded.has_value(),
          "successful decode remains after later failure");
  }
}
void retired_and_ready() {
  for (unsigned mode = 0; mode < 3; ++mode) {
    Fixture f;
    enable(f);
    auto c = f.session.add_animation_cache_clip(bytes());
    f.task([&](auto &s) {
      s.with_world([&](auto &w) {
        if (mode == 2)
          w.frame = -1;
        else {
          w.destroy(*f.objects[0]);
          if (mode == 1)
            w.new_object(0x400, {});
        }
      });
      s.start_animation_cache(f.hp[0], c);
    });
    f.prime();
    rejects<std::exception>(
        [&] { f.tick(); },
        "retired reused and invalid frame identities refuse");
    check(f.session.snapshot().animation_cache->operations.empty(),
          "retirement preflight leaves Runtime untouched");
  }
  {
    S::ObjectBindings b;
    b.is_frozen = {"frozen", [] { return true; }};
    Fixture f(0, 1, b);
    enable(f);
    auto c = f.session.add_animation_cache_clip(bytes());
    f.task([&](auto &s) {
      auto t = s.start_animation_cache(f.hp[0], c);
      s.advance_animation_cache(f.hp[0]);
      s.decode_animation_cache(f.hp[0], t);
    });
    f.prime();
    f.tick();
    check(f.runs == 0 &&
              f.session.snapshot().animation_cache->operations.size() == 3,
          "frozen World still admits explicit ready tasks");
  }
  {
    Fixture f;
    enable(f);
    f.task([](auto &) {});
    f.session.submit(sample(), 0, 0, 1, true);
    f.session.set_level_pending(true);
    f.session.submit(sample());
    check(f.session.snapshot().animation_cache->operations.empty() &&
              f.session.snapshot().animation_cache->calls_this_tick == 0,
          "no_sim pending no-ready preserve cache accounting");
  }
}
void consumer_failure_after_decode() {
  Fixture f(0, 2);
  enable(f);
  auto c = R::animation_decoder::parse_clip(bytes());
  c.events[0] = {0, 1};
  const auto clip = f.session.add_animation_cache_clip(c);
  f.task([&](auto &s) {
    s.with_world([](auto &w) {
      const auto count = w.handles.count_free();
      for (std::size_t i = 1; i < count; ++i)
        w.handles.add(w.always_draw, nullptr);
    });
    auto t = s.start_animation_cache(f.hp[0], clip);
    auto event = s.advance_animation_cache(f.hp[0]);
    s.decode_animation_cache(f.hp[0], t);
    if (event & 1)
      s.take_damage(f.hp[0], R::ONE, f.hp[1]);
  });
  f.prime();
  rejects<R::HandleError>([&] { f.tick(); },
                          "real authored HP consumer fails after decode");
  auto s = f.session.snapshot();
  check(s.animation_cache->operations.back().completed &&
            binding(s).last_decoded.has_value(),
        "completed decode survives later HP failure");
  check(s.failed && s.failure_operation == "take_damage" &&
            !s.hp->adapter.model.writes.empty() && s.world->handle_free == 0,
        "actual HP prefix remains after second allocation failure");
}
void serializer_refusal() {
  S::Session s(config());
  auto v = s.snapshot();
  v.animation_cache.emplace();
  rejects<std::runtime_error>(
      [&] { champions::test::session_trace::snapshot_base_fields(v); },
      "legacy serializer refuses active cache");
  Fixture f;
  enable(f);
  R::detail::Checkpoint a;
  rejects<std::runtime_error>(
      [&] { R::detail::CheckpointAccess::run(f.session, a); },
      "closed checkpoint explicitly refuses cache");
  check(a.bytes.empty(), "checkpoint refusal precedes serialization");
}
} // namespace
int main() {
  try {
    ready_and_owned();
    setup_and_phase();
    prefixes_and_budgets();
    retired_and_ready();
    consumer_failure_after_decode();
    serializer_refusal();
    std::cout << "Session animation cache API checks: " << checks
              << "; failures: 0\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "after " << checks << " checks: " << e.what() << '\n';
    return 1;
  }
}
