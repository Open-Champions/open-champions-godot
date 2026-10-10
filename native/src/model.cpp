#include "champions/model.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

// Independently authored from the published RTA model contract and its research
// reader: Open-Champions/champions-return-to-arms-re c3d7264,
// docs/formats/model-texture.md and tools/model_info.py. No game code/assets.
namespace champions {
namespace {
constexpr std::size_t max_input = 16U * 1024U * 1024U;
constexpr std::size_t max_packets = 32768;
constexpr std::size_t max_triangles = 200000;
void need(bool ok, const char *message) {
  if (!ok)
    throw FormatError(message);
}
struct Reader {
  Bytes bytes;
  std::uint64_t u(std::size_t at, unsigned count, std::size_t end) const {
    need(end <= bytes.size && at <= end && count <= end - at,
         "model field or packet is truncated");
    std::uint64_t value = 0;
    for (unsigned i = 0; i < count; ++i)
      value |= std::uint64_t(bytes.data[at + i]) << (8 * i);
    return value;
  }
  std::uint64_t u(std::size_t at, unsigned count) const {
    return u(at, count, bytes.size);
  }
};
std::int32_t signedValue(std::uint32_t value, unsigned bits) {
  const auto limit = std::uint64_t(1) << bits;
  return static_cast<std::int32_t>(value < limit / 2 ? std::int64_t(value)
                                                     : std::int64_t(value) -
                                                           std::int64_t(limit));
}
using Row = std::array<std::int32_t, 4>;
struct Unpack {
  unsigned command, address;
  bool is_unsigned;
  std::vector<Row> rows;
};
Unpack unpack(const Reader &reader, std::size_t &cursor, std::size_t end) {
  const auto code = static_cast<std::uint32_t>(reader.u(cursor, 4, end));
  const auto command = code >> 24;
  const auto imm = code & 65535;
  need((command & 0xe0) == 0x60 && (command & 0x10) == 0 && (command & 3) != 3,
       "unsupported masked, interrupt or packed VIF UNPACK");
  need((imm & 0x8000) && (imm & 0x3c00) == 0,
       "model UNPACK must use bounded relative TOPS addressing");
  const auto count = ((code >> 16) & 255) ? ((code >> 16) & 255) : 256;
  const auto components = ((command >> 2) & 3) + 1;
  const auto width = 4U >> (command & 3);
  const auto size = std::size_t(count) * components * width;
  const auto padded = (size + 3) & ~std::size_t(3);
  need(cursor <= end && 4 <= end - cursor && padded <= end - cursor - 4,
       "model UNPACK exceeds its mesh boundary");
  Unpack result{command, imm & 0x3ffU, (imm & 0x4000) != 0, {}};
  result.rows.resize(count);
  for (unsigned i = 0; i < count; ++i)
    for (unsigned c = 0; c < components; ++c) {
      const auto raw = static_cast<std::uint32_t>(
          reader.u(cursor + 4 + (i * components + c) * width, width, end));
      // V4-32 GIF words are preserved bitwise separately below; signed
      // conversion uses arithmetic, avoiding implementation-defined
      // unsigned-to-signed casts.
      result.rows[i][c] = signedValue(raw, width * 8);
      if (result.is_unsigned && width < 4)
        result.rows[i][c] = static_cast<std::int32_t>(raw);
    }
  cursor += 4 + padded;
  return result;
}
std::uint32_t nextCode(const Reader &r, std::size_t &cursor, std::size_t end) {
  while (cursor < end) {
    const auto code = static_cast<std::uint32_t>(r.u(cursor, 4, end));
    if (code != 0)
      return code;
    cursor += 4;
  }
  throw FormatError("model packet ends before its required command");
}
Unpack expectedUnpack(const Reader &r, std::size_t &cursor, std::size_t end,
                      unsigned command, unsigned address, bool is_unsigned,
                      unsigned count = 0, bool opaque_words = false) {
  (void)nextCode(r, cursor, end);
  auto result = unpack(r, cursor, end);
  need(result.command == command && result.address == address &&
           (opaque_words || result.is_unsigned == is_unsigned) &&
           (!count || result.rows.size() == count),
       "unexpected model UNPACK kind, address, sign or count");
  return result;
}
std::vector<ModelSkinInfluences> decodeWeights(const std::vector<Row> &entries,
                                               unsigned vertices) {
  std::size_t at = 0;
  std::vector<ModelSkinInfluences> result;
  result.reserve(vertices);
  auto append = [](ModelSkinInfluences &skin, std::int32_t address,
                   std::int32_t weight) {
    need(address >= 0 && address <= 252 && address % 4 == 0,
         "invalid model joint address");
    need(weight >= 0 && weight <= 255 && skin.count < 4,
         "invalid model influence weight or count");
    skin.joints[skin.count] = static_cast<std::uint8_t>(address / 4);
    skin.weights[skin.count] = static_cast<std::uint8_t>(weight);
    ++skin.count;
  };
  while (result.size() < vertices) {
    need(at < entries.size(), "weight list does not cover every vertex");
    const auto &row = entries[at++];
    ModelSkinInfluences skin{};
    if (row[2] == 255) {
      need(row[3] > 0, "empty rigid weight run");
      // w1 is unused for a rigid run. A final run may extend beyond the
      // packet's vertex count; preserve the existing bounded acceptance.
      append(skin, row[0], 255);
      const auto count =
          std::min(std::size_t(row[3]), std::size_t(vertices) - result.size());
      result.insert(result.end(), count, skin);
    } else {
      append(skin, row[0], row[1]);
      append(skin, row[2], row[3]);
      unsigned total = unsigned(row[1] + row[3]);
      if (total != 255) {
        need(at < entries.size(), "truncated multi-joint weight");
        const auto &more = entries[at++];
        append(skin, more[0], more[1]);
        total += unsigned(more[1]);
        if (more[2] != 255) {
          append(skin, more[2], more[3]);
          total += unsigned(more[3]);
        }
      }
      need(total == 255, "vertex weights do not sum to255");
      result.push_back(skin);
    }
  }
  need(at == entries.size() ||
           (at + 1 == entries.size() && entries[at] == Row{0, 255, 255, 255}),
       "unsupported trailing weight entries");
  return result;
}
struct Cell {
  int kind = -1;
  int vertex = -1;
  bool no_draw = false;
};
void emitPacket(const std::vector<Unpack> &data, const Unpack &uv,
                unsigned vertices, unsigned strip_count,
                const std::vector<ModelSkinInfluences> &skin, ModelMesh &mesh,
                std::size_t &triangle_count) {
  std::array<Cell, 256> memory{};
  auto address = [](std::int32_t encoded) {
    const auto base = unsigned(encoded) & 0x7fffU;
    need(base <= 253,
         "strip slot or copy address exceeds bounded output memory");
    return base;
  };
  auto retag = [](Cell cell, unsigned lane, bool no_draw) {
    if (lane == 2 && cell.kind == 2)
      cell.no_draw = no_draw;
    return cell;
  };
  const auto &slots = data[2].rows;
  for (std::size_t row = 0; row < slots.size(); ++row) {
    const auto a = address(slots[row][0]), source = address(slots[row][1]),
               destination = address(slots[row][2]);
    const std::array<Cell, 2> snapshot{
        {memory[source + 1], memory[source + 2]}};
    for (unsigned k = 1; k <= 2; ++k) {
      memory[destination + k] =
          retag(snapshot[k - 1], k, (unsigned(slots[row][2]) & 0x8000) != 0);
      memory[a + k] = row >= 2 ? Cell{int(k), int(row) - 2,
                                      (unsigned(slots[row][0]) & 0x8000) != 0}
                               : Cell{};
    }
  }
  const auto &copies = data[4].rows;
  for (std::size_t row = 1; row < copies.size(); ++row) {
    const auto s1 = address(copies[row][0]), d1 = address(copies[row][1]);
    const auto s2 = address(copies[row][2]), d2 = address(copies[row][3]);
    const std::array<Cell, 2> one{{memory[s1 + 1], memory[s1 + 2]}},
        two{{memory[s2 + 1], memory[s2 + 2]}};
    for (unsigned k = 1; k <= 2; ++k) {
      memory[d2 + k] =
          retag(two[k - 1], k, (unsigned(copies[row][3]) & 0x8000) != 0);
      memory[d1 + k] =
          retag(one[k - 1], k, (unsigned(copies[row][1]) & 0x8000) != 0);
    }
  }
  for (unsigned i = 0; i < strip_count; ++i) {
    const auto &cell = memory[i * 3 + 2];
    need(cell.kind == 2 && cell.vertex >= 0 && unsigned(cell.vertex) < vertices,
         "strip position has no valid vertex");
  }
  for (unsigned i = 2; i < strip_count; ++i) {
    std::array<unsigned, 3> strip{{i - 2, i - 1, i}};
    std::array<unsigned, 3> vi{{unsigned(memory[(i - 2) * 3 + 2].vertex),
                                unsigned(memory[(i - 1) * 3 + 2].vertex),
                                unsigned(memory[i * 3 + 2].vertex)}};
    if (memory[i * 3 + 2].no_draw || vi[0] == vi[1] || vi[1] == vi[2] ||
        vi[0] == vi[2])
      continue;
    const auto &p = data[0].rows;
    const auto &normals = data[1].rows;
    std::array<std::int64_t, 3> u{}, v{}, face{};
    for (unsigned c = 0; c < 3; ++c) {
      u[c] = std::int64_t(p[vi[1]][c]) - p[vi[0]][c];
      v[c] = std::int64_t(p[vi[2]][c]) - p[vi[0]][c];
    }
    face = {u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2],
            u[0] * v[1] - u[1] * v[0]};
    std::int64_t dot = 0;
    for (unsigned c = 0; c < 3; ++c)
      dot +=
          face[c] * (normals[vi[0]][c] + normals[vi[1]][c] + normals[vi[2]][c]);
    if (dot < 0) {
      std::swap(vi[0], vi[1]);
      std::swap(strip[0], strip[1]);
    }
    need(triangle_count < max_triangles,
         "model triangle output budget exceeded");
    ++triangle_count;
    for (unsigned c = 0; c < 3; ++c) {
      ModelCorner corner{};
      for (unsigned axis = 0; axis < 3; ++axis) {
        corner.position[axis] = float(p[vi[c]][axis]) / 16.0f;
        corner.normal[axis] = float(normals[vi[c]][axis]) / 127.0f;
      }
      corner.uv_texels = {float(uv.rows[strip[c]][0]) / 16.0f,
                          float(uv.rows[strip[c]][1]) / 16.0f};
      corner.skin = skin[vi[c]];
      mesh.corners.push_back(corner);
    }
  }
}
void meshStream(const Reader &r, std::size_t start, std::size_t end,
                ModelMesh &mesh, std::size_t &packets, std::size_t &triangles) {
  auto cursor = start;
  while (cursor < end) {
    auto code = static_cast<std::uint32_t>(r.u(cursor, 4, end));
    if (code == 0 || code == 0x01000101U) {
      cursor += 4;
      continue;
    }
    need((code >> 16) == 0x0400 && (code & 65535) >= 1 && (code & 65535) <= 41,
         "expected model ITOP vertex count1..41; unsupported VIF command or "
         "program");
    need(packets < max_packets, "model packet budget exceeded");
    ++packets;
    const auto n = code & 65535;
    cursor += 4;
    std::vector<Unpack> data;
    data.push_back(expectedUnpack(r, cursor, end, 0x69, 0, false, n));
    data.push_back(expectedUnpack(r, cursor, end, 0x6a, n, false, n));
    data.push_back(expectedUnpack(r, cursor, end, 0x69, 2 * n, true, n + 2));
    data.push_back(expectedUnpack(r, cursor, end, 0x6e, 3 * n + 2, true));
    const auto skin = decodeWeights(data[3].rows, n);
    const auto copy_address = 3 * n + 2 + unsigned(data[3].rows.size());
    data.push_back(expectedUnpack(r, cursor, end, 0x6d, copy_address, true));
    need(data[4].rows[0][0] == std::int32_t(data[4].rows.size() - 1) &&
             data[4].rows[0][1] == 0 && data[4].rows[0][2] == 0 &&
             data[4].rows[0][3] == 0,
         "invalid model copy count header");
    const auto tag_address = copy_address + unsigned(data[4].rows.size());
    need(tag_address < 128, "model packet input exceeds128 VU qwords");
    data.push_back(
        expectedUnpack(r, cursor, end, 0x6c, tag_address, false, 1, true));
    const auto &tag = data[5].rows[0];
    const auto low = std::uint64_t(static_cast<std::uint32_t>(tag[0])) |
                     (std::uint64_t(static_cast<std::uint32_t>(tag[1])) << 32);
    const auto high = std::uint64_t(static_cast<std::uint32_t>(tag[2])) |
                      (std::uint64_t(static_cast<std::uint32_t>(tag[3])) << 32);
    const auto count = unsigned(low & 0x7fff),
               prim = unsigned((low >> 47) & 0x7ff);
    constexpr auto mask =
        0xffffULL | (1ULL << 46) | (0x7ffULL << 47) | (15ULL << 60);
    need(count >= 1 && count <= 83 && (low & ~mask) == 0 && (low & 0x8000) &&
             (low & (1ULL << 46)) && (prim == 0x15c || prim == 0x11c) &&
             (low >> 60) == 3 && high == 0x413,
         "unsupported model GIF tag; expected program0x42 triangle strip");
    code = nextCode(r, cursor, end);
    need(code == 0x14000042U, "only model program0x42 is supported; "
                              "reflection/world programs are unsupported");
    cursor += 4;
    need(nextCode(r, cursor, end) == 0x01000103U,
         "model UV cycle must have stride3");
    cursor += 4;
    const auto uv = expectedUnpack(r, cursor, end, 0x65, 0x81, true, count);
    need(nextCode(r, cursor, end) == 0x01000101U,
         "model packet must restore the VIF cycle");
    cursor += 4;
    emitPacket(data, uv, n, count, skin, mesh, triangles);
  }
}
} // namespace
RtaModel decodeRtaModel(Bytes bytes) {
  need(bytes.data && bytes.size >= 0x40 && bytes.size <= max_input &&
           bytes.size % 16 == 0,
       "model must be a16-byte-aligned lump of64 bytes to16MiB");
  const Reader r{bytes};
  const auto count = unsigned(r.u(0x12, 1));
  need(count >= 1 && count <= 64, "model mesh count must be1..64");
  need(r.u(0x26, 1) == 1, "only one model mesh table is supported");
  const auto flags = static_cast<std::uint16_t>(r.u(0x10, 2));
  need(
      (flags & ~0x0fU) == 0,
      "model reflection, shell, facing-point or unknown flags are unsupported");
  need(r.u(0x14, 4) == 0 && r.u(0x23, 1) == 0 && r.u(0x27, 1) == 0,
       "model runtime fields or unsupported facing/effect segment are present");
  const auto attachment =
      signedValue(static_cast<std::uint32_t>(r.u(0x13, 1)), 8);
  need(attachment >= -1 && attachment < 16, "invalid model attachment slot");
  const auto record_start = 0x28 + std::size_t(count + 1) * 4;
  const auto header_end = record_start + std::size_t(count) * 10;
  need(header_end <= bytes.size, "model mesh records exceed file");
  std::vector<std::size_t> offsets;
  for (unsigned i = 0; i <= count; ++i) {
    const auto offset = static_cast<std::size_t>(r.u(0x28 + i * 4, 4));
    need(offset >= header_end && offset <= bytes.size && offset % 16 == 0 &&
             (offsets.empty() || offset >= offsets.back()),
         "invalid aligned model mesh offsets");
    offsets.push_back(offset);
  }
  std::array<std::size_t, 6> tables{};
  constexpr unsigned table_bytes[6] = {0x21, 0x22, 0x23, 0x24, 0x25, 0x27};
  std::vector<std::size_t> boundaries{offsets[0]};
  for (unsigned i = 0; i < tables.size(); ++i) {
    tables[i] = static_cast<std::size_t>(r.u(table_bytes[i], 1)) * 16;
    if (tables[i]) {
      need(tables[i] >= header_end && tables[i] < offsets[0],
           "model ancillary table lies outside header region");
      boundaries.push_back(tables[i]);
    }
  }
  std::sort(boundaries.begin(), boundaries.end());
  need(std::adjacent_find(boundaries.begin(), boundaries.end()) ==
           boundaries.end(),
       "model ancillary tables overlap at their starts");
  auto fits = [&](std::size_t offset, std::size_t size) {
    const auto next =
        std::upper_bound(boundaries.begin(), boundaries.end(), offset);
    need(offset != 0 && next != boundaries.end() && size <= *next - offset,
         "model ancillary table exceeds its own extent");
  };
  need(bool(flags & 2) == bool(tables[0]) && bool(flags & 4) == bool(tables[1]),
       "model table flags disagree with offsets");
  if (tables[0])
    fits(tables[0], 32 * 16);
  if (tables[1])
    fits(tables[1], 16 * 0x34);
  need(tables[4] != 0, "model hull table is required by this lump layout");
  fits(tables[4], std::size_t(count) * 24);
  if (tables[3]) {
    fits(tables[3], 16);
    const auto sample_count = static_cast<std::size_t>(r.u(tables[3] + 4, 4));
    const auto qwords = static_cast<std::size_t>(r.u(tables[3], 4));
    need(sample_count < 16384 && qwords <= max_input / 16,
         "model sample-point count or packet exceeds budget");
    fits(tables[3], 16 + qwords * 16);
    if (sample_count)
      fits(tables[3], 32 + sample_count * 8 + ((sample_count - 1) / 45) * 16);
  }
  RtaModel result{flags, r.u(8, 8), r.u(0, 8), {}, tables[0] != 0, {}};
  result.attachment_slot = static_cast<std::int8_t>(attachment);
  if (result.has_variants) {
    for (std::size_t i = 0; i < result.variants.size(); ++i) {
      result.variants[i] = {r.u(tables[0] + i * 16, 8),
                            r.u(tables[0] + i * 16 + 8, 8)};
    }
  }
  std::size_t packets = 0, triangles = 0;
  for (unsigned i = 0; i < count; ++i) {
    const auto record = record_start + i * 10;
    const auto joint = static_cast<std::uint8_t>(r.u(record + 6, 1));
    need(joint < 64 && r.u(record + 7, 1) == 0,
         "unsupported mesh record joint or reserved byte");
    ModelMesh mesh{i, ((result.hidden_mask >> i) & 1U) == 0, joint, {}};
    meshStream(r, offsets[i], offsets[i + 1], mesh, packets, triangles);
    result.meshes.push_back(std::move(mesh));
  }
  return result;
}
std::uint64_t modelVisibilityMask(const RtaModel &model, int selection) {
  need(selection >= -1 && selection < 32,
       "Choose Default or a single model variant from 0 to 31.");
  const std::uint64_t default_mask = ~model.hidden_mask;
  if (selection == -1)
    return default_mask;
  need(model.has_variants, "This model has no variant table.");
  const auto &variant = model.variants[static_cast<std::size_t>(selection)];
  return (default_mask | variant.show) & ~variant.hide;
}
} // namespace champions
