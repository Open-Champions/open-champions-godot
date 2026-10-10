#include "champions/netwire.hpp"
#include <algorithm>
#include <limits>
// Authored from ref_netwire.py, published revision 54258f9. No retail inputs.
namespace champions::simulation::rta::netwire {
Integer s8(std::uint64_t x) noexcept {
  auto v = x & 255;
  return v < 128 ? Integer(v) : Integer(v) - 256;
}
Integer s16(std::uint64_t x) noexcept {
  auto v = x & 65535;
  return v < 32768 ? Integer(v) : Integer(v) - 65536;
}
Integer s32(std::uint64_t x) noexcept {
  auto v = x & 0xffffffff;
  return v < 0x80000000 ? Integer(v) : Integer(v) - 0x100000000LL;
}
Integer checked_add(Integer a, Integer b) {
  if ((b > 0 && a > INT64_MAX - b) || (b < 0 && a < INT64_MIN - b))
    throw ResourceLimit("integer addition exceeds model representation");
  return a + b;
}
Integer checked_sub(Integer a, Integer b) {
  if ((b > 0 && a < INT64_MIN + b) || (b < 0 && a > INT64_MAX + b))
    throw ResourceLimit("integer subtraction exceeds model representation");
  return a - b;
}
std::size_t python_index(Integer i, std::size_t n) {
  if (n > std::size_t(INT64_MAX))
    throw ResourceLimit("container size");
  if (i < 0)
    i = checked_add(i, Integer(n));
  if (i < 0 || std::uint64_t(i) >= n)
    throw IndexError("index outside sequence");
  return std::size_t(i);
}
namespace {
void size_limit(std::size_t n, std::size_t max) {
  if (n > max)
    throw ResourceLimit("netwire allocation/work limit");
}
void frame_limit(Integer n) {
  if (n > Integer(MAX_FRAMES))
    throw ResourceLimit("frame work limit");
}
void range(Integer v, Integer low, Integer high) {
  if (v < low || v > high)
    throw ValueError("field outside packed range");
}
void append_le(ByteVector &out, std::uint64_t v, unsigned n) {
  for (unsigned i = 0; i < n; ++i)
    out.push_back(std::uint8_t(v >> (8 * i)));
}
std::uint32_t get(const ByteVector &v, std::size_t p, unsigned n) {
  if (p > v.size() || n > v.size() - p)
    throw ValueError("short packed record");
  std::uint32_t out = 0;
  for (unsigned i = 0; i < n; ++i)
    out |= std::uint32_t(v[p + i]) << (8 * i);
  return out;
}
Integer clamp(Integer v, Integer lo, Integer hi) {
  return std::max(lo, std::min(hi, v));
}
std::size_t slice_start(Integer i, std::size_t n) {
  if (i < 0) {
    if (i < -Integer(n))
      return 0;
    return std::size_t(Integer(n) + i);
  }
  return std::min(std::size_t(i), n);
}
Integer absolute32(Integer v) { return v < 0 ? s32(0 - std::uint64_t(v)) : v; }
Integer times(Integer a, Integer b) {
  if (a == 0 || b == 0)
    return 0;
  if ((a == -1 && b == INT64_MIN) || (b == -1 && a == INT64_MIN))
    throw ResourceLimit("integer product");
  if (a > 0 ? (b > 0 ? a > INT64_MAX / b : b < INT64_MIN / a)
            : (b > 0 ? a < INT64_MIN / b : a < INT64_MAX / b))
    throw ResourceLimit("integer product");
  return a * b;
}
} // namespace
void BitWriter::start(Integer first) {
  words = {std::uint16_t(first), 0};
  pos = 16;
}
void BitWriter::put(Integer value, Integer bits) {
  range(bits, 0, 16);
  const auto v = std::uint16_t(std::uint64_t(value) << (16 - bits));
  const Integer index = pos >= 0 ? pos / 16 : -1 - (-(pos + 1)) / 16;
  const auto shift = unsigned(std::uint64_t(pos) & 15);
  if (index >= Integer(MAX_BYTES / 2) - 1)
    throw ResourceLimit("bit writer allocation limit");
  while (Integer(words.size()) < index + 2)
    words.push_back(0);
  auto i = python_index(index, words.size()),
       j = python_index(index + 1, words.size());
  words[i] |= std::uint16_t(v >> shift);
  words[j] = std::uint16_t(std::uint32_t(v) << (16 - shift));
  pos = checked_add(pos, bits);
}
ByteVector BitWriter::tobytes() const {
  size_limit(words.size(), MAX_BYTES / 2);
  ByteVector out;
  out.reserve(words.size() * 2);
  for (auto w : words)
    append_le(out, w, 2);
  return out;
}
BitReader::BitReader(const ByteVector &v, Integer p) : pos(p) {
  size_limit(v.size(), MAX_BYTES);
  for (std::size_t i = 0; i < v.size(); i += 2)
    words.push_back(std::uint16_t(
        v[i] | (i + 1 < v.size() ? std::uint16_t(v[i + 1]) << 8 : 0)));
}
std::uint32_t BitReader::get(Integer bits) {
  range(bits, 0, 32);
  if (++work > MAX_WORK)
    throw ResourceLimit(
        "bit reader work exhausted (model may loop on missing runs)");
  const Integer index = pos >= 0 ? pos / 16 : -1 - (-(pos + 1)) / 16;
  const auto shift = unsigned(std::uint64_t(pos) & 15);
  auto word = [&](Integer i) -> std::uint32_t {
    return i < Integer(words.size()) ? words[python_index(i, words.size())] : 0;
  };
  auto hi = word(index), lo = word(checked_add(index, 1));
  pos = checked_add(pos, bits);
  const std::uint32_t v = ((hi << 16) | lo) << shift;
  return bits == 0 ? 0 : v >> (32 - bits);
}
ByteVector Channel::pack() const {
  range(index, INT32_MIN, INT32_MAX);
  range(pred, INT32_MIN, INT32_MAX);
  ByteVector out;
  append_le(out, std::uint64_t(index), 4);
  append_le(out, std::uint64_t(pred), 4);
  append_le(out, value, 4);
  append_le(out, velocity, 4);
  return out;
}
Channel Channel::unpack(const ByteVector &v) {
  return {s32(get(v, 0, 4)), s32(get(v, 4, 4)), get(v, 8, 4), get(v, 12, 4)};
}
AxisSymbols axis_symbols(Channel &c, const std::vector<Integer> &samples) {
  size_limit(samples.size(), MAX_FRAMES);
  AxisSymbols out;
  for (auto sample : samples) {
    auto step = STEPS[python_index(c.index, STEPS.size())];
    auto diff = checked_sub(Integer(std::uint64_t(sample) & 255), c.pred);
    auto mag = diff < 0 ? std::uint64_t(-(diff + 1)) + 1 : std::uint64_t(diff);
    bool move = mag / std::uint64_t(step) >= 1, negative = diff < 0 && move;
    c.pred = negative
                 ? std::max(checked_sub(c.pred, move ? step : 0), Integer{0})
                 : std::min(checked_add(c.pred, move ? step : 0), Integer{255});
    c.index = clamp(checked_add(c.index, move ? 2 : -1), 0, 8);
    out.emplace_back(move, negative);
  }
  return out;
}
void write_axis_symbols(BitWriter &w, const AxisSymbols &s) {
  size_limit(s.size(), MAX_FRAMES);
  Integer literal = 0, zeros = 0;
  bool seen = false;
  for (auto [move, d] : s) {
    (void)d;
    if (!literal) {
      if (!move) {
        if (++zeros == 31) {
          w.put(31, 5);
          zeros = 0;
        }
      } else {
        w.put(zeros, 5);
        zeros = 0;
        literal = 1;
        seen = false;
      }
    } else {
      w.put(move, 1);
      ++literal;
      seen = seen || move;
      if (literal == 6) {
        literal = seen ? 1 : 0;
        seen = false;
      }
    }
  }
  if (zeros)
    w.put(zeros, 5);
  std::vector<Integer> moves;
  for (auto [m, d] : s)
    if (m)
      moves.push_back(d);
  if (moves.empty())
    return;
  Integer current = moves[0], run = 0;
  w.put(current, 1);
  for (auto d : moves) {
    if (d != current) {
      w.put(run, 3);
      run = 0;
      current = d;
    }
    if (++run == 7) {
      w.put(7, 3);
      run = 0;
    }
  }
  if (run)
    w.put(run, 3);
}
void encode_axis(BitWriter &w, Channel &c, const std::vector<Integer> &s) {
  write_axis_symbols(w, axis_symbols(c, s));
}
AxisSymbols read_axis_symbols(BitReader &r, Integer total) {
  frame_limit(total);
  std::vector<Integer> moves;
  Integer literal = 0;
  bool seen = false;
  while (Integer(moves.size()) < total) {
    if (!literal) {
      auto z = r.get(5);
      if (checked_add(Integer(moves.size()), z) > total)
        throw MalformedPacket("stick zero run exceeds count");
      moves.insert(moves.end(), z, 0);
      if (Integer(moves.size()) == total)
        break;
      if (z != 31) {
        moves.push_back(1);
        literal = 1;
        seen = false;
      }
    } else {
      auto bit = r.get(1);
      moves.push_back(bit);
      ++literal;
      seen = seen || bit;
      if (literal == 6) {
        literal = seen ? 1 : 0;
        seen = false;
      }
    }
  }
  std::vector<Integer> directions(total > 0 ? std::size_t(total) : 0, 0);
  std::vector<std::size_t> where;
  for (std::size_t i = 0; i < moves.size(); ++i)
    if (moves[i])
      where.push_back(i);
  if (!where.empty()) {
    auto current = r.get(1);
    std::size_t done = 0;
    while (done < where.size()) {
      auto run = r.get(3);
      if (run > where.size() - done)
        throw MalformedPacket("direction run exceeds moves");
      for (std::size_t i = done; i < done + run; ++i)
        directions[where[i]] = current;
      done += run;
      if (run != 7)
        current ^= 1;
    }
  }
  AxisSymbols out;
  for (std::size_t i = 0; i < moves.size(); ++i)
    out.emplace_back(moves[i], directions[i]);
  return out;
}
ByteVector apply_axis_symbols(Channel &c, const AxisSymbols &s, const Fpu &f) {
  size_limit(s.size(), MAX_FRAMES);
  const Raw damping = f.sub(F_ONE, f.sqrt(SPRING_SQUARE));
  ByteVector out;
  for (auto [move, d] : s) {
    const auto step = times(STEPS[python_index(c.index, STEPS.size())], move);
    auto pred = d ? checked_sub(c.pred, step) : checked_add(c.pred, step);
    c.index = clamp(
        checked_add(c.index, INDEX_ADJ[python_index(move, INDEX_ADJ.size())]),
        0, 8);
    c.pred = pred = clamp(pred, 0, 255);
    const auto target = f.from_int(std::uint32_t(pred));
    auto value = c.value, velocity = c.velocity;
    for (Integer i = 0; i < SPRING_STEPS; ++i) {
      value = f.add(value, velocity);
      velocity = f.mul(
          f.add(velocity, f.mul(f.sub(target, value), SPRING_GAIN)), damping);
    }
    c.value = value;
    c.velocity = velocity;
    out.push_back(std::uint8_t(clamp(f.to_int(value), 0, 255)));
  }
  return out;
}
ByteVector decode_axis(BitReader &r, Channel &c, Integer count, Integer skip,
                       const Fpu &f) {
  auto symbols = read_axis_symbols(r, checked_add(count, skip));
  auto first = slice_start(skip, symbols.size());
  return apply_axis_symbols(
      c, AxisSymbols(symbols.begin() + first, symbols.end()), f);
}
WireRecord rec12(Integer buttons, const std::array<Integer, 4> &sticks,
                 const std::array<Integer, 2> &pressure, Integer yaw,
                 Integer flag) {
  ByteVector out;
  append_le(out, std::uint64_t(buttons) & 65535, 2);
  for (auto x : sticks) {
    range(x, 0, 255);
    out.push_back(std::uint8_t(x));
  }
  for (auto x : pressure) {
    range(x, 0, 255);
    out.push_back(std::uint8_t(x));
  }
  const auto word =
      std::uint32_t(s16(std::uint64_t(yaw) & 65534)) | std::uint32_t(flag & 1);
  append_le(out, word, 4);
  WireRecord result;
  std::copy(out.begin(), out.end(), result.begin());
  return result;
}
RecordFields rec12_fields(const ByteVector &v) {
  auto yaw = get(v, 8, 4);
  return {get(v, 0, 2), {v[2], v[3], v[4], v[5]}, {v[6], v[7]}, yaw};
}
RecordFields rec12_fields(const WireRecord &v) {
  return rec12_fields(ByteVector(v.begin(), v.end()));
}
ByteVector CodecState::pack() const {
  ByteVector out;
  for (const auto &c : ch) {
    auto b = c.pack();
    out.insert(out.end(), b.begin(), b.end());
  }
  for (auto x : {yaw_prev, yaw_pred, yaw_delta}) {
    range(x, -32768, 32767);
    append_le(out, std::uint64_t(x), 2);
  }
  range(pad46, 0, 65535);
  append_le(out, std::uint64_t(pad46), 2);
  append_le(out, std::uint64_t(count), 4);
  return out;
}
CodecState CodecState::unpack(const ByteVector &v) {
  if (v.size() < 76)
    throw ValueError("short codec state");
  CodecState out;
  for (std::size_t i = 0; i < 4; ++i)
    out.ch[i] = Channel::unpack(
        ByteVector(v.begin() + 16 * i, v.begin() + 16 * i + 16));
  out.yaw_prev = s16(get(v, 64, 2));
  out.yaw_pred = s16(get(v, 66, 2));
  out.yaw_delta = s16(get(v, 68, 2));
  out.pad46 = get(v, 70, 2);
  out.count = s32(get(v, 72, 4));
  return out;
}
BitWriter encode_frames(CodecState &state,
                        const std::vector<WireRecord> &records) {
  if (records.empty())
    throw ValueError("encoder requires a frame");
  size_limit(records.size(), MAX_FRAMES);
  std::vector<RecordFields> fields;
  for (const auto &r : records)
    fields.push_back(rec12_fields(r));
  BitWriter w;
  w.start(fields[0].buttons);
  state.count = s32(std::uint64_t(state.count) + records.size());
  Integer current = fields[0].buttons, run = 0;
  for (const auto &f : fields) {
    if (f.buttons == current)
      ++run;
    else {
      while (f.buttons != current) {
        w.put(run, 5);
        run = 0;
        auto diff = std::uint32_t(f.buttons ^ current);
        unsigned bit = 0;
        while (!(diff & (1u << bit)))
          ++bit;
        w.put(bit, 4);
        current ^= Integer{1} << bit;
      }
      run = 1;
    }
    if (run == 31) {
      w.put(31, 5);
      run = 0;
    }
  }
  if (run)
    w.put(run, 5);
  for (std::size_t p = 0; p < 4; ++p) {
    std::vector<Integer> s;
    for (const auto &f : fields)
      s.push_back(f.sticks[p]);
    encode_axis(w, state.ch[WIRE_ORDER[p]], s);
  }
  run = 0;
  for (const auto &f : fields) {
    const auto raw = s32(f.yaw & 0xfffffffe);
    const auto change = s32(std::uint64_t(raw) - std::uint64_t(state.yaw_prev));
    state.yaw_prev = s16(std::uint64_t(raw));
    const auto error = s32(std::uint64_t(raw) - std::uint64_t(state.yaw_pred));
    bool absolute =
        !(absolute32(change) < YAW_JUMP + 1) || absolute32(error) > YAW_FAR;
    if (!absolute) {
      auto delta = change;
      if (absolute32(error) > YAW_NUDGE)
        delta = s32(std::uint64_t(change) + std::uint64_t(error / 15));
      if (absolute32(s32(std::uint64_t(delta) -
                         std::uint64_t(state.yaw_delta))) < YAW_KEEP &&
          s32(std::uint64_t(state.yaw_delta) * std::uint64_t(change)) >= 0) {
        ++run;
        state.yaw_pred =
            s16(std::uint64_t(state.yaw_pred) + std::uint64_t(state.yaw_delta));
        if (run == 31) {
          w.put(31, 5);
          run = 0;
        }
      } else {
        delta = clamp(delta, -255, 255);
        w.put(run, 5);
        w.put(1, 1);
        w.put(delta & 511, 9);
        run = 0;
        state.yaw_delta = s16(std::uint64_t(delta));
        state.yaw_pred =
            s16(std::uint64_t(state.yaw_pred) + std::uint64_t(delta));
      }
    } else {
      w.put(run, 5);
      w.put(0, 1);
      w.put(f.yaw & 65534, 16);
      run = 0;
      state.yaw_pred = state.yaw_prev;
    }
  }
  if (run)
    w.put(run, 5);
  current = fields[0].yaw & 1;
  run = 0;
  w.put(current, 1);
  for (const auto &f : fields) {
    if (Integer(f.yaw & 1) != current) {
      w.put(run, 5);
      run = 0;
      current = f.yaw & 1;
    }
    if (++run == 31) {
      w.put(31, 5);
      run = 0;
    }
  }
  if (run)
    w.put(run, 5);
  w.put(STREAM_TRAILER, 16);
  return w;
}
FrameSymbols read_frames(const ByteVector &data, Integer count) {
  if (count < 1)
    throw MalformedPacket("no frames");
  frame_limit(count);
  BitReader r(data);
  FrameSymbols out;
  out.count = count;
  Integer current = r.get(16), run = r.get(5);
  for (;;) {
    if (checked_add(Integer(out.buttons.size()), run) > count)
      throw MalformedPacket("button run exceeds count");
    out.buttons.insert(out.buttons.end(), std::size_t(run), current);
    if (Integer(out.buttons.size()) == count)
      break;
    if (run != 31)
      current ^= Integer{1} << r.get(4);
    run = r.get(5);
  }
  for (auto &s : out.sticks)
    s = read_axis_symbols(r, count);
  while (Integer(out.yaw.size()) < count) {
    run = r.get(5);
    if (checked_add(Integer(out.yaw.size()), run) > count)
      throw MalformedPacket("yaw run exceeds count");
    out.yaw.insert(out.yaw.end(), std::size_t(run), YawSymbol{});
    if (Integer(out.yaw.size()) == count)
      break;
    if (run != 31) {
      if (r.get(1)) {
        auto v = r.get(9);
        out.yaw.push_back(
            {YawSymbol::Kind::Delta, v & 256 ? Integer(v) - 512 : Integer(v)});
      } else
        out.yaw.push_back({YawSymbol::Kind::Absolute, r.get(16)});
    }
  }
  current = r.get(1);
  for (;;) {
    run = r.get(5);
    if (checked_add(Integer(out.flags.size()), run) > count)
      throw MalformedPacket("flag run exceeds count");
    out.flags.insert(out.flags.end(), std::size_t(run), current);
    if (Integer(out.flags.size()) == count)
      break;
    if (run != 31)
      current ^= 1;
  }
  r.get(16);
  out.bits = r.pos;
  return out;
}
std::vector<WireRecord> apply_frames(CodecState &state, const FrameSymbols &s,
                                     Integer skip, const Fpu &f) {
  const auto count = checked_sub(s.count, skip);
  if (count < 0 || skip < 0)
    throw ValueError("skip outside packet");
  frame_limit(s.count);
  std::array<ByteVector, 4> sticks;
  for (std::size_t p = 0; p < 4; ++p) {
    const auto first = slice_start(skip, s.sticks[p].size());
    sticks[p] = apply_axis_symbols(
        state.ch[WIRE_ORDER[p]],
        AxisSymbols(s.sticks[p].begin() + first, s.sticks[p].end()), f);
  }
  std::vector<WireRecord> out;
  for (Integer index = skip; index < s.count; ++index) {
    const auto &op = s.yaw.at(python_index(index, s.yaw.size()));
    if (op.kind == YawSymbol::Kind::Keep)
      state.yaw_pred =
          s16(std::uint64_t(state.yaw_pred) + std::uint64_t(state.yaw_delta));
    else if (op.kind == YawSymbol::Kind::Delta) {
      state.yaw_delta = s16(std::uint64_t(op.value));
      state.yaw_pred =
          s16(std::uint64_t(state.yaw_pred) + std::uint64_t(op.value));
    } else
      state.yaw_pred = s16(std::uint64_t(op.value));
    auto flag = s.flags.at(python_index(index, s.flags.size()));
    const auto word =
        (std::uint32_t(state.yaw_pred) & 0xfffffffe) | std::uint32_t(flag);
    std::array<Integer, 4> samples;
    for (std::size_t p = 0; p < 4; ++p)
      samples[p] = sticks[p].at(python_index(index - skip, sticks[p].size()));
    const auto buttons = s.buttons.at(python_index(index, s.buttons.size()));
    range(buttons, 0, 65535);
    range(flag, 0, 0xffffffffLL);
    ByteVector v;
    append_le(v, buttons, 2);
    for (auto b : samples)
      v.push_back(std::uint8_t(b));
    v.push_back(0);
    v.push_back(0);
    append_le(v, word, 4);
    WireRecord record;
    std::copy(v.begin(), v.end(), record.begin());
    out.push_back(record);
  }
  state.count = s32(std::uint64_t(state.count) + std::uint64_t(count));
  return out;
}
std::pair<std::vector<WireRecord>, Integer>
decode_frames(CodecState &s, const ByteVector &data, Integer count,
              Integer skip, const Fpu &f) {
  auto symbols = read_frames(data, count);
  return {apply_frames(s, symbols, skip, f), symbols.bits};
}
WireRecord local_roundtrip(const WireRecord &r, CodecState &e, CodecState &d,
                           const Fpu &f) {
  auto w = encode_frames(e, {r});
  auto out = decode_frames(d, w.tobytes(), 1, 0, f).first[0];
  out[6] = r[6];
  out[7] = r[7];
  return out;
}
std::map<Fpu::Model, ByteVector>
fpu_sensitivity(const std::vector<Integer> &samples,
                const std::vector<Fpu::Model> &models) {
  std::map<Fpu::Model, ByteVector> out;
  for (auto model : models) {
    Channel enc, dec;
    out[model] =
        apply_axis_symbols(dec, axis_symbols(enc, samples), Fpu(model));
  }
  return out;
}
} // namespace champions::simulation::rta::netwire
