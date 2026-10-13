#include "champions/player.hpp"
#include <iostream>
namespace P = champions::simulation::rta::player;
void require(bool b) {
  if (!b)
    throw std::runtime_error("player host assertion");
}
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
    std::cout << "player host policies pass\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
