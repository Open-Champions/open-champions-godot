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
std::string key(const std::string &name) {
  std::vector<std::string> bytes;
  for (unsigned char c : name) bytes.push_back(n(c));
  return sequence(bytes);
}
std::string name(const T::Value &v) {
  std::string out;
  for (const auto &c : v.bounded_array(256)) out += char(c.integer<std::uint8_t>());
  return out;
}
std::vector<M::Argument> arguments(const T::Value &v) {
  std::vector<M::Argument> out;
  for (const auto &a : v.bounded_array(32)) {
    if (!a.list || a.items.empty()) throw std::runtime_error("argument shape");
    auto kind = a.items[0].integer<unsigned>();
    if (kind == 1) { auto &r = a.array(4); out.push_back(champions::simulation::rta::Vec3{r[1].integer<std::uint32_t>(),r[2].integer<std::uint32_t>(),r[3].integer<std::uint32_t>()}); }
    else {
      auto &r = a.array(2);
      if (kind == 0) {
        if (r[1].negative) out.push_back(r[1].integer<std::int64_t>());
        else out.push_back(r[1].integer<std::uint64_t>());
      } else if (kind == 2) { if (r[1].integer<unsigned>()) throw std::runtime_error("stack payload"); out.push_back(M::StackHandle{}); }
      else if (kind == 3) { if (r[1].integer<unsigned>()) throw std::runtime_error("null payload"); out.push_back(M::NullArgument{}); }
      else throw std::runtime_error("argument kind");
    }
  }
  return out;
}
std::string result_json(const M::Result &value) {
  if (auto v = std::get_if<std::uint32_t>(&value)) return n(*v);
  if (auto v = std::get_if<champions::simulation::rta::Vec3>(&value)) return sequence({n((*v)[0]),n((*v)[1]),n((*v)[2])});
  if (auto v = std::get_if<M::QueryResult>(&value)) return sequence({n((*v)[0]),n((*v)[1])});
  if (auto v = std::get_if<M::AttachResult>(&value)) return sequence({sequence({n((*v)[0][0]),n((*v)[0][1]),n((*v)[0][2])}),sequence({n((*v)[1][0]),n((*v)[1][1]),n((*v)[1][2])})});
  if (auto v = std::get_if<std::vector<std::uint32_t>>(&value)) { std::vector<std::string> out; for (auto x : *v) out.push_back(n(x)); return sequence(out); }
  if (auto v = std::get_if<std::int64_t>(&value)) return n(*v);
  if (auto v = std::get_if<std::uint64_t>(&value)) return n(*v);
  if (auto v = std::get_if<M::BufferResult>(&value)) { std::vector<std::string> bytes; for (auto b : v->bytes) bytes.push_back(n(b)); return sequence({"\"buf\"",sequence(bytes)}); }
  return "null";
}
std::string call(const M::Call &c) {
  std::vector<std::string> args;
  for (const auto &a : c.args) {
    if (auto v = std::get_if<std::uint32_t>(&a))
      args.push_back(sequence({"0", n(*v)}));
    else if (auto point = std::get_if<champions::simulation::rta::Vec3>(&a)) {
      auto vec = *point;
      args.push_back(sequence({"1", n(vec[0]), n(vec[1]), n(vec[2])}));
    } else if (std::holds_alternative<M::StackHandle>(a)) args.push_back(sequence({"2", "0"}));
    else args.push_back(sequence({"3", "0"}));
  }
  return sequence({c.service == M::Service::Named ? key(c.name) : n(static_cast<unsigned>(c.service)), sequence(args)});
}
std::string snapshot(const M::Tick &t, const std::vector<std::string> *under = nullptr) {
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
  std::vector<std::string> fields{sequence(bytes), sequence(log), sequence(writes),
                   sequence(calls), sequence(events), n(t.draws), n(t.R0)};
  if (under) fields.push_back(sequence(*under));
  return sequence(fields);
}
M::Result callback_result(const T::Value &input) {
  namespace R = champions::simulation::rta;
  if (!input.list) return input.integer<std::uint32_t>();
  const auto &value = input.bounded_array(4);
  if (value.empty()) throw std::runtime_error("empty callback result");
  auto kind = value[0].integer<unsigned>();
  if (kind == 6) { input.array(2); return value[1].integer<std::int64_t>(); }
  if (kind == 7) { input.array(2); return value[1].integer<std::uint64_t>(); }
  if (kind == 8) { input.array(1); return M::NullArgument{}; }
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
  if (kind == 5) {
    input.array(3);
    auto vector = [](const T::Value &v) { const auto &a = v.array(3); return R::Vec3{a[0].integer<std::uint32_t>(),a[1].integer<std::uint32_t>(),a[2].integer<std::uint32_t>()}; };
    return M::AttachResult{vector(value[1]), vector(value[2])};
  }
  if (kind == 4) {
    input.array(3);
    return M::QueryResult{value[1].integer<std::uint32_t>(), value[2].integer<std::uint32_t>()};
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
  const auto &root = input.bounded_array(5);
  if (root.size() != 3 && root.size() != 4 && root.size() != 5) throw std::runtime_error("root shape");
  std::vector<std::string> under_calls;
  M::Memory::UnderlyingLoad under;
  M::Memory *owner = nullptr;
  if (root.size() >= 4 && !root[3].items.empty()) {
    const auto &spec = root[3].array(3);
    auto salt = spec[0].integer<std::uint8_t>();
    auto fail = spec[1].integer<unsigned>();
    auto effects = spec[2].bounded_array(16);
    under = [&, salt, fail, effects](M::Address a, std::size_t size) {
      under_calls.push_back(sequence({n(a), n(size)}));
      for (const auto &effect : effects) put(*owner, effect);
      if (fail && under_calls.size() == fail) throw std::logic_error("injected underlying failure");
      std::uint64_t value = 0;
      for (std::size_t i = 0; i < size; ++i)
        value |= std::uint64_t((a + i + salt) & 255) << (8 * i);
      return value;
    };
  } else if (root.size() >= 4) root[3].array(0);
  M::Memory mem({}, under);
  owner = &mem;
  auto observations = root.size() >= 4 ? &under_calls : nullptr;
  M::Tick tick(mem, 0x10000);
  for (const auto &p : root[0].bounded_array(512))
    put(mem, p);
  auto handler = [](const T::Value &binding) -> M::Tick::Callback {
    const auto &r = binding.array(4);
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
    return [results, index, effects, fail](M::Tick &t,
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
  };
  for (const auto &b : root[1].bounded_array(95)) {
    const auto &r = b.array(4);
    auto id = r[0].integer<unsigned>();
    if (id > 94)
      throw std::runtime_error("service range");
    auto service = static_cast<M::Service>(id);
    if (tick.callbacks.count(service))
      throw std::runtime_error("duplicate binding");
    tick.callbacks[service] = handler(b);
  }
  if (root.size() == 5) {
    const auto &config = root[4].array(2);
    for (const auto &v : config[0].bounded_array(128)) tick.external.insert(name(v));
    for (const auto &binding : config[1].bounded_array(128)) {
      const auto &r = binding.array(4); auto k = name(r[0]);
      if (tick.named_callbacks->count(k)) throw std::runtime_error("duplicate named binding");
      (*tick.named_callbacks)[k] = handler(binding);
    }
  }
  auto initial = snapshot(tick, observations);
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
        tick.run_legacy_entry();
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
      case 22: {
        op.array(6);
        auto helper = r[1].integer<unsigned>();
        auto a = r[2].integer<std::uint32_t>(), b = r[3].integer<std::uint32_t>();
        tick.R0 = r[4].integer<std::uint32_t>(); tick.instant = r[5].integer<std::uint32_t>();
        switch (helper) {
          case 0: tick.pad = a; tick.state_neutral(); break;
          case 1: result = tick.neutral_start_ability(a, b, tick.R0); break;
          case 2: tick.pad = a; result = tick.neutral_attack(b, std::uint32_t(tick.slot())); break;
          case 3: tick.old_arrow_notices(); break;
          default: throw std::runtime_error("neutral helper range");
        }
        break;
      }
      case 23: {
        op.array(6);
        auto helper = r[1].integer<unsigned>();
        auto a = r[2].integer<std::uint32_t>(), b = r[3].integer<std::uint32_t>();
        tick.R0 = r[4].integer<std::uint32_t>(); tick.instant = r[5].integer<std::uint32_t>();
        if (helper == 0) { tick.pad = a; tick.state_melee(); }
        else if (helper == 1) result = tick.melee_next_swing(a, R::s32(b), tick.R0);
        else if (helper == 2) result = tick.draw3();
        else throw std::runtime_error("melee helper range");
        break;
      }
      case 24: {
        op.array(6); auto helper = r[1].integer<unsigned>();
        auto a = r[2].integer<std::uint32_t>(), b = r[3].integer<std::uint32_t>();
        auto extra = r[4].integer<std::uint32_t>();
        tick.R0 = r[5].integer<std::uint32_t>();
        switch (helper) {
          case 0: tick.pad = a; tick.state_ranged(); break;
          case 1: tick.ranged_throw(a, R::s32(b)); break;
          case 2: tick.ranged_shoot(); break;
          case 3: tick.missile(tick.vec(a), tick.vec(b), R::s32(extra)); break;
          case 4: { if (b > 1) throw std::runtime_error("divisor flag"); auto v = tick.aim_at_target(b ? std::optional<std::uint32_t>(a) : std::nullopt, extra); structured_result = sequence({n(v[0]),n(v[1]),n(v[2])}); break; }
          case 5: if (b > 1) throw std::runtime_error("object flag"); result = tick.obj_rand(b ? std::optional<std::uint32_t>(a) : std::nullopt); break;
          case 6: result = tick.strstr(a,b); break;
          default: throw std::runtime_error("ranged helper range");
        }
        break;
      }
      case 25: {
        op.array(6); auto helper = r[1].integer<unsigned>();
        auto a = r[2].integer<std::uint32_t>(), b = r[3].integer<std::uint32_t>();
        auto extra = r[4].integer<std::uint32_t>(); tick.R0 = r[5].integer<std::uint32_t>();
        switch (helper) {
          case 0: tick.pad = a; tick.state_cast(); break;
          case 1: tick.pad = b; tick.cast_old_style(a); break;
          case 2: { if (extra > 64) throw std::runtime_error("light colour cap");
            std::vector<std::uint32_t> colours;
            for (unsigned i = 0; i < extra; ++i) colours.push_back(tick.u32(M::Address(b) + i*4));
            tick.light_effect(a, 0, colours); break; }
          case 3: tick.old_fire_nova(a); break;
          case 4: result = tick.strcmp(a,b); break;
          case 5: tick.pad = a; tick.instant = extra; tick.dispatch_state(); break;
          default: throw std::runtime_error("legacy cast helper range");
        }
        break;
      }
      case 26:
        op.array(1); tick.top();
        structured_result = sequence({n(tick.pad), n(tick.instant), n(tick.weapon0)});
        break;
      case 27:
        op.array(2); tick.pad = r[1].integer<std::uint32_t>();
        tick.instant_abilities(); result = tick.instant;
        break;
      case 28:
        op.array(1);
        structured_result = sequence({n(tick.pad), n(tick.instant), n(tick.weapon0)});
        break;
      case 29:
        op.array(2); tick.pad = r[1].integer<std::uint32_t>(); tick.tail();
        break;
      case 30: {
        op.array(1);
        auto reason = tick.run();
        if (reason == "end") result = 0;
        else if (reason == "scripted") result = 1;
        else if (reason == "recall during block move") result = 2;
        else if (reason == "ability button index") result = 3;
        else throw std::runtime_error("unknown full-tick Leave");
        break;
      }
      case 31: {
        op.array(4);
        auto helper = r[1].integer<unsigned>();
        auto a = r[2].integer<M::Address>();
        if (helper <= 3) {
          // Reserved trailing zero keeps a single stable utility-command shape.
          if (r[3].integer<unsigned>() != 0) throw std::runtime_error("read argument");
          if (helper == 0) result = M::s8(a);
          else if (helper == 1) result = tick.u8(a);
          else if (helper == 2) result = tick.u16(a);
          else structured_result = n(tick.u64(a)); // Preserve all unsigned 64 bits.
        } else if (helper <= 6) {
          auto value = r[3].negative ? std::uint64_t(r[3].integer<std::int64_t>())
                                   : r[3].integer<std::uint64_t>();
          if (helper == 4) tick.w8(a, value);
          else if (helper == 5) tick.w16(a, value);
          else tick.w32(a, value);
        } else if (helper == 7) {
          auto bytes = tick.cstr(a, r[3].integer<std::int64_t>());
          std::vector<std::string> out;
          for (unsigned char byte : bytes) out.push_back(n(byte));
          structured_result = sequence(out);
        } else throw std::runtime_error("utility helper");
        break;
      }
      case 32:
        op.array(3);
        structured_result = n(mem.load(r[1].integer<M::Address>(), r[2].integer<std::size_t>()));
        break;
      case 33: {
        op.array(3);
        std::vector<std::uint8_t> data;
        for (const auto &v : r[2].bounded_array(4096)) data.push_back(v.integer<std::uint8_t>());
        mem.write_bytes(r[1].integer<M::Address>(), data);
        break;
      }
      case 34:
        op.array(3);
        structured_result = result_json(tick.call(name(r[1]), arguments(r[2])));
        break;
      case 35:
        op.array(6);
        structured_result = result_json(tick.vcall(name(r[1]), r[2].integer<std::uint32_t>(), r[3].integer<unsigned>(), r[4].integer<unsigned>(), arguments(r[5])));
        break;
      case 36:
        op.array(4);
        structured_result = result_json(tick.helper(name(r[1]), handler(r[2]), arguments(r[3])));
        break;
      case 39:
        op.array(2); structured_result = result_json(tick.use_potion(r[1].integer<std::uint32_t>())); break;
      case 37:
        op.array(2); tick.external.clear();
        for (const auto &v : r[1].bounded_array(128)) tick.external.insert(name(v));
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
    steps.push_back(sequence({n(status), structured_result.empty() ? n(result) : structured_result, snapshot(tick, observations)}));
  }
  return sequence({initial, sequence(steps)});
}
int main() {
  std::ios::sync_with_stdio(false);
  std::cin.tie(nullptr);
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
