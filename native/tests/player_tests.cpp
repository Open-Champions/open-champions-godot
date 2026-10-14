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
    // Neutral entry stores survive a missing formatting service. A known
    // war-cry class passes None; an unlisted class passes the stack marker.
    P::Memory neutral;
    P::Tick nt(neutral, 0x10000);
    nt.R0 = 0x4d8af0; nt.pad = 0x41bfd8;
    neutral.poke(0x10482, 1, 1); neutral.poke(0x10488, 4, 0x70000);
    neutral.poke(0x10118, 1, 4); neutral.poke(0x70000, 2, 'o');
    neutral.poke(0x535e30, 2, 'n'); neutral.poke(0x70020, 2, 0xe);
    neutral.poke(0x70024, 2, 0x201); neutral.poke(0x41bfe4, 4, 4);
    refuses<P::MissingHandler>([&] { nt.state_neutral(); });
    require(nt.writes.size() == 2 && nt.calls.back().service == P::Service::Sprintf);
    require(std::holds_alternative<P::NullArgument>(nt.calls.back().args[1]));
    nt.callbacks[P::Service::Sprintf] = [](P::Tick &self, const auto &args) -> P::Result {
      require(std::holds_alternative<P::NullArgument>(args[1]));
      self.callbacks.erase(P::Service::Sprintf);
      return 0x80000u;
    };
    refuses<P::MissingHandler>([&] { nt.neutral_start_ability(0x70000, 0, nt.R0); });
    require(nt.calls.back().service == P::Service::ResFind);
    require(std::get<std::uint32_t>(nt.calls.back().args[1]) == 0x80000);
    neutral.poke(0x4d8af0 + 0x4a86, 1, 4);
    refuses<P::MissingHandler>([&] { nt.neutral_start_ability(0x70000, 0, nt.R0); });
    require(std::holds_alternative<P::StackHandle>(nt.calls.back().args[1]));
    neutral.poke(0x4d8af0 + 0x4a86, 1, 0);
    nt.callbacks[P::Service::Sprintf] = [](P::Tick &, const auto &) -> P::Result {
      return champions::simulation::rta::Vec3{};
    };
    refuses<P::Error>([&] { nt.neutral_start_ability(0x70000, 0, nt.R0); });
    require(nt.calls.back().service == P::Service::Sprintf);
    neutral.poke(0x70025, 1, 0);
    nt.pad = UINT64_MAX;
    auto before = nt.writes.size();
    refuses<P::ResourceLimit>([&] { nt.state_neutral(); });
    require(nt.writes.size() == before + 1 && nt.writes.back().address == 0x10484);
    P::Tick nb(neutral, 0x10000, P::Tick::Policy::ReferenceZero);
    neutral.poke(0x70020, 2, 0x25); neutral.poke(0x10118, 1, 4);
    neutral.poke(0x1044c, 4, 0);
    refuses<P::ResourceLimit>([&] { nb.neutral_start_ability(0x70000, 0, UINT64_MAX); });
    require(nb.writes.size() == 5 && nb.writes.back().address == 0x10448);
    nb.R0 = UINT64_MAX;
    refuses<P::ResourceLimit>([&] { nb.neutral_attack(8, 0); });
    require(nb.writes.size() == 5);
    // Legacy arrow selection writes the record even for a copy. With a real
    // bow and no ammunition it instead emits the missing-ammunition notice.
    neutral.poke(0x70020, 2, 4); neutral.poke(0x70022, 2, 0xabcd);
    neutral.poke(0x4d8af0 + 0x4438, 4, 0x82000);
    neutral.poke(0x4d8af0 + 0x4a86, 1, 0); neutral.poke(0x1000c, 4, 0x20000);
    require(!nb.neutral_start_ability(0x70000, 0, 0x4d8af0));
    require(neutral.load(0x4d8af0 + 0x4a7c, 1) == 1 && neutral.load(0x104ae, 2) == 0xabcd);
    neutral.poke(0x4d8af0 + 0x4a86, 1, 4); neutral.poke(0x1000c, 4, 0);
    require(!nb.neutral_start_ability(0x70000, 0, 0x4d8af0));
    require(neutral.load(0x41b828 + 3*4, 4) == 0x78);
    require(nb.calls.back().service == P::Service::NoticeCount);
    // Every ammunition match is kept, with the last one selected; no original
    // bag bytes or definitions are needed.
    P::Tick nz(neutral, 0x10000, P::Tick::Policy::ReferenceZero);
    nz.R0 = 0x4d8af0; nz.pad = 0x41bfd8;
    neutral.poke(0x4d8af0 + 0x50, 4, 1); neutral.poke(0x4d8af0 + 0x4a86, 1, 4);
    neutral.poke(0x4d8af0 + 0x4410, 4, 0); neutral.poke(0x41bfe0, 4, 8);
    neutral.poke(0x1058c, 4, 2); neutral.poke(0x10488, 4, 0);
    neutral.poke(0x4d8af0 + 0x130, 4, 0x81000);
    neutral.poke(0x4d8af0 + 0x130 + 63*0x48, 4, 0x81000);
    neutral.poke(0x8101c, 1, 8);
    require(!nz.neutral_attack(8, 0));
    require(neutral.load(0x4d8af0 + 0x4410, 4) == 0x4d8af0 + 0x130 + 63*0x48);
    // A pressure callback may change the pad input owner; move uses current
    // Tick.pad exactly as the reference helper does, and refuses its bad span.
    nz.callbacks[P::Service::ButtonValue] = [](P::Tick &self, const auto &) {
      self.pad = UINT64_MAX; return 0u;
    };
    refuses<P::ResourceLimit>([&] { nz.neutral_attack(8, 0); });
    require(!nz.writes.empty() && nz.calls.back().service == P::Service::ButtonValue);
    // Melee keeps all pre-service stores on failure, and separates scalar,
    // query-pair and object-list returns. Missing services never invent hits.
    P::Memory melee;
    P::Tick mt(melee, 0x10000);
    refuses<P::MissingHandler>([&] { mt.state_melee(); });
    require(mt.writes.size() == 1 && mt.calls.back().service == P::Service::AnimStep);
    P::Tick mz(melee, 0x10000, P::Tick::Policy::ReferenceZero);
    mz.pad = 0x41bfd8;
    melee.poke(0x4f8420, 4, 1);
    mz.callbacks[P::Service::FindSpell] = [](P::Tick &, const auto &) { return 0x70000u; };
    mz.callbacks[P::Service::AncestralQuery] = [](P::Tick &, const auto &) { return 0u; };
    refuses<P::Error>([&] { mz.state_melee(); });
    require(mz.calls.back().service == P::Service::AncestralQuery);
    mz.callbacks[P::Service::AncestralQuery] = [](P::Tick &self, const auto &) -> P::Result {
      self.callbacks.erase(P::Service::AncestralQuery);
      return P::QueryResult{1, 0x3f800000};
    };
    melee.poke(0x4f8420, 4, 2);
    auto prefix = mz.writes.size();
    mz.state_melee(); // ReferenceZero supplies the now-missing second pair.
    require(mz.writes.size() > prefix);
    melee.poke(0x4f8420, 4, 0);
    mz.callbacks[P::Service::CollectBox] = [](P::Tick &, const auto &) -> P::Result {
      return std::vector<std::uint32_t>(4097, 1);
    };
    refuses<P::ResourceLimit>([&] { mz.state_melee(); });
    require(mz.calls.back().service == P::Service::CollectBox);
    mz.callbacks[P::Service::CollectBox] = [](P::Tick &, const auto &) { return 0u; };
    refuses<P::Error>([&] { mz.state_melee(); });
    melee.poke(0x4f8420, 4, 65537);
    refuses<P::ResourceLimit>([&] { mz.state_melee(); });
    // Step and attack flags are visible before invalid captured-record refusal.
    prefix = mz.writes.size();
    refuses<P::ResourceLimit>([&] { mz.melee_next_swing(UINT64_MAX, INT32_MIN, 0); });
    require(mz.writes.size() == prefix + 4 && melee.load(0x10460, 1) == 255);
    P::Memory sweep;
    P::Tick st(sweep, 0x10000, P::Tick::Policy::ReferenceZero);
    st.pad = 0x41bfd8;
    sweep.poke(0x4d8af0 + 0x4a86, 1, 2); sweep.poke(0x44fdcc, 4, 200);
    st.callbacks[P::Service::IterBegin] = [](P::Tick &, const auto &) { return 0x73000u; };
    st.callbacks[P::Service::IterNext] = [](P::Tick &, const auto &) { return 0x73000u; };
    refuses<P::ResourceLimit>([&] { st.state_melee(); });
    require(st.calls.back().service == P::Service::IterNext);
    // Third seed is independent of shared gameplay draw accounting.
    P::Memory rng;
    P::Tick dt(rng, 0x10000);
    require(dt.draw3() == (0x3c6ef35fu >> 16));
    require(rng.load(0x404468, 4) == 0x3c6ef35f && dt.draws == 0);
    require(rng.load(0x404464, 4) == 0 && dt.calls.empty());
    // Ranged callback shapes retain prior stores and synchronous failure effects.
    P::Memory ranged;
    P::Tick rz(ranged, 0x10000, P::Tick::Policy::ReferenceZero);
    ranged.poke(0x44fdcc, 4, 11); ranged.poke(0x4d8af0 + 0x43f8, 4, 0x71000);
    ranged.poke(0x71004, 2, 2);
    rz.callbacks[P::Service::AttachPoint] = [](P::Tick &self, const auto &) -> P::Result {
      self.mem.store(0x88000, 4, 123); return champions::simulation::rta::Vec3{};
    };
    refuses<P::Error>([&] { rz.ranged_throw(0x70000, 11); });
    require(ranged.load(0x103cc, 1) == 1 && ranged.load(0x88000, 4) == 123);
    require(rz.calls.back().service == P::Service::AttachPoint && rz.draws == 0);
    rz.callbacks[P::Service::AttachPoint] = [](P::Tick &self, const auto &) -> P::Result {
      self.callbacks.erase(P::Service::AttachPoint); return P::AttachResult{};
    };
    rz.ranged_throw(0x70000, 11);
    require(rz.draws == 2 && ranged.load(0x71004, 2) == 1);
    // Bow requires a single position; throw requires position plus direction.
    rz.callbacks[P::Service::AttachPoint] = [](P::Tick &, const auto &) -> P::Result { return P::AttachResult{}; };
    refuses<P::Error>([&] { rz.ranged_shoot(); });
    rz.callbacks.erase(P::Service::AttachPoint);
    rz.callbacks[P::Service::CollectArc] = [](P::Tick &, const auto &) -> P::Result { return std::vector<std::uint32_t>(4097, 1); };
    auto draws_before = rz.draws;
    refuses<P::ResourceLimit>([&] { rz.ranged_shoot(); });
    require(rz.draws == draws_before + 1 && rz.calls.back().service == P::Service::CollectArc);
    rz.callbacks[P::Service::CollectArc] = [](P::Tick &, const auto &) { return 1u; };
    refuses<P::Error>([&] { rz.ranged_shoot(); });
    rz.R0 = UINT64_MAX;
    auto calls_before = rz.calls.size();
    refuses<P::ResourceLimit>([&] { rz.state_ranged(); });
    require(rz.calls.size() == calls_before);
    rz.R0 = 0x4d8af0; rz.pad = UINT64_MAX;
    ranged.poke(0x44fdcc, 4, 0);
    refuses<P::ResourceLimit>([&] { rz.state_ranged(); });
    require(rz.calls.back().service == P::Service::RecalcLoad);
    // Allocation happens before all constructor reads; a missing constructor
    // and callback self-removal preserve the allocation call and effects.
    P::Memory projectile;
    P::Tick pt(projectile, 0x10000);
    pt.callbacks[P::Service::PoolNew] = [](P::Tick &self, const auto &) {
      self.mem.store(0x88000, 4, 321); self.callbacks.erase(P::Service::PoolNew); return 0x79000u;
    };
    refuses<P::MissingHandler>([&] { pt.missile({}, {}, -1); });
    require(pt.calls.size() == 2 && projectile.load(0x88000, 4) == 321);
    require(std::get<std::uint32_t>(pt.calls.back().args[6]) == UINT32_MAX);
    refuses<P::MissingHandler>([&] { pt.missile({}, {}, 0); });
    // Object zero is an explicit object, distinct from the default player.
    require(pt.obj_rand(0) == (0x3c6ef35fu >> 16));
    require(projectile.load(0x44, 4) == 0x3c6ef35f && projectile.load(0x10044, 4) == 0 && pt.draws == 0);
    require(pt.obj_rand() == (0x3c6ef35fu >> 16));
    require(projectile.load(0x10044, 4) == 0x3c6ef35f && pt.draws == 0);
    refuses<P::ResourceLimit>([&] { pt.obj_rand(UINT32_MAX); });
    // Substring reads are bounded and retain binary bytes and empty needles.
    projectile.poke(0x80000, 4, 0x0062ff61);
    projectile.poke(0x81000, 2, 0x00ff);
    require(pt.strstr(0x80000, 0x81000) == 1 && pt.strstr(0x80000, 0x82000) == 1);
    projectile.poke(0x81000, 2, 'z'); require(pt.strstr(0x80000, 0x81000) == 0);
    {
    // Cast state retains ordered animation/movement and captured ability.
    P::Memory cast;
    P::Tick ct(cast, 0x10000, P::Tick::Policy::ReferenceZero);
    ct.pad = 0x41bfd8;
    cast.poke(0x10488, 4, 0x75000); cast.poke(0x75020, 2, 0);
    cast.poke(0x41b170, 4, 0x76000); cast.poke(0x1047b, 1, 1);
    cast.poke(0x76008, 4, 0); cast.poke(0x1058c, 4, 5);
    ct.callbacks[P::Service::FindTrack] = [](P::Tick &, const auto &) { return std::uint32_t(-1000000000); };
    refuses<P::ResourceLimit>([&] { ct.state_cast(); });
    require(ct.calls.size() == 3 && ct.writes.empty());
    require(ct.calls[0].service == P::Service::AnimStep && ct.calls[1].service == P::Service::Move);
    // Wrong result shape is a refusal after synchronous callback effects.
    ct.callbacks[P::Service::FindTrack] = [](P::Tick &self, const auto &) -> P::Result {
      self.mem.store(0x88000, 4, 123); return champions::simulation::rta::Vec3{};
    };
    refuses<P::Error>([&] { ct.state_cast(); });
    require(cast.load(0x88000, 4) == 123);
    ct.callbacks[P::Service::FindTrack] = [](P::Tick &self, const auto &) {
      self.callbacks.erase(P::Service::FindTrack); return UINT32_MAX;
    };
    cast.poke(0x1047b, 1, 0);
    ct.callbacks[P::Service::SkillCanUse] = [](P::Tick &, const auto &) { return 1u; };
    ct.state_cast(); require(cast.load(0x1047b, 1) == 1);
    require(ct.calls.back().service == P::Service::AnimStart);
    // Missing constructors preserve allocation and its memory effects.
    P::Memory effects;
    P::Tick et(effects, 0x10000);
    et.callbacks[P::Service::PoolNew] = [](P::Tick &self, const auto &) {
      self.mem.store(0x88000, 4, 456); return 0x79000u;
    };
    refuses<P::MissingHandler>([&] { et.light_effect(0x42100000, 17, {1,2,3,4}); });
    require(et.calls.size() == 2 && effects.load(0x88000, 4) == 456);
    require(et.calls.back().args.size() == 12 && std::get<std::uint32_t>(et.calls.back().args[2]) == 17);
    auto call_count = et.calls.size();
    refuses<P::ResourceLimit>([&] { et.light_effect(0, 0, std::vector<std::uint32_t>(65)); });
    require(et.calls.size() == call_count);
    et.callbacks[P::Service::AttachPoint] = [](P::Tick &, const auto &) -> P::Result { return P::AttachResult{}; };
    refuses<P::Error>([&] { et.cast_old_style(0); });
    require(et.writes.empty());
    // Fire nova initialization is lazy; a failing particle retains the built
    // flag and all earlier particle calls, and never gets as far as damage.
    P::Tick nt(effects, 0x10000, P::Tick::Policy::ReferenceZero);
    unsigned particles = 0;
    nt.callbacks[P::Service::ParticleSpawn] = [&](P::Tick &self, const auto &) {
      if (++particles == 7) { self.mem.store(0x88000, 4, 789); throw std::logic_error("particle failure"); }
      return 0u;
    };
    refuses<std::logic_error>([&] { nt.old_fire_nova(0x75000); });
    require(particles == 7 && effects.load(0x4ef3f8, 4) == 1 && effects.load(0x88000, 4) == 789);
    require(nt.calls.size() == 8 && nt.calls.back().service == P::Service::ParticleSpawn && nt.draws == 0);
    // Every dispatch destination agrees with a fresh independent direct call.
    for (unsigned state = 0; state < 17; ++state) {
      P::Memory dm, sm;
      dm.poke(0x1058c, 4, state); sm.poke(0x1058c, 4, state);
      dm.poke(0x4d8af0 + 0x4b08, 2, 0xffff); sm.poke(0x4d8af0 + 0x4b08, 2, 0xffff);
      P::Tick direct(dm, 0x10000, P::Tick::Policy::ReferenceZero);
      P::Tick dispatched(sm, 0x10000, P::Tick::Policy::ReferenceZero);
      direct.pad = dispatched.pad = 0x41bfd8; direct.R0 = dispatched.R0 = 0x4d8af0;
      auto observe = [&](P::Tick &x, bool dispatch) {
        try {
          if (dispatch) x.dispatch_state();
          else switch (state) {
          case 0: x.state_ranged(); break; case 1: x.state_melee(); break;
          case 2: x.state_neutral(); break; case 3: x.state_hit(); break;
          case 4: x.state_dead(); break; case 5: x.state_cast(); break;
          case 6: case 14: x.state_skill(); break; case 7: x.state_jump(); break;
          case 8: x.state_block(); break; case 9: x.state_rush(); break;
          case 10: x.state_pound(); break; case 11: x.state_block_move(); break;
          case 12: x.state_warcry(); break; case 13: x.state_vehicle(); break;
          default: break;
          }
          return 0;
        } catch (const P::Leave &) { return 1; }
      };
      require(observe(direct, false) == observe(dispatched, true));
      require(dm.bytes() == sm.bytes() && direct.draws == dispatched.draws);
      require(direct.events.size() == dispatched.events.size());
      for (std::size_t i = 0; i < direct.events.size(); ++i) {
        const auto &a = direct.events[i], &b = dispatched.events[i];
        require(a.is_call == b.is_call);
        if (a.is_call) { require(a.call.service == b.call.service && a.call.args.size() == b.call.args.size()); }
        else { require(a.write.address == b.write.address && a.write.size == b.write.size && a.write.value == b.write.value); }
      }
    }
    // strcmp is binary, returns only zero/one, and caps each read at 256.
    effects.poke(0x80000, 2, 0xff); effects.poke(0x81000, 2, 0xfe);
    require(et.strcmp(0x80000, 0x81000) == 1 && et.strcmp(0x80000, 0x80000) == 0);
    effects.poke(UINT32_MAX, 1, 1);
    refuses<P::ResourceLimit>([&] { et.strcmp(UINT32_MAX, 0); });
    }
    std::cout << "player host policies pass\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
