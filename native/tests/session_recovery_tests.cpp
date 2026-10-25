#include "champions/frame_clock.hpp"
#include "champions/native_save.hpp"
#include "session_recovery_recipes.hpp"
#include "session_recovery_trace.hpp"
#include <functional>
#include <iostream>
namespace R = champions::simulation::rta;
namespace C = R::session_control;
namespace N = R::native_save;
namespace T = champions::test::recovery_trace;
unsigned checks = 0;
std::uint64_t digest = UINT64_C(14695981039346656037);
void observe(const std::string &s) {
  for (unsigned char c : s) {
    digest ^= c;
    digest *= UINT64_C(1099511628211);
  }
  digest ^= 10;
  digest *= UINT64_C(1099511628211);
}
void check(bool ok, const char *s) {
  ++checks;
  if (!ok)
    throw std::runtime_error(s);
}
void refuses(const std::function<void()> &f) {
  bool failed = false;
  try {
    f();
  } catch (const std::exception &) {
    failed = true;
  }
  check(failed, "expected refusal");
}
C::Config config(unsigned f = 0) {
  C::Config c;
  c.model = R::Fpu::Model(f / 4);
  c.mul_bias = f & 2;
  c.div_nearest = f & 1;
  c.movement = C::MovementPolicy::DryCreature;
  c.max_ready_ticks = 256;
  c.recovery.emplace();
  return c;
}
void equal(N::Scene &a, N::Scene &b) {
  auto x = T::control(a.snapshot()), y = T::control(b.snapshot());
  if (x != y) {
    auto k =
        std::mismatch(x.begin(), x.end(), y.begin(), y.end()).first - x.begin();
    std::cerr << "difference at " << k << " of " << x.size() << "/" << y.size()
              << "\n"
              << x.substr(k > 80 ? k - 80 : 0, 200) << "\n"
              << y.substr(k > 80 ? k - 80 : 0, 200) << "\n";
  }
  check(x == y, "complete state mismatch");
  observe(x);
}
struct Fixture {
  R::session::Session session;
  R::session::HpId hp = 0;
  std::uint64_t serial = 0;
  std::uint32_t address = 0;
  Fixture(R::session_recovery::Initial initial = {},
          std::size_t history = 65536, bool twice = false, bool reuse = false,
          bool alias = false)
      : session([] {
          R::session::Config c;
          c.network_mode = 1;
          return c;
        }()) {
    R::session::ObjectBindings callbacks;
    callbacks.policy = R::objects::CallbackPolicy::ReferenceBase;
    session.enable_objects(17, callbacks);
    session.with_world([&](auto &w) {
      auto *o = w.new_object(0x500, {0, 0, 0});
      o->flags |= 0x20;
      serial = o->serial;
      address = o->addr;
      w.run_every_frame(*o);
    });
    R::session::HpLimits limits;
    limits.memory.history_entries = history;
    session.enable_hp(limits);
    hp = session.bind_hp(serial, address,
                         {initial.health, alias ? 0x100000u : 0u, {}});
    if (alias) {
      session.enable_terrain({});
      session.enable_creature_movement();
      R::session::CreatureInitial creature;
      creature.base_table = R::session_recovery::RECORD;
      session.bind_creature_mover(hp, creature);
    }
    session.enable_recovery(hp, initial);
    session.register_object_task(
        0xf5000002, "authored_recovery", 20, 0,
        [this, twice, reuse](auto &running) {
          if (reuse)
            running.with_world([&](auto &w) {
              w.destroy(*w.objects.at(address), 3);
              auto *replacement = w.new_object(0x500, {0, 0, 0});
              check(replacement->addr == address, "address was not reused");
            });
          running.recover_player(hp);
          if (twice)
            running.recover_player(hp);
        });
  }
  void submit(bool no_sim = false) {
    session.submit({0, 0, 128, 128, 128, 128, 0, 0}, 0, 0, 1, no_sim);
  }
};
void direct_checks() {
  {
    Fixture f;
    f.session.set_level_pending(true);
    for (unsigned k = 0; k < 3; ++k)
      f.submit();
    check(f.session.snapshot().recovery->writes.empty(),
          "level pending recovered");
    f.session.set_level_pending(false);
    for (unsigned k = 0; k < 3; ++k)
      f.submit(true);
    check(f.session.snapshot().recovery->writes.empty(), "no_sim recovered");
    f.submit();
    check(!f.session.snapshot().recovery->writes.empty(),
          "ready recovery missing");
  }
  {
    Fixture f({}, 1);
    f.submit();
    refuses([&] { f.submit(); });
    auto s = f.session.snapshot();
    observe(T::observation(s));
    check(s.failed && s.recovery->phase == "regen_health",
          "failure phase lost");
    check(s.recovery->writes.size() == 2 &&
              s.hp->adapter.model.stores.size() == 1,
          "failure write/store prefix");
    check(s.recovery->writes[0].value == 0x42480a3d &&
              s.recovery->writes[1].value == 0x424c0a3d,
          "independent Python health failure prefix anchor");
    check(f.session.observe_actor(f.hp).hp == 0x42480a3d,
          "completed health not published on failure");
    auto before = T::observation(s);
    refuses([&] { f.submit(); });
    check(before == T::observation(f.session.snapshot()),
          "failure retried recovery");
  }
  {
    Fixture f({}, 3);
    f.submit();
    refuses([&] { f.submit(); });
    auto s = f.session.snapshot();
    observe(T::observation(s));
    check(s.failed && s.recovery->phase == "regen_mana",
          "mana failure phase lost");
    check(s.recovery->writes.size() == 4 && s.recovery->calls.size() == 2 &&
              s.hp->adapter.model.stores.size() == 3,
          "mana failure ordered prefix");
    check(s.recovery->writes.back().address ==
                  R::session_recovery::RECORD + 0xc0 &&
              s.recovery->writes.back().value == 0x41a00a3d,
          "independent Python mana prefix anchor");
    check(f.session.observe_actor(f.hp).hp == 0x424c0a3d &&
              s.hp->adapter.model.backing.at(R::session_recovery::RECORD +
                                             0xb4) == 0,
          "completed health not retained across mana failure");
  }
  {
    Fixture f({}, 65536, true);
    f.submit();
    refuses([&] { f.submit(); });
    check(f.session.failed() &&
              f.session.snapshot().recovery->calls_this_tick == 1,
          "duplicate ready recovery accepted");
  }
  {
    Fixture f({}, 65536, false, true);
    f.submit();
    refuses([&] { f.submit(); });
    check(f.session.snapshot().recovery->writes.empty(),
          "replacement recovered stale generation");
  }
  {
    R::session_recovery::Initial i;
    i.health = 0;
    i.pending_health = 0x41200000;
    Fixture f(i);
    f.submit();
    f.submit();
    check(f.session.snapshot().recovery->writes.empty(),
          "dead recovery wrote fields");
  }
  {
    Fixture f;
    f.session.with_world(
        [&](auto &w) { w.objects.at(f.address)->flags |= 0x20000; });
    f.submit();
    f.submit();
    check(f.session.snapshot().recovery->writes.empty(),
          "copy recovery wrote fields");
  }
  refuses([] { Fixture f({}, 65536, false, false, true); });
  {
    Fixture f;
    refuses([&] { f.session.recover_player(f.hp); });
    check(f.session.snapshot().recovery->writes.empty(),
          "out-of-task recovery wrote fields");
  }
}
void render_checks() {
  std::string expected;
  for (auto fps : {30u, 60u, 144u, 240u}) {
    N::Scene scene(config());
    champions::presentation::rta::FrameClock clock;
    unsigned tick = 0;
    for (unsigned frame = 1; frame <= fps; ++frame) {
      const auto now = std::uint64_t(frame) * 1000000 / fps;
      const auto previous = std::uint64_t(frame - 1) * 1000000 / fps;
      clock.advance(now - previous, [&] {
        scene.advance(tick++ % 8 == 0 ? 17 : 0);
        return true;
      });
    }
    check(tick == 60, "render schedule submission count");
    auto actual = T::control(scene.snapshot());
    if (expected.empty())
      expected = actual;
    else
      check(expected == actual, "render rate changed authoritative trace");
  }
}
int main() {
  try {
    direct_checks();
    render_checks();
    for (unsigned f = 0; f < 16; ++f) {
      N::Scene a(config(f));
      check(a.observation().hp == 0x42480000, "supplied HP");
      a.advance(17);
      check(a.observation().ready_ticks == 0, "pending input recovered");
      a.set_paused(true);
      a.advance(31);
      for (bool indexed : {false, true}) {
        auto bytes = indexed ? a.save_indexed_snapshot() : a.save();
        check(bytes[8] == (indexed ? 4 : 2), "new version missing");
        auto b = N::Scene::load(bytes, "rta");
        equal(a, *b);
        check(b->observation().paused, "pause lost");
        a.set_paused(false);
        b->set_paused(false);
        for (auto command : {0u, 17u, 17u, 0u, 16u, 0u}) {
          a.advance(command);
          b->advance(command);
          equal(a, *b);
        }
        a.set_paused(true);
      }
      auto fast = N::Scene::load(a.save_indexed_snapshot(), "rta");
      check(!fast->history_pending(), "recovery load left unchecked history");
      a.set_paused(false);
      fast->set_paused(false);
      for (unsigned k = 0; k < 5; ++k) {
        a.advance(k & 1 ? 0 : 17);
        fast->advance(k & 1 ? 0 : 17);
        check(!fast->history_pending(),
              "recovery continuation lost owned history");
      }
      equal(a, *fast);
      auto bad = a.save();
      bad[8] = 99;
      const auto before = T::control(a.snapshot());
      refuses([&] { N::Scene::load(bad, "rta"); });
      check(before == T::control(a.snapshot()), "invalid load changed scene");
      auto bad_snapshot = a.save_indexed_snapshot();
      bad_snapshot.back() ^= 1;
      refuses([&] { N::Scene::load(bad_snapshot, "rta"); });
      check(before == T::control(a.snapshot()),
            "corrupt snapshot changed scene");
      auto bad_history = a.save_indexed_snapshot();
      const auto u32 = [&](std::size_t at) {
        std::uint32_t v = 0;
        for (unsigned n = 0; n < 4; ++n)
          v |= std::uint32_t(bad_history[at + n]) << (8 * n);
        return v;
      };
      const auto history_at = 56 + u32(24) + u32(32);
      bad_history.at(history_at) ^= 1;
      refuses([&] { N::Scene::load(bad_history, "rta"); });
      check(before == T::control(a.snapshot()),
            "corrupt history changed scene");
      a.reset();
      check(a.observation().hp == 0x42480000 &&
                a.observation().pending_health == 0x41200000,
            "reset lost setup");
      a.stop();
      const auto stopped = T::control(a.snapshot());
      a.advance(16);
      check(stopped == T::control(a.snapshot()), "stop advanced recovery");
    }
    for (unsigned f = 0; f < 16; ++f)
      for (unsigned variant : {1u, 6u, 7u, 11u, 12u, 13u}) {
        auto cfg = champions::test::recovery_recipes::config(f, variant);
        cfg.recovery->blessed_type_token = 0xf5001234;
        N::Scene source(cfg);
        source.advance(17);
        source.set_paused(true);
        for (const auto &bytes : {source.save(), source.save_snapshot()}) {
          auto loaded = N::Scene::load(bytes, "rta");
          N::Scene uninterrupted(cfg);
          uninterrupted.advance(17);
          uninterrupted.set_paused(true);
          equal(uninterrupted, *loaded);
          uninterrupted.set_paused(false);
          loaded->set_paused(false);
          for (auto command : {0u, 17u, 0u}) {
            uninterrupted.advance(command);
            loaded->advance(command);
            equal(uninterrupted, *loaded);
          }
        }
      }
    auto invalid = config();
    invalid.recovery->equipment[1] = 1;
    refuses([&] { N::Scene a(invalid); });
    invalid = config();
    invalid.recovery->ranks.clear();
    refuses([&] { N::Scene a(invalid); });
    invalid = config();
    invalid.recovery->slot = 1;
    refuses([&] { N::Scene a(invalid); });
    invalid = config();
    invalid.recovery->player_class = -1;
    refuses([&] { N::Scene a(invalid); });
    invalid = config();
    invalid.max_ready_ticks = 512;
    refuses([&] { N::Scene a(invalid); });
    N::Scene budget(config());
    for (unsigned k = 0; k < 257; ++k)
      budget.advance(k & 1 ? 0 : 16);
    check(!budget.observation().failed &&
              budget.observation().ready_ticks == 256,
          "whole recovery budget failed");
    std::cout << checks << " recovery checks PASS FNV1a64=" << std::hex
              << digest << "\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
