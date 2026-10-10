#pragma once
#include "champions/core_arithmetic.hpp"
#include <array>
#include <functional>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>
namespace champions::simulation::rta::netwire {
using Integer = std::int64_t;
using ByteVector = std::vector<std::uint8_t>;
using WireRecord = std::array<std::uint8_t, 12>;
using Pad = std::array<std::uint8_t, 0x58>;
using Position = std::array<Raw, 3>;
class MalformedPacket : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};
class ValueError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};
class IndexError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};
class ResourceLimit : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};
inline constexpr std::size_t MAX_BYTES = 16u * 1024u * 1024u,
                             MAX_FRAMES = 65536, MAX_WORK = 1048576;
// Netwire and core have the same raw32 arithmetic contract. Each machine owns
// an explicit context; no operation consults or changes the thread's core
// context.
class Fpu : public rta::Fpu {
public:
  using rta::Fpu::Fpu;
  static bool lt(Raw a, Raw b) noexcept { return compare(a, b) < 0; }
};
inline const Fpu DEFAULT_FPU{};
inline constexpr Raw F_ONE = 0x3f800000, F_128 = 0x43000000,
                     SPRING_GAIN = 0x3d4ccccc, SPRING_SQUARE = 0x3e4ccccc;
inline constexpr std::array<Integer, 9> STEPS = {4,  6,  8,  12, 16,
                                                 24, 32, 48, 64};
inline constexpr std::array<Integer, 2> INDEX_ADJ = {-1, 2};
inline constexpr std::array<std::size_t, 4> WIRE_ORDER = {2, 3, 0, 1};
inline constexpr Integer SPRING_STEPS = 10, YAW_JUMP = 1000, YAW_FAR = 3000,
                         YAW_NUDGE = 200, YAW_KEEP = 31;
inline constexpr std::uint16_t STREAM_TRAILER = 0xa5a5;
Integer s8(std::uint64_t) noexcept;
Integer s16(std::uint64_t) noexcept;
Integer s32(std::uint64_t) noexcept;
Integer checked_add(Integer, Integer);
Integer checked_sub(Integer, Integer);
std::size_t python_index(Integer, std::size_t);
struct BitWriter {
  std::vector<std::uint16_t> words;
  Integer pos = 0;
  void start(Integer);
  void put(Integer, Integer);
  std::size_t written() const { return words.size(); }
  ByteVector tobytes() const;
};
struct BitReader {
  std::vector<std::uint16_t> words;
  Integer pos = 0;
  std::size_t work = 0;
  explicit BitReader(const ByteVector &, Integer pos = 0);
  std::uint32_t get(Integer bits);
};
struct Channel {
  Integer index = 0, pred = 0;
  Raw value = F_128, velocity = 0;
  ByteVector pack() const;
  static Channel unpack(const ByteVector &);
  Channel copy() const { return *this; }
  bool operator==(const Channel &other) const { return pack() == other.pack(); }
};
using AxisSymbol = std::pair<Integer, Integer>;
using AxisSymbols = std::vector<AxisSymbol>;
AxisSymbols axis_symbols(Channel &, const std::vector<Integer> &);
void write_axis_symbols(BitWriter &, const AxisSymbols &);
void encode_axis(BitWriter &, Channel &, const std::vector<Integer> &);
AxisSymbols read_axis_symbols(BitReader &, Integer);
ByteVector apply_axis_symbols(Channel &, const AxisSymbols &,
                              const Fpu & = DEFAULT_FPU);
ByteVector decode_axis(BitReader &, Channel &, Integer count, Integer skip = 0,
                       const Fpu & = DEFAULT_FPU);
WireRecord rec12(Integer buttons = 0,
                 const std::array<Integer, 4> &sticks = {128, 128, 128, 128},
                 const std::array<Integer, 2> &pressure = {0, 0},
                 Integer yaw = 0, Integer flag = 0);
struct RecordFields {
  Integer buttons;
  std::array<Integer, 4> sticks;
  std::array<Integer, 2> pressure;
  std::uint32_t yaw;
};
RecordFields rec12_fields(const ByteVector &);
RecordFields rec12_fields(const WireRecord &);
struct CodecState {
  std::array<Channel, 4> ch{};
  Integer yaw_prev = 0, yaw_pred = 0, yaw_delta = 0, pad46 = 0, count = 0;
  ByteVector pack() const;
  static CodecState unpack(const ByteVector &);
  CodecState copy() const { return *this; }
  bool operator==(const CodecState &other) const {
    return pack() == other.pack();
  }
};
struct YawSymbol {
  enum class Kind { Keep, Delta, Absolute };
  Kind kind = Kind::Keep;
  Integer value = 0;
};
struct FrameSymbols {
  Integer count = 0;
  std::vector<Integer> buttons;
  std::array<AxisSymbols, 4> sticks;
  std::vector<YawSymbol> yaw;
  std::vector<Integer> flags;
  Integer bits = 0;
};
BitWriter encode_frames(CodecState &, const std::vector<WireRecord> &);
FrameSymbols read_frames(const ByteVector &, Integer);
std::vector<WireRecord> apply_frames(CodecState &, const FrameSymbols &,
                                     Integer skip = 0,
                                     const Fpu & = DEFAULT_FPU);
std::pair<std::vector<WireRecord>, Integer>
decode_frames(CodecState &, const ByteVector &, Integer count, Integer skip = 0,
              const Fpu & = DEFAULT_FPU);
WireRecord local_roundtrip(const WireRecord &, CodecState &, CodecState &,
                           const Fpu & = DEFAULT_FPU);
std::map<Fpu::Model, ByteVector>
fpu_sensitivity(const std::vector<Integer> &,
                const std::vector<Fpu::Model> &models = {
                    Fpu::Model::Ps2, Fpu::Model::Ps2g1, Fpu::Model::Chop,
                    Fpu::Model::Nearest});
inline constexpr std::uint32_t PKT_LAN_ANNOUNCE = 0xbd3d,
                               PKT_LAN_START = 0xbd3e, PKT_INPUT = 0xbd3f,
                               SYNC_MARKER = 0xbd40, PKT_KICK_REQUEST = 0xbd41,
                               PKT_RAW_RELAY = 0xbd42, PKT_KICKED = 0xbd43,
                               PKT_DIRECT = 0xbd44, NO_SYNC = 0xffffffff;
inline constexpr std::size_t RECV_BUFFER = 0x7c0, SEND_BUFFER = 0x1390,
                             RAW_RELAY_DEPTH = 40, MSG_RING = 0x2000,
                             PEER_SIZE = 0x44e0, RING_OFFSET = 0xac,
                             SYNC_OFFSET = 0x18ac, MSG_OFFSET = 0x24ac,
                             PAD_SIZE = 0x58;
inline constexpr std::array<Integer, 4> SEND_CYCLES = {6, 2, 4, 6};
ByteVector header(Integer kind, Integer last, Integer slot, Integer mask,
                  Integer byte9, Integer count, Integer ack_delta);
struct Packet {
  Integer kind = 0, last = 0, slot = 0, mask = 0, byte9 = 0, count = 0,
          ack_delta = 0;
  std::optional<FrameSymbols> symbols;
  std::optional<std::vector<WireRecord>> raw_records;
  Integer stream_bytes = 0, sync_offset = 0;
  std::optional<std::array<std::uint32_t, 3>> sync;
  std::uint32_t sync_word = 0;
  std::vector<std::pair<Integer, ByteVector>> blocks;
  Integer blocks_offset = 0;
  Integer first() const;
  Integer ack() const;
  Integer sim_frame() const;
};
Packet parse_packet(const ByteVector &);
ByteVector kick_request(Integer, Integer);
ByteVector kicked_notice();
ByteVector direct_message(Integer, const ByteVector &);
ByteVector lan_packet(bool start = false);
struct Peer {
  std::array<std::uint8_t, 4> ip{};
  Integer port = 0;
  std::array<std::uint8_t, 8> unknown8{};
  CodecState recv_state;
  Integer send_frame = 0;
  CodecState send_state;
  std::array<std::uint8_t, 256 * 24> ring{};
  std::array<std::uint8_t, 256 * 12> sync_ring{};
  std::array<std::uint8_t, MSG_RING> msg_ring{};
  Integer msg_write = 0, msg_read = 0;
  std::array<Integer, 2> pad_prev{};
  Integer newest = 0, acked = 0, lead = 0, sim = 0, silence = 0, heard = 0,
          local = 0, dropped = 0, kick = 0, oos = 0, drop_frame = 0,
          kicked_notice = 0;
  std::tuple<std::uint32_t, WireRecord, std::uint32_t, std::uint32_t>
      record(Integer) const;
  WireRecord wire_record(Integer) const;
  void set_record(Integer, const WireRecord &,
                  std::optional<Integer> start = {},
                  std::optional<Integer> count = {});
  std::uint32_t msg_count(Integer) const;
  std::uint32_t msg_start(Integer) const;
  std::array<std::uint32_t, 3> sync(Integer) const;
  void set_sync(Integer, Integer, Integer);
  bool inactive() const;
  ByteVector pack() const;
  static Peer unpack(const ByteVector &);
};
struct Sent {
  Integer peer = 0;
  std::array<std::uint8_t, 4> ip{};
  Integer port = 0;
  ByteVector data;
  std::vector<std::size_t> undefined;
};
struct PadGlobals {
  Integer start_block = 0, mute = 0, allow = 0;
};
struct NetMainResult {
  std::vector<Sent> sent;
  bool held = false;
  Integer steps = 0;
};
class Machine {
public:
  Fpu fpu;
  Integer network_mode = 1, num_peers = 2;
  std::array<Peer, 4> peers{};
  Integer lif = 1, last_sent = 1, net_frame = 1, cycle = 0, phase = 0,
          modem = 0, last_hash_frame = 1, stall = 0, ended = 0, eq = 0,
          min_ready = 100, win = 0, long_stall = 0, kicked = 0, kick_state = 0;
  std::array<Integer, 4> kick_acks{}, cam_yaw{};
  Integer msg_attached = 0;
  std::array<std::array<Integer, 3>, 4> input_lock{};
  CodecState sim_enc, sim_dec;
  std::array<Integer, 4> send_cycles = SEND_CYCLES;
  Integer sim_frame = 0, rand_seed = 0, sync_acc = 0;
  bool level_pending = false;
  Integer num_players = 2;
  std::array<std::optional<Position>, 4> positions{};
  std::array<Pad, 4> pads{};
  PadGlobals pad_globals;
  using Handler = std::function<void(const ByteVector &)>;
  std::map<Integer, Handler> handlers;
  std::array<Handler, 4> direct_handlers;
  // Optional composition hook, absent from the pinned Python wire model.
  // Runs only for a ready, non-level-pending tick, before peer pads/messages.
  std::function<void(Machine &)> before_frame;
  std::function<void(Machine &)> run_tasks;
  std::function<void(Integer)> on_kill, first_active_hook;
  std::vector<Sent> outbox;
  std::vector<std::string> log;
  std::function<std::array<std::uint8_t, 2>(Integer, Integer)> stack_bytes;
  std::function<std::uint32_t(Integer)> sample_garbage;
  explicit Machine(Integer num_peers = 2, Integer local = 0,
                   const Fpu &fpu = DEFAULT_FPU);
  Integer local_slot() const;
  Integer active_mask() const;
  std::vector<Integer> remote_active() const;
  bool is_first_active() const;
  void queue_message(Integer, const ByteVector &);
  std::optional<Integer> attach_messages();
  std::pair<std::vector<std::pair<Integer, ByteVector>>, Integer>
      frame_messages(Integer, Integer) const;
  Sent build_packet(Integer dest, Integer first, Integer last, Integer kind,
                    Integer owner, const CodecState *state, bool raw = false);
  Sent send_input(Integer);
  void send_direct(Integer, Integer, const ByteVector &);
  void kick_peer(Integer);
  std::vector<Integer> receive(const ByteVector &, bool raw = false);
  std::optional<std::vector<Integer>> dispatch(const ByteVector &);
  NetMainResult net_main(const ByteVector &, Integer yaw = 0,
                         Integer camera_flag = 0, Integer frame_step = 1,
                         bool no_sim = false, bool hold_arg = false,
                         Integer render_frame = 0,
                         const std::vector<ByteVector> &inbox = {});
  void tick(Integer);
};
std::uint32_t sync_hash(Integer, Integer,
                        const std::vector<std::optional<Position>> &);
std::array<Raw, 2> decode_stick(Integer, Integer, const Fpu & = DEFAULT_FPU);
Pad &pad_apply(Pad &, const ByteVector &, std::array<Integer, 2> &,
               PadGlobals *glob = nullptr, const Fpu & = DEFAULT_FPU);
inline Pad &pad_apply(Pad &p, const WireRecord &r, std::array<Integer, 2> &prev,
                      PadGlobals *g = nullptr, const Fpu &f = DEFAULT_FPU) {
  return pad_apply(p, ByteVector(r.begin(), r.end()), prev, g, f);
}
inline constexpr Raw STICK_SCALE = 0x3c000000, STICK_DEAD = 0x3ea3d70a,
                     STICK_GAIN = 0x3fbc3c3c, PRESSURE_SCALE = 0x3b808080,
                     DIGITAL = 0x3f4ccccc;
inline constexpr std::size_t LAUNCH_SIZE = 0x200, LAUNCH_INFO = 0xd0,
                             LAUNCH_MIN = 16;
struct LaunchPacket {
  std::uint32_t player_id = 0, kind = 0, argument = 0;
  ByteVector payload;
};
std::uint16_t launch_checksum(const ByteVector &);
ByteVector launch_packet(Integer, Integer, Integer,
                         const ByteVector &payload = {},
                         std::optional<Integer> size = LAUNCH_SIZE);
std::pair<ByteVector, std::uint32_t> launch_data_packet(Integer, Integer,
                                                        const ByteVector &);
std::optional<LaunchPacket> parse_launch_packet(const ByteVector &);
bool launch_data_ok(Integer, const ByteVector &);
} // namespace champions::simulation::rta::netwire
