// Authored Session-owned object-world replay. Expected values are produced
// independently by the pinned Python contracts; this executable computes every
// output field.
#include "champions/session.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {
std::vector<std::string> split(const std::string &s, char separator) {
  std::vector<std::string> result;
  std::size_t start = 0;
  for (;;) {
    const auto end = s.find(separator, start);
    result.push_back(s.substr(start, end == s.npos ? end : end - start));
    if (end == s.npos)
      return result;
    start = end + 1;
  }
}
std::int64_t integer(const std::string &s) {
  if (s.empty() || s == "-0" || s.front() == '+' ||
      (s.size() > 1 && s.front() == '0') ||
      (s.size() > 2 && s[0] == '-' && s[1] == '0'))
    throw std::runtime_error("noncanonical integer");
  for (std::size_t i = s.front() == '-' ? 1 : 0; i < s.size(); ++i)
    if (s[i] < '0' || s[i] > '9')
      throw std::runtime_error("invalid integer");
  std::size_t used = 0;
  const auto value = std::stoll(s, &used);
  if (used != s.size())
    throw std::runtime_error("invalid integer");
  return value;
}
std::string sequence(const std::vector<std::string> &values) {
  std::string out = "[";
  for (const auto &value : values) {
    if (out.size() > 1)
      out += ',';
    out += value;
  }
  return out + ']';
}
template <class C> std::string bytes(const C &values) {
  constexpr char hex[] = "0123456789abcdef";
  std::string out = "b";
  for (const auto value : values) {
    const auto ch = static_cast<unsigned char>(value);
    out += hex[ch >> 4];
    out += hex[ch & 15];
  }
  return out;
}
std::string text(const std::string &value) {
  auto out = bytes(value);
  out.front() = 's';
  return out;
}
template <class C> std::string integers(const C &values) {
  std::vector<std::string> out;
  for (const auto value : values)
    out.push_back(std::to_string(value));
  return sequence(out);
}

namespace S = champions::simulation::rta::session;
namespace N = champions::simulation::rta::netwire;
namespace A = champions::simulation::rta::amx;
namespace R = champions::simulation::rta;
std::string n(std::int64_t x) { return std::to_string(x); }
template <class T> std::string optional_number(const std::optional<T> &v) {
  return v ? std::to_string(*v) : "_";
}
std::string texts(const std::vector<std::string> &v) {
  std::vector<std::string> out;
  for (const auto &x : v)
    out.push_back(text(x));
  return sequence(out);
}
std::string codec(const N::CodecState &c) {
  std::vector<std::string> channels;
  for (const auto &v : c.ch)
    channels.push_back(
        sequence({n(v.index), n(v.pred), n(v.value), n(v.velocity)}));
  return sequence({sequence(channels), n(c.yaw_prev), n(c.yaw_pred),
                   n(c.yaw_delta), n(c.pad46), n(c.count)});
}
std::string sent(const N::Sent &v) {
  return sequence({n(v.peer), bytes(v.ip), n(v.port), bytes(v.data),
                   integers(v.undefined)});
}
std::string sends(const std::vector<N::Sent> &v) {
  std::vector<std::string> out;
  for (const auto &x : v)
    out.push_back(sent(x));
  return sequence(out);
}
std::string network(const N::Machine &m) {
  std::vector<std::string> peers, locks, positions, pads, handlers, direct;
  for (const auto &p : m.peers)
    peers.push_back(sequence(
        {bytes(p.pack()),
         integers(std::vector<N::Integer>{
             p.port, p.send_frame, p.msg_write, p.msg_read, p.newest, p.acked,
             p.lead, p.sim, p.silence, p.heard, p.local, p.dropped, p.kick,
             p.oos, p.drop_frame, p.kicked_notice}),
         integers(p.pad_prev), codec(p.recv_state), codec(p.send_state)}));
  for (const auto &x : m.input_lock)
    locks.push_back(integers(x));
  for (const auto &x : m.positions)
    positions.push_back(x ? integers(*x) : "_");
  for (const auto &x : m.pads)
    pads.push_back(bytes(x));
  for (const auto &x : m.handlers)
    handlers.push_back(sequence({n(x.first), n(bool(x.second))}));
  for (const auto &x : m.direct_handlers)
    direct.push_back(n(bool(x)));
  return sequence(
      {integers(std::vector<N::Integer>{
           m.network_mode,    m.num_peers,     m.lif,        m.last_sent,
           m.net_frame,       m.cycle,         m.phase,      m.modem,
           m.last_hash_frame, m.stall,         m.ended,      m.eq,
           m.min_ready,       m.win,           m.long_stall, m.kicked,
           m.kick_state,      m.msg_attached,  m.sim_frame,  m.rand_seed,
           m.sync_acc,        m.level_pending, m.num_players}),
       sequence(peers),
       integers(m.kick_acks),
       integers(m.cam_yaw),
       sequence(locks),
       integers(m.send_cycles),
       codec(m.sim_enc),
       codec(m.sim_dec),
       sequence(positions),
       sequence(pads),
       integers(std::vector<N::Integer>{
           m.pad_globals.start_block, m.pad_globals.mute, m.pad_globals.allow}),
       sends(m.outbox),
       texts(m.log),
       sequence(handlers),
       sequence(direct),
       n(bool(m.run_tasks)),
       n(bool(m.on_kill)),
       n(bool(m.first_active_hook)),
       n(bool(m.sample_garbage)),
       n(bool(m.stack_bytes)),
       sequence({n(static_cast<int>(m.fpu.model)), n(m.fpu.mul_bias),
                 n(m.fpu.div_nearest)})});
}
std::string definition(const R::items::Definition &v) {
  return sequence({n(v.addr), n(v.index), n(v.preset), text(v.name), n(v.level),
                   n(v.weight), n(v.count), n(v.damage_min), n(v.damage_max),
                   n(v.price), n(v.armor), n(v.attack), n(v.slot),
                   n(v.category), n(v.subtype), n(v.damage_bits),
                   std::to_string(v.flags), v.picture ? text(*v.picture) : "_",
                   n(v.model_row)});
}
std::string item_record(const S::ItemRecord &v) {
  std::vector<std::string> mods;
  for (const auto &m : v.mods)
    mods.push_back(m ? sequence({n(m->code), integers(m->values)}) : "_");
  return sequence({n(v.count), std::to_string(v.flags), integers(v.a),
                   integers(v.b), n(v.prefix), n(v.suffix), sequence(mods)});
}
std::string combat_definition(const R::combat::ItemDef &v) {
  return sequence({integers(v.name), n(v.dmg_min), n(v.dmg_max), n(v.armor),
                   n(v.attack), n(v.category), n(v.subtype),
                   std::to_string(v.dmg_type), std::to_string(v.flags)});
}
std::string item(const S::ItemSnapshot &v) {
  std::vector<std::string> mods;
  for (const auto &m : v.combat_mods)
    mods.push_back(integers(m));
  return sequence({text(v.definition), item_record(v.record),
                   combat_definition(v.combat_definition), n(v.combat_count),
                   std::to_string(v.combat_flags), sequence(mods)});
}
std::string operand(const A::Operand &o) {
  if (std::holds_alternative<std::monostate>(o))
    return "_";
  if (auto x = std::get_if<A::Cell>(&o))
    return n(*x);
  if (auto x = std::get_if<std::pair<A::Cell, A::Cell>>(&o))
    return sequence({n(x->first), n(x->second)});
  if (auto x = std::get_if<A::DebugSymbol>(&o))
    return sequence({n(x->address), n(x->flags), n(x->name)});
  const auto &x = std::get<A::CaseTable>(o);
  std::vector<std::string> pairs;
  for (const auto &p : x.cases)
    pairs.push_back(sequence({n(p.first), n(p.second)}));
  return sequence(
      {n(x.count), optional_number(x.default_target), sequence(pairs)});
}
std::string script(const S::ScriptSnapshot &v) {
  std::vector<std::string> state, decoded, profile, bindings;
  for (const auto &x : v.stored.state)
    state.push_back(optional_number(x));
  if (v.decoded)
    for (const auto &p : *v.decoded)
      decoded.push_back(
          sequence({n(p.first), n(p.second.opcode), operand(p.second.operand),
                    n(p.second.next)}));
  if (v.profile)
    for (const auto &p : *v.profile)
      profile.push_back(sequence({n(p.first), std::to_string(p.second)}));
  if (v.bindings)
    for (const auto x : *v.bindings)
      bindings.push_back(n(static_cast<int>(x)));
  return sequence({sequence({bytes(v.stored.image), sequence(state)}),
                   n(v.params),
                   n(v.parsed),
                   n(v.header_read),
                   n(v.callback_present),
                   n(v.size),
                   n(v.cod),
                   n(v.dat),
                   n(v.hea0),
                   n(v.start),
                   n(v.num_publics),
                   n(v.publics_off),
                   n(v.num_natives),
                   n(v.natives_off),
                   n(v.num_libraries),
                   n(v.num_pubvars),
                   n(v.data_offset),
                   optional_number(v.budget),
                   v.decoded ? sequence(decoded) : "_",
                   v.profile ? sequence(profile) : "_",
                   v.bindings ? sequence(bindings) : "_",
                   v.binding_present ? integers(*v.binding_present) : "_",
                   integers(v.pending_present),
                   texts(v.pending_names),
                   texts(v.declared_native_names),
                   texts(v.registered_names)});
}
std::string old_snapshot(const S::Snapshot &v) {
  if (v.hp && (v.hp->base_combat || v.hp->adapter.world.shared_random))
    throw std::runtime_error(
        "legacy trace cannot omit active combat extension");
  std::vector<std::string> definitions, items, scripts, events, messages;
  for (const auto &p : v.message_handlers)
    messages.push_back(sequence({n(p.first), text(p.second)}));
  for (const auto &p : v.definitions)
    definitions.push_back(sequence({text(p.first), definition(p.second)}));
  for (const auto &p : v.items)
    items.push_back(sequence({text(p.first), item(p.second)}));
  for (const auto &p : v.scripts)
    scripts.push_back(sequence({text(p.first), script(p.second)}));
  for (const auto &e : v.events)
    events.push_back(
        sequence({text(e.kind), text(e.name), n(e.frame), integers(e.values)}));
  const auto &c = v.config;
  return sequence(
      {sequence({text(c.profile), n(static_cast<int>(c.model)), n(c.mul_bias),
                 n(c.div_nearest), n(c.peers), n(c.local), n(c.network_mode),
                 n(c.seed), n(c.network_seed)}),
       n(v.started), n(v.failed), n(v.submitting), n(v.executing),
       text(v.failure_operation), texts(v.tasks), sequence(messages),
       network(v.network), n(v.network_task_callback), sequence(definitions),
       sequence(items), sequence(scripts), n(v.seed), integers(v.draws),
       sequence(events)});
}
namespace O = R::objects;
template <class T> std::string opt(const std::optional<T> &x) {
  return x ? std::to_string(*x) : "_";
}
std::string str(const std::optional<std::string> &x) {
  return x ? bytes(*x) : "_";
}
std::string strings(const std::vector<std::string> &x) {
  std::vector<std::string> out;
  for (const auto &v : x)
    out.push_back(bytes(v));
  return sequence(out);
}
std::string world_snapshot(const O::WorldSnapshot &s) {
  R::Pool baseline;
  std::set<std::uint32_t> keys;
  for (const auto &v : baseline.link)
    keys.insert(v.first);
  for (const auto &v : s.pool.link)
    keys.insert(v.first);
  std::vector<std::string> delta, links, objs, live, names, tasks, centres,
      pads, spawn;
  for (auto k : keys) {
    auto a = baseline.link.find(k);
    auto b = s.pool.link.find(k);
    if (a == baseline.link.end() || b == s.pool.link.end() ||
        a->second != b->second)
      delta.push_back(
          sequence({n(k), b == s.pool.link.end() ? "_" : n(b->second)}));
  }
  for (const auto &l : s.links) {
    const auto kind = static_cast<int>(l.kind);
    bool keep = true;
    if (kind == 0 && l.id < 0x90000000ULL && !l.next && !l.prev && !l.partner &&
        !l.object && !l.tag)
      keep = false;
    if (kind == 1) {
      const auto index = (l.id - R::HandlePool::NODE_BASE) / 20;
      const auto next = index + 1 < R::HandlePool::NODE_COUNT ? l.id + 20 : 0;
      keep = l.next != next || l.prev || l.partner || l.object || l.tag;
    }
    if (keep)
      links.push_back(
          sequence({std::to_string(l.id), n(kind), std::to_string(l.next),
                    std::to_string(l.prev), std::to_string(l.partner),
                    std::to_string(l.object), n(l.tag)}));
  }
  for (const auto &o : s.objects) {
    std::string cd = "_";
    if (o.class_def) {
      std::vector<std::string> entries;
      for (const auto &e : o.class_def->entries)
        entries.push_back(sequence({bytes(e.first), n(e.second)}));
      cd = sequence({bytes(o.class_def->lump), n(o.class_def->generation),
                     sequence(entries)});
    }
    objs.push_back(sequence({n(o.serial),
                             n(o.addr),
                             n(o.flags),
                             integers(o.pos),
                             n(o.radius),
                             n(o.height),
                             n(o.vz),
                             n(o.angle),
                             n(o.seed),
                             n(o.vtable),
                             n(o.draw_half_width),
                             n(o.draw_height),
                             str(o.class_name),
                             bytes(o.type_name),
                             n(o.alive),
                             opt(o.tag),
                             n(o.props_id),
                             o.props ? strings(*o.props) : "_",
                             n(o.class_def_id),
                             cd}));
  }
  for (const auto &x : s.live)
    live.push_back(sequence({n(x.first), n(x.second)}));
  for (unsigned i = 0; i < 512; ++i)
    if (s.name_keys[i] || s.name_objects[i])
      names.push_back(
          sequence({n(i), str(s.name_keys[i]), n(s.name_objects[i])}));
  for (const auto &t : s.tasks.tasks)
    tasks.push_back(
        sequence({n(t.fn), n(t.priority), bytes(t.name), n(t.flags)}));
  for (const auto &x : s.centres)
    centres.push_back(integers(x));
  for (const auto &p : s.pads)
    pads.push_back(sequence({n(p.held), n(p.pressed), n(p.edge2)}));
  for (const auto &p : s.spawn_pos)
    spawn.push_back(integers(p));
  return sequence(
      {sequence({integers(s.pool.base), integers(s.pool.head), n(s.pool.last),
                 n(s.pool.end), sequence(delta)}),
       n(s.shared_seed), std::to_string(s.handle_free), n(s.iterator_serial),
       "[6096,28879]", sequence(links), sequence(objs), sequence(live),
       sequence(names), n(s.name_count),
       sequence({sequence(tasks), n(s.tasks.cursor)}),
       sequence({n(s.cursor15), n(s.cursor60), n(s.frame), n(s.network_mode),
                 n(s.cull_by_camera), n(s.pool_free_count), n(s.tearing_down),
                 n(s.lump_generation), n(s.menu_lockout),
                 n(s.menu_lockout_hold), n(s.warmup_frames), n(s.start_rank),
                 n(s.exclude_mask)}),
       integers(s.camera_focus), sequence(centres), sequence(pads),
       sequence(spawn), integers(s.spawn_facing),
       n(static_cast<int>(s.callback_policy)), integers(s.callbacks)});
}

std::string snapshot(const S::Snapshot &s) {
  std::vector<std::string> tasks, bindings;
  for (const auto &t : s.object_task_handlers)
    tasks.push_back(sequence({n(t.first), text(t.second)}));
  for (const auto &b : s.object_bindings)
    bindings.push_back(sequence({text(b.kind), text(b.name), n(b.present)}));
  return sequence({old_snapshot(s), s.world ? world_snapshot(*s.world) : "_",
                   sequence(tasks), sequence(bindings), text(s.world_phase),
                   n(s.world_depth), n(s.network_before_frame_callback)});
}

const std::array<std::string, 16> scenario_names{
    "ready",    "two_ready",       "delayed",      "no_ready",
    "hold",     "no_sim",          "pending",      "frozen",
    "mutation", "seed_domain",     "run_failure",  "end_failure",
    "message",  "message_failure", "nested_world", "hash_collision"};
const std::array<std::uint32_t, 4> seeds{0, 1, 0xdeadbeef, 0xffffffff};
const std::array<std::uint32_t, 4> task_ids{0xf0000001, 0xf0000002, 0xf0000003,
                                            0xf0000004};
const std::array<std::string, 4> task_names{"authored_pre", "authored_post",
                                            "authored_draw", "authored_late"};
const std::array<std::int64_t, 4> priorities{0, 125, 21, 20};
N::ByteVector ready_packet(const S::Config &c) {
  N::Machine remote(2, 1, N::Fpu(c.model, c.mul_bias, c.div_nearest));
  for (unsigned f = 1; f <= 12; ++f)
    remote.peers[1].set_record(f,
                               N::rec12(f * 17,
                                        {128 + f, 64 + f, 255 - f, f * 9},
                                        {f * 11, f * 13}, f * 123, f & 1),
                               0, 0);
  return remote.build_packet(0, 1, 12, N::PKT_RAW_RELAY, 1, nullptr, true).data;
}
std::string world_pads(const O::WorldSnapshot &w) {
  std::vector<std::string> result;
  for (const auto &p : w.pads)
    result.push_back(sequence({n(p.held), n(p.pressed), n(p.edge2)}));
  return sequence(result);
}
std::pair<std::string, std::string> scenario(const std::string &operation,
                                             std::int64_t config,
                                             const std::string &arguments) {
  const auto args = split(arguments, ';');
  if (args.size() != 2 || config < 0 || config > 15)
    throw std::runtime_error("invalid scenario/configuration");
  const auto id = integer(args[0]), variant = integer(args[1]);
  if (id < 0 || id >= 1024 || variant < 0 || variant > 3 || id / 64 != config ||
      (id / 16) % 4 != variant ||
      scenario_names[std::size_t(id % 16)] != operation)
    throw std::runtime_error("inconsistent scenario identity");
  S::Config c;
  c.model = static_cast<R::Fpu::Model>(config / 4);
  c.mul_bias = config & 2;
  c.div_nearest = config & 1;
  c.peers = 2;
  c.local = 0;
  c.network_mode = 1;
  c.seed = seeds[std::size_t(variant)] ^ 0xa5a5a5a5;
  S::Session session(c);
  std::vector<std::string> events, results;
  std::vector<O::GameObject *> objects;
  std::int64_t ready_calls = 0;
  auto oid = [&](const O::GameObject &obj) {
    const auto it = std::find(objects.begin(), objects.end(), &obj);
    if (it == objects.end())
      throw std::runtime_error("unknown authored object");
    return std::int64_t(it - objects.begin() + 1);
  };
  auto note = [&](const std::string &kind,
                  std::vector<std::string> values = {}) {
    const auto s = session.snapshot();
    const auto &w = *s.world;
    std::vector<std::string> row{text(kind)};
    row.insert(row.end(), values.begin(), values.end());
    row.insert(row.end(),
               {n(w.frame), n(s.network.net_frame), world_pads(w),
                n(w.shared_seed), text(s.world_phase), n(s.world_depth)});
    events.push_back(sequence(row));
  };
  auto create = [&](O::World &w, std::int32_t x) {
    auto *obj =
        w.new_object(0x180, {R::from_int(static_cast<std::uint32_t>(x)), 0, 0});
    obj->tag = 0;
    objects.push_back(obj);
    return obj;
  };
  S::ObjectBindings bindings;
  bindings.on_run = {"authored_on_run", [&](O::GameObject &obj) {
                       session.with_world([&](O::World &w) {
                         note("run", {n(oid(obj)), n(obj.flags)});
                         *obj.tag += 1;
                         const auto draw = w.shared.draw();
                         note("draw", {n(draw)});
                         if (operation == "mutation" && &obj == objects[0] &&
                             *obj.tag == 1) {
                           if (variant == 0)
                             w.run_every_60(obj);
                           else if (variant == 1)
                             w.run_every_15(*create(w, 512));
                           else if (variant == 2)
                             w.delete_object(*objects[1]);
                           else
                             w.tasks.add(task_ids[3], 20, "authored_late", 0);
                         }
                         if (operation == "run_failure")
                           throw S::Error("authored run failure");
                       });
                     }};
  bindings.on_destroy = {"authored_on_destroy",
                         [&](O::GameObject &obj, std::int64_t flags) {
                           session.with_world([&](O::World &w) {
                             note("destroy", {n(oid(obj)), n(flags)});
                             w.destroy(obj, flags);
                           });
                         }};
  bindings.is_frozen = {"authored_is_frozen", [&]() {
                          bool frozen = false;
                          session.with_world([&](O::World &) {
                            note("frozen");
                            frozen = operation == "frozen" && variant != 3;
                          });
                          return frozen;
                        }};
  bindings.res_find = {"authored_res_find",
                       [](const std::string &, const std::string &) {
                         return std::uint32_t(0);
                       }};
  bindings.end_of_tick = {"authored_end_of_tick", [&]() {
                            session.with_world([&](O::World &w) {
                              note("end");
                              if (operation == "end_failure") {
                                w.pads[0].pressed = 77;
                                throw S::Error("authored end failure");
                              }
                            });
                          }};
  session.enable_objects(seeds[std::size_t(variant)], bindings);
  for (std::size_t i = 0; i < 4; ++i)
    session.register_object_task(
        task_ids[i], task_names[i], priorities[i], i == 2 ? 1 : 0,
        [&, i](S::Session &) {
          if (i == 0)
            ++ready_calls;
          session.with_world([&](O::World &w) {
            note(task_names[i]);
            if (i == 0 && operation == "nested_world") {
              w.pads[0].pressed = 0x55;
              session.with_world([&](O::World &) { note("nested"); });
            }
          });
        });
  session.with_world([&](O::World &w) {
    auto *a = create(w, 0);
    auto *b = create(w, 256);
    auto *d = create(w, -256);
    w.run_every_frame(*a);
    w.run_every_15(*b);
    w.run_every_60(*d);
    w.tasks.remove(task_ids[3]);
    if (operation == "frozen" && variant == 0)
      w.delete_object(*b);
  });
  if (operation == "message" || operation == "message_failure") {
    session.register_message(7, "authored_message",
                             [&](S::Session &, const N::ByteVector &payload) {
                               session.with_world([&](O::World &w) {
                                 note("message", {integers(payload)});
                                 objects[0]->tag = payload.at(0) + 10;
                                 w.pads[0].held ^= 0x20;
                                 const auto draw = w.shared.draw();
                                 note("message_draw", {n(draw)});
                                 session.with_world([&](O::World &nested) {
                                   nested.pads[0].pressed = 0x33;
                                   note("message_nested");
                                   if (operation == "message_failure")
                                     throw S::Error("authored message failure");
                                 });
                               });
                             });
    session.queue_message(7, {static_cast<std::uint8_t>(variant + 1), 0x44});
  }
  auto step_view = [&]() {
    const auto s = session.snapshot();
    const auto &w = *s.world;
    return sequence({n(ready_calls), n(s.network.net_frame),
                     n(s.network.sim_frame), n(w.frame), n(w.shared_seed),
                     n(s.network.rand_seed), n(s.seed), world_pads(w),
                     text(s.world_phase), n(s.world_depth), n(s.failed),
                     world_snapshot(w)});
  };
  const auto packet = ready_packet(c);
  if (operation == "pending")
    session.set_level_pending(true);
  for (unsigned k = 0; k < 4; ++k) {
    if (operation == "pending" && k == 3)
      session.set_level_pending(false);
    std::vector<N::ByteVector> inbox;
    if ((operation == "delayed" && k == 2) ||
        (operation != "delayed" && operation != "no_ready" && k == 0))
      inbox.push_back(packet);
    const auto buttons = 257 * (k + 1);
    const N::ByteVector sample{static_cast<std::uint8_t>(buttons),
                               static_cast<std::uint8_t>(buttons >> 8),
                               static_cast<std::uint8_t>(128 + k),
                               static_cast<std::uint8_t>(64 + 3 * k),
                               static_cast<std::uint8_t>(255 - k),
                               static_cast<std::uint8_t>(17 * k),
                               static_cast<std::uint8_t>(31 * k),
                               static_cast<std::uint8_t>(47 * k)};
    try {
      const auto result = session.submit(
          sample, k * 1234, k & 1,
          operation == "two_ready"        ? 2
          : operation == "hold" && k == 0 ? 4
                                          : 1,
          operation == "no_sim" || (operation == "hold" && k == 0),
          operation == "hold" && k > 0, k, inbox);
      results.push_back(sequence(
          {sends(result.sent), n(result.held), n(result.steps), step_view()}));
    } catch (const S::Error &) {
      results.push_back(sequence({text("failed"), step_view()}));
    }
  }
  if (operation == "hash_collision") {
    const auto before = session.snapshot();
    const auto tag = objects[0]->tag;
    session.with_world([&](O::World &) { objects[0]->tag = *tag + 777; });
    const auto after = session.snapshot();
    results.push_back(
        sequence({n(before.network.peers[0].sync(before.network.net_frame) ==
                    after.network.peers[0].sync(after.network.net_frame)),
                  n(tag != objects[0]->tag)}));
  }
  if (operation == "seed_domain") {
    const auto s = session.snapshot();
    results.push_back(
        sequence({n(s.seed == (seeds[std::size_t(variant)] ^ 0xa5a5a5a5)),
                  n(s.world->shared_seed == s.network.rand_seed)}));
  }
  return {"ok", sequence({sequence(results), sequence(events),
                          snapshot(session.snapshot())})};
}
// Pinned independently generated corpus coverage and full computed trace
// digest.
constexpr std::uint64_t expected_digest = 0xbda12ed632219af5ULL;
constexpr std::size_t expected_rows = 1024;
int run(const std::vector<std::filesystem::path> &args) {
  if (args.size() < 1 || args.size() > 2)
    throw std::runtime_error(
        "usage: session-objects-trace VECTORS.tsv [TRACE.tsv]");
  constexpr std::uintmax_t max_fixture = 1024ULL * 1024 * 1024;
  constexpr std::size_t max_line = 16 * 1024 * 1024;
  if (!std::filesystem::is_regular_file(args[0]) ||
      std::filesystem::file_size(args[0]) > max_fixture)
    throw std::runtime_error(
        "fixture is missing, not regular, or exceeds budget");
  if (args.size() == 2) {
    if (std::filesystem::weakly_canonical(args[0]) ==
            std::filesystem::weakly_canonical(args[1]) ||
        (std::filesystem::exists(args[1]) &&
         std::filesystem::equivalent(args[0], args[1])))
      throw std::runtime_error("trace must not overwrite fixture");
  }
  std::ifstream input(args[0], std::ios::binary);
  if (!input)
    throw std::runtime_error("cannot open fixture");
  std::ofstream output;
  if (args.size() == 2) {
    output.open(args[1], std::ios::binary);
    if (!output)
      throw std::runtime_error("cannot open trace");
  }
  auto line = [&]() {
    std::string value;
    for (char ch; input.get(ch);) {
      if (ch == '\n')
        return value;
      if (value.size() == max_line)
        throw std::runtime_error("fixture line exceeds budget");
      if (static_cast<unsigned char>(ch) > 127 || ch == '\r' || ch == '\0')
        throw std::runtime_error("fixture must be ASCII with LF separators");
      value += ch;
    }
    if (!value.empty())
      throw std::runtime_error("fixture ends without newline");
    return value;
  };
  if (line() != "# session_objects_vectors_v1")
    throw std::runtime_error("fixture schema mismatch");
  std::set<std::string> seen;
  std::size_t rows = 0;
  std::uint64_t digest = 14695981039346656037ULL;
  while (input.peek() != std::char_traits<char>::eof()) {
    const auto columns = split(line(), '\t');
    if (columns.size() != 5)
      throw std::runtime_error("fixture requires five columns");
    const auto key = columns[0] + '\t' + columns[1] + '\t' + columns[2];
    if (!seen.insert(key).second)
      throw std::runtime_error("duplicate scenario");
    const auto actual = scenario(columns[0], integer(columns[1]), columns[2]);
    if (actual.first != columns[3] || actual.second != columns[4]) {
      std::cerr << "session mismatch at row " << rows + 1 << " (" << columns[0]
                << ")\n";
      return 1;
    }
    const auto record = key + '\t' + actual.first + '\t' + actual.second + '\n';
    if (args.size() == 2) {
      output << record;
      if (!output)
        throw std::runtime_error("trace write failed");
    }
    for (unsigned char ch : record) {
      digest ^= ch;
      digest *= 1099511628211ULL;
    }
    if (++rows > 65536)
      throw std::runtime_error("fixture row budget exceeded");
  }
  if (input.bad())
    throw std::runtime_error("fixture read failed");
  if (rows != expected_rows || digest != expected_digest)
    throw std::runtime_error("incomplete or changed session corpus");
  if (args.size() == 2) {
    output.flush();
    if (!output)
      throw std::runtime_error("trace write failed");
  }
  std::cout << rows
            << " exact session object vectors passed; FNV1a64=" << std::hex
            << digest << '\n';
  return 0;
}
int entry(const std::vector<std::filesystem::path> &args) {
  try {
    return run(args);
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
} // namespace
#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
#else
int main(int argc, char **argv) {
#endif
  std::vector<std::filesystem::path> args;
  for (int i = 1; i < argc; ++i)
    args.emplace_back(argv[i]);
  return entry(args);
}
