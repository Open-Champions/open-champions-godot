// Input-only runner: expectations never enter this process.
#include "champions/player.hpp"
#include "player_transport.hpp"
#include <iostream>
#include <memory>
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
    else if (auto point = std::get_if<champions::simulation::rta::Vec3>(&a)) {
      auto vec = *point;
      args.push_back(sequence({"1", n(vec[0]), n(vec[1]), n(vec[2])}));
    } else args.push_back(sequence({"2", "0"}));
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
M::Result callback_result(const T::Value &input) {
  namespace R = champions::simulation::rta;
  if (!input.list) return input.integer<std::uint32_t>();
  const auto &value = input.bounded_array(4);
  if (value.empty()) throw std::runtime_error("empty callback result");
  auto kind = value[0].integer<unsigned>();
  if (kind == 1) {
    input.array(4);
    return R::Vec3{value[1].integer<std::uint32_t>(), value[2].integer<std::uint32_t>(), value[3].integer<std::uint32_t>()};
  }
  if (kind == 2) {
    input.array(2);
    std::vector<std::uint32_t> objects;
    for (const auto &obj : value[1].bounded_array(4096)) objects.push_back(obj.integer<std::uint32_t>());
    return objects;
  }
  throw std::runtime_error("callback result kind");
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
  for (const auto &b : root[1].bounded_array(60)) {
    const auto &r = b.array(4);
    auto id = r[0].integer<unsigned>();
    if (id > 59)
      throw std::runtime_error("service range");
    auto service = static_cast<M::Service>(id);
    if (tick.callbacks.count(service))
      throw std::runtime_error("duplicate binding");
    std::vector<M::Result> results;
    if (r[1].list && !r[1].items.empty() && r[1].items[0].integer<unsigned>() == 3) {
      const auto &values = r[1].array(2)[1].bounded_array(128);
      if (values.empty()) throw std::runtime_error("empty result sequence");
      for (const auto &value : values) results.push_back(callback_result(value));
    } else results.push_back(callback_result(r[1]));
    auto index = std::make_shared<std::size_t>(0);
    auto effects = r[2].bounded_array(16);
    auto fail = r[3].integer<unsigned>();
    if (fail > 1)
      throw std::runtime_error("failure flag");
    tick.callbacks[service] = [results, index, effects, fail](M::Tick &t,
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
      auto i = *index;
      if (i + 1 < results.size()) ++*index;
      return results[i];
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
    std::string structured_result;
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
      case 15:
        op.array(3);
        tick.update_cast(r[1].integer<std::uint32_t>(), r[2].integer<std::uint32_t>());
        break;
      case 16:
        op.array(4);
        result = tick.weapon_swap_input(r[1].integer<std::uint32_t>(), r[2].integer<std::uint32_t>(), r[3].integer<std::uint32_t>());
        break;
      case 17:
        op.array(3);
        tick.cast_skill(r[1].integer<std::uint32_t>(), r[2].integer<std::uint32_t>());
        break;
      case 18: {
        op.array(3);
        auto limit = r[2].integer<unsigned>();
        if (limit > 1) throw std::runtime_error("height flag");
        auto [a, b] = tick.hands_midpoint(r[1].integer<std::uint32_t>(), limit);
        structured_result = sequence({sequence({n(a[0]), n(a[1]), n(a[2])}), sequence({n(b[0]), n(b[1]), n(b[2])})});
        break;
      }
      case 19:
        op.array(2);
        result = tick.skill_of(r[1].integer<std::uint32_t>());
        break;
      case 20: {
        op.array(4);
        auto state = r[1].integer<unsigned>();
        tick.pad = r[2].integer<std::uint32_t>(); tick.R0 = r[3].integer<std::uint32_t>();
        using Handler = void (M::Tick::*)();
        static constexpr Handler handlers[] = {&M::Tick::state_hit, &M::Tick::state_dead,
          &M::Tick::state_jump, &M::Tick::state_rush, &M::Tick::state_block_move,
          &M::Tick::state_warcry, &M::Tick::state_vehicle, &M::Tick::state_pound,
          &M::Tick::state_skill, &M::Tick::state_block};
        if (state >= 10) throw std::runtime_error("state range");
        (tick.*handlers[state])(); break;
      }
      case 21: {
        op.array(4); auto helper = r[1].integer<unsigned>();
        auto a = r[2].integer<std::uint32_t>(), b = r[3].integer<std::uint32_t>();
        switch (helper) {
          case 0: result = tick.anim_step(a); break;
          case 1: result = tick.move(a,b); break;
          case 2: result = tick.frames(); break;
          case 3: result = tick.top_track(R::s32(a)); break;
          case 4: tick.stop_loop_sound(); break;
          case 5: tick.clear_extra_tracks(); break;
          case 6: result = tick.slow_scale(a); break;
          case 7: result = tick.haste_of(a); break;
          case 8: result = tick.anim_name(R::s32(a)); break;
          case 9: result = tick.selected_ability().value_or(0); break;
          case 10: tick.sound(a,b); break;
          case 11: { auto v = tick.unit_sphere_shared(); structured_result = sequence({n(v[0]),n(v[1]),n(v[2])}); break; }
          default: throw std::runtime_error("helper range");
        }
        break;
      }
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
    } catch (const M::Leave &) {
      status = 5;
    } catch (const M::Unsupported &) {
      status = 1;
    } catch (const M::MissingHandler &) {
      status = 2;
    } catch (const M::ResourceLimit &) {
      status = 3;
    } catch (const std::logic_error &) {
      status = 4;
    }
    steps.push_back(sequence({n(status), structured_result.empty() ? n(result) : structured_result, snapshot(tick)}));
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
