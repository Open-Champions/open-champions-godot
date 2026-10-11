// Authored Session-owned restricted-combat replay. Expected values are produced
// independently by the pinned Python contracts; this executable computes every
// output field.
#include "champions/session.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
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
template <class T> std::string n(T x) { return std::to_string(x); }
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
        !l.object && !l.object_raw && !l.tag)
      keep = false;
    if (kind == 1) {
      const auto index = (l.id - R::HandlePool::NODE_BASE) / 20;
      const auto next = index + 1 < R::HandlePool::NODE_COUNT ? l.id + 20 : 0;
      keep = l.next != next || l.prev || l.partner || l.object ||
             l.object_raw || l.tag;
    }
    if (keep)
      links.push_back(
          sequence({std::to_string(l.id), n(kind), std::to_string(l.next),
                    std::to_string(l.prev), std::to_string(l.partner),
                    std::to_string(l.object), n(l.tag), n(l.object_raw)}));
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

std::string session_objects_snapshot(const S::Snapshot &s) {
  std::vector<std::string> tasks, bindings;
  for (const auto &t : s.object_task_handlers)
    tasks.push_back(sequence({n(t.first), text(t.second)}));
  for (const auto &b : s.object_bindings)
    bindings.push_back(sequence({text(b.kind), text(b.name), n(b.present)}));
  return sequence({old_snapshot(s), s.world ? world_snapshot(*s.world) : "_",
                   sequence(tasks), sequence(bindings), text(s.world_phase),
                   n(s.world_depth), n(s.network_before_frame_callback)});
}

namespace C = R::combat;
namespace H = R::creature_hp;
namespace B = R::combat_creature;
namespace RH = R::raw_handles;
std::string number(std::int64_t v) { return std::to_string(v); }
std::string text(const C::Text &value) {
  std::string out;
  for (std::uint32_t c : value) {
    if (c <= 0x7f)
      out += char(c);
    else if (c <= 0x7ff) {
      out += char(0xc0 | (c >> 6));
      out += char(0x80 | (c & 63));
    } else if (c <= 0xffff) {
      out += char(0xe0 | (c >> 12));
      out += char(0x80 | ((c >> 6) & 63));
      out += char(0x80 | (c & 63));
    } else if (c <= 0x10ffff) {
      out += char(0xf0 | (c >> 18));
      out += char(0x80 | ((c >> 12) & 63));
      out += char(0x80 | ((c >> 6) & 63));
      out += char(0x80 | (c & 63));
    } else
      throw std::runtime_error("invalid text code point");
  }
  return text(out);
}
std::string event_value(const C::EventValue &event) {
  return std::visit(
      [](const auto &v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::monostate>)
          return "_";
        else if constexpr (std::is_same_v<T, C::Text>)
          return text(v);
        else if constexpr (std::is_same_v<T, std::vector<R::Raw>>)
          return integers(v);
        else
          return std::to_string(v);
      },
      event.value);
}
std::string write(const H::Write &value) {
  return sequence({n(value.address), n(value.size), n(value.value)});
}
std::string call(const H::Call &value) {
  return sequence({text(value.name), integers(value.args)});
}
template <class T> std::string refs(const T &v) {
  std::vector<std::string> r;
  for (const auto &x : v)
    r.push_back(optional_number(x));
  return sequence(r);
}
std::string object(const B::ObjSnapshot &record) {
  const auto &o = record.value;
  std::vector<std::string> spellrefs;
  for (const auto &s : record.spells)
    spellrefs.push_back(optional_number(s));
  return sequence({text(o.tag),
                   n(o.flags),
                   integers(o.pos),
                   number(o.facing),
                   n(o.owner_link),
                   optional_number(record.owner),
                   text(o.cls),
                   number(o.innoruuk_immune),
                   integers(o.res),
                   integers(o.flash),
                   n(o.cflags),
                   number(o.kind),
                   n(o.dot),
                   n(o.field_dc),
                   number(o.armor),
                   number(o.stamp_f0),
                   n(o.hp),
                   n(o.max_hp),
                   integers(o.status),
                   number(o.poison_a),
                   number(o.poison_b),
                   optional_number(record.status_source),
                   sequence(spellrefs),
                   number(o.counter2),
                   number(o.counter4),
                   number(o.counter11),
                   number(o.counter12),
                   number(o.slot),
                   number(o.state),
                   number(o.attack_start),
                   number(o.attack_chain),
                   integers(o.bonus),
                   number(o.model_points),
                   number(o.boss_rank),
                   n(o.exp_value),
                   n(o.own_seed),
                   optional_number(record.last_attacker)});
}
std::string player(const B::PlayerSnapshot &record) {
  const auto &p = record.value;
  std::vector<std::string> skills;
  for (const auto &s : p.skills)
    skills.push_back(sequence({number(s.first), number(s.second)}));
  return sequence({number(p.cls),
                   number(p.level58),
                   number(p.level59),
                   number(p.exp),
                   integers(p.base),
                   integers(p.eff),
                   n(p.hp),
                   n(p.max_hp),
                   n(p.mana),
                   n(p.max_mana),
                   number(p.attack),
                   number(p.dmg_min),
                   number(p.dmg_max),
                   n(p.stagger),
                   number(p.skill_points),
                   number(p.stat_points),
                   integers(p.passives),
                   n(p.load_limit),
                   refs(record.bag2),
                   optional_number(record.object),
                   refs(record.equip),
                   refs(record.weapon_sets),
                   optional_number(record.second_weapon),
                   number(p.active_set),
                   sequence(skills)});
}
std::string memory_limits(const H::Limits &x) {
  return integers(std::array<std::size_t, 4>{x.memory_bytes, x.history_entries,
                                             x.callback_entries, x.depth});
}
std::string shared_random(const std::optional<C::SharedRandomSnapshot> &value) {
  if (!value)
    return "_";
  std::vector<std::string> draws;
  for (const auto &d : value->draws)
    draws.push_back(sequence({n(d.value), n(d.state)}));
  return sequence({text(value->authority), n(value->state), sequence(draws)});
}
std::string adapter_snapshot(const B::Snapshot &s) {
  std::vector<std::string> objs, spells, items, definitions, players, wevents,
      bindings, regions, labels, memory, writes, stores, calls, events,
      callbacks, frames;
  for (const auto &x : s.objects)
    objs.push_back(object(x));
  for (const auto &x : s.spells) {
    const auto &v = x.value;
    spells.push_back(sequence({text(v.kind), n(v.skill), n(v.level),
                               optional_number(x.owner), n(v.f18), n(v.f1c),
                               n(v.f20), n(v.f2c)}));
  }
  for (const auto &x : s.items) {
    std::vector<std::string> mods;
    for (const auto &m : x.value.mods)
      mods.push_back(integers(m));
    items.push_back(sequence({optional_number(x.definition), n(x.value.count),
                              n(x.value.flags), sequence(mods)}));
  }
  for (const auto &x : s.definitions)
    definitions.push_back(sequence({text(x.name), n(x.dmg_min), n(x.dmg_max),
                                    n(x.armor), n(x.attack), n(x.category),
                                    n(x.subtype), n(x.dmg_type), n(x.flags)}));
  for (const auto &x : s.world.players)
    players.push_back(player(x));
  for (const auto &e : s.world.events) {
    std::vector<std::string> v;
    for (const auto &x : e)
      v.push_back(event_value(x));
    wevents.push_back(sequence(v));
  }
  const auto &w = s.world;
  const auto world = sequence(
      {sequence({n(w.seed.state), integers(w.seed.draws)}),
       sequence({n(w.cosmetic.state), integers(w.cosmetic.draws)}),
       sequence({n(w.frame), n(w.pvp_a), n(w.cutscene), n(w.numbers_option),
                 n(w.network_mode), n(w.monster_level), integers(w.focus),
                 integers(w.music), n(w.music_index), n(w.num_players),
                 integers(w.alive), n(w.medal_mission), n(w.difficulty),
                 text(w.level_name), n(w.load_cheat)}),
       sequence(players), sequence(wevents), integers(w.callbacks),
       refs(w.owned_spells), shared_random(w.shared_random)});
  for (const auto &x : s.bindings)
    bindings.push_back(sequence({n(x.address), n(x.generation), n(x.extent),
                                 n(unsigned(x.projection)),
                                 n(unsigned(x.dispatch)), text(x.label)}));
  for (const auto &x : s.regions)
    regions.push_back(sequence({n(x.address), n(x.size), text(x.label)}));
  for (const auto &x : s.callbacks)
    labels.push_back(sequence({text(x.name), text(x.label)}));
  for (const auto &x : s.model.backing)
    memory.push_back(sequence({n(x.first), n(x.second)}));
  for (const auto &x : s.model.writes)
    writes.push_back(write(x));
  for (const auto &x : s.model.stores)
    stores.push_back(write(x));
  for (const auto &x : s.model.calls)
    calls.push_back(call(x));
  for (const auto &x : s.model.events)
    events.push_back(
        sequence({n(unsigned(x.kind)), write(x.write), call(x.call)}));
  for (const auto &x : s.model.callbacks)
    callbacks.push_back(sequence({text(x.first), n(x.second)}));
  for (const auto &x : s.callback_frames) {
    std::vector<std::string> values;
    for (const auto &v : x.expected)
      values.push_back(sequence({n(v.flags), integers(v.position), n(v.cflags),
                                 n(v.hp), integers(v.status)}));
    frames.push_back(sequence({sequence(values), n(x.frame)}));
  }
  auto model = sequence({sequence(memory), sequence(stores), sequence(writes),
                         sequence(calls), sequence(events), n(s.model.draws),
                         n(unsigned(s.model.policy)), sequence(callbacks),
                         n(s.model.overlay_present)});
  return sequence(
      {sequence({n(s.limits.entries), n(s.limits.raw_regions),
                 n(s.limits.depth), memory_limits(s.limits.memory)}),
       n(s.operation_depth), n(s.raw_callback_depth), sequence(frames),
       sequence(bindings), sequence(regions), sequence(labels), sequence(objs),
       sequence(spells), sequence(items), sequence(definitions), world, model});
}
std::string heads(const std::vector<RH::HeadBinding> &values) {
  std::vector<std::string> out;
  for (const auto &h : values)
    out.push_back(sequence({n(h.address), n(h.identity), n(h.object_serial),
                            text(h.label), n(h.active)}));
  return sequence(out);
}
std::string physical(const RH::Snapshot &s) {
  std::vector<std::string> nodes;
  for (std::size_t i = 0; i < s.nodes.size(); ++i) {
    const auto &x = s.nodes[i];
    if (x.next == (i + 1 < s.nodes.size() ? x.address + 20 : 0) && !x.prev &&
        !x.partner && !x.object && !x.tag && !x.object_serial && !x.opaque)
      continue;
    nodes.push_back(
        sequence({n(x.address), n(x.next), n(x.prev), n(x.partner), n(x.object),
                  n(x.tag), n(x.object_serial), n(x.opaque)}));
  }
  return sequence({n(s.attached), n(s.free), heads(s.heads), sequence(nodes)});
}
std::string snapshot(const S::Snapshot &s) {
  if (s.creature_movement)
    throw std::runtime_error(
        "legacy trace cannot omit active creature movement extension");
  if (s.terrain)
    throw std::runtime_error(
        "legacy trace cannot omit active terrain extension");
  std::string hp = "_";
  if (s.hp) {
    const auto &h = *s.hp;
    std::vector<std::string> bindings;
    for (const auto &x : h.bindings)
      bindings.push_back(sequence(
          {n(x.id), n(x.world_serial), n(x.address), n(x.adapter_generation)}));
    hp = sequence(
        {sequence({n(h.limits.records), n(h.limits.calls_per_tick),
                   memory_limits(h.limits.memory)}),
         sequence(bindings), text(h.phase), n(h.depth), n(h.calls_this_tick),
         adapter_snapshot(h.adapter),
         sequence({n(h.registry.attached), heads(h.registry.heads)}),
         h.physical ? physical(*h.physical) : "_",
         text(h.physical_error.empty() ? "" : "physical_unavailable"),
         text(h.publication_error), n(h.object_run_completed),
         optional_number(h.task_priority),
         h.base_combat
             ? sequence({sequence({integers(h.base_combat->config.focus),
                                   n(h.base_combat->config.cutscene)}),
                         integers(h.base_combat->configured)})
             : "_"});
  }
  return sequence({session_objects_snapshot(s), hp});
}

// Parse only this program's computed canonical observations, never the expected
// fixture result. Deterministic deltas retain every changed scalar and byte.
struct Value {
  bool list = false;
  std::string atom;
  std::vector<Value> children;
};
Value parse_value(const std::string &s, std::size_t &p, unsigned depth = 0) {
  if (depth > 64 || p >= s.size())
    throw std::runtime_error("internal snapshot parse");
  Value v;
  if (s[p] == '[') {
    v.list = true;
    ++p;
    if (s.at(p) != ']')
      for (;;) {
        v.children.push_back(parse_value(s, p, depth + 1));
        if (s.at(p) != ',')
          break;
        ++p;
      }
    if (s.at(p++) != ']')
      throw std::runtime_error("internal snapshot list");
  } else {
    auto begin = p;
    while (p < s.size() && s[p] != ',' && s[p] != ']')
      ++p;
    v.atom = s.substr(begin, p - begin);
    if (v.atom.empty())
      throw std::runtime_error("internal snapshot atom");
  }
  return v;
}
Value parsed(const std::string &s) {
  std::size_t p = 0;
  auto v = parse_value(s, p);
  if (p != s.size())
    throw std::runtime_error("internal snapshot tail");
  return v;
}
std::string encoded(const Value &v) {
  if (!v.list)
    return v.atom;
  std::vector<std::string> c;
  for (const auto &x : v.children)
    c.push_back(encoded(x));
  return sequence(c);
}
void changes(const Value &a, const Value &b, std::vector<std::size_t> &path,
             std::vector<std::string> &out) {
  if (a.list && b.list && a.children.size() == b.children.size()) {
    for (std::size_t i = 0; i < a.children.size(); ++i) {
      path.push_back(i);
      changes(a.children[i], b.children[i], path, out);
      path.pop_back();
    }
    return;
  }
  if (!a.list && !b.list && a.atom == b.atom)
    return;
  if (!a.list && !b.list && a.atom.size() == b.atom.size() && !a.atom.empty() &&
      a.atom[0] == 'b' && b.atom[0] == 'b') {
    std::size_t i = 1;
    while (i < a.atom.size()) {
      if (a.atom.compare(i, 2, b.atom, i, 2) == 0) {
        i += 2;
        continue;
      }
      auto begin = i;
      while (i < a.atom.size() && a.atom.compare(i, 2, b.atom, i, 2) != 0)
        i += 2;
      out.push_back(sequence({integers(path), n((begin - 1) / 2),
                              "b" + b.atom.substr(begin, i - begin)}));
    }
    return;
  }
  out.push_back(sequence({integers(path), encoded(b)}));
}
std::string delta(const Value &a, const Value &b) {
  std::vector<std::size_t> path;
  std::vector<std::string> out;
  changes(a, b, path, out);
  return sequence(out);
}
Value apply_delta(Value value, const std::string &encoded_delta) {
  const auto patches = parsed(encoded_delta);
  if (!patches.list)
    throw std::runtime_error("internal delta list");
  for (const auto &patch : patches.children) {
    if (!patch.list ||
        (patch.children.size() != 2 && patch.children.size() != 3) ||
        !patch.children[0].list)
      throw std::runtime_error("internal delta entry");
    Value *target = &value;
    for (const auto &index : patch.children[0].children) {
      if (index.list || !target->list)
        throw std::runtime_error("internal delta path");
      auto i = integer(index.atom);
      if (i < 0 || std::size_t(i) >= target->children.size())
        throw std::runtime_error("internal delta index");
      target = &target->children[std::size_t(i)];
    }
    if (patch.children.size() == 2)
      *target = patch.children[1];
    else {
      const auto &replacement = patch.children[2];
      auto offset = integer(patch.children[1].atom);
      if (target->list || replacement.list || target->atom.empty() ||
          replacement.atom.empty() || target->atom[0] != 'b' ||
          replacement.atom[0] != 'b' || offset < 0 ||
          std::size_t(offset) > (target->atom.size() - 1) / 2)
        throw std::runtime_error("internal byte delta");
      const auto begin = 1 + std::size_t(offset) * 2;
      if (replacement.atom.size() - 1 > target->atom.size() - begin)
        throw std::runtime_error("internal delta byte extent");
      target->atom.replace(begin, replacement.atom.size() - 1,
                           replacement.atom.substr(1));
    }
  }
  return value;
}
void delta_anchors() {
  const auto a = parsed("[[18446744073709551615,b0011223344],[],s6162]");
  const auto b = parsed("[[18446744073709551614,bff1122aa44],[0],s6163]");
  const auto d = delta(a, b);
  if (d != "[[[0,0],18446744073709551614],[[0,1],0,bff],[[0,1],3,baa],[[1],[0]]"
           ",[[2],s6163]]" ||
      encoded(apply_delta(a, d)) != encoded(b) ||
      encoded(a) != "[[18446744073709551615,b0011223344],[],s6162]")
    throw std::runtime_error("independent lossless delta anchor failed");
}
const std::vector<std::string> names{
    "fixed_hit",     "ranged_hit",       "miss",         "armor_split",
    "range_gate",    "blocked",          "delayed_hold", "pool_failure",
    "post_failure",  "legacy_isolation", "opposite",     "invulnerable",
    "domain_refusal"};
bool floating(const std::string &s) {
  return s != "blocked" && s != "domain_refusal";
}
unsigned variants(const std::string &s) {
  if (s == "blocked")
    return 3;
  return s == "fixed_hit" || s == "ranged_hit" || s == "miss" ||
                 s == "armor_split" || s == "range_gate" || s == "delayed_hold"
             ? 2
             : 1;
}
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
struct TaskFailure : std::runtime_error {
  TaskFailure() : std::runtime_error("authored task failure") {}
};
struct Replay {
  S::Config config;
  std::string name;
  unsigned variant;
  std::uint32_t seed;
  S::Session session;
  std::array<std::uint32_t, 2> addresses{};
  std::array<std::uint64_t, 2> serials{};
  std::array<S::HpId, 2> ids{};
  std::vector<std::string> events, observations;
  unsigned ready_calls = 0, hp_calls = 0;
  static S::Config settings(unsigned cfg, unsigned v, unsigned local) {
    S::Config c;
    c.model = static_cast<R::Fpu::Model>(cfg / 4);
    c.mul_bias = cfg & 2;
    c.div_nearest = cfg & 1;
    c.peers = 2;
    c.local = local;
    c.network_mode = 1;
    c.seed = (v ? 0xdeadbeef : 1) ^ 0xa5a5a5a5;
    return c;
  }
  Replay(unsigned cfg, unsigned v, std::string name_, unsigned local = 0)
      : config(settings(cfg, v, local)), name(std::move(name_)), variant(v),
        seed(v ? 0xdeadbeef : 1), session(config) {
    S::ObjectBindings bindings;
    bindings.on_run = {"authored_on_run", [this](O::GameObject &o) {
                         effect("run", {n(o.serial)});
                         o.tag = o.tag.value_or(0) + 1;
                       }};
    bindings.on_destroy = {
        "authored_on_destroy", [this](O::GameObject &o, std::int64_t flags) {
          session.with_world([&](O::World &w) { w.destroy(o, flags); });
        }};
    bindings.is_frozen = {"authored_is_frozen", [] { return false; }};
    bindings.res_find = {"authored_res_find",
                         [](const auto &, const auto &) { return 0u; }};
    bindings.end_of_tick = {"authored_end_of_tick", [this] { effect("end"); }};
    session.enable_objects(seed, bindings);
    for (unsigned i = 0; i < 3; ++i)
      session.register_object_task(
          0xf1000001 + i,
          std::array<std::string, 3>{"hp_pre", "hp_apply", "hp_post"}[i],
          20 + 10 * i, 0, [this, i](S::Session &) { task(i); });
    session.with_world([&](O::World &w) {
      for (unsigned i = 0; i < 2; ++i) {
        auto *o = w.new_object(0x400,
                               {R::from_int(i ? 3 : 1),
                                R::from_int(i ? 4 : std::min(variant, 1u)), 0});
        o->flags = 32;
        o->tag = 0;
        addresses[i] = o->id;
        serials[i] = o->serial;
      }
    });
    session.with_world(
        [&](O::World &w) { w.run_every_frame(*w.objects.at(addresses[0])); });
    session.enable_hp();
    for (unsigned i = 0; i < 2; ++i) {
      S::HpInitial initial;
      initial.hp = 0x42c80000;
      if (i == 0 && name == "invulnerable")
        initial.cflags = C::CF_INVULNERABLE;
      ids[i] = session.bind_hp(serials[i], addresses[i], initial);
    }
    for (unsigned i = 0; i < 2; ++i)
      session.seed_hp_backing(ids[i], {{0xc8, 4, 0},
                                       {0x231, 1, 0},
                                       {0x2dc, 4, 0},
                                       {0x2e0, 2, 0},
                                       {0x304, 4, 0},
                                       {0x3b0, 2, 0}});
    S::BaseCombatConfig combat_config;
    if (name == "fixed_hit" && variant)
      combat_config.focus = {R::from_int(1000), 0};
    combat_config.cutscene = name == "ranged_hit" && variant;
    session.enable_base_combat(combat_config);
    session.configure_base_combat(
        ids[0], S::BaseCombatInitial{std::int16_t(
                    name == "miss" || name == "armor_split" ? 100 : 0)});
    session.configure_base_combat(ids[1]);
    if (name == "blocked" && variant == 1)
      session.set_level_pending(true);
  }
  void effect(const std::string &kind, std::vector<std::string> fields = {}) {
    auto s = session.snapshot();
    std::vector<std::string> row{text(kind)};
    row.insert(row.end(), fields.begin(), fields.end());
    row.insert(row.end(), {n(s.world->frame), n(s.network.net_frame),
                           n(s.world->shared_seed)});
    events.push_back(sequence(row));
  }
  void task(unsigned i) {
    if (i == 0)
      ++ready_calls;
    effect(std::array<std::string, 3>{"hp_pre", "hp_apply", "hp_post"}[i]);
    if (i == 0) {
      std::uint32_t value = 0;
      session.with_world([&](O::World &w) { value = w.shared.draw(); });
      effect("shared_pre", {n(value)});
      if (name == "armor_split" && variant) {
        session.with_world([&](O::World &w) { value = w.shared.draw(); });
        effect("shared_extra", {n(value)});
      }
      if (name == "pool_failure")
        session.with_world([](O::World &w) {
          auto count = w.handles.count_free();
          for (std::size_t j = 1; j < count; ++j)
            w.handles.add(w.always_draw, nullptr);
        });
      if (name == "legacy_isolation")
        effect("legacy_draw", {n(session.draw())});
    } else if (i == 1) {
      if (session.snapshot().world->pads[0].held & 1) {
        R::Raw lo = 0x40000000, hi = lo;
        std::int32_t attack = 100;
        std::uint32_t flags = 0x40;
        if (name == "fixed_hit" && variant) {
          lo = hi = 0x3f000001;
          flags |= 0x4000;
        }
        if (name == "ranged_hit") {
          lo = 0x3fa00000;
          hi = 0x40b00000;
          if (variant)
            flags |= 0x4000;
        }
        if (name == "miss") {
          attack = 0;
          if (variant)
            flags |= 0x8000;
        }
        if (name == "armor_split")
          attack = 25;
        if (name == "range_gate") {
          lo = variant ? 0x40400000 : 0;
          hi = variant ? 0x3f800000 : 0;
        }
        if (name == "domain_refusal")
          flags = 1;
        auto result =
            session.apply_base_damage(ids[0], ids[1], lo, hi, attack, flags);
        ++hp_calls;
        effect("damage_result", {n(result)});
      }
    } else {
      std::uint32_t value = 0;
      session.with_world([&](O::World &w) { value = w.shared.draw(); });
      effect("shared_post", {n(value)});
      if (name == "post_failure")
        throw TaskFailure();
    }
  }
  std::string summary(const S::Snapshot &s) {
    std::vector<std::string> hp;
    for (const auto &o : s.hp->adapter.objects)
      hp.push_back(n(o.value.hp));
    const auto actual_ready =
        std::count_if(s.events.begin(), s.events.end(), [](const auto &e) {
          return e.kind == "task" && e.name == "runObjects";
        });
    return sequence({n(actual_ready), n(hp_calls), n(s.network.net_frame),
                     n(s.world->frame), sequence(hp), n(s.failed)});
  }
  std::pair<std::string, std::vector<N::Sent>>
  step(unsigned k, const std::vector<N::ByteVector> &inbox, Value &previous) {
    N::ByteVector sample{1,
                         0,
                         std::uint8_t(128 + k),
                         std::uint8_t(64 + 3 * k),
                         std::uint8_t(255 - k),
                         std::uint8_t(17 * k),
                         std::uint8_t(31 * k),
                         std::uint8_t(47 * k)};
    std::string result;
    std::vector<N::Sent> sent;
    bool hold = name == "delayed_hold" && variant == 1;
    try {
      auto r = session.submit(sample, k * 1234, k & 1, hold && k == 0 ? 4 : 1,
                              (name == "blocked" && variant == 2) ||
                                  (hold && k == 0),
                              hold && k > 0, k, inbox);
      sent = r.sent;
      result = sequence({sends(sent), n(r.held), n(r.steps)});
    } catch (const TaskFailure &) {
      result = sequence({text("error"), text("TaskFailure")});
    } catch (const R::HandleError &) {
      result = sequence({text("error"), text("HandleError")});
    } catch (const S::Error &) {
      result = sequence({text("error"), text("Refusal")});
    }
    auto snap = session.snapshot();
    auto current = parsed(snapshot(snap));
    auto patch = delta(previous, current);
    if (encoded(apply_delta(previous, patch)) != encoded(current))
      throw std::runtime_error("computed delta is not lossless");
    previous = std::move(current);
    return {sequence({result, summary(snap), patch}), sent};
  }
  std::string run() {
    auto previous = parsed(snapshot(session.snapshot()));
    const auto initial = encoded(previous);
    std::vector<std::string> steps;
    for (unsigned k = 0; k < 4; ++k) {
      if (name == "blocked" && variant == 1 && k == 2)
        session.set_level_pending(false);
      const bool delayed = name == "delayed_hold" && variant == 0,
                 no_ready = name == "blocked" && variant == 0;
      std::vector<N::ByteVector> inbox;
      if ((delayed && k == 2) || (!delayed && !no_ready && k == 0))
        inbox.push_back(ready_packet(config));
      steps.push_back(step(k, inbox, previous).first);
    }
    return sequence(
        {initial, sequence(steps), sequence(events), sequence(observations)});
  }
};
std::string opposite(unsigned cfg) {
  std::array<std::unique_ptr<Replay>, 2> peers{
      std::make_unique<Replay>(cfg, 0, "opposite", 0),
      std::make_unique<Replay>(cfg, 0, "opposite", 1)};
  std::vector<std::string> initial, steps;
  std::array<Value, 2> previous;
  std::array<std::vector<N::ByteVector>, 2> queues;
  std::array<std::map<std::int64_t, std::string>, 2> completed;
  for (unsigned i = 0; i < 2; ++i) {
    previous[i] = parsed(snapshot(peers[i]->session.snapshot()));
    initial.push_back(encoded(previous[i]));
  }
  const std::array<unsigned, 12> order{0, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1};
  for (unsigned turn = 0; turn < order.size(); ++turn) {
    auto which = order[turn];
    auto &o = *peers[which];
    std::vector<N::ByteVector> inbox;
    if (turn >= 3) {
      inbox = queues[which];
      queues[which].clear();
    }
    auto old = o.ready_calls;
    auto step = o.step(turn, inbox, previous[which]);
    for (const auto &p : step.second)
      queues.at(static_cast<std::size_t>(p.peer)).push_back(p.data);
    if (o.ready_calls > old) {
      auto snap = o.session.snapshot();
      std::vector<std::string> hp, draws;
      for (const auto &v : snap.hp->adapter.objects)
        hp.push_back(n(v.value.hp));
      for (const auto &v : snap.hp->adapter.world.shared_random->draws)
        draws.push_back(sequence({n(v.value), n(v.state)}));
      completed[which][snap.network.net_frame] =
          sequence({n(snap.world->shared_seed), sequence(hp), sequence(draws)});
    }
    steps.push_back(sequence({n(which), step.first}));
  }
  std::vector<std::string> equal, q;
  for (const auto &[frame, value] : completed[0])
    if (completed[1].count(frame))
      equal.push_back(sequence({n(frame), n(value == completed[1].at(frame))}));
  if (equal.empty())
    throw std::runtime_error("opposite replay lacks common completed tick");
  for (const auto &row : queues) {
    std::vector<std::string> payloads;
    for (const auto &x : row)
      payloads.push_back(bytes(x));
    q.push_back(sequence(payloads));
  }
  return sequence(
      {sequence(initial), sequence(steps),
       sequence({sequence(peers[0]->events), sequence(peers[1]->events)}),
       sequence({sequence(q), sequence(equal)})});
}
std::pair<std::string, std::string>
scenario(const std::string &name, std::int64_t cfg, const std::string &args) {
  auto a = split(args, ';');
  if (a.size() != 2 || cfg < 0 || cfg > 15)
    throw std::runtime_error("invalid scenario/configuration");
  auto id = integer(a[0]), v = integer(a[1]);
  std::int64_t counter = 0;
  bool valid = false;
  for (const auto &op : names)
    for (unsigned c = 0; c < (floating(op) ? 16u : 1u); ++c)
      for (unsigned variant = 0; variant < variants(op); ++variant) {
        if (counter == id)
          valid = name == op && cfg == c && v == variant;
        ++counter;
      }
  if (!valid)
    throw std::runtime_error("inconsistent scenario identity");
  R::ScopedFpu context(
      R::Fpu(static_cast<R::Fpu::Model>(cfg / 4), cfg & 2, cfg & 1));
  if (name == "opposite")
    return {"ok", opposite(unsigned(cfg))};
  Replay r(unsigned(cfg), unsigned(v), name);
  return {"ok", r.run()};
}
// Pinned independently generated corpus coverage and full computed trace
// digest.
constexpr std::uint64_t expected_digest = 0xa3f7caffe63bbbf4ULL;
constexpr std::size_t expected_rows = 276;
int run(const std::vector<std::filesystem::path> &args) {
  delta_anchors();
  if (args.size() < 1 || args.size() > 2)
    throw std::runtime_error(
        "usage: session-combat-trace VECTORS.tsv [TRACE.tsv]");
  constexpr std::uintmax_t max_fixture = 64ULL * 1024 * 1024;
  constexpr std::size_t max_line = 1024 * 1024;
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
  if (line() != "# session_combat_vectors_v1")
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
      std::cerr << "session-combat mismatch at row " << rows + 1 << " ("
                << columns[0] << ")\n";
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
    throw std::runtime_error("incomplete or changed session-combat corpus");
  if (args.size() == 2) {
    output.flush();
    if (!output)
      throw std::runtime_error("trace write failed");
  }
  std::cout << rows
            << " exact session-combat vectors passed; FNV1a64=" << std::hex
            << digest << '\n';
  return 0;
}
void check_terrain_observation_guard() {
  auto state = S::Session().snapshot();
  const auto before = snapshot(state);
  state.terrain.emplace();
  bool refused = false;
  try {
    (void)snapshot(state);
  } catch (const std::runtime_error &error) {
    refused = std::string(error.what()) ==
              "legacy trace cannot omit active terrain extension";
  }
  if (!refused)
    throw std::runtime_error("legacy terrain observation guard did not refuse");
  state.terrain.reset();
  state.creature_movement.emplace();
  refused = false;
  try {
    (void)snapshot(state);
  } catch (const std::runtime_error &error) {
    refused = std::string(error.what()) ==
              "legacy trace cannot omit active creature movement extension";
  }
  if (!refused)
    throw std::runtime_error(
        "legacy creature observation guard did not refuse");
  state.creature_movement.reset();
  if (snapshot(state) != before)
    throw std::runtime_error("disabled terrain changed legacy observation");
}
int entry(const std::vector<std::filesystem::path> &args) {
  try {
    check_terrain_observation_guard();
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
