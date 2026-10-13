#include "champions/player.hpp"
#include <iostream>
namespace P = champions::simulation::rta::player;
void require_at(bool b, int line) {
  if (!b)
    throw std::runtime_error("player host assertion at line " + std::to_string(line));
}
#define require(b) require_at((b), __LINE__)
template <class E, class F> void refuses(F f) {
  try {
    f();
  } catch (const E &) {
    return;
  }
  throw std::runtime_error("missing refusal");
}
int main() {
  try {
    P::Memory m;
    P::Tick t(m, 0x10000);
    refuses<P::MissingHandler>([&] { t.run(); });
    require(t.calls.size() == 1 && t.writes.empty());
    P::Tick z(m, 0x10000, P::Tick::Policy::ReferenceZero);
    refuses<P::Unsupported>([&] { z.run(); });
    require(m.load(0x104c4, 4) == 0 && !z.writes.empty());
    refuses<P::ResourceLimit>([&] { P::Tick bad(m, 0xfffffff0); });
    refuses<P::ResourceLimit>([&] { t.skill_rank(0, 123); });
    refuses<P::ResourceLimit>([&] { t.can_block(0x80000000, 0); });
    require(t.passive_level(0xffffffff, 0) == 0);
    refuses<P::ResourceLimit>([&] { t.idle_fidget(0x10000, UINT64_MAX); });
    m.poke(0x1047f, 1, 1);
    refuses<P::ResourceLimit>([&] { t.try_block_move(0x10000, UINT64_MAX); });
    m.poke(0x1047f, 1, 0);
    t.callbacks[P::Service::DialogOpen] = [](P::Tick &self, const auto &) {
      self.callbacks.erase(P::Service::DialogOpen);
      self.mem.poke(0x20000, 4, 99);
      return 1u;
    };
    require(t.choose_pad() == 0x8667f0 && m.load(0x20000, 4) == 99);
    refuses<P::MissingHandler>([&] { t.choose_pad(); });
    t.callbacks[P::Service::DialogOpen] = [](P::Tick &self, const auto &) {
      self.choose_pad();
      return 0u;
    };
    refuses<P::ResourceLimit>([&] { t.choose_pad(); });
    t.callbacks[P::Service::DialogOpen] = [](P::Tick &, const auto &) {
      return 1u;
    };
    require(t.choose_pad() == 0x8667f0);
    P::Memory recovery;
    P::Tick rt(recovery, 0x10000);
    recovery.poke(0x4d8af0 + 0xac, 4, 0x3f800000);
    recovery.poke(0x4d8af0 + 0xc4, 4, 0x42c80000);
    recovery.poke(0x4d8af0 + 0x4b08, 2, 0xffff);
    refuses<P::MissingHandler>([&] { rt.regen_mana(0x10000); });
    require(rt.calls.size() == 1 && rt.calls[0].service == P::Service::BlessedType);
    // Timed-list refusal retains earlier stores. Repeated wanted id 0 has
    // expired by the second entry but the missing terminator still refuses.
    recovery.poke(0x4d8af0 + 0x43f0, 4, 0x30000);
    recovery.poke(0x30000, 4, 1);
    recovery.poke(0x3002a, 2, 1);
    refuses<P::ResourceLimit>([&] { rt.tick_enchant_timers(0x10000); });
    require(recovery.load(0x30024, 2) == 0xffff && rt.writes.size() == 8);
    refuses<P::Unsupported>([&] { rt.use_potion(2); });
    recovery.poke(0x4d8af0 + 0xb0, 4, 0x42c80000);
    recovery.poke(0x4d8af0 + 0x3740, 4, 0x7fffffff);
    refuses<P::ResourceLimit>([&] { rt.use_potion(0); });
    require(recovery.load(0x4d8af0 + 0xb8, 4) != 0);
    // Also cap the dynamically reread bag count after consumption. Callback
    // returns raw FPU bits; its first log entry survives a later refusal.
    P::Memory load;
    P::Tick lt(load, 0x10000, P::Tick::Policy::ReferenceZero);
    load.poke(0x4d8af0 + 0xac, 4, 0x3f800000);
    load.poke(0x4d8af0 + 0xb0, 4, 0x42c80000);
    load.poke(0x4d8af0 + 0x3740, 4, 1);
    load.poke(0x4d8af0 + 0x2544, 2, 2);
    load.poke(0x4d8af0 + 0x1330, 4, 0x7fffffff);
    lt.callbacks[P::Service::ItemWeight] = [](P::Tick &, const auto &) { return 0x3f800000u; };
    refuses<P::ResourceLimit>([&] { lt.use_potion(0); });
    require(load.load(0x4d8af0 + 0x2544, 2) == 1 && !lt.calls.empty());
    P::Memory casting;
    P::Tick ct(casting, 0x10000);
    refuses<P::MissingHandler>([&] { ct.hands_midpoint(0x10000, true); });
    require(ct.calls.size() == 1 && ct.calls.back().service == P::Service::AttachPoint);
    ct.callbacks[P::Service::AttachPoint] = [](P::Tick &, const auto &) { return 0u; };
    refuses<P::Error>([&] { ct.hands_midpoint(0x10000, true); });
    ct.callbacks[P::Service::AttachPoint] = [](P::Tick &self, const auto &) -> P::Result {
      self.callbacks[P::Service::AttachPoint] = [](P::Tick &, const auto &) -> P::Result {
        return champions::simulation::rta::Vec3{4, 5, 6};
      };
      return champions::simulation::rta::Vec3{1, 2, 3};
    };
    auto hands = ct.hands_midpoint(0x10000, false);
    require(hands.first[0] == 1 && hands.second[0] == 4);
    // Arc result resources refuse after earlier can-use and hand callbacks;
    // no mana/recast writes are fabricated on failure.
    P::Tick cz(casting, 0x10000, P::Tick::Policy::ReferenceZero);
    casting.poke(0x70020, 2, 1);
    casting.poke(0x41b174, 4, 0x71000);
    cz.callbacks[P::Service::SkillCanUse] = [](P::Tick &, const auto &) { return 1u; };
    cz.callbacks[P::Service::CollectArc] = [](P::Tick &, const auto &) -> P::Result {
      return std::vector<std::uint32_t>(4097, 1);
    };
    refuses<P::ResourceLimit>([&] { cz.cast_skill(0x10000, 0x70000); });
    require(cz.writes.empty() && cz.calls.back().service == P::Service::CollectArc);
    cz.callbacks[P::Service::CollectArc] = [](P::Tick &, const auto &) { return 1u; };
    refuses<P::Error>([&] { cz.cast_skill(0x10000, 0x70000); });
    casting.poke(0x1058c, 4, 2);
    refuses<P::ResourceLimit>([&] { cz.weapon_swap_input(0x10000, UINT64_MAX, 0); });
    cz.callbacks[P::Service::FindTrack] = [](P::Tick &, const auto &) { return 0x7fffffffu; };
    refuses<P::ResourceLimit>([&] { cz.update_cast(0x10000, 0x41bfd8); });
    require(cz.writes.empty());
    // Recall is the reference's successful control exit, before any callback.
    P::Memory action;
    P::Tick at(action, 0x10000);
    action.poke(0x41b51d, 1, 128);
    refuses<P::Leave>([&] { at.state_block_move(); });
    require(at.calls.empty() && at.writes.empty());
    action.poke(0x41b51d, 1, 0);
    action.poke(0x104a8, 4, 7);
    at.callbacks[P::Service::SoundStop] = [](P::Tick &self, const auto &) {
      self.callbacks.erase(P::Service::SoundStop);
      return 0u;
    };
    at.stop_loop_sound();
    require(action.load(0x104a8, 4) == UINT32_MAX);
    action.poke(0x104a8, 4, 8);
    refuses<P::MissingHandler>([&] { at.stop_loop_sound(); });
    require(action.load(0x104a8, 4) == 8); // No post-callback store on failure.
    at.pad = UINT64_MAX;
    refuses<P::ResourceLimit>([&] { at.move(1); });
    action.poke(0x10024, 4, 0xffffffff);
    refuses<P::ResourceLimit>([&] { at.anim_name(4); });
    refuses<P::ResourceLimit>([&] { at.top_track(INT64_MIN); });
    refuses<P::ResourceLimit>([&] { at.top_track(INT64_MAX); });
    refuses<P::ResourceLimit>([&] { at.anim_name(INT64_MAX); });
    action.poke(0x4d8af0 + 0x4a60, 2, 2);
    require(!at.selected_ability().has_value());
    action.poke(0x103c8, 4, 0x2fe1b1e);
    auto zero_record = at.record();
    action.poke(P::Address(zero_record) + 0x4a60, 2, 0);
    action.poke(P::Address(zero_record) + 0x4a84, 1, 126);
    require(at.selected_ability().has_value() && *at.selected_ability() == 0);
    action.poke(0x103c8, 4, 0);
    // A malformed attach result retains aura stores and the recorded call.
    P::Tick az(action, 0x10000, P::Tick::Policy::ReferenceZero);
    action.poke(0x104a8, 4, UINT32_MAX);
    az.callbacks[P::Service::AttachPoint] = [](P::Tick &, const auto &) { return 0u; };
    refuses<P::Error>([&] { az.state_dead(); }); // zero frame equals supplied zero double
    require(az.writes.size() == 2 && az.calls.back().service == P::Service::AttachPoint);
    std::cout << "player host policies pass\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
