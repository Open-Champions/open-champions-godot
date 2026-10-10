#include "champions/amx.hpp"
#include <algorithm>
#include <limits>

// Authored from ref_amx.py at 5ad489a445d41612d0795c063e1ce67557d5c441.
// Opcode names, operand classes and numeric handler identities are published
// metadata; no original instructions, executable tables or scripts are used.
namespace champions::simulation::rta::amx {
const std::array<Opcode, NUM_OPCODES> OPCODES{{
    {"NONE", OperandKind::None, 0x13eea8},
    {"LOAD.pri", OperandKind::Value, 0x13eeb8},
    {"LOAD.alt", OperandKind::Value, 0x13eef0},
    {"LOAD.S.pri", OperandKind::Value, 0x13ef28},
    {"LOAD.S.alt", OperandKind::Value, 0x13ef60},
    {"LREF.pri", OperandKind::Value, 0x13ef98},
    {"LREF.alt", OperandKind::Value, 0x13efd8},
    {"LREF.S.pri", OperandKind::Value, 0x13f018},
    {"LREF.S.alt", OperandKind::Value, 0x13f058},
    {"LOAD.I", OperandKind::None, 0x13f098},
    {"LODB.I", OperandKind::Value, 0x13f0f0},
    {"CONST.pri", OperandKind::Value, 0x13f1a0},
    {"CONST.alt", OperandKind::Value, 0x13f1d0},
    {"ADDR.pri", OperandKind::Value, 0x13f200},
    {"ADDR.alt", OperandKind::Value, 0x13f238},
    {"STOR.pri", OperandKind::Value, 0x13f270},
    {"STOR.alt", OperandKind::Value, 0x13f2a8},
    {"STOR.S.pri", OperandKind::Value, 0x13f2e0},
    {"STOR.S.alt", OperandKind::Value, 0x13f318},
    {"SREF.pri", OperandKind::Value, 0x13f350},
    {"SREF.alt", OperandKind::Value, 0x13f390},
    {"SREF.S.pri", OperandKind::Value, 0x13f3d0},
    {"SREF.S.alt", OperandKind::Value, 0x13f410},
    {"STOR.I", OperandKind::None, 0x13f450},
    {"STRB.I", OperandKind::Value, 0x13f4a0},
    {"LIDX", OperandKind::None, 0x13f550},
    {"LIDX.B", OperandKind::Value, 0x13f5b0},
    {"IDXADDR", OperandKind::None, 0x13f620},
    {"IDXADDR.B", OperandKind::Value, 0x13f650},
    {"ALIGN.pri", OperandKind::Value, 0x13f688},
    {"ALIGN.alt", OperandKind::Value, 0x13f6e8},
    {"LCTRL", OperandKind::Value, 0x13f740},
    {"SCTRL", OperandKind::Value, 0x13f7d8},
    {"MOVE.pri", OperandKind::None, 0x13f848},
    {"MOVE.alt", OperandKind::None, 0x13f878},
    {"XCHG", OperandKind::None, 0x13f8a0},
    {"PUSH.pri", OperandKind::None, 0x13f8d8},
    {"PUSH.alt", OperandKind::None, 0x13f908},
    {"PUSH.R", OperandKind::Value, 0x13f970},
    {"PUSH.C", OperandKind::Value, 0x13f938},
    {"PUSH", OperandKind::Value, 0x13f9d0},
    {"PUSH.S", OperandKind::Value, 0x13fa10},
    {"POP.pri", OperandKind::None, 0x13fa50},
    {"POP.alt", OperandKind::None, 0x13fa80},
    {"STACK", OperandKind::Value, 0x13fab0},
    {"HEAP", OperandKind::Value, 0x13fb08},
    {"PROC", OperandKind::None, 0x13fb58},
    {"RET", OperandKind::None, 0x13fb90},
    {"RETN", OperandKind::None, 0x13fbf8},
    {"CALL", OperandKind::Code, 0x13fc68},
    {"CALL.pri", OperandKind::None, 0x13fca8},
    {"JUMP", OperandKind::Code, 0x13fce0},
    {"JREL", OperandKind::Value, 0x13fd10},
    {"JZER", OperandKind::Code, 0x13fd40},
    {"JNZ", OperandKind::Code, 0x13fd70},
    {"JEQ", OperandKind::Code, 0x13fda0},
    {"JNEQ", OperandKind::Code, 0x13fdd0},
    {"JLESS", OperandKind::Code, 0x13fe00},
    {"JLEQ", OperandKind::Code, 0x13fe38},
    {"JGRTR", OperandKind::Code, 0x13fe70},
    {"JGEQ", OperandKind::Code, 0x13fea8},
    {"JSLESS", OperandKind::Code, 0x13fee0},
    {"JSLEQ", OperandKind::Code, 0x13ff18},
    {"JSGRTR", OperandKind::Code, 0x13ff50},
    {"JSGEQ", OperandKind::Code, 0x13ff88},
    {"SHL", OperandKind::None, 0x13ffc0},
    {"SHR", OperandKind::None, 0x13fff0},
    {"SSHR", OperandKind::None, 0x140020},
    {"SHL.C.pri", OperandKind::Value, 0x140050},
    {"SHL.C.alt", OperandKind::Value, 0x140088},
    {"SHR.C.pri", OperandKind::Value, 0x1400c0},
    {"SHR.C.alt", OperandKind::Value, 0x1400f8},
    {"SMUL", OperandKind::None, 0x140130},
    {"SDIV", OperandKind::None, 0x140160},
    {"SDIV.alt", OperandKind::None, 0x1401b8},
    {"UMUL", OperandKind::None, 0x140208},
    {"UDIV", OperandKind::None, 0x140238},
    {"UDIV.alt", OperandKind::None, 0x140278},
    {"ADD", OperandKind::None, 0x1402c8},
    {"SUB", OperandKind::None, 0x1402f8},
    {"SUB.alt", OperandKind::None, 0x140328},
    {"AND", OperandKind::None, 0x140358},
    {"OR", OperandKind::None, 0x140388},
    {"XOR", OperandKind::None, 0x1403b8},
    {"NOT", OperandKind::None, 0x1403e8},
    {"NEG", OperandKind::None, 0x140418},
    {"INVERT", OperandKind::None, 0x140448},
    {"ADD.C", OperandKind::Value, 0x140478},
    {"SMUL.C", OperandKind::Value, 0x1404b0},
    {"ZERO.pri", OperandKind::None, 0x1404e8},
    {"ZERO.alt", OperandKind::None, 0x140518},
    {"ZERO", OperandKind::Value, 0x140548},
    {"ZERO.S", OperandKind::Value, 0x140580},
    {"SIGN.pri", OperandKind::None, 0x1405b8},
    {"SIGN.alt", OperandKind::None, 0x140600},
    {"EQ", OperandKind::None, 0x140640},
    {"NEQ", OperandKind::None, 0x140670},
    {"LESS", OperandKind::None, 0x1406a0},
    {"LEQ", OperandKind::None, 0x1406d0},
    {"GRTR", OperandKind::None, 0x140700},
    {"GEQ", OperandKind::None, 0x140730},
    {"SLESS", OperandKind::None, 0x140760},
    {"SLEQ", OperandKind::None, 0x140790},
    {"SGRTR", OperandKind::None, 0x1407c0},
    {"SGEQ", OperandKind::None, 0x1407f0},
    {"EQ.C.pri", OperandKind::Value, 0x140820},
    {"EQ.C.alt", OperandKind::Value, 0x140858},
    {"INC.pri", OperandKind::None, 0x140890},
    {"INC.alt", OperandKind::None, 0x1408c0},
    {"INC", OperandKind::Value, 0x1408f0},
    {"INC.S", OperandKind::Value, 0x140930},
    {"INC.I", OperandKind::None, 0x140970},
    {"DEC.pri", OperandKind::None, 0x1409a8},
    {"DEC.alt", OperandKind::None, 0x1409d8},
    {"DEC", OperandKind::Value, 0x140a08},
    {"DEC.S", OperandKind::Value, 0x140a48},
    {"DEC.I", OperandKind::None, 0x140a88},
    {"MOVS", OperandKind::Value, 0x140ac0},
    {"CMPS", OperandKind::Value, 0x140b08},
    {"FILL", OperandKind::Value, 0x140b58},
    {"HALT", OperandKind::Value, 0x140bb8},
    {"BOUNDS", OperandKind::Value, 0x140be0},
    {"SYSREQ.pri", OperandKind::None, 0x140c28},
    {"SYSREQ.C", OperandKind::Value, 0x140c88},
    {"FILE", OperandKind::File, 0x140d00},
    {"LINE", OperandKind::Line, 0x140d38},
    {"SYMBOL", OperandKind::Symbol, 0x140d78},
    {"SRANGE", OperandKind::Srange, 0x140dc8},
    {"JUMP.pri", OperandKind::None, 0x140e08},
    {"SWITCH", OperandKind::Code, 0x140e30},
    {"CASETBL", OperandKind::CaseTable, 0x140eb8},
}};
namespace {
Address sum(Address a, Address b) {
  if ((b > 0 && a > INT64_MAX - b) || (b < 0 && a < INT64_MIN - b))
    throw ResourceLimit("image offset overflow");
  return a + b;
}
Address records(Address base, Address i) {
  if (i > INT64_MAX / RECORD_SIZE ||
      i < INT64_MIN / static_cast<Address>(RECORD_SIZE))
    throw ResourceLimit("table index overflow");
  return sum(base, i * RECORD_SIZE);
}
void span(const ByteVector &image, Address p, Address count) {
  if (p < 0 || count < 0 || static_cast<std::uint64_t>(p) > image.size() ||
      static_cast<std::uint64_t>(count) >
          image.size() - static_cast<std::size_t>(p))
    throw HostFault("image access outside owned bytes");
}
std::uint32_t u32(const ByteVector &v, Address p) {
  span(v, p, 4);
  auto i = static_cast<std::size_t>(p);
  return std::uint32_t(v[i]) | (std::uint32_t(v[i + 1]) << 8) |
         (std::uint32_t(v[i + 2]) << 16) | (std::uint32_t(v[i + 3]) << 24);
}
std::uint16_t u16(const ByteVector &v, Address p) {
  span(v, p, 2);
  auto i = static_cast<std::size_t>(p);
  return static_cast<std::uint16_t>(v[i] | (std::uint16_t(v[i + 1]) << 8));
}
Address signed16(std::uint16_t v) {
  return v < 0x8000 ? v : static_cast<Address>(v) - 0x10000;
}
Cell signed8(std::uint8_t v) {
  return v < 128 ? v : static_cast<Cell>(v) - 256;
}
void put32(ByteVector &v, Address p, std::uint32_t value) {
  span(v, p, 4);
  for (unsigned i = 0; i < 4; ++i)
    v[static_cast<std::size_t>(p) + i] =
        static_cast<std::uint8_t>(value >> (8 * i));
}
void put16(ByteVector &v, Address p, std::uint16_t value) {
  span(v, p, 2);
  v[static_cast<std::size_t>(p)] = static_cast<std::uint8_t>(value);
  v[static_cast<std::size_t>(p) + 1] = static_cast<std::uint8_t>(value >> 8);
}
void append32(ByteVector &v, std::uint32_t x) {
  if (v.size() > MAX_IMAGE_BYTES - 4)
    throw ResourceLimit("authored image exceeds allocation limit");
  for (unsigned i = 0; i < 4; ++i)
    v.push_back(static_cast<std::uint8_t>(x >> (8 * i)));
}
bool unsigned_less(const std::string &a, const std::string &b) {
  return std::lexicographical_compare(
      a.begin(), a.end(), b.begin(), b.end(), [](char x, char y) {
        return static_cast<unsigned char>(x) < static_cast<unsigned char>(y);
      });
}
std::string padded_name(const std::string &name) {
  if (name.size() > 19)
    throw ValueError("name longer than 19 bytes");
  auto out = name;
  out.resize(20, '\0');
  return out;
}
} // namespace
Cell s32(std::uint64_t value) noexcept {
  auto x = static_cast<std::uint32_t>(value);
  return x <= 0x7fffffffu ? static_cast<Cell>(x) : -1 - static_cast<Cell>(~x);
}
std::uint32_t opcode_number(const std::string &name) {
  for (std::uint32_t i = 0; i < NUM_OPCODES; ++i)
    if (name == OPCODES[i].name)
      return i;
  throw ValueError("unknown opcode name");
}
ByteVector encode_compact(const std::vector<Cell> &cells) {
  if (cells.size() > MAX_ASSEMBLY_CELLS)
    throw ResourceLimit("compact input exceeds cell limit");
  ByteVector out;
  for (Cell value : cells) {
    unsigned groups = 1;
    while (groups < 5) {
      const auto half = Address{1} << (7 * groups - 1);
      if (-half <= value && value < half)
        break;
      ++groups;
    }
    for (unsigned group = groups; group > 0; --group) {
      auto part =
          (static_cast<std::uint32_t>(value) >> (7 * (group - 1))) & 0x7fu;
      if (group == 5 && value < 0)
        part |= 0x70u;
      out.push_back(static_cast<std::uint8_t>(part | (group > 1 ? 0x80 : 0)));
    }
    if (out.size() > MAX_IMAGE_BYTES)
      throw ResourceLimit("compact output exceeds byte limit");
  }
  return out;
}
void expand_compact(ByteVector &image, Address start, Address coded_size,
                    Address expanded_size) {
  if (image.size() > MAX_IMAGE_BYTES)
    throw ResourceLimit("compact image exceeds byte limit");
  while (coded_size > 0) {
    std::uint32_t value = 0;
    std::uint64_t shift = 0;
    std::uint8_t byte = 0;
    for (;;) {
      --coded_size;
      const auto p = sum(start, coded_size);
      span(image, p, 1);
      byte = image[static_cast<std::size_t>(p)];
      value |= std::uint32_t(byte & 0x7fu) << (shift & 31u);
      shift += 7;
      if (coded_size <= 0)
        break;
      const auto previous = sum(start, coded_size - 1);
      span(image, previous, 1);
      if (!(image[static_cast<std::size_t>(previous)] & 0x80))
        break;
    }
    if (byte & 0x40)
      while (shift < 32) {
        value |= std::uint32_t{0xff} << shift;
        shift += 8;
      }
    expanded_size = sum(expanded_size, -4);
    if (expanded_size < 0 || (expanded_size & 3))
      throw HostFault("compact expansion below code start or unaligned");
    put32(image, sum(start, expanded_size), value);
  }
}
Machine::Machine(const ByteVector &input, const NativeMap &natives_)
    : pending_natives(natives_) {
  if (input.size() > MAX_IMAGE_BYTES)
    throw ResourceLimit("script exceeds 64 MiB image limit");
  if (input.size() < STRUCT_SIZE + HEADER_SIZE)
    throw ValueError("image shorter than machine structure and header");
  image = input;
  cip = s32(u32(image, 0x0c));
  frm = s32(u32(image, 0x10));
  hea = s32(u32(image, 0x14));
  stk = s32(u32(image, 0x18));
  stp = s32(u32(image, 0x1c));
  flags = u32(image, 0x20);
  curline = s32(u32(image, 0x24));
  curfile = s32(u32(image, 0x28));
  dbgcode = s32(u32(image, 0x2c));
  dbgaddr = s32(u32(image, 0x30));
  dbgparam = s32(u32(image, 0x34));
  error = s32(u32(image, 0x5c));
  if (u32(image, 4) != 0)
    callback = [this](Address i, Address p) { return foreign_callback(i, p); };
}
void Machine::parse_header() {
  const Address h = STRUCT_SIZE;
  size = s32(u32(image, h));
  cod = s32(u32(image, h + 0x0c));
  dat = s32(u32(image, h + 0x10));
  hea0 = s32(u32(image, h + 0x14));
  hdr_stp = s32(u32(image, h + 0x18));
  start = s32(u32(image, h + 0x1c));
  num_publics = signed16(u16(image, h + 0x20));
  publics_off = s32(u32(image, h + 0x24));
  num_natives = signed16(u16(image, h + 0x28));
  natives_off = s32(u32(image, h + 0x2c));
  num_libraries = signed16(u16(image, h + 0x30));
  num_pubvars = signed16(u16(image, h + 0x38));
  data_offset = h + dat;
  header_read = true;
}
Cell Machine::header_cell(Address offset) const {
  if (!parsed)
    throw HostFault("machine base pointer is not loaded");
  return s32(u32(image, sum(STRUCT_SIZE, offset)));
}
std::string Machine::header_name(Address offset) const {
  if (!parsed)
    throw HostFault("machine base pointer is not loaded");
  const auto p = sum(STRUCT_SIZE, offset);
  span(image, p, 1);
  std::string out;
  for (auto i = static_cast<std::size_t>(p); i < image.size(); ++i) {
    if (!image[i])
      return out;
    out.push_back(static_cast<char>(image[i]));
  }
  throw HostFault("name has no image terminator");
}
std::pair<std::string, Cell> Machine::public_entry(Address i) const {
  const auto p = records(publics_off, i);
  return {header_name(sum(p, 4)), header_cell(p)};
}
std::vector<std::pair<std::string, Cell>> Machine::publics() const {
  if (!header_read)
    throw HostFault("header not parsed");
  std::vector<std::pair<std::string, Cell>> out;
  for (Address i = 0; i < num_publics; ++i)
    out.push_back(public_entry(i));
  return out;
}
std::string Machine::native_name(Address i) const {
  return header_name(sum(records(natives_off, i), 4));
}
std::vector<std::string> Machine::native_names() const {
  if (!header_read)
    throw HostFault("header not parsed");
  std::vector<std::string> out;
  for (Address i = 0; i < num_natives; ++i)
    out.push_back(native_name(i));
  return out;
}
void Machine::check_block(Address p, Address count, const char *) const {
  span(image, p, count);
}
Cell Machine::load() {
  const Address h = STRUCT_SIZE;
  if (u16(image, h + 4) != MAGIC || u16(image, h + 0x0a) != RECORD_SIZE)
    return ERR_FORMAT;
  span(image, h + 7, 1);
  if (signed8(image[h + 7]) >= 4)
    return ERR_VERSION;
  const auto top = s32(u32(image, h + 0x18));
  if (top < 1 || top > 0x7ffffffe)
    return ERR_FORMAT;
  const auto header_flags = u16(image, h + 8);
  if (header_flags & 1)
    return ERR_FORMAT;
  parse_header();
  if (header_flags & 4) {
    if (flags & FLAG_LOADED)
      throw HostFault("second load expands compact image again");
    check_block(h + cod, hea0 - cod, "compact code");
    expand_compact(image, h + cod, size - cod, hea0 - cod);
  }
  if (flags & FLAG_LOADED)
    return ERR_NONE;
  flags |= FLAG_LOADED;
  parsed = true;
  *hdr_stp -= 4;
  put32(image, h + 0x18, static_cast<std::uint32_t>(*hdr_stp));
  check_block(h + *hdr_stp, 4, "sentinel");
  put32(image, h + *hdr_stp, 0);
  hea = hea0 - dat;
  stk = stp = *hdr_stp - dat;
  if (!callback)
    callback = [this](Address i, Address p) { return call_native(i, p); };
  curline = curfile = 0;
  relocate();
  if (!pending_natives.empty())
    register_natives(pending_natives);
  return ERR_NONE;
}
Cell Machine::relocate() {
  if ((cod > 0 && dat < INT64_MIN + cod) || (cod < 0 && dat > INT64_MAX + cod))
    throw ResourceLimit("code length exceeds int64 representation");
  const Address base = sum(STRUCT_SIZE, cod), length = dat - cod;
  auto out = std::make_shared<Decoded>();
  std::map<Address, bool> seen;
  auto read = [&](Address off, bool changed = false) {
    if (off < 0 || (off & 3) || length < 4 || off > length - 4)
      throw HostFault("relocation offset outside code or unaligned");
    span(image, sum(base, off), 4);
    auto found = seen.find(off);
    if (found != seen.end() && (found->second || changed))
      throw HostFault("relocation revisits prepared cell");
    seen[off] = changed;
    return s32(u32(image, base + off));
  };
  Address pos = 0;
  while (pos < length) {
    if (out->size() >= MAX_DECODED)
      throw ResourceLimit("decoded instruction limit");
    const auto instruction_start = pos;
    auto it = seen.find(pos);
    if (it != seen.end() && it->second)
      return ERR_INVINSTR;
    const auto op = read(pos, true);
    if (op < 1 || op >= static_cast<Cell>(NUM_OPCODES))
      return ERR_INVINSTR;
    auto kind = OPCODES[static_cast<std::size_t>(op)].kind;
    pos += 4;
    Operand operand;
    switch (kind) {
    case OperandKind::Value:
    case OperandKind::Code:
      operand = read(pos, kind == OperandKind::Code);
      pos += 4;
      break;
    case OperandKind::File: {
      const auto count = read(pos);
      curfile = read(pos + 4);
      dbgcode = 1;
      dbgname = pos + 8;
      pos += 4 + static_cast<Address>(count);
      break;
    }
    case OperandKind::Line: {
      dbgcode = 2;
      const auto line = read(pos);
      const auto file = read(pos + 4);
      curline = line;
      curfile = file;
      operand = std::pair<Cell, Cell>{static_cast<Cell>(curline),
                                      static_cast<Cell>(curfile)};
      pos += 8;
      break;
    }
    case OperandKind::Symbol: {
      const auto count = read(pos);
      const auto address = read(pos + 4);
      const auto param = read(pos + 8);
      dbgaddr = address;
      dbgparam = param;
      dbgname = pos + 12;
      dbgcode = 3;
      operand = DebugSymbol{static_cast<Cell>(dbgaddr),
                            static_cast<Cell>(dbgparam), *dbgname};
      pos += 4 + static_cast<Address>(count);
      break;
    }
    case OperandKind::Srange: {
      const auto address = read(pos);
      const auto param = read(pos + 4);
      dbgaddr = address;
      dbgparam = param;
      dbgcode = 8;
      operand = std::pair<Cell, Cell>{static_cast<Cell>(dbgaddr),
                                      static_cast<Cell>(dbgparam)};
      pos += 8;
      break;
    }
    case OperandKind::CaseTable: {
      CaseTable table;
      table.count = read(pos);
      if (table.count >= 0)
        table.default_target = read(pos + 4, true);
      for (Address i = 1; i <= table.count; ++i) {
        const auto value = read(pos + 8 * i);
        const auto target = read(pos + 8 * i + 4, true);
        if (table.cases.size() >= MAX_DECODED)
          throw ResourceLimit("case table exceeds allocation limit");
        table.cases.emplace_back(value, target);
      };
      pos += 8 + 8 * static_cast<Address>(table.count);
      operand = std::move(table);
      break;
    }
    case OperandKind::None:
      break;
    }
    out->emplace(instruction_start, Instruction{op, std::move(operand), pos});
  }
  flags |= FLAG_RELOC;
  decoded = std::move(out);
  return ERR_NONE;
}
std::pair<Cell, Cell> Machine::find_public(const std::string &input) const {
  if (!parsed)
    throw HostFault("machine base pointer is not loaded");
  auto name = input.substr(0, input.find('\0'));
  Address first = 0, last = sum(num_publics, -1);
  while (first <= last) {
    const auto mid = first + (last - first) / 2;
    const auto entry = header_name(sum(records(publics_off, mid), 4));
    if (unsigned_less(name, entry))
      last = mid - 1;
    else if (unsigned_less(entry, name))
      first = mid + 1;
    else
      return {ERR_NONE, static_cast<Cell>(mid)};
  }
  return {ERR_NOTFOUND, NOT_FOUND_INDEX};
}
Assembly assemble(const std::vector<AsmLine> &program) {
  if (program.size() > MAX_ASSEMBLY_CELLS)
    throw ResourceLimit("assembly line limit");
  Assembly out;
  std::vector<AsmValue> pending;
  auto push = [&](AsmValue value) {
    if (pending.size() >= MAX_ASSEMBLY_CELLS)
      throw ResourceLimit("assembly cell limit");
    pending.push_back(std::move(value));
  };
  for (const auto &line : program) {
    if (auto label = std::get_if<Label>(&line)) {
      if (label->name.empty() || label->name.back() != ':')
        throw ValueError("label must end with colon");
      out.labels[label->name.substr(0, label->name.size() - 1)] =
          static_cast<Address>(pending.size() * 4);
      continue;
    }
    const auto &ins = std::get<AsmInstruction>(line);
    if (ins.mnemonic == "cell") {
      if (ins.operands.empty())
        throw ValueError("raw cell missing operand");
      push(ins.operands.front());
      continue;
    }
    push(static_cast<Cell>(opcode_number(ins.mnemonic)));
    if (ins.mnemonic == "CASETBL") {
      if (ins.operands.empty())
        throw ValueError("case table missing default");
      if (ins.cases.size() > MAX_ASSEMBLY_CELLS)
        throw ResourceLimit("case table too large");
      push(static_cast<Cell>(ins.cases.size()));
      push(ins.operands.front());
      for (const auto &pair : ins.cases) {
        push(pair.first);
        push(pair.second);
      }
    } else if (ins.mnemonic == "FILE" || ins.mnemonic == "SYMBOL") {
      if (ins.text.size() > MAX_IMAGE_BYTES - 4 ||
          ins.operands.size() > MAX_ASSEMBLY_CELLS)
        throw ResourceLimit("debug record too large");
      auto text = ins.text;
      text.push_back('\0');
      text.resize((text.size() + 3) & ~std::size_t{3}, '\0');
      if (text.size() > MAX_IMAGE_BYTES)
        throw ResourceLimit("debug text too large");
      push(static_cast<Cell>(4 * ins.operands.size() + text.size()));
      for (const auto &arg : ins.operands)
        push(arg);
      for (std::size_t i = 0; i < text.size(); i += 4) {
        std::uint32_t v = 0;
        for (unsigned j = 0; j < 4; ++j)
          v |= std::uint32_t(static_cast<unsigned char>(text[i + j]))
               << (8 * j);
        push(s32(v));
      }
    } else
      for (const auto &arg : ins.operands)
        push(arg);
  }
  for (const auto &arg : pending) {
    if (auto n = std::get_if<Cell>(&arg))
      out.cells.push_back(*n);
    else {
      const auto it = out.labels.find(std::get<std::string>(arg));
      if (it == out.labels.end())
        throw ValueError("unresolved assembly label");
      out.cells.push_back(s32(static_cast<std::uint64_t>(it->second)));
    }
  }
  return out;
}
ByteVector build_image(const std::vector<Cell> &code,
                       const ImageOptions &options) {
  if (code.size() > MAX_ASSEMBLY_CELLS ||
      options.data.size() > MAX_IMAGE_BYTES || options.stack_cells < 0 ||
      options.stack_cells > static_cast<Address>(MAX_IMAGE_BYTES / 4))
    throw ResourceLimit("authored image allocation limit");
  if (options.data.size() % 4)
    throw ValueError("data must contain whole cells");
  if (options.publics.size() > 32767 || options.natives.size() > 32767)
    throw ValueError("name count exceeds signed header field");
  std::vector<std::pair<std::string, Cell>> entries;
  for (const auto &e : options.publics)
    entries.emplace_back(padded_name(e.first), e.second);
  if (options.sort_publics)
    std::sort(entries.begin(), entries.end(), [](const auto &a, const auto &b) {
      return a.first == b.first ? a.second < b.second
                                : unsigned_less(a.first, b.first);
    });
  ByteVector tables;
  for (const auto &e : entries) {
    append32(tables, static_cast<std::uint32_t>(e.second));
    tables.insert(tables.end(), e.first.begin(), e.first.end());
  }
  for (const auto &name : options.natives) {
    auto text = padded_name(name);
    append32(tables, 0);
    tables.insert(tables.end(), text.begin(), text.end());
  }
  const auto cod = HEADER_SIZE + tables.size(), dat = cod + code.size() * 4,
             hea = dat + options.data.size(),
             stp = hea + static_cast<std::size_t>(options.stack_cells) * 4;
  if (stp > MAX_IMAGE_BYTES - STRUCT_SIZE)
    throw ResourceLimit("authored image exceeds byte limit");
  ByteVector body;
  for (auto cell : code)
    append32(body, static_cast<std::uint32_t>(cell));
  body.insert(body.end(), options.data.begin(), options.data.end());
  auto size = hea;
  auto flags = options.flags;
  if (options.compact) {
    std::vector<Cell> cells;
    ByteVector encoded;
    for (std::size_t i = 0; i < body.size(); i += 4) {
      cells.push_back(s32(u32(body, static_cast<Address>(i))));
      auto next = encode_compact({cells.back()});
      encoded.insert(encoded.end(), next.begin(), next.end());
      if (encoded.size() > cells.size() * 4)
        throw ValueError("compact prefix cannot expand in place");
    }
    size = cod + encoded.size();
    encoded.resize(body.size(), 0);
    body = std::move(encoded);
    flags |= 4;
  }
  ByteVector out(STRUCT_SIZE + HEADER_SIZE, 0);
  const Address h = STRUCT_SIZE;
  put32(out, h, static_cast<std::uint32_t>(size));
  put16(out, h + 4, MAGIC);
  out[h + 6] = static_cast<std::uint8_t>(options.file_version);
  out[h + 7] = static_cast<std::uint8_t>(options.amx_version);
  put16(out, h + 8, flags);
  put16(out, h + 10, RECORD_SIZE);
  put32(out, h + 12, static_cast<std::uint32_t>(cod));
  put32(out, h + 16, static_cast<std::uint32_t>(dat));
  put32(out, h + 20, static_cast<std::uint32_t>(hea));
  put32(out, h + 24, static_cast<std::uint32_t>(stp));
  put32(out, h + 28, static_cast<std::uint32_t>(options.start));
  put16(out, h + 32, static_cast<std::uint16_t>(entries.size()));
  put32(out, h + 36, HEADER_SIZE);
  put16(out, h + 40, static_cast<std::uint16_t>(options.natives.size()));
  put32(out, h + 44,
        static_cast<std::uint32_t>(HEADER_SIZE + entries.size() * RECORD_SIZE));
  put32(out, h + 52, static_cast<std::uint32_t>(cod));
  put32(out, h + 60, static_cast<std::uint32_t>(cod));
  out.insert(out.end(), tables.begin(), tables.end());
  out.insert(out.end(), body.begin(), body.end());
  out.resize(STRUCT_SIZE + stp, 0);
  return out;
}
} // namespace champions::simulation::rta::amx
