// Input-only runner: expectations never enter this process.
#include "champions/player.hpp"
#include "player_transport.hpp"
#include <iostream>
namespace T = champions::test::player_transport;
namespace M = champions::simulation::rta::player;
using T::n;
using T::sequence;
std::string wr(const M::Write &w) {
  return sequence({n(w.address), n(w.size), n(w.value)});
}
std::string call(const M::Call &c) {
  std::vector<std::string> args;
  for (const auto &a : c.args) {
    if (auto v = std::get_if<std::uint32_t>(&a))
      args.push_back(sequence({"0", n(*v)}));
    else {
      auto vec = std::get<champions::simulation::rta::Vec3>(a);
      args.push_back(sequence({"1", n(vec[0]), n(vec[1]), n(vec[2])}));
    }
  }
  return sequence({n(static_cast<unsigned>(c.service)), sequence(args)});
}
std::string snapshot(const M::Tick &t) {
  std::vector<std::string> bytes, log, writes, calls, events;
  for (auto [a, b] : t.mem.bytes())
    bytes.push_back(sequence({n(a), n(b)}));
  for (const auto &w : t.mem.log())
    log.push_back(wr(w));
  for (const auto &w : t.writes)
    writes.push_back(wr(w));
  for (const auto &c : t.calls)
    calls.push_back(call(c));
  for (const auto &e : t.events)
    events.push_back(e.is_call ? sequence({"1", call(e.call)})
                               : sequence({"0", wr(e.write)}));
  return sequence({sequence(bytes), sequence(log), sequence(writes),
                   sequence(calls), sequence(events), n(t.draws), n(t.R0)});
}
void put(M::Memory &m, const T::Value &v, bool store = false) {
  const auto &r = v.array(3);
  auto a = r[0].integer<std::uint32_t>();
  auto s = r[1].integer<unsigned>();
  auto x = r[2].integer<std::uint64_t>();
  if (store)
    m.store(a, s, x);
  else
    m.poke(a, s, x);
}
std::string execute(unsigned config, const std::string &line) {
  namespace R = champions::simulation::rta;
  if (config > 15)
    throw std::runtime_error("configuration range");
  R::set_fpu(static_cast<R::Fpu::Model>(config / 4), config & 2, config & 1);
  auto input = T::Parser(line).parse();
  const auto &root = input.array(3);
  M::Memory mem;
  M::Tick tick(mem, 0x10000);
  for (const auto &p : root[0].bounded_array(512))
    put(mem, p);
  for (const auto &b : root[1].bounded_array(30)) {
    const auto &r = b.array(4);
    auto id = r[0].integer<unsigned>();
    if (id > 29)
      throw std::runtime_error("service range");
    auto service = static_cast<M::Service>(id);
    if (tick.callbacks.count(service))
      throw std::runtime_error("duplicate binding");
    auto result = r[1].integer<std::uint32_t>();
    auto effects = r[2].bounded_array(16);
    auto fail = r[3].integer<unsigned>();
    if (fail > 1)
      throw std::runtime_error("failure flag");
    tick.callbacks[service] = [result, effects, fail](M::Tick &t,
                                                      const auto &) {
      for (const auto &effect : effects) {
        const auto &e = effect.array(4);
        auto log = e[0].integer<unsigned>();
        if (log > 1)
          throw std::runtime_error("effect flag");
        T::Value v;
        v.list = true;
        v.items = {e[1], e[2], e[3]};
        put(t.mem, v, log);
      }
      if (fail)
        throw std::logic_error("injected callback failure");
      return result;
    };
  }
  auto initial = snapshot(tick);
  std::vector<std::string> steps;
  for (const auto &op : root[2].bounded_array(128)) {
    const auto &r = op.bounded_array(6);
    if (r.empty())
      throw std::runtime_error("empty command");
    auto code = r[0].integer<unsigned>();
    std::int64_t result = 0;
    unsigned status = 0;
    try {
      switch (code) {
      case 0:
        op.array(1);
        tick.run();
        result = 1;
        break;
      case 1:
        op.array(1);
        result = tick.choose_pad();
        break;
      case 2:
        op.array(3);
        tick.idle_fidget(r[1].integer<std::uint32_t>(),
                         r[2].integer<std::uint32_t>());
        break;
      case 3:
        op.array(3);
        result = tick.can_block(r[1].integer<std::uint32_t>(),
                                r[2].integer<std::uint32_t>());
        break;
      case 4:
        op.array(3);
        tick.try_block_move(r[1].integer<std::uint32_t>(),
                            r[2].integer<std::uint32_t>());
        break;
      case 5:
        op.array(3);
        result = tick.has_anim(r[1].integer<std::uint32_t>(),
                               r[2].integer<std::uint32_t>());
        break;
      case 6:
        op.array(3);
        result = tick.skill_rank(r[1].integer<std::uint32_t>(),
                                 r[2].integer<std::uint32_t>());
        break;
      case 7:
        op.array(3);
        result = tick.passive_level(r[1].integer<std::uint32_t>(),
                                    r[2].integer<std::uint64_t>());
        break;
      case 10:
        op.array(2);
        tick.regen_health(r[1].integer<std::uint32_t>());
        break;
      case 11:
        op.array(2);
        tick.regen_mana(r[1].integer<std::uint32_t>());
        break;
      case 12:
        op.array(2);
        tick.tick_enchant_timers(r[1].integer<std::uint32_t>());
        break;
      case 13:
        op.array(2);
        tick.use_potion(r[1].integer<std::uint32_t>());
        break;
      case 14:
        op.array(6);
        tick.use_potion_real(r[1].integer<std::uint32_t>(),
                             r[2].integer<std::uint32_t>(),
                             r[3].integer<std::uint32_t>(),
                             r[4].integer<std::uint32_t>(),
                             r[5].integer<std::uint32_t>());
        break;
      case 8:
      case 9: {
        op.array(4);
        T::Value v;
        v.list = true;
        v.items = {r[1], r[2], r[3]};
        put(mem, v, code == 9);
        break;
      }
      default:
        throw std::runtime_error("unknown command");
      }
    } catch (const M::Unsupported &) {
      status = 1;
    } catch (const M::MissingHandler &) {
      status = 2;
    } catch (const M::ResourceLimit &) {
      status = 3;
    } catch (const std::logic_error &) {
      status = 4;
    }
    steps.push_back(sequence({n(status), n(result), snapshot(tick)}));
  }
  return sequence({initial, sequence(steps)});
}
int main() {
  try {
    for (std::string line; std::getline(std::cin, line);) {
      if (line.size() > 65536)
        throw std::runtime_error("input line cap");
      auto sep = line.find(';');
      if (sep == line.npos)
        throw std::runtime_error("missing configuration");
      auto config = T::Parser(line.substr(0, sep)).parse().integer<unsigned>();
      std::cout << execute(config, line.substr(sep + 1)) << '\n';
    }
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
