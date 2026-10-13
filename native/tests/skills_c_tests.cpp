#include "champions/skills_c.hpp"
#include <iostream>
namespace sk = champions::simulation::rta::skills_c;
using champions::simulation::rta::ONE;
namespace {
void require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
template <class F> void refuses(F f) {
  try {
    f();
  } catch (const sk::Unsupported &) {
    return;
  }
  throw std::runtime_error("expected explicit refusal");
}
void base(sk::Model &m) {
  m.mem.poke(0x1060, 4, 0x2000);
  m.mem.poke(0x2000 + 8 * 10 + 4, 4, 0x3c85fc);
  m.mem.poke(0x2000 + 8 * 14 + 4, 4, 0x3c88dc);
}
} // namespace
int main() {
  try {
    sk::Model m;
    base(m);
    // Active entry points never silently succeed for the unsupported range.
    for (unsigned id = 69; id <= 101; ++id)
      refuses([&] { m.cast_or_run(id); });
    refuses([&] { m.execute(static_cast<sk::Operation>(30), {0x1000}); });
    refuses([&] { m.execute(sk::Operation::Stat, {0x1000}); });
    refuses([&] { m.skill_call(0x1000, 16, {}); });
    refuses([&] { m.skill_call(UINT64_MAX, 10, {}); });
    // Unknown bodies record the callback prefix then require a handler.
    refuses([&] { m.skill_call(0x1000, 11, {0, 20, 10}); });
    require(m.calls.size() == 1 && m.calls[0].slot == 11,
            "missing callback prefix");
    // A handler can replace itself without invalidating its active callable.
    m.callbacks[11] = [](sk::Model &self, const auto &) {
      self.mem.store(0x3000, 2, 0x1234);
      self.callbacks[11] = [](sk::Model &, const auto &) {
        return std::vector<std::uint32_t>{8};
      };
      return std::vector<std::uint32_t>{7};
    };
    require(m.skill_call(0x1000, 11, {0, 20, 10})[0] == 7,
            "active handler changed");
    require(m.skill_call(0x1000, 11, {0, 20, 10})[0] == 8,
            "replacement not used");
    require(m.mem.load(0x3000, 2) == 0x1234 && m.mem.log().size() == 1,
            "callback write lost");
    // Recursion refuses explicitly; the depth guard restores after failure.
    m.callbacks[11] = [](sk::Model &self, const auto &a) {
      return self.skill_call(a[0], 11, {0, 20, 10});
    };
    refuses([&] { m.skill_call(0x1000, 11, {0, 20, 10}); });
    m.callbacks[11] = [](sk::Model &, const auto &) {
      return std::vector<std::uint32_t>{9};
    };
    require(m.skill_call(0x1000, 11, {0, 20, 10})[0] == 9,
            "depth not restored");
    // Queries refuse cyclic lists, and a failed runtime call releases depth.
    m.mem.poke(0x5000, 4, 0x5100);
    m.mem.poke(0x5100, 4, 0x5100);
    refuses([&] { m.execute(sk::Operation::HandleItems, {0x5000}); });
    m.mem.poke(0x5000, 4, 0);
    require(m.execute(sk::Operation::HandleItems, {0x5000}).empty(),
            "empty list/depth recovery");
    require(m.execute(sk::Operation::SpellBase,
                      {0x6000, 256, 0x3000, 0x1000, 0x1234}).empty(),
            "base constructor return");
    require(m.mem.load(0x6000,1) == 0 && m.mem.load(0x6014,4) == 0x1234,
            "level narrowing/final table");
    require(m.draws == 1, "constructor shared draw count");
    m.execute(sk::Operation::SpellBall, {0x6000});
    require(m.draws == 1, "own RNG changed shared count");
    // Reference this-adjustment wraps raw32; subsequent memory spans refuse.
    m.mem.poke(0x2050, 2, 0xfff0);
    m.external.insert(10);
    m.callbacks[10] = [](sk::Model &, const auto &a) {
      require(a[0] == 0xff0, "signed this adjustment");
      return std::vector<std::uint32_t>{17};
    };
    require(m.skill_call(0x1000, 10, {0, 2})[0] == 17,
            "forced callback failed");
    require(m.execute(sk::Operation::ManaDefault, {0x1000, 2})[0] == ONE,
            "default mana wrong");
    std::cout
        << "skills-c API refusals, callback lifetime and recovery passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
