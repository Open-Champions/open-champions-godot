#include "champions/netwire.hpp"
#include <algorithm>
#include <limits>
namespace champions::simulation::rta::netwire {
namespace {
template <class C> std::uint32_t u32(const C &b, std::size_t p) {
  if (p > b.size() || b.size() - p < 4)
    throw MalformedPacket("short word");
  return std::uint32_t(b[p]) | (std::uint32_t(b[p + 1]) << 8) |
         (std::uint32_t(b[p + 2]) << 16) | (std::uint32_t(b[p + 3]) << 24);
}
template <class C> void put(C &b, std::size_t p, Integer n) {
  for (unsigned i = 0; i < 4; ++i)
    b[p + i] = std::uint8_t(std::uint64_t(n) >> (8 * i));
}
void word(ByteVector &b, Integer n) {
  auto p = b.size();
  b.resize(p + 4);
  put(b, p, n);
}
template <class C> void append(ByteVector &b, const C &c) {
  b.insert(b.end(), c.begin(), c.end());
}
std::size_t ring_slot(Integer n) { return std::uint64_t(n) & 255; }
std::size_t msg_slot(Integer n) { return std::uint64_t(n) & 8191; }
bool bit(Integer mask, Integer index) {
  return index >= 64 ? mask < 0 : bool((std::uint64_t(mask) >> index) & 1);
}
Peer &peer(Machine &m, Integer n) {
  return m.peers[python_index(n, m.peers.size())];
}
const Peer &peer(const Machine &m, Integer n) {
  return m.peers[python_index(n, m.peers.size())];
}
void bytes_limit(std::size_t n) {
  if (n > MAX_BYTES)
    throw ResourceLimit("session byte budget");
}
void loops(Integer n) {
  if (n > Integer(MAX_WORK))
    throw ResourceLimit("session work budget");
}
std::vector<WireRecord> records(const Peer &p, Integer first, Integer end) {
  auto n = checked_sub(end, first);
  if (n <= 0)
    return {};
  if (n > Integer(MAX_FRAMES))
    throw ResourceLimit("session frame budget");
  std::vector<WireRecord> out;
  out.reserve(std::size_t(n));
  for (Integer i = 0; i < n; ++i)
    out.push_back(p.wire_record(checked_add(first, i)));
  return out;
}
WireRecord wire(const ByteVector &b, std::size_t p) {
  if (p > b.size() || b.size() - p < 12)
    throw MalformedPacket("short wire record");
  WireRecord r{};
  std::copy_n(b.begin() + std::ptrdiff_t(p), 12, r.begin());
  return r;
}
} // namespace
ByteVector header(Integer kind, Integer last, Integer slot, Integer mask,
                  Integer byte9, Integer count, Integer ack_delta) {
  ByteVector out(12);
  put(out, 0, kind);
  put(out, 4, last);
  out[8] = std::uint8_t(std::uint64_t(slot) | (std::uint64_t(mask) << 4));
  out[9] = std::uint8_t(byte9);
  out[10] = std::uint8_t(count);
  out[11] = std::uint8_t(ack_delta);
  return out;
}
Integer Packet::first() const {
  return checked_add(checked_sub(last, count), 1);
}
Integer Packet::ack() const { return checked_add(last, ack_delta); }
Integer Packet::sim_frame() const { return checked_sub(last, byte9); }
Packet parse_packet(const ByteVector &data) {
  bytes_limit(data.size());
  if (data.size() < 16)
    throw MalformedPacket("short packet");
  Packet p;
  p.kind = u32(data, 0);
  p.last = s32(u32(data, 4));
  p.slot = data[8] & 15;
  p.mask = data[8] >> 4;
  p.byte9 = data[9];
  p.count = data[10];
  p.ack_delta = s8(data[11]);
  if (p.kind != PKT_INPUT && p.kind != PKT_RAW_RELAY)
    throw MalformedPacket("wrong packet kind");
  if (!p.count)
    throw MalformedPacket("no frames");
  if (p.kind == PKT_INPUT) {
    p.symbols = read_frames(ByteVector(data.begin() + 12, data.end()), p.count);
    p.stream_bytes = (p.symbols->bits + 7) >> 3;
  } else {
    p.stream_bytes = p.count * 12;
    if (data.size() < std::size_t(12 + p.stream_bytes))
      throw MalformedPacket("short relay");
    p.raw_records = std::vector<WireRecord>{};
    for (Integer i = 0; i < p.count; ++i)
      p.raw_records->push_back(wire(data, 12 + std::size_t(i) * 12));
  }
  p.sync_offset = (12 + p.stream_bytes + 3) & ~Integer(3);
  if (std::uint64_t(p.sync_offset) + 4 > data.size())
    throw MalformedPacket("missing sync word");
  auto offset = std::size_t(p.sync_offset);
  p.sync_word = u32(data, offset);
  offset += 4;
  if (p.sync_word == SYNC_MARKER) {
    if (data.size() - offset < 12)
      throw MalformedPacket("short sync");
    p.sync = std::array<std::uint32_t, 3>{
        u32(data, offset), u32(data, offset + 4), u32(data, offset + 8)};
    offset += 12;
  }
  p.blocks_offset = Integer(offset);
  while (offset != data.size()) {
    if (data.size() - offset < 2 ||
        std::size_t(data[offset + 1]) > data.size() - offset - 2)
      throw MalformedPacket("short message block");
    auto size = data[offset + 1];
    p.blocks.push_back(
        {p.last - data[offset],
         ByteVector(data.begin() + std::ptrdiff_t(offset + 2),
                    data.begin() + std::ptrdiff_t(offset + 2 + size))});
    offset += size + 2;
  }
  return p;
}
ByteVector kick_request(Integer slot, Integer mask) {
  ByteVector b;
  word(b, PKT_KICK_REQUEST);
  word(b, slot);
  word(b, mask);
  return b;
}
ByteVector kicked_notice() {
  ByteVector b;
  word(b, PKT_KICKED);
  return b;
}
ByteVector direct_message(Integer index, const ByteVector &payload) {
  if (index < 0 || index >= 4)
    throw ValueError("handler index");
  bytes_limit(payload.size());
  ByteVector b;
  word(b, PKT_DIRECT + index);
  append(b, payload);
  return b;
}
ByteVector lan_packet(bool start) {
  ByteVector b;
  word(b, start ? PKT_LAN_START : PKT_LAN_ANNOUNCE);
  return b;
}
std::tuple<std::uint32_t, WireRecord, std::uint32_t, std::uint32_t>
Peer::record(Integer frame) const {
  auto p = ring_slot(frame) * 24;
  return {u32(ring, p), wire_record(frame), u32(ring, p + 16),
          u32(ring, p + 20)};
}
WireRecord Peer::wire_record(Integer frame) const {
  WireRecord r{};
  std::copy_n(ring.begin() + std::ptrdiff_t(ring_slot(frame) * 24 + 4), 12,
              r.begin());
  return r;
}
void Peer::set_record(Integer frame, const WireRecord &r,
                      std::optional<Integer> start,
                      std::optional<Integer> count) {
  auto p = ring_slot(frame) * 24;
  put(ring, p, frame);
  std::copy(r.begin(), r.end(), ring.begin() + std::ptrdiff_t(p + 4));
  if (start)
    put(ring, p + 16, *start);
  if (count)
    put(ring, p + 20, *count);
}
std::uint32_t Peer::msg_count(Integer f) const {
  return u32(ring, ring_slot(f) * 24 + 20);
}
std::uint32_t Peer::msg_start(Integer f) const {
  return u32(ring, ring_slot(f) * 24 + 16);
}
std::array<std::uint32_t, 3> Peer::sync(Integer f) const {
  auto p = ring_slot(f) * 12;
  return {u32(sync_ring, p), u32(sync_ring, p + 4), u32(sync_ring, p + 8)};
}
void Peer::set_sync(Integer f, Integer saved, Integer value) {
  auto p = ring_slot(f) * 12;
  put(sync_ring, p, f);
  put(sync_ring, p + 4, saved);
  put(sync_ring, p + 8, value);
}
bool Peer::inactive() const { return dropped || kick || oos; }
ByteVector Peer::pack() const {
  ByteVector b;
  append(b, ip);
  word(b, port);
  append(b, unknown8);
  append(b, recv_state.pack());
  word(b, send_frame);
  append(b, send_state.pack());
  append(b, ring);
  append(b, sync_ring);
  append(b, msg_ring);
  for (auto v : {msg_write, msg_read, pad_prev[0], pad_prev[1], newest, acked,
                 lead, sim, silence, heard})
    word(b, v);
  for (auto v : {local, dropped, kick, oos})
    b.push_back(std::uint8_t(v));
  word(b, drop_frame);
  word(b, kicked_notice);
  return b;
}
Peer Peer::unpack(const ByteVector &b) {
  if (b.size() != PEER_SIZE)
    throw ValueError("peer image size");
  Peer p;
  std::copy_n(b.begin(), 4, p.ip.begin());
  p.port = u32(b, 4);
  std::copy_n(b.begin() + 8, 8, p.unknown8.begin());
  p.recv_state =
      CodecState::unpack(ByteVector(b.begin() + 0x10, b.begin() + 0x5c));
  p.send_frame = s32(u32(b, 0x5c));
  p.send_state =
      CodecState::unpack(ByteVector(b.begin() + 0x60, b.begin() + 0xac));
  std::copy_n(b.begin() + RING_OFFSET, p.ring.size(), p.ring.begin());
  std::copy_n(b.begin() + SYNC_OFFSET, p.sync_ring.size(), p.sync_ring.begin());
  std::copy_n(b.begin() + MSG_OFFSET, p.msg_ring.size(), p.msg_ring.begin());
  p.msg_write = u32(b, 0x44ac);
  p.msg_read = u32(b, 0x44b0);
  p.pad_prev = {u32(b, 0x44b4), u32(b, 0x44b8)};
  Integer *values[] = {&p.newest, &p.acked,   &p.lead,
                       &p.sim,    &p.silence, &p.heard};
  for (std::size_t i = 0; i < 6; ++i)
    *values[i] = s32(u32(b, 0x44bc + 4 * i));
  p.local = b[0x44d4];
  p.dropped = b[0x44d5];
  p.kick = b[0x44d6];
  p.oos = b[0x44d7];
  p.drop_frame = s32(u32(b, 0x44d8));
  p.kicked_notice = s32(u32(b, 0x44dc));
  return p;
}
Machine::Machine(Integer n, Integer local, const Fpu &f)
    : fpu(f), num_peers(n), num_players(n) {
  for (auto &p : peers)
    p.acked = 1;
  peer(*this, local).local = 1;
}
Integer Machine::local_slot() const {
  if (!network_mode)
    return 0;
  loops(num_peers);
  for (Integer i = 0; i < num_peers; ++i)
    if (peer(*this, i).local)
      return i;
  return -1;
}
Integer Machine::active_mask() const {
  loops(num_peers);
  Integer mask = 0;
  for (Integer i = 0; i < num_peers; ++i)
    if (!peer(*this, i).inactive())
      mask |= Integer(1) << i;
  return mask;
}
std::vector<Integer> Machine::remote_active() const {
  loops(num_peers);
  std::vector<Integer> out;
  for (Integer i = 0; i < num_peers; ++i) {
    auto &p = peer(*this, i);
    if (!(p.local || p.dropped || p.kick))
      out.push_back(i);
  }
  return out;
}
bool Machine::is_first_active() const {
  Integer first = 0;
  loops(num_players);
  for (Integer i = 0; i < num_players; ++i) {
    bool alive = true;
    if (network_mode)
      alive = i < num_peers && !peer(*this, i).dropped;
    if (alive) {
      first = i;
      break;
    }
  }
  return first == local_slot();
}
void Machine::queue_message(Integer ident, const ByteVector &payload) {
  bytes_limit(payload.size());
  auto &p = peer(*this, local_slot());
  auto pos = std::uint64_t(p.msg_write);
  p.msg_ring[pos & 8191] = std::uint8_t(ident);
  p.msg_ring[(pos + 1) & 8191] = std::uint8_t(payload.size());
  for (std::size_t i = 0; i < payload.size(); ++i)
    p.msg_ring[(pos + 2 + i) & 8191] = payload[i];
  p.msg_write = Integer((pos + payload.size() + 2) & 8191);
}
std::optional<Integer> Machine::attach_messages() {
  if (!(last_sent < checked_sub(lif, 1)))
    return {};
  auto &p = peer(*this, local_slot());
  if (p.msg_write == msg_attached)
    return {};
  auto frame = std::max(checked_add(last_sent, 1), net_frame);
  auto base = ring_slot(frame) * 24;
  auto count = u32(p.ring, base + 20);
  if (!count)
    put(p.ring, base + 16, msg_attached);
  put(p.ring, base + 20,
      std::uint32_t(
          count +
          ((std::uint64_t(p.msg_write) - std::uint64_t(msg_attached)) & 8191)));
  msg_attached = p.msg_write;
  return frame;
}
std::pair<std::vector<std::pair<Integer, ByteVector>>, Integer>
Machine::frame_messages(Integer slot, Integer frame) const {
  auto &p = peer(*this, slot);
  Integer left = p.msg_count(frame), pos = p.msg_start(frame);
  std::vector<std::pair<Integer, ByteVector>> out;
  std::size_t work = 0;
  while (left) {
    if (left < 0)
      throw MalformedPacket("frame message count");
    if (++work > MAX_WORK)
      throw ResourceLimit("frame message budget");
    auto ident = p.msg_ring[msg_slot(pos)];
    auto size = p.msg_ring[(std::uint64_t(pos) + 1) & 8191];
    ByteVector payload;
    for (unsigned i = 0; i < size; ++i)
      payload.push_back(p.msg_ring[(std::uint64_t(pos) + 2 + i) & 8191]);
    out.push_back({ident, std::move(payload)});
    left -= size + 2;
    pos = Integer((std::uint64_t(pos) + size + 2) & 8191);
  }
  return {out, pos};
}
Sent Machine::build_packet(Integer dest, Integer first, Integer last,
                           Integer kind, Integer owner, const CodecState *state,
                           bool raw) {
  auto &p = peer(*this, dest);
  auto count = checked_add(checked_sub(last, first), 1);
  if (count < 1 || count > 255)
    throw ValueError("packet frame count");
  auto byte9 = raw ? local_slot()
                   : checked_sub(last, Integer(std::uint64_t(net_frame) & 255));
  auto out = header(kind, last, owner, active_mask(), byte9, count,
                    checked_sub(p.newest, last));
  auto &source = peer(*this, owner);
  std::vector<std::size_t> undefined;
  if (raw) {
    for (Integer i = 0; i < count; ++i)
      append(out, source.wire_record(checked_add(first, i)));
  } else {
    if (!state)
      throw ValueError("missing codec state");
    auto work = *state;
    if (first > p.send_frame)
      encode_frames(work, records(source, p.send_frame, first));
    auto writer =
        encode_frames(work, records(source, first, checked_add(last, 1)));
    auto used = (writer.pos + 7) >> 3;
    auto padded = std::size_t((used + 3) & ~Integer(3));
    auto stream = writer.tobytes();
    for (auto i = stream.size(); i < padded; ++i)
      undefined.push_back(12 + i);
    stream.resize(padded, 0);
    append(out, stream);
  }
  while (out.size() & 3)
    out.push_back(0);
  if (last_hash_frame == net_frame)
    word(out, NO_SYNC);
  else {
    last_hash_frame = net_frame;
    word(out, SYNC_MARKER);
    auto base = ring_slot(net_frame) * 12;
    out.insert(out.end(), source.sync_ring.begin() + std::ptrdiff_t(base),
               source.sync_ring.begin() + std::ptrdiff_t(base + 12));
  }
  for (Integer i = 0; i < count; ++i) {
    auto frame = checked_add(first, i);
    auto size = source.msg_count(frame);
    if (size) {
      auto start = source.msg_start(frame);
      out.push_back(std::uint8_t(std::uint64_t(last) - std::uint64_t(frame)));
      out.push_back(std::uint8_t(size));
      if (size > MAX_BYTES - out.size())
        throw ResourceLimit("packet construction byte budget");
      for (std::uint32_t j = 0; j < size; ++j)
        out.push_back(source.msg_ring[(std::uint64_t(start) + j) & 8191]);
    }
  }
  if (out.size() > SEND_BUFFER)
    throw ValueError("packet exceeds original send buffer");
  Sent sent{dest, p.ip, p.port, std::move(out), std::move(undefined)};
  outbox.push_back(sent);
  return sent;
}
Sent Machine::send_input(Integer dest) {
  auto &p = peer(*this, dest);
  auto &local = peer(*this, local_slot());
  if (p.acked > p.send_frame) {
    encode_frames(p.send_state, records(local, p.send_frame, p.acked));
    p.send_frame = p.acked;
  } else if (p.acked < p.send_frame)
    throw ValueError("acknowledgement went backwards");
  auto sent = build_packet(dest, p.acked, checked_sub(lif, 1), PKT_INPUT,
                           local_slot(), &p.send_state);
  last_sent = lif;
  return sent;
}
void Machine::send_direct(Integer index, Integer mask,
                          const ByteVector &payload) {
  if (!network_mode)
    return;
  auto data = direct_message(index, payload);
  auto n = num_peers;
  loops(n);
  for (Integer slot = 0; slot < n; ++slot) {
    if (!bit(mask, slot))
      continue;
    auto &p = peer(*this, slot);
    if (p.local) {
      auto handler = direct_handlers[python_index(index, 4)];
      if (handler) {
        const ByteVector owned_payload(payload);
        handler(owned_payload);
      }
    } else if (!p.dropped)
      outbox.push_back({slot, p.ip, p.port, data, {}});
  }
}
void Machine::kick_peer(Integer slot) {
  if (slot < num_peers) {
    auto &p = peer(*this, slot);
    if (!(p.local || p.dropped || p.kick)) {
      log.push_back("kick " + std::to_string(slot));
      p.kick = 1;
      p.drop_frame = -1;
      p.kicked_notice = 1;
      kick_acks = {};
      kick_state = 1;
    }
  }
}
std::vector<Integer> Machine::receive(const ByteVector &input, bool raw) {
  bytes_limit(input.size());
  const ByteVector data(input);
  if (data.size() < 12)
    throw MalformedPacket("short receive header");
  Integer last = s32(u32(data, 4)), byte9 = data[9], count = data[10],
          ack = s8(data[11]);
  if ((data[8] >> 4) != active_mask())
    return {};
  Integer slot = data[8] & 15;
  if (slot > 3)
    throw MalformedPacket("receiver slot");
  auto &p = peer(*this, slot);
  if (p.local || p.dropped || (p.kick && !raw) || p.oos)
    return {};
  p.silence = 0;
  p.heard = 1;
  if (raw) {
    if (byte9 > 3)
      throw MalformedPacket("relay slot");
    kick_acks[std::size_t(byte9)] = 1;
  } else
    p.sim = last - byte9;
  p.lead = s32(std::uint64_t(last) - std::uint64_t(p.sim));
  if (!(p.newest < last))
    return {};
  p.acked = std::max(p.acked, last + ack);
  auto first = last - count + 1;
  if (checked_add(p.newest, 1) < first)
    return {};
  auto skip = checked_add(checked_sub(p.newest, first), 1);
  Integer stream_bytes;
  std::vector<WireRecord> recs;
  if (raw) {
    stream_bytes = count * 12;
    if (data.size() < std::size_t(12 + stream_bytes))
      throw MalformedPacket("short raw relay");
    for (Integer i = skip; i < count; ++i)
      recs.push_back(wire(data, 12 + std::size_t(i) * 12));
  } else {
    auto symbols =
        read_frames(ByteVector(data.begin() + 12, data.end()), count);
    recs = apply_frames(p.recv_state, symbols, skip, fpu);
    stream_bytes = (symbols.bits + 7) >> 3;
  }
  std::vector<Integer> stored;
  for (std::size_t i = 0; i < recs.size(); ++i) {
    auto frame = checked_add(checked_add(first, skip), Integer(i));
    auto record = recs[i];
    if (!raw && stack_bytes) {
      auto callback = stack_bytes;
      auto pressure = callback(slot, frame);
      record[6] = pressure[0];
      record[7] = pressure[1];
    }
    p.set_record(frame, record, {}, 0);
    stored.push_back(frame);
  }
  p.newest = last;
  auto offset = std::size_t((12 + stream_bytes + 3) & ~Integer(3));
  if (offset > data.size() || data.size() - offset < 4)
    throw MalformedPacket("missing sync word");
  auto word_ = u32(data, offset);
  offset += 4;
  if (word_ == SYNC_MARKER) {
    if (data.size() - offset < 12)
      throw MalformedPacket("short sync record");
    auto frame = u32(data, offset), saved = u32(data, offset + 4),
         value = u32(data, offset + 8);
    offset += 12;
    p.set_sync(frame, saved, value);
    auto mine = peer(*this, local_slot()).sync(frame);
    if (mine[0] == frame && mine[0] && mine[2] != value) {
      p.oos = 1;
      log.push_back("out of sync " + std::to_string(slot));
    }
  }
  while (offset != data.size()) {
    if (data.size() - offset < 2 ||
        std::size_t(data[offset + 1]) > data.size() - offset - 2)
      throw MalformedPacket("short message block");
    auto frame = ring_slot(last - data[offset]);
    auto size = data[offset + 1];
    auto base = frame * 24;
    if (!u32(p.ring, base + 20)) {
      put(p.ring, base + 16, p.msg_write);
      put(p.ring, base + 20, size);
      for (unsigned i = 0; i < size; ++i)
        p.msg_ring[(std::uint64_t(p.msg_write) + i) & 8191] =
            data[offset + 2 + i];
      p.msg_write = Integer((std::uint64_t(p.msg_write) + size) & 8191);
    }
    offset += size + 2;
  }
  return stored;
}
std::optional<std::vector<Integer>> Machine::dispatch(const ByteVector &data) {
  bytes_limit(data.size());
  if (data.size() < 4)
    throw MalformedPacket("short packet type");
  auto kind = u32(data, 0);
  if (kind >= PKT_DIRECT && kind < PKT_DIRECT + 4) {
    auto handler = direct_handlers[kind - PKT_DIRECT];
    if (handler)
      handler(ByteVector(data.begin() + 4, data.end()));
    return {};
  }
  if (kind == PKT_KICK_REQUEST) {
    if (data.size() < 12)
      throw MalformedPacket("short kick request");
    auto requester = u32(data, 4);
    auto mask = s32(u32(data, 8));
    auto n = num_peers;
    loops(n);
    for (Integer slot = 0; slot < n; ++slot) {
      if (bit(mask, slot) && !peer(*this, slot).kick) {
        auto &p = peer(*this, slot);
        p.kick = 1;
        p.drop_frame = -1;
        kick_acks = {};
        kick_state = 1;
      }
    }
    if (requester > 3)
      throw MalformedPacket("requester slot");
    n = num_peers;
    loops(n);
    for (Integer slot = 0; slot < n; ++slot) {
      auto &p = peer(*this, slot);
      if (!p.local && !p.dropped && bit(mask, slot))
        build_packet(
            requester,
            std::max(checked_sub(p.newest, RAW_RELAY_DEPTH), Integer(0)),
            p.newest, PKT_RAW_RELAY, slot, nullptr, true);
    }
    return {};
  }
  if (kind == PKT_INPUT)
    return receive(data);
  if (kind == PKT_RAW_RELAY)
    return receive(data, true);
  if (kind == PKT_KICKED)
    kicked = 1;
  return {};
}
} // namespace champions::simulation::rta::netwire
