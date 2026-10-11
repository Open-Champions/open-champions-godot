// Authored composition replay. Expected values are produced independently by
// the pinned Python contracts; this executable computes every output field.
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
std::string snapshot(const S::Snapshot &v) {
  if (v.animation)
    throw std::runtime_error(
        "legacy trace cannot omit active animation extension");
  if (v.creature_movement)
    throw std::runtime_error(
        "legacy trace cannot omit active creature movement extension");
  if (v.terrain)
    throw std::runtime_error(
        "legacy trace cannot omit active terrain extension");
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
} // namespace

namespace {

A::AsmInstruction op(const std::string &name,
                     std::vector<A::AsmValue> args = {}) {
  return {name, std::move(args), {}, {}};
}
A::ByteVector authored_script(unsigned scenario) {
  std::vector<A::AsmLine> program;
  if (scenario == 7)
    program = {op("PROC"), A::Label{"loop:"},
               op("JUMP", {std::string("loop")})};
  else if (scenario == 8)
    program = {op("PROC"), op("HALT", {A::Cell(11)})};
  else
    program = {op("PROC"),
               op("PUSH.C", {A::Cell(3)}),
               op("PUSH.C", {A::Cell(0x15)}),
               op("PUSH.C", {A::Cell(8)}),
               op("SYSREQ.C", {A::Cell(0)}),
               op("STACK", {A::Cell(12)}),
               op("PUSH.C", {A::Cell(0)}),
               op("SYSREQ.C", {A::Cell(1)}),
               op("STACK", {A::Cell(4)}),
               op("STOR.pri", {A::Cell(0)}),
               op("PUSH.C", {A::Cell(0x41100000)}),
               op("PUSH.C", {A::Cell(0x40400000)}),
               op("PUSH.C", {A::Cell(8)}),
               op("SYSREQ.C", {A::Cell(2)}),
               op("STACK", {A::Cell(12)}),
               op("STOR.pri", {A::Cell(4)}),
               op("RETN")};
  A::ImageOptions options;
  options.data.resize(32);
  options.publics = {{"probe", 0}};
  options.natives = {"authored_set", "authored_evaluate", "authored_roll"};
  options.stack_cells = 64;
  options.start = 0;
  return A::build_image(A::assemble(program).cells, options);
}
N::ByteVector ready_packet(const S::Config &config) {
  N::Machine sender(2, 1,
                    N::Fpu(config.model, config.mul_bias, config.div_nearest));
  for (unsigned f = 1; f <= 8; ++f)
    sender.peers[1].set_record(f,
                               N::rec12(f * 17,
                                        {128 + f, 64 + f, 255 - f, f * 9},
                                        {f * 11, f * 13}, f * 123, f & 1),
                               0, 0);
  return sender.build_packet(0, 1, 8, N::PKT_RAW_RELAY, 1, nullptr, true).data;
}
std::pair<std::string, std::string> scenario(const std::string &operation,
                                             std::int64_t config,
                                             const std::string &arguments) {
  const std::array<std::string, 12> names{
      "ready",    "delayed",        "no_ready",       "hold",
      "no_sim",   "level_pending",  "native_failure", "budget_failure",
      "vm_error", "hash_collision", "message",        "message_failure"};
  const std::array<std::uint32_t, 4> seeds{0, 1, 3735928559U, 4294967295U};
  const auto args = split(arguments, ';');
  if (args.size() != 3 || config < 0 || config > 15)
    throw std::runtime_error("scenario argument/config shape");
  const auto id = integer(args[0]), seed = integer(args[1]),
             which = integer(args[2]);
  if (which < 0 || which >= std::int64_t(names.size()) ||
      operation != names[std::size_t(which)] || id < 0 || id >= 768 ||
      id / 48 != config || id % 12 != which ||
      seed != seeds[std::size_t(id / 12) % 4])
    throw std::runtime_error("unknown or inconsistent scenario arguments");
  S::Config c;
  c.model = static_cast<R::Fpu::Model>(config / 4);
  c.mul_bias = config & 2;
  c.div_nearest = config & 1;
  c.peers = 2;
  c.network_mode = 1;
  c.seed = static_cast<std::uint32_t>(seed);
  S::Session session(c);
  R::items::Definition d;
  d.addr = R::items::A_DEFS;
  d.name = "authored-blade";
  d.level = 1;
  d.weight = R::ONE;
  d.count = 1;
  d.damage_min = 3;
  d.damage_max = 9;
  d.price = 17;
  d.armor = 5;
  d.attack = 7;
  d.damage_bits = 1;
  session.add_definition("definition", d);
  S::ItemRecord item;
  item.count = 1;
  session.add_item("item", "definition", item);
  A::NativeMap bindings;
  bindings["authored_set"] =
      [&](A::Machine &,
          const std::vector<A::Cell> &a) -> std::optional<A::Address> {
    if (a.size() != 2)
      throw std::runtime_error("authored set arity");
    return session.set_modifier("item", a[0], a[1], 7, 0);
  };
  bindings["authored_evaluate"] =
      [&](A::Machine &,
          const std::vector<A::Cell> &a) -> std::optional<A::Address> {
    if (!a.empty())
      throw std::runtime_error("authored evaluate arity");
    const auto value = session.evaluate_item("item");
    return A::s32(
        R::add(R::from_int(std::uint32_t(value.attack)), value.weight));
  };
  bindings["authored_roll"] =
      [&](A::Machine &,
          const std::vector<A::Cell> &a) -> std::optional<A::Address> {
    if (a.size() != 2)
      throw std::runtime_error("authored roll arity");
    if (which == 6) {
      session.draw();
      throw std::runtime_error("authored callback failure");
    }
    return A::s32(
        session.roll_damage(std::uint32_t(a[0]), std::uint32_t(a[1])));
  };
  session.add_script("probe", authored_script(unsigned(which)), bindings);
  session.register_task("authored_script", [](S::Session &s) {
    s.execute("probe", 0);
    const auto net = s.network();
    s.set_sync_state(net.sim_frame + 1, net.sync_acc + 17, net.positions);
  });
  if (which == 10 || which == 11) {
    session.register_message(
        7, "authored_message",
        [&](S::Session &s, const N::ByteVector &payload) {
          if (payload.empty())
            throw std::runtime_error("authored message empty");
          s.set_modifier("item", 0x0d, payload[0], 0, 0);
          if (which == 11)
            throw std::runtime_error("authored message failure");
        });
    session.queue_message(7, {25});
  }
  const auto packet = ready_packet(c);
  std::vector<std::string> results;
  if (which == 5)
    session.set_level_pending(true);
  for (unsigned k = 0; k < 4; ++k) {
    if (which == 5 && k == 3)
      session.set_level_pending(false);
    std::vector<N::ByteVector> inbox;
    if ((which == 1 && k == 2) || (which != 1 && which != 2 && k == 0))
      inbox.push_back(packet);
    N::ByteVector sample{std::uint8_t(k + 1),   std::uint8_t(k + 1),
                         std::uint8_t(128 + k), std::uint8_t(64 + 3 * k),
                         std::uint8_t(255 - k), std::uint8_t(17 * k),
                         std::uint8_t(31 * k),  std::uint8_t(47 * k)};
    try {
      auto r = session.submit(sample, k * 1234, k & 1, 1, which == 4 && k < 2,
                              which == 3 && k >= 2, k, inbox);
      results.push_back(sequence({sends(r.sent), n(r.held), n(r.steps)}));
    } catch (const std::exception &) {
      if (!session.failed())
        throw;
      results.push_back(text("failed"));
    }
  }
  if (which == 9) {
    const auto before = session.snapshot();
    auto armor = before.items.at("item").combat_definition;
    R::combat::Item old;
    old.defn = &armor;
    old.count = before.items.at("item").combat_count;
    old.flags = before.items.at("item").combat_flags;
    old.mods = before.items.at("item").combat_mods;
    const auto old_armor = R::combat::item_armor_value(old);
    session.set_modifier("item", 0x0d, 99, 0, 0);
    const auto after = session.snapshot();
    auto changed = after.items.at("item").combat_definition;
    R::combat::Item current;
    current.defn = &changed;
    current.count = after.items.at("item").combat_count;
    current.flags = after.items.at("item").combat_flags;
    current.mods = after.items.at("item").combat_mods;
    results.push_back(
        sequence({n(before.network.peers[0].sync(before.network.net_frame) ==
                    after.network.peers[0].sync(after.network.net_frame)),
                  n(old_armor != R::combat::item_armor_value(current))}));
  }
  return {"ok", sequence({sequence(results), snapshot(session.snapshot())})};
}
// Pinned independently generated corpus coverage and full computed trace
// digest.
constexpr std::uint64_t expected_digest = 0xac8aa765117ef5d9ULL;
constexpr std::size_t expected_rows = 768;
int run(const std::vector<std::filesystem::path> &args) {
  if (args.size() < 1 || args.size() > 2)
    throw std::runtime_error("usage: session-replay VECTORS.tsv [TRACE.tsv]");
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
  if (line() != "# session_vectors_v1")
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
  std::cout << rows << " exact session vectors passed; FNV1a64=" << std::hex
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
  state.animation.emplace();
  refused = false;
  try {
    (void)snapshot(state);
  } catch (const std::runtime_error &error) {
    refused = std::string(error.what()) ==
              "legacy trace cannot omit active animation extension";
  }
  if (!refused)
    throw std::runtime_error(
        "legacy animation observation guard did not refuse");
  state.animation.reset();
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
