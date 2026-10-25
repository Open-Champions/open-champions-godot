#pragma once
#include "champions/detail/checkpoint.hpp"
#include "champions/session.hpp"
#include "champions/session_control.hpp"
namespace champions::simulation::rta::detail {
inline void checkpoint_fields(Checkpoint &a, creature_hp::Write &v) {
  a.fields(v.address, v.size, v.value);
  Checkpoint::require(v.size <= 8, "checkpoint write width");
}
inline void checkpoint_fields(Checkpoint &a, creature_hp::Call &v) {
  a.fields(v.name, v.args);
}
inline void checkpoint_fields(Checkpoint &a, creature_hp::Event &v) {
  a.fields(v.kind, v.write, v.call);
  Checkpoint::require(unsigned(v.kind) <= 1, "checkpoint HP event kind");
}
inline void checkpoint_fields(Checkpoint &a, creature_movement::Value &v) {
  // Match the existing callback complexity and inactive-payload contract.
  namespace M = creature_movement;
  if (!a.value_depth && !a.value_group)
    a.value_nodes = a.value_bytes = 0;
  Checkpoint::require(a.value_depth <= M::MAX_VALUE_DEPTH &&
                          ++a.value_nodes <= M::MAX_VALUE_NODES,
                      "checkpoint callback value budget");
  ++a.value_depth;
  try {
    a.fields(v.kind, v.scalar, v.vec);
    auto n = a.count(v.items.size(), M::MAX_CONTACTS);
    if (a.reading)
      v.items.resize(n);
    for (auto &item : v.items)
      a.value(item);
    a.value(v.bytes);
    Checkpoint::require(
        unsigned(v.kind) <= 5 &&
            (v.kind == M::ValueKind::Integer || !v.scalar) &&
            (v.kind == M::ValueKind::Vector || v.vec == M::Vec3{}) &&
            (v.kind == M::ValueKind::List || v.kind == M::ValueKind::Tuple ||
             v.items.empty()) &&
            (v.kind == M::ValueKind::Buffer || v.bytes.empty()),
        "checkpoint callback inactive payload");
    Checkpoint::require(v.bytes.size() <= M::MAX_BUFFER_BYTES - a.value_bytes,
                        "checkpoint callback buffer budget");
    a.value_bytes += unsigned(v.bytes.size());
  } catch (...) {
    --a.value_depth;
    throw;
  }
  --a.value_depth;
}
inline void checkpoint_fields(Checkpoint &a, creature_movement::Call &v) {
  a.value(v.name);
  auto n = a.count(v.args.size(), creature_movement::MAX_CONTACTS);
  if (a.reading)
    v.args.resize(n);
  a.value_group = true;
  a.value_nodes = a.value_bytes = 0;
  try {
    for (auto &arg : v.args)
      a.value(arg);
  } catch (...) {
    a.value_group = false;
    throw;
  }
  a.value_group = false;
}
inline void checkpoint_fields(Checkpoint &a, creature_movement::Event &v) {
  a.fields(v.kind, v.write, v.call);
  Checkpoint::require(unsigned(v.kind) <= 1, "checkpoint movement event kind");
}
inline void checkpoint_fields(Checkpoint &a,
                              session::CreatureCallbackResult &v) {
  a.fields(v.call, v.value);
}
inline void checkpoint_fields(Checkpoint &a, session::Event &v) {
  a.fields(v.kind, v.name, v.frame, v.values);
}
inline void checkpoint_fields(Checkpoint &a, world_queries::MoveResult &v) {
  a.fields(v.position, v.vz, v.bits);
}
inline void checkpoint_fields(Checkpoint &a, netwire::Channel &v) {
  a.fields(v.index, v.pred, v.value, v.velocity);
}
inline void checkpoint_fields(Checkpoint &a, netwire::CodecState &v) {
  a.fields(v.ch, v.yaw_prev, v.yaw_pred, v.yaw_delta, v.pad46, v.count);
}
inline void checkpoint_fields(Checkpoint &a, netwire::Peer &v) {
  a.fields(v.ip, v.port, v.unknown8, v.recv_state, v.send_frame, v.send_state,
           v.ring, v.sync_ring, v.msg_ring, v.msg_write, v.msg_read, v.pad_prev,
           v.newest, v.acked, v.lead, v.sim, v.silence, v.heard, v.local,
           v.dropped, v.kick, v.oos, v.drop_frame, v.kicked_notice);
}
inline void checkpoint_fields(Checkpoint &a, netwire::Sent &v) {
  a.fields(v.peer, v.ip, v.port, v.data, v.undefined);
}
inline void checkpoint_fields(Checkpoint &a, netwire::Machine &v) {
  a.fields(v.network_mode, v.num_peers, v.peers, v.lif, v.last_sent,
           v.net_frame, v.cycle, v.phase, v.modem, v.last_hash_frame, v.stall,
           v.ended, v.eq, v.min_ready, v.win, v.long_stall, v.kicked,
           v.kick_state, v.kick_acks, v.cam_yaw, v.msg_attached, v.input_lock,
           v.sim_enc, v.sim_dec, v.send_cycles, v.sim_frame, v.rand_seed,
           v.sync_acc, v.level_pending, v.num_players, v.positions, v.pads,
           v.pad_globals.start_block, v.pad_globals.mute, v.pad_globals.allow);
  a.history(v.outbox, v.log);
}
inline void checkpoint_fields(Checkpoint &a, combat::EventValue &v) {
  auto kind = std::uint32_t(v.value.index());
  a.value(kind);
  Checkpoint::require(kind <= 5, "checkpoint combat event kind");
  if (a.reading)
    switch (kind) {
    case 0:
      v.value = std::monostate{};
      break;
    case 1:
      v.value = std::int64_t{};
      break;
    case 2:
      v.value = std::uint64_t{};
      break;
    case 3:
      v.value = false;
      break;
    case 4:
      v.value = combat::Text{};
      break;
    case 5:
      v.value = std::vector<Raw>{};
      break;
    }
  std::visit(
      [&](auto &x) {
        using T = std::decay_t<decltype(x)>;
        if constexpr (!std::is_same<T, std::monostate>::value)
          a.value(x);
      },
      v.value);
}
} // namespace champions::simulation::rta::detail

namespace champions::simulation::rta::detail {
inline void checkpoint_fields(Checkpoint &a, session_recovery::Initial &i) {
  a.fields(i.slot, i.record, i.player_class, i.health, i.max_health, i.mana,
           i.max_mana, i.pending_health, i.health_rate, i.pending_mana,
           i.mana_rate, i.health_bonus, i.mana_bonus, i.passive_masks,
           i.passive_levels, i.ranks, i.equipment, i.spells,
           i.blessed_type_token, i.copy_at_setup);
  session_recovery::validate(i);
}
// This codec is closed to the only two services reachable in recovery's
// no-spell/empty-rank domain. No callback, pointer or general player state.
inline void checkpoint_fields(Checkpoint &a, player::Call &c) {
  a.fields(c.service, c.name);
  Checkpoint::require(c.name.empty() &&
                          (c.service == player::Service::BlessedType ||
                           c.service == player::Service::FindSpell),
                      "checkpoint recovery call outside closed service domain");
  auto n = a.count(c.args.size(), 2);
  Checkpoint::require(n ==
                          (c.service == player::Service::BlessedType ? 0u : 2u),
                      "checkpoint recovery call arity");
  if (a.reading)
    c.args.assign(n, std::uint32_t(0));
  for (auto &v : c.args) {
    Checkpoint::require(std::holds_alternative<std::uint32_t>(v),
                        "checkpoint recovery scalar argument");
    a.value(std::get<std::uint32_t>(v));
  }
}
inline void checkpoint_fields(Checkpoint &a, player::Event &e) {
  a.value(e.is_call);
  if (e.is_call)
    a.value(e.call);
  else
    a.value(e.write);
}
} // namespace champions::simulation::rta::detail

namespace champions::simulation::rta::detail {
inline void checkpoint_fields(Checkpoint &a, session::TurnOperation &v) {
  a.fields(v.kind, v.binding, v.mode, v.point, v.frame, v.result, v.completed);
}
inline void checkpoint_fields(Checkpoint &a, session_control::Facing &v) {
  a.fields(v.policy, v.initial.rate, v.initial.angular_velocity, v.initial.slow,
           v.mode);
}
} // namespace champions::simulation::rta::detail
