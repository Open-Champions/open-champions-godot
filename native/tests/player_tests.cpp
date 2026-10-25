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
    // Generic constructor/call/vcall/helper family and shared callback identity.
    P::Memory generic_mem;
    auto named = std::make_shared<P::Tick::NamedCallbacks>();
    P::Tick generic(generic_mem, 0x10000, P::Tick::Policy::RequireHandlers, named, {"delegated"});
    require(generic.P == 0x10000 && &generic.mem == &generic_mem && generic.named_callbacks == named);
    require(generic.calls.empty() && generic.writes.empty() && generic.events.empty() && !generic.draws && !generic.pad && !generic.instant);
    (*named)["arbitrary"] = [&](P::Tick &t, const auto &args) -> P::Result {
      require(&t == &generic && args.size() == 6);
      require(std::get<std::uint32_t>(args[0]) == UINT32_MAX);
      require(std::get<std::uint32_t>(args[1]) == 1);
      require(std::get<std::uint32_t>(args[2]) == UINT32_MAX);
      require(std::get<champions::simulation::rta::Vec3>(args[3])[0] == 0x80000000);
      require(std::holds_alternative<P::StackHandle>(args[4]) && std::holds_alternative<P::NullArgument>(args[5]));
      (*named)["arbitrary"] = [](auto &, const auto &) -> P::Result { return std::int64_t(-7); };
      t.w8(0x90000, 42);
      return P::QueryResult{1, 9};
    };
    auto generic_result = generic.call("arbitrary", {std::int64_t(-1), UINT64_C(0x100000001), UINT64_MAX, champions::simulation::rta::Vec3{0x80000000,1,2}, P::StackHandle{}, P::NullArgument{}});
    require(std::get<P::QueryResult>(generic_result)[1] == 9 && generic.calls[0].name == "arbitrary");
    require(generic.events.size() == 2 && generic.events[0].is_call && !generic.events[1].is_call);
    require(std::get<std::int64_t>(generic.call("arbitrary")) == -7);
    (*named)["raw64"] = [](auto &, const auto &) -> P::Result { return UINT64_MAX; };
    require(std::get<std::uint64_t>(generic.call("raw64")) == UINT64_MAX);
    (*named)["binary_buffer"] = [](auto &, const auto &) -> P::Result { return P::BufferResult{{0,128,255,1}}; };
    require(std::get<P::BufferResult>(generic.call("binary_buffer")).bytes == std::vector<std::uint8_t>({0,128,255,1}));
    (*named)["none"] = [](auto &, const auto &) -> P::Result { return P::NullArgument{}; };
    require(std::holds_alternative<P::NullArgument>(generic.call("none")));
    refuses<P::MissingHandler>([&] { generic.call("missing"); });
    require(generic.calls.back().name == "missing");
    generic_mem.poke(1 + 0x14, 4, 0x91000);
    generic_mem.poke(0x91000 + 8 * 987, 2, 0xfffe);
    (*named)["Authored.v987"] = [](auto &, const auto &args) -> P::Result { require(std::get<std::uint32_t>(args[0]) == UINT32_MAX); return 17u; };
    require(std::get<std::uint32_t>(generic.vcall("Authored",1,0x14,987)) == 17);
    // The unused slot function pointer is never read; adjustment reads precede logs.
    auto call_count = generic.calls.size();
    refuses<P::ResourceLimit>([&] { generic.vcall("Authored",UINT32_MAX,1,0); });
    require(generic.calls.size() == call_count);
    unsigned internal_calls = 0;
    P::Tick::Callback internal = [&](auto &, const auto &args) -> P::Result {
      ++internal_calls; require(std::get<std::int64_t>(args[0]) == -1); return 31u;
    };
    (*named)["delegated"] = [](auto &, const auto &args) -> P::Result { require(std::get<std::uint32_t>(args[0]) == UINT32_MAX); return 47u; };
    require(std::get<std::uint32_t>(generic.helper("delegated", internal, {std::int64_t(-1)})) == 47 && internal_calls == 0);
    generic.external.clear(); call_count = generic.calls.size();
    require(std::get<std::uint32_t>(generic.helper("delegated", internal, {std::int64_t(-1)})) == 31 && internal_calls == 1);
    require(generic.calls.size() == call_count);
    generic.external.insert("delegated"); named->erase("delegated");
    refuses<P::MissingHandler>([&] { generic.helper("delegated", internal, {std::int64_t(-1)}); });
    require(internal_calls == 1);
    (*named)["throw"] = [](auto &t, const auto &) -> P::Result { t.w8(0x90001,99); throw std::logic_error("callback"); };
    refuses<std::logic_error>([&] { generic.call("throw"); });
    require(generic.mem.load(0x90001,1) == 99 && generic.writes.back().value == 99);
    (*named)["recursive"] = [](auto &t, const auto &) -> P::Result { return t.call("recursive"); };
    refuses<P::ResourceLimit>([&] { generic.call("recursive"); });
    refuses<P::ResourceLimit>([&] { generic.call("recursive"); });
    require(std::get<std::int64_t>(generic.call("arbitrary")) == -7);
    refuses<P::ResourceLimit>([&] { generic.call(std::string(257,'x')); });
    generic.external.insert("Player_usePotion");
    (*named)["Player_usePotion"] = [](auto &, const auto &args) -> P::Result { require(args.size() == 5 && std::get<std::uint32_t>(args[4]) == UINT32_MAX); return 93u; };
    require(std::get<std::uint32_t>(generic.use_potion(1)) == 93);
    // Generic names can replace established services without changing service IDs.
    (*named)["Player_animStep"] = [](auto &, const auto &) -> P::Result { return 5u; };
    require(generic.anim_step() == 5);
    require(generic.calls.back().service == P::Service::AnimStep && generic.calls.back().name.empty());
    // Complete player Memory family, using the shared storage without an
    // overlay. Whole missing spans delegate once; mixed reads snapshot bytes.
    std::vector<std::pair<P::Address, std::size_t>> reads;
    P::Memory *owner = nullptr;
    P::Memory delegated({}, [&](P::Address a, std::size_t width) {
      reads.emplace_back(a, width);
      owner->poke(0x90001, 1, 0xee);
      owner->poke(0x90006, 1, 0xab);
      std::uint64_t value = 0;
      for (std::size_t i = 0; i < width; ++i)
        value |= std::uint64_t((a + i + 73) & 255) << (8 * i);
      return value;
    });
    owner = &delegated;
    require(delegated.bytes().empty() && delegated.log().empty());
    delegated.poke(0x90001, 1, 0x11);
    delegated.poke(0x90003, 1, 0x33);
    require(delegated.load(0x90000, 8) == UINT64_C(0x504f4e4d334b1149));
    require(reads.size() == 6 && reads[0].first == 0x90000 && reads[4].first == 0x90006);
    require(delegated.bytes().at(0x90001) == 0xee && delegated.bytes().at(0x90006) == 0xab);
    reads.clear();
    require(delegated.load(0x80000, 8) == UINT64_C(0x504f4e4d4c4b4a49));
    require(reads.size() == 1 && reads[0].second == 8);
    reads.clear();
    require(delegated.load(0, 0) == 0 && reads.size() == 1 && reads[0].second == 0);
    delegated.write_bytes(0x90000, {0,255,128,1,2,3,4,5});
    reads.clear();
    require(delegated.load(0x90000, 8) == UINT64_C(0x050403020180ff00) && reads.empty());
    require(delegated.log().empty());
    delegated.store(0x90001, 2, 0x123456);
    require(delegated.log().size() == 1 && delegated.load(0x90001, 2) == 0x3456);
    delegated.write_bytes(UINT64_MAX, {});
    auto before_bytes = delegated.bytes();
    refuses<P::ResourceLimit>([&] { delegated.write_bytes(UINT32_MAX, {1,2}); });
    require(delegated.bytes() == before_bytes && delegated.log().size() == 1);
    champions::simulation::rta::creature_hp::Limits byte_limits;
    byte_limits.memory_bytes = 2;
    P::Memory byte_limited(byte_limits);
    byte_limited.write_bytes(10, {1,2});
    refuses<P::ResourceLimit>([&] { byte_limited.write_bytes(11, {3,4}); });
    require(byte_limited.load(10, 2) == 0x0201 && byte_limited.log().empty());
    byte_limited.write_bytes(10, {3,4});
    require(byte_limited.load(10, 2) == 0x0403);
    P::Memory callback_failure({}, [&](auto, auto) -> std::uint64_t {
      byte_limited.write_bytes(10, {5,6});
      throw std::logic_error("under failure");
    });
    refuses<std::logic_error>([&] { callback_failure.load(20, 4); });
    require(byte_limited.load(10, 2) == 0x0605 && callback_failure.bytes().empty());
    P::Memory wide_result({}, [](auto, auto) { return UINT64_C(256); });
    refuses<P::Error>([&] { wide_result.load(0, 1); });
    P::Memory *recursive_owner = nullptr;
    P::Memory recursive({}, [&](auto a, auto width) { return recursive_owner->load(a, width); });
    recursive_owner = &recursive;
    refuses<P::ResourceLimit>([&] { recursive.load(0, 1); });
    refuses<P::ResourceLimit>([&] { recursive.load(0, 1); });
    // Independently callable utility family: widths, truncation, binary strings,
    // read/write span refusals and the exact attempted-store failure prefix.
    P::Memory utility;
    P::Tick ut(utility, 0x10000);
    for (unsigned byte = 0; byte < 256; ++byte) {
      auto expected = byte < 128 ? int(byte) : int(byte) - 256;
      require(P::s8(byte) == expected);
      require(P::s8(UINT64_C(0xffffffffffffff00) | byte) == expected);
      ut.w8(0x90003, byte);
      require(ut.u8(0x90003) == byte && ut.signed8(0x90003) == expected);
    }
    require(P::s8(std::uint64_t(-1)) == -1);
    require(P::s8(std::uint64_t(-129)) == 127);
    require(P::s8(std::uint64_t(INT64_MIN)) == 0);
    ut.w16(0x90004, UINT64_MAX);
    ut.w32(0x90006, UINT64_C(0x1234567887654321));
    require(ut.u16(0x90004) == 0xffff && ut.u32(0x90006) == 0x87654321);
    utility.poke(0x90011, 8, UINT64_MAX);
    require(ut.u64(0x90011) == UINT64_MAX && ut.u64(0x90030) == 0);
    require(ut.writes.size() == 258 && ut.events.size() == 258 && utility.log().size() == 258);
    require(ut.writes.back().value == 0x87654321 && ut.events.back().write.value == 0x87654321);
    auto utility_before = utility.bytes();
    refuses<P::ResourceLimit>([&] { ut.w16(UINT32_MAX, 0xffff); });
    refuses<P::ResourceLimit>([&] { ut.w32(UINT32_MAX - 2, 0xffffffff); });
    refuses<P::ResourceLimit>([&] { ut.w8(UINT64_MAX, 1); });
    refuses<P::ResourceLimit>([&] { ut.u8(UINT64_MAX); });
    refuses<P::ResourceLimit>([&] { ut.u16(UINT32_MAX); });
    refuses<P::ResourceLimit>([&] { ut.u64(UINT32_MAX - 6); });
    require(utility.bytes() == utility_before && ut.writes.size() == 258);
    ut.w8(UINT32_MAX, 0xff);
    require(ut.u8(UINT32_MAX) == 255);
    require(ut.cstr(UINT32_MAX, 1) == std::string(1, char(255)));
    refuses<P::ResourceLimit>([&] { ut.cstr(UINT32_MAX, 2); });
    require(ut.cstr(UINT64_MAX, 0).empty() && ut.cstr(UINT64_MAX, -1).empty());
    refuses<P::ResourceLimit>([&] { ut.cstr(UINT64_MAX, 1); });
    for (unsigned i = 0; i < 510; ++i) utility.poke(0x91000 + i, 1, i % 255 + 1);
    require(ut.cstr(0x91000).size() == 256 && ut.cstr(0x91000, 257).size() == 257);
    auto binary = ut.cstr(0x91000, INT64_MAX);
    require(binary.size() == 510 && static_cast<unsigned char>(binary[254]) == 255);
    utility.poke(0x91100, 1, 0);
    require(ut.cstr(0x91000, 510).size() == 256);
    struct NonzeroOverlay : champions::simulation::rta::creature_hp::ReadOverlay {
      bool covers(std::uint32_t) const override { return true; }
      std::uint8_t read_byte(std::uint32_t) const override { return 0xff; }
    };
    refuses<P::Error>([&] { delegated.attach_overlay(std::make_shared<NonzeroOverlay>()); });
    P::Memory string_memory;
    string_memory.attach_overlay(std::make_shared<NonzeroOverlay>());
    refuses<P::Error>([&] { string_memory.write_bytes(0, {1,2}); });
    require(string_memory.bytes().empty() && string_memory.log().empty());
    P::Tick string_tick(string_memory, 0x10000);
    require(string_tick.cstr(0, 1024 * 1024).size() == 1024 * 1024);
    refuses<P::ResourceLimit>([&] { string_tick.cstr(0, 1024 * 1024 + 1); });
    require(string_memory.bytes().empty() && string_memory.log().empty() && string_tick.events.empty());
    // Store logging precedes sparse-backing refusal at both Tick and Memory.
    champions::simulation::rta::creature_hp::Limits tiny;
    tiny.memory_bytes = 0;
    P::Memory limited(tiny);
    P::Tick failed(limited, 0x10000);
    refuses<P::ResourceLimit>([&] { failed.w32(0x90000, UINT64_MAX); });
    require(limited.bytes().empty() && limited.log().size() == 1);
    require(failed.writes.size() == 1 && failed.events.size() == 1);
    require(failed.writes[0].value == UINT32_MAX && limited.log()[0].value == UINT32_MAX);
    P::Memory m;
    P::Tick t(m, 0x10000);
    refuses<P::MissingHandler>([&] { t.run(); });
    require(t.calls.size() == 1 && t.writes.empty());
    P::Tick z(m, 0x10000, P::Tick::Policy::ReferenceZero);
    refuses<P::Unsupported>([&] { z.run_legacy_entry(); });
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
    // Pre-switch is independently usable, with explicit Leave on scripted input.
    // Preserve the old run boundary even after adding the complete top body.
    {
      constexpr auto player = 0x10000u, record = 0x4d8af0u;
      P::Memory memory;
      P::Tick tick(memory, player, P::Tick::Policy::ReferenceZero);
      memory.poke(player + 0x231, 1, 1);
      refuses<P::Leave>([&] { tick.top(); });
      require(memory.load(player + 0x488, 4) == 0 && memory.load(player + 0x4c4, 4) == 0);
      memory.poke(player + 0x231, 1, 0);
      memory.poke(player + 0xc, 4, 8); // Avoid real recovery for this host-boundary test.
      tick.top();
      require(memory.load(player + 0x4c4, 4) == 0x3f800000 && tick.pad == 0x8667f0);
      require(tick.instant == 0 && tick.weapon0 == 0 && tick.R0 == record);
      tick.pad = UINT64_MAX;
      refuses<P::ResourceLimit>([&] { tick.instant_abilities(); });
      require(tick.instant == 0);
      P::Tick strict(memory, player);
      strict.callbacks[P::Service::CreatureRun] = [](P::Tick &self, const auto &) -> P::Result {
        self.mem.store(0x88000, 4, 42); return champions::simulation::rta::Vec3{};
      };
      refuses<P::Error>([&] { strict.top(); });
      require(memory.load(0x88000, 4) == 42 && strict.calls.size() == 1 && strict.writes.empty());
      // New service self-removal retains its call and effects before failure.
      P::Tick trails(memory, player, P::Tick::Policy::ReferenceZero);
      memory.poke(player + 0xc, 4, 0);
      trails.callbacks[P::Service::WeaponTrails] = [](P::Tick &self, const auto &) {
        self.callbacks.erase(P::Service::WeaponTrails);
        self.mem.store(0x88000, 4, 43);
        throw std::logic_error("trail failure");
        return 0u;
      };
      refuses<std::logic_error>([&] { trails.top(); });
      require(memory.load(player + 0x4c4, 4) == 0x3f800000 && memory.load(0x88000, 4) == 43);
      require(trails.calls.back().service == P::Service::WeaponTrails && trails.pad == 0);
      memory.poke(player + 0xc, 4, 8);
      memory.poke(player + 0x4c4, 4, 0);
      refuses<P::Unsupported>([&] { trails.run_legacy_entry(); });
      require(memory.load(player + 0x4c4, 4) == 0);
      // Forced ability takes priority over held movement buttons. Null skill is
      // still delegated; cost callback changes the slot and mana used afterwards.
      P::Tick instant_tick(memory, player, P::Tick::Policy::ReferenceZero);
      memory.poke(player + 0x488, 4, 0x75000);
      memory.poke(0x75025, 1, 7);
      memory.poke(0x75020, 2, 3);
      memory.poke(0x75000, 4, 0x007a7978);
      memory.poke(0x535e30, 4, 0x007a7978);
      memory.poke(0x41bfe0, 4, 0xf000);
      memory.poke(player + 0x4ae, 2, 0);
      instant_tick.pad = 0x41bfd8;
      instant_tick.callbacks[P::Service::SkillCanUse] = [](P::Tick &, const auto &) { return 1u; };
      instant_tick.callbacks[P::Service::SkillCost] = [](P::Tick &self, const auto &) {
        self.mem.poke(0x103c8, 4, 1);
        self.mem.poke(0x4d8af0 + 0x4c30 + 0xc0, 4, 0x3f800000);
        self.mem.poke(0x10488, 4, 0);
        self.mem.poke(0x41b994, 4, 64);
        return 0x3f800000u;
      };
      instant_tick.instant_abilities();
      require(instant_tick.instant == 64 && instant_tick.calls.size() == 3);
      require(std::get<std::uint32_t>(instant_tick.calls[0].args[0]) == 0);
    }
    // Complete run catches only control Leaves. Missing/malformed services and
    // callback exceptions keep their prefixes; the legacy API stays explicit.
    {
      constexpr auto player = 0x10000u, record = 0x4d8af0u;
      P::Memory memory;
      P::Tick tick(memory, player, P::Tick::Policy::ReferenceZero);
      memory.poke(player + 0x231, 1, 1);
      memory.poke(player + 0x488, 4, 77);
      require(tick.run() == "scripted");
      require(memory.load(player + 0x488, 4) == 0 && memory.load(player + 0x4c4, 4) == 0);
      memory.poke(player + 0x231, 1, 0);
      memory.poke(player + 0xc, 4, 8); // Isolate orchestration from recovery tables.
      memory.poke(player + 0x58c, 4, 11);
      memory.poke(0x41b51d, 1, 1);
      memory.poke(record + 0x4bc8, 1, 1);
      require(tick.run() == "recall during block move");
      require(memory.load(player + 0x4c4, 4) == 0x3f800000 && memory.load(record + 0x4bc8, 1) == 1);
      memory.poke(0x41b51d, 1, 0);
      memory.poke(player + 0x58c, 4, 15);
      memory.poke(0x4d8aa0, 4, 0x3f800000);
      memory.poke(record + 0x4a60, 2, 2);
      auto begin = tick.calls.size();
      require(tick.run() == "ability button index");
      require(tick.calls.back().service == P::Service::LightLink2 && tick.calls.size() > begin);
      require(memory.load(record + 0x4bc8, 1) == 1);
      memory.poke(0x4d8aa0, 4, 0);
      memory.poke(player + 0xc, 4, 0x400008);
      memory.poke(record + 0x4bc9, 1, 2);
      memory.poke(record + 0x4bca, 2, 0xffff);
      memory.poke(player + 0x488, 4, 77);
      require(tick.run() == "end");
      require(memory.load(record + 0x4bc8, 1) == 0 && memory.load(record + 0x4bc9, 1) == 1);
      require(memory.load(record + 0x4bca, 2) == 0xfffe && memory.load(player + 0xc, 4) == 8);
      require(memory.load(player + 0x488, 4) == 0);
      memory.poke(0x4d8aa0, 4, 0x3f800000);
      memory.poke(record + 0x4a60, 2, 0);
      // Wrong result shapes fail after synchronous callback effects.
      tick.callbacks[P::Service::LightLink2] = [](P::Tick &self, const auto &) -> P::Result {
        self.mem.store(0x88000, 4, 99); return champions::simulation::rta::Vec3{};
      };
      refuses<P::Error>([&] { tick.run(); });
      require(memory.load(0x88000, 4) == 99 && tick.calls.back().service == P::Service::LightLink2);
      // Callback self-removal is safe; its thrown Leave exits a full run.
      tick.callbacks[P::Service::LightLink2] = [](P::Tick &self, const auto &) -> P::Result {
        self.callbacks.erase(P::Service::LightLink2);
        self.mem.store(0x88000, 4, 100); throw P::Leave("host control exit");
      };
      require(tick.run() == "host control exit" && memory.load(0x88000, 4) == 100);
      tick.callbacks[P::Service::LightLink2] = [](P::Tick &self, const auto &) -> P::Result {
        self.mem.store(0x88000, 4, 101); throw std::logic_error("tail callback failure");
      };
      refuses<std::logic_error>([&] { tick.run(); });
      require(memory.load(0x88000, 4) == 101);
      P::Tick strict(memory, player);
      refuses<P::MissingHandler>([&] { strict.tail(); });
      require(strict.calls.size() == 1 && strict.calls.back().service == P::Service::LightLink2);
      // A malformed pad is refused only after the ordered attach/aim callbacks.
      memory.poke(0x4d8aa0, 4, 0);
      memory.poke(record + 0xac, 4, 0x3f800000);
      memory.poke(player + 0x488, 4, 0);
      tick.pad = UINT64_MAX;
      refuses<P::ResourceLimit>([&] { tick.tail(); });
      require(tick.calls.back().service == P::Service::PlayerAim);
    }
    std::cout << "player host policies pass\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
