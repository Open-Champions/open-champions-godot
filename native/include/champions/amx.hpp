#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace champions::simulation::rta::amx {
using Cell = std::int32_t;
using ByteVector = std::vector<std::uint8_t>;
using Address = std::int64_t;
class HostFault : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};
class Limit : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};
class ResourceLimit : public Limit {
public:
  using Limit::Limit;
};
class ZeroDivisionError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};
class ValueError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};
inline constexpr std::uint32_t M32 = 0xffffffffu, STRUCT_SIZE = 0x60,
                               HEADER_SIZE = 0x40, RECORD_SIZE = 24,
                               MAGIC = 0xf1e0, STKMARGIN = 0x40;
inline constexpr std::uint32_t FLAG_LOADED = 0x2000, FLAG_BROWSE = 0x4000,
                               FLAG_RELOC = 0x8000;
inline constexpr Cell ERR_NONE = 0, ERR_STACKERR = 3, ERR_BOUNDS = 4,
                      ERR_MEMACCESS = 5, ERR_INVINSTR = 6, ERR_STACKLOW = 7,
                      ERR_HEAPLOW = 8, ERR_CALLBACK = 9, ERR_DIVIDE = 11,
                      ERR_MEMORY = 16, ERR_FORMAT = 17, ERR_VERSION = 18,
                      ERR_NOTFOUND = 19, ERR_INDEX = 20, ERR_DEBUG = 21,
                      ERR_INIT = 22, ERR_USERDATA = 23, ERR_INIT_JIT = 24;
inline constexpr Cell EXEC_MAIN = -1, NOT_FOUND_INDEX = 0x7fffffff;
// Explicit resource refusal, never a fabricated VM result. Limits exceed the
// published private corpus; they do not grant accesses beyond the image.
inline constexpr std::size_t MAX_IMAGE_BYTES = 64u * 1024u * 1024u,
                             MAX_DECODED = 1024u * 1024u,
                             MAX_ASSEMBLY_CELLS = MAX_IMAGE_BYTES / 4;
Cell s32(std::uint64_t) noexcept;
std::pair<Cell, Cell> trunc_div(Cell, Cell);
std::pair<Cell, Cell> floored_div(Cell, Cell);
enum class OperandKind {
  None,
  Value,
  Code,
  File,
  Line,
  Symbol,
  Srange,
  CaseTable
};
struct Opcode {
  const char *name;
  OperandKind kind;
  std::uint32_t handler;
};
inline constexpr std::size_t NUM_OPCODES = 0x83;
extern const std::array<Opcode, NUM_OPCODES> OPCODES;
std::uint32_t opcode_number(const std::string &);
struct CaseTable {
  Cell count = 0;
  std::optional<Cell> default_target;
  std::vector<std::pair<Cell, Cell>> cases;
};
struct DebugSymbol {
  Cell address = 0, flags = 0;
  Address name = 0;
};
using Operand = std::variant<std::monostate, Cell, std::pair<Cell, Cell>,
                             DebugSymbol, CaseTable>;
struct Instruction {
  Cell opcode = 0;
  Operand operand;
  Address next = 0;
};
using Decoded = std::map<Address, Instruction>;
using Profile = std::map<Cell, std::uint64_t>;
void expand_compact(ByteVector &, Address start, Address coded_size,
                    Address expanded_size);
ByteVector encode_compact(const std::vector<Cell> &);
class Machine;
using Native =
    std::function<std::optional<Address>(Machine &, const std::vector<Cell> &)>;
using NativeMap = std::map<std::string, Native>;
using Callback = std::function<std::pair<Address, Address>(Address, Address)>;
enum class BindingKind { Unbound, Foreign, Bound };
struct Binding {
  BindingKind kind = BindingKind::Unbound;
  Native function;
};
using ExecuteResult = std::pair<Address, std::optional<Address>>;
struct Snapshot {
  ByteVector image;
  std::vector<std::optional<Address>> state;
};
class Machine {
public:
  // Machine owns its bytes. Native callbacks may reenter, mutate ordinary
  // state, or replace decoded/profile shared pointers; active calls retain old
  // maps. Destruction/moving of a machine while callbacks use it is not
  // supported.
  ByteVector image;
  Address cip = 0, frm = 0, hea = 0, stk = 0, stp = 0;
  std::uint32_t flags = 0;
  Address curline = 0, curfile = 0, dbgcode = 0, dbgaddr = 0, dbgparam = 0;
  std::optional<Address> dbgname;
  Address error = 0, params = 0;
  Callback callback;
  std::shared_ptr<Profile> profile;
  std::optional<Address> budget;
  std::shared_ptr<Decoded> decoded;
  std::optional<std::vector<Binding>> bound;
  bool parsed = false, header_read = false;
  NativeMap pending_natives;
  Address size = 0, cod = 0, dat = 0, hea0 = 0, start = 0, num_publics = 0,
          publics_off = 0, num_natives = 0, natives_off = 0, num_libraries = 0,
          num_pubvars = 0, data_offset = 0;
  std::optional<Address> hdr_stp;
  explicit Machine(const ByteVector &, const NativeMap &natives = {});
  Machine(const Machine &) = delete;
  Machine &operator=(const Machine &) = delete;
  Machine(Machine &&) = delete;
  Machine &operator=(Machine &&) = delete;
  void parse_header();
  Cell header_cell(Address) const;
  std::string header_name(Address) const;
  std::pair<std::string, Cell> public_entry(Address) const;
  std::vector<std::pair<std::string, Cell>> publics() const;
  std::string native_name(Address) const;
  std::vector<std::string> native_names() const;
  Cell load();
  void check_block(Address position, Address count, const char *what) const;
  Cell relocate();
  std::pair<Cell, Cell> find_public(const std::string &) const;
  std::vector<Binding> &natives();
  Cell register_natives(const NativeMap &);
  std::pair<Address, Address> foreign_callback(Address, Address);
  std::pair<Address, Address> call_native(Address, Address);
  Address call(const std::string &, Address pri = 0);
  ExecuteResult execute(Address index, const std::vector<Cell> &params = {},
                        Address pri = 0);
  std::size_t position(Address, Address size) const;
  std::size_t block(Address, std::uint64_t count) const;
  Cell cell(Address) const;
  void set_cell(Address, std::uint64_t);
  ByteVector memory() const;
  Cell get_addr(Address) const;
  std::string get_string(Address) const;
  Cell set_string(Address, const std::string &, bool pack);
  Address str_len(Address) const;
  std::pair<Cell, std::optional<Address>> allot(Address cells);
  Cell release(Address);
  Cell raise_error(Address);
  Snapshot snapshot() const;
  void restore(const Snapshot &);
};
// Authored fixture helpers: byte strings, no locale or host pointer relocation.
using AsmValue = std::variant<Cell, std::string>;
struct Label {
  std::string name;
};
struct AsmInstruction {
  std::string mnemonic;
  std::vector<AsmValue> operands;
  std::string text;
  std::vector<std::pair<Cell, AsmValue>> cases;
};
using AsmLine = std::variant<Label, AsmInstruction>;
struct Assembly {
  std::vector<Cell> cells;
  std::map<std::string, Address> labels;
};
Assembly assemble(const std::vector<AsmLine> &);
struct ImageOptions {
  ByteVector data;
  std::vector<std::pair<std::string, Cell>> publics;
  std::vector<std::string> natives;
  Address stack_cells = 2048;
  Cell start = -1;
  std::uint16_t flags = 0;
  std::int8_t file_version = 3, amx_version = 1;
  bool compact = false, sort_publics = true;
};
ByteVector build_image(const std::vector<Cell> &code,
                       const ImageOptions &options = {});
} // namespace champions::simulation::rta::amx
