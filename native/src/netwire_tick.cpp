#include "champions/netwire.hpp"
#include <algorithm>
#include <limits>

namespace champions::simulation::rta::netwire {
namespace {
std::uint32_t read32(const std::uint8_t *p) {
  std::uint32_t v = 0;
  for (unsigned i = 0; i < 4; ++i)
    v |= std::uint32_t(p[i]) << (i * 8);
  return v;
}
void put32(std::uint8_t *p, std::uint32_t v) {
  for (unsigned i = 0; i < 4; ++i)
    p[i] = std::uint8_t(v >> (i * 8));
}
Integer peer_count(Integer n) {
  if (n > Integer(MAX_WORK))
    throw ResourceLimit("peer loop work budget");
  return std::max<Integer>(0, n);
}
std::size_t slot(Integer n) { return python_index(n, 4); }
void bounded_steps(Integer n) {
  if (n > Integer(MAX_WORK))
    throw ResourceLimit("tick work budget");
}
} // namespace
std::uint32_t sync_hash(Integer seed, Integer acc,
                        const std::vector<std::optional<Position>> &positions) {
  if (positions.size() > MAX_WORK)
    throw ResourceLimit("sync position budget");
  std::uint32_t total = std::uint32_t(seed) + std::uint32_t(acc);
  for (const auto &p : positions)
    if (p)
      for (auto raw : *p)
        total += std::uint32_t(Fpu::to_int(raw));
  return total;
}
std::array<Raw, 2> decode_stick(Integer x, Integer y, const Fpu &f) {
  // The documented inputs are bytes; wider host integers use the same raw32
  // conversion.
  auto fy = f.mul(f.from_int(std::uint32_t(y) - 128u), STICK_SCALE);
  auto fx = f.mul(f.from_int(std::uint32_t(x) - 128u), STICK_SCALE);
  auto length = f.sqrt(f.add(f.mul(fx, fx), f.mul(fy, fy)));
  if (f.lt(length, STICK_DEAD))
    return {0, 0};
  auto scale = f.mul(f.sub(F_ONE, f.div(STICK_DEAD, length)), STICK_GAIN);
  return {f.mul(fx, scale), f.mul(fy, scale)};
}
Pad &pad_apply(Pad &pad, const ByteVector &record, std::array<Integer, 2> &prev,
               PadGlobals *globals, const Fpu &f) {
  if (record.size() < 8)
    throw ValueError("pad record requires eight bytes");
  PadGlobals defaults;
  auto &g = globals ? *globals : defaults;
  std::uint32_t word =
      std::uint32_t(record[0]) | (std::uint32_t(record[1]) << 8);
  auto previous = std::uint32_t(prev[0]);
  if (g.start_block) {
    g.start_block = checked_sub(g.start_block, 1);
    word &= 0xf7ff;
  }
  std::array<Raw, 22> values{};
  const auto flag = [&](std::uint32_t mask) {
    return (word & mask) ? F_ONE : Raw(0);
  };
  for (const auto &p :
       std::array<std::pair<unsigned, unsigned>, 10>{{{0x28, 0x2000},
                                                      {0x2c, 0x8000},
                                                      {0x30, 0x1000},
                                                      {0x34, 0x4000},
                                                      {0x40, 0x20},
                                                      {0x44, 0x80},
                                                      {0x48, 0x10},
                                                      {0x4c, 0x40},
                                                      {0x3c, 1},
                                                      {0x54, 2}}})
    values[p.first / 4] = flag(p.second);
  values[0x38 / 4] = f.mul(f.from_int(record[6]), PRESSURE_SCALE);
  values[0x50 / 4] = f.mul(f.from_int(record[7]), PRESSURE_SCALE);
  auto second = decode_stick(record[4], record[5], f),
       first = decode_stick(record[2], record[3], f);
  values[0x1c / 4] = second[0];
  values[0x20 / 4] = second[1];
  values[0x14 / 4] = first[0];
  values[0x18 / 4] = first[1];
  if (f.lt(DIGITAL, first[0]))
    word |= 0x800000;
  if (f.lt(first[0], DIGITAL | 0x80000000u))
    word |= 0x400000;
  if (f.lt(DIGITAL, first[1]))
    word |= 0x200000;
  if (f.lt(first[1], DIGITAL | 0x80000000u))
    word |= 0x100000;
  auto pressed = (word ^ previous) & word, repeat = pressed;
  if (!word || Integer(word) != prev[0])
    prev[1] = 0;
  else {
    prev[1] = std::uint32_t(prev[1]) + std::uint32_t(1);
    if (s32(prev[1]) >= 21 && (prev[1] & 3) == 0)
      repeat = word;
  }
  if (g.mute) {
    for (unsigned offset = 0x14; offset < 0x58; offset += 4)
      values[offset / 4] = 0;
    word &= std::uint32_t(g.allow);
    pressed &= std::uint32_t(g.allow);
    repeat &= std::uint32_t(g.allow);
  }
  put32(pad.data() + 8, word);
  put32(pad.data() + 12, pressed);
  put32(pad.data() + 16, repeat);
  for (unsigned offset = 0x14; offset < 0x58; offset += 4)
    if (offset != 0x24)
      put32(pad.data() + offset, values[offset / 4]);
  prev[0] = word;
  return pad;
}
std::uint16_t launch_checksum(const ByteVector &chunk) {
  if (chunk.size() > MAX_BYTES)
    throw ResourceLimit("launch chunk budget");
  std::uint32_t total = 0x217;
  if (chunk.empty())
    return std::uint16_t(total);
  auto first = std::uint32_t(s8(chunk[0]));
  for (std::size_t i = 0; i < chunk.size(); ++i) {
    auto value = s8(chunk[i]);
    if ((i & 1) == 0)
      total -= std::uint32_t(value & 0x8f37) * std::uint32_t(i & 7);
    else
      total += std::uint32_t(value * 3) ^ 0x30e15fu;
    total += first;
  }
  return std::uint16_t(total);
}
ByteVector launch_packet(Integer id, Integer kind, Integer argument,
                         const ByteVector &payload,
                         std::optional<Integer> size) {
  if (payload.size() > MAX_BYTES - 12)
    throw ResourceLimit("launch packet budget");
  auto total = payload.size() + 12;
  if (size) {
    if (*size < 0 || std::uint64_t(*size) < total)
      throw ValueError("payload does not fit");
    if (std::uint64_t(*size) > MAX_BYTES)
      throw ResourceLimit("launch packet budget");
    total = std::size_t(*size);
  }
  ByteVector out(total);
  put32(out.data(), std::uint32_t(id));
  put32(out.data() + 4, std::uint32_t(kind));
  put32(out.data() + 8, std::uint32_t(argument));
  std::copy(payload.begin(), payload.end(), out.begin() + 12);
  return out;
}
std::pair<ByteVector, std::uint32_t>
launch_data_packet(Integer id, Integer sequence, const ByteVector &chunk) {
  auto arg =
      (std::uint32_t(launch_checksum(chunk)) << 16) | std::uint32_t(sequence);
  return {launch_packet(id, 4, arg, chunk, std::nullopt), arg};
}
std::optional<LaunchPacket> parse_launch_packet(const ByteVector &data) {
  if (data.size() < LAUNCH_MIN)
    return {};
  if (data.size() > MAX_BYTES)
    throw ResourceLimit("launch packet budget");
  return LaunchPacket{read32(data.data()), read32(data.data() + 4),
                      read32(data.data() + 8),
                      ByteVector(data.begin() + 12, data.end())};
}
bool launch_data_ok(Integer argument, const ByteVector &chunk) {
  return launch_checksum(chunk) == ((std::uint64_t(argument) >> 16) & 0xffff);
}

NetMainResult Machine::net_main(const ByteVector &sample, Integer yaw,
                                Integer camera_flag, Integer frame_step,
                                bool no_sim, bool hold_arg,
                                Integer render_frame,
                                const std::vector<ByteVector> &inbox) {
  outbox.clear();
  const auto local = local_slot();
  if (inbox.size() > MAX_WORK)
    throw ResourceLimit("inbox work budget");
  // Python list iteration observes appended/replaced future datagrams. Copy the
  // current packet so callback mutation cannot invalidate its storage.
  for (std::size_t i = 0; i < inbox.size(); ++i) {
    if (i == MAX_WORK)
      throw ResourceLimit("inbox callback work budget");
    const auto data = inbox[i];
    dispatch(data);
  }
  if (!no_sim)
    for (Integer i = 0, n = peer_count(num_peers); i < n; ++i) {
      auto &p = peers[slot(i)];
      auto counter = p.kicked_notice;
      if (counter) {
        p.kicked_notice = checked_add(counter, 1);
        if (counter > 120)
          p.kicked_notice = 0;
        if (counter % 10 == 0)
          outbox.push_back({i, p.ip, p.port, kicked_notice(), {}});
      }
    }
  for (Integer i = 0, n = peer_count(num_peers); i < n; ++i) {
    auto &p = peers[slot(i)];
    if (!(p.local || p.dropped || p.kick)) {
      auto quiet = p.silence;
      p.silence = checked_add(quiet, 1);
      if (quiet > 900) {
        p.kick = 1;
        p.drop_frame = -1;
        kick_acks = {};
        kick_state = 1;
      }
    }
  }
  if (kick_state == 1 && render_frame % 30 == 0) {
    bool everyone = true;
    for (Integer i = 0, n = peer_count(num_peers); i < n; ++i) {
      auto &p = peers[slot(i)];
      if (!(p.local || p.dropped || p.kick) && !kick_acks[slot(i)]) {
        Integer mask = 0;
        for (Integer j = 0, m = peer_count(num_peers); j < m; ++j)
          if (peers[slot(j)].kick == 1)
            mask |= Integer(1) << j;
        outbox.push_back({i, p.ip, p.port, kick_request(local, mask), {}});
        everyone = false;
      }
    }
    if (everyone) {
      kick_state = 2;
      for (Integer i = 0, n = peer_count(num_peers); i < n; ++i)
        if (peers[slot(i)].kick)
          peers[slot(i)].drop_frame = checked_add(peers[slot(i)].newest, 1);
    }
  }
  if (kick_state == 2 && render_frame % 30 == 0) {
    log.push_back("kick agreed");
    for (Integer i = 0, n = peer_count(num_peers); i < n; ++i)
      if (peers[slot(i)].kick == 1)
        peers[slot(i)].kick = 2;
    kick_state = 0;
  }
  auto min_sim = net_frame, min_lead = checked_sub(lif, net_frame);
  for (Integer i = 0, n = peer_count(num_peers); i < n; ++i) {
    auto &p = peers[slot(i)];
    if (!(p.local || p.dropped || p.kick)) {
      min_lead = std::min(min_lead, p.lead);
      min_sim = std::min(min_sim, p.sim);
    }
  }
  auto excess = checked_sub(checked_sub(lif, net_frame), min_lead);
  if (sample.size() < 8)
    throw ValueError("sample requires eight bytes");
  WireRecord record{};
  std::copy_n(sample.begin(), 8, record.begin());
  auto buttons = std::uint32_t(record[0]) | (std::uint32_t(record[1]) << 8);
  put32(record.data() + 8, std::uint32_t(s16(std::uint64_t(yaw) & 0xfffe)) |
                               (camera_flag ? 1u : 0u));
  auto &lock = input_lock[slot(local)];
  if (lock[0]) {
    lock[0] = checked_sub(lock[0], 1);
    record[6] = record[7] = 0;
    buttons &= ~std::uint32_t(lock[1]) & 0xffff;
    if (lock[2])
      std::fill(record.begin() + 2, record.begin() + 6, 128);
  }
  record[0] = std::uint8_t(buttons);
  record[1] = std::uint8_t(buttons >> 8);
  bool held = checked_sub(lif, min_sim) > 38;
  if (excess < 5)
    eq = 0;
  else {
    eq = checked_add(eq, 1);
    if (eq > 30) {
      eq = 0;
      held = true;
    }
  }
  if (hold_arg)
    held = true;
  auto window = win;
  win = checked_add(window, 1);
  auto lowest = min_ready;
  if (window > 60) {
    win = 0;
    min_ready = 100;
    if (lowest > 2)
      held = true;
  }
  if (lif < 3)
    held = false;
  if (!held) {
    bounded_steps(frame_step);
    for (Integer i = 0; i < frame_step; ++i) {
      auto callback = sample_garbage;
      Integer start = callback ? callback(lif) : 0;
      peers[slot(local)].set_record(lif, record, start, 0);
      lif = checked_add(lif, 1);
    }
  }
  attach_messages();
  auto targets = remote_active();
  cycle =
      send_cycles.at(python_index(Integer(targets.size()), send_cycles.size()));
  if (modem) { // Python's arithmetic right shift rounds negative values
               // downward.
    auto half = cycle / 2;
    if (cycle < 0 && cycle % 2)
      half = checked_sub(half, 1);
    cycle = checked_add(cycle, half);
  }
  for (std::size_t turn = 0; turn < targets.size(); ++turn) {
    // At most four targets; avoid an intermediate multiply overflow.
    auto n = Integer(targets.size()), q = cycle / n, r = cycle % n;
    if (r < 0) {
      q = checked_sub(q, 1);
      r += n;
    }
    auto phase_at = checked_add(q * Integer(turn), (r * Integer(turn)) / n);
    if (phase == phase_at)
      send_input(targets[turn]);
  }
  phase = checked_add(phase, 1);
  if (phase >= cycle)
    phase = 0;
  for (Integer i = 0, n = peer_count(num_peers); i < n; ++i) {
    auto &p = peers[slot(i)];
    if (p.kick && !p.dropped && p.drop_frame == net_frame) {
      log.push_back("drop " + std::to_string(i));
      p.dropped = 1;
      if (is_first_active()) {
        auto callback = first_active_hook;
        if (callback)
          callback(i);
      }
    }
  }
  Integer ready = 0, limit = checked_sub(checked_sub(lif, net_frame), 1);
  while (ready < limit) {
    auto frame = checked_add(net_frame, ready);
    bool missing = false;
    for (Integer i = 0, n = peer_count(num_peers); i < n; ++i)
      if (!peers[slot(i)].dropped &&
          std::get<0>(peers[slot(i)].record(frame)) != std::uint32_t(frame)) {
        missing = true;
        break;
      }
    if (missing)
      break;
    if (ready == Integer(MAX_WORK))
      throw ResourceLimit("ready scan work budget");
    ++ready;
  }
  min_ready = std::min(min_ready, ready);
  auto steps = no_sim ? Integer(0) : std::min(ready, frame_step);
  if (steps < 1)
    stall = checked_add(stall, 1);
  else {
    stall = 0;
    tick(steps);
  }
  if (!ended && stall > 300)
    long_stall = checked_add(long_stall, 1);
  else
    long_stall = 0;
  return {outbox, held, steps};
}
void Machine::tick(Integer steps) {
  const auto local = local_slot();
  bounded_steps(steps);
  for (Integer step = 0; step < steps; ++step) {
    if (level_pending)
      continue;
    if (before_frame) {
      auto callback = before_frame;
      callback(*this);
    }
    auto frame = net_frame;
    for (Integer i = 0, n = peer_count(num_peers); i < n; ++i) {
      auto &p = peers[slot(i)];
      if (p.dropped)
        continue;
      auto record = std::get<1>(p.record(frame));
      if (i == local)
        record = local_roundtrip(record, sim_enc, sim_dec, fpu);
      pad_apply(pads[slot(i)], record, p.pad_prev, &pad_globals, fpu);
      cam_yaw[slot(i)] =
          std::uint32_t(record[8]) | (std::uint32_t(record[9]) << 8);
      if (p.msg_count(frame)) {
        auto messages = frame_messages(i, frame);
        for (const auto &message : messages.first) {
          auto found = handlers.find(message.first);
          if (found != handlers.end()) {
            auto callback = found->second;
            if (callback)
              callback(message.second);
          }
        }
        if (i != local)
          p.msg_read = messages.second;
      }
    }
    for (Integer i = 0, n = peer_count(num_peers); i < n; ++i) {
      auto &p = peers[slot(i)];
      if (p.kick && p.drop_frame == net_frame && positions[slot(i)]) {
        auto callback = on_kill;
        if (callback)
          callback(i);
      }
    }
    {
      auto callback = run_tasks;
      if (callback)
        callback(*this);
    }
    net_frame = checked_add(net_frame, 1);
    auto end = num_players < 0
                   ? std::max<Integer>(0, checked_add(4, num_players))
                   : std::min<Integer>(4, num_players);
    std::vector<std::optional<Position>> selected(positions.begin(),
                                                  positions.begin() + end);
    auto value = sync_hash(rand_seed, sync_acc, selected);
    peers[slot(local)].set_sync(net_frame, sim_frame, value);
    for (Integer i = 0, n = peer_count(num_peers); i < n; ++i) {
      auto &p = peers[slot(i)];
      if (p.local || p.dropped)
        continue;
      auto theirs = p.sync(net_frame);
      if (theirs[0] == std::uint32_t(net_frame) && theirs[0] &&
          theirs[2] != value) {
        p.oos = 1;
        log.push_back("out of sync " + std::to_string(i));
      }
    }
  }
}
} // namespace champions::simulation::rta::netwire
