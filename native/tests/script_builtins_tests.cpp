#include "champions/script_builtins.hpp"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>

namespace b = champions::simulation::rta::builtins;
namespace a = champions::simulation::rta::amx;
namespace {
const std::vector<std::string> names = {"getv", "setv", "addv", "getpv", "setpv"};
struct CallbackFault : std::runtime_error { using std::runtime_error::runtime_error; };
void require(bool ok, const std::string &message) {
  if (!ok) throw std::runtime_error(message);
}
std::string hex(std::uint64_t value, int width = 8) {
  std::ostringstream out;
  out << std::hex << std::setfill('0') << std::setw(width) << value;
  return out.str();
}
std::string number(a::Address value) { return hex(static_cast<std::uint32_t>(value)); }
std::vector<std::string> split(const std::string &value, char sep) {
  std::vector<std::string> result;
  std::size_t start = 0;
  for (;;) {
    auto end = value.find(sep, start);
    result.push_back(value.substr(start, end - start));
    if (end == value.npos) return result;
    start = end + 1;
  }
}
std::uint32_t numeric(const std::string &value) {
  require(value.size() == 8, "input integer width");
  std::size_t consumed = 0;
  auto result = std::stoull(value, &consumed, 16);
  require(consumed == 8 && result <= UINT32_MAX, "input integer");
  return static_cast<std::uint32_t>(result);
}
std::string unhex(const std::string &value) {
  if (value == "-") return {};
  require(value.size() % 2 == 0, "input string width");
  std::string result;
  for (std::size_t i = 0; i < value.size(); i += 2) {
    auto part = value.substr(i, 2);
    std::size_t consumed = 0;
    auto ch = std::stoul(part, &consumed, 16);
    require(consumed == 2 && ch <= 255, "input byte");
    result.push_back(static_cast<char>(ch));
  }
  return result;
}
std::string text(const std::string &value) {
  std::string result;
  for (unsigned char ch : value) result += hex(ch, 2);
  return result.empty() ? "-" : result;
}
template<class T> std::string join(const T &values) {
  std::string result;
  for (const auto &value : values) {
    if (!result.empty()) result += ',';
    result += value;
  }
  return result.empty() ? "-" : result;
}
struct Runner {
  std::unique_ptr<b::Variables> variables;
  std::unique_ptr<a::Machine> machine;
  unsigned mode = 0;
  std::vector<std::string> events;
  void reset() {
    variables = std::make_unique<b::Variables>();
    a::ImageOptions options;
    options.data.resize(4096);
    options.natives = names;
    options.stack_cells = 128;
    machine = std::make_unique<a::Machine>(a::build_image({0}, options), variables->natives());
    require(machine->load() == 0, "fixture machine load");
    variables->request_config_save = [this]() {
      events.push_back("memcard_requestConfigSave");
      if (mode == 1) throw CallbackFault("authored callback exception");
      if (mode == 2 || mode == 3) {
        auto value = variables->quest_get("Callback");
        variables->quest_set("Callback", value + 1);
        variables->profile_set("Unsaved", 99, false);
      }
      if (mode == 3) variables->forget_string_slot();
    };
  }
  std::string snapshot() const {
    std::vector<std::string> memory, writes, slots, registers;
    for (const auto &entry : variables->memory.bytes)
      if (entry.second) memory.push_back(hex(entry.first) + ':' + hex(entry.second, 2));
    for (const auto &entry : variables->memory.writes)
      writes.push_back(hex(entry.first) + ':' + hex(entry.second));
    if (variables->string_slot) slots.push_back("00000020:" + number(*variables->string_slot));
    const auto &m = *machine;
    for (auto value : {m.cip, m.frm, m.hea, m.stk, m.stp, a::Address(m.flags),
                       m.curline, m.curfile, m.dbgcode, m.dbgaddr, m.dbgparam, m.error, m.params})
      registers.push_back(number(value));
    std::string image;
    for (auto value : m.image) image += hex(value, 2);
    return join(memory) + '|' + join(writes) + '|' + join(slots) + '|' + join(events) +
           '|' + join(registers) + '|' + image;
  }
  std::string run(const std::string &op, const std::string &inputs) {
    auto args = split(inputs, '|');
    require(args.size() == 6, "input field count");
    auto name = unhex(args[0]);
    auto value = a::s32(numeric(args[1]));
    require(args[2] == "0" || args[2] == "1", "pack/save flag");
    bool pack = args[2] == "1";
    auto address = a::s32(numeric(args[3]));
    auto count = a::s32(numeric(args[4]));
    mode = static_cast<unsigned>(std::stoul(args[5]));
    require(mode <= 3, "callback mode");
    if (op == "reset") reset();
    require(bool(variables), "missing reset");
    variables->memory.writes.clear();
    events.clear();
    std::string result = "-";
    try {
      auto it = std::find(names.begin(), names.end(), op == "nulcell" ? "setv" : op);
      if (it != names.end()) {
        machine->set_string(0, name, pack);
        if (op == "nulcell") {
          machine->set_cell(0, 0x80000000);
          machine->set_cell(4, 0);
        }
        machine->set_cell(3000, static_cast<std::uint32_t>(count));
        machine->set_cell(3004, static_cast<std::uint32_t>(address));
        machine->set_cell(3008, static_cast<std::uint32_t>(value));
        machine->set_cell(3012, 0xfacefeed);
        machine->error = 7;
        auto called = machine->call_native(it - names.begin(), 3000);
        require(called.first == 0, "dispatcher error");
        result = number(called.second);
      } else if (op == "shortadd" || op == "shortset") {
        machine->set_string(0, name, false);
        auto params = static_cast<a::Address>(machine->image.size()) - machine->data_offset - 8;
        machine->set_cell(params, 0);
        machine->set_cell(params + 4, 0);
        machine->error = 7;
        machine->call_native(op == "shortadd" ? 2 : 1, params);
      } else if (op == "get") result = number(variables->quest_get(name));
      else if (op == "set") variables->quest_set(name, value);
      else if (op == "pget") result = number(variables->profile_get(name));
      else if (op == "pset") variables->profile_set(name, value, pack);
      else if (op == "clear") variables->quest_clear_all();
      else if (op == "forget") variables->forget_string_slot();
      else if (op == "index") {
        auto entry = variables->quest_by_index(value);
        if (entry) result = text(entry->first) + ':' + number(entry->second);
      } else if (op == "unknown") variables->dispatch("unported", *machine);
      else require(op == "reset", "unknown fixture operation");
    } catch (const b::StackGarbage &) { result = "StackGarbage"; }
    catch (const CallbackFault &) { result = "CallbackFault"; }
    catch (const a::HostFault &) { result = "HostFault"; }
    return result;
  }
};
void api_tests() {
  b::Variables variables;
  bool refused = false;
  try { variables.profile_set("new", 12, true); }
  catch (const a::HostFault &) { refused = true; }
  require(refused && variables.profile_get("NEW") == 12 &&
          variables.memory.writes.size() == 2, "missing callback preserves write prefix");
  variables.profile_set("NEW", 12, true); // equal value does not require callback
  unsigned calls = 0;
  variables.request_config_save = [&]() { ++calls; variables.profile_set("Other", 7, false); };
  variables.profile_set("NEW", 13, true);
  require(calls == 1 && variables.profile_get("other") == 7, "reentrant save callback");
  variables.quest_set(std::string("a\0b", 3), 9);
  require(variables.memory.read(b::QUEST_VARS) == 'a' &&
          variables.memory.read(b::QUEST_VARS + 1) == 0 &&
          variables.memory.read(b::QUEST_VARS + 2) == 'b' &&
          variables.memory.read(b::QUEST_VARS + 3) == 0 &&
          variables.memory.read(b::QUEST_VARS + 4) == 9,
          "embedded NUL retains pinned raw-name write semantics");
  Runner runner;
  runner.reset();
  runner.machine->params = static_cast<a::Address>(runner.machine->image.size()) -
                           runner.machine->data_offset - 8;
  runner.machine->set_cell(runner.machine->params + 4, 0);
  runner.machine->set_string(0, "Prefix", false);
  refused = false;
  try { runner.variables->dispatch("addv", *runner.machine); }
  catch (const a::HostFault &) { refused = true; }
  require(refused && runner.variables->quest_by_index(0)->first == "prefix",
          "addv get inserts before invalid delta read");
  runner.machine->params = std::numeric_limits<a::Address>::max();
  refused = false;
  try { runner.variables->dispatch("getv", *runner.machine); }
  catch (const a::HostFault &) { refused = true; }
  require(refused, "host parameter arithmetic overflow refusal");
  // Real AMX bytecode invocation: no direct native runner shortcuts.
  a::ImageOptions options;
  options.data.resize(128);
  options.natives = {"setv", "addv"};
  options.publics = {{"start", 0}};
  auto assembled = a::assemble({a::AsmInstruction{"PROC", {}, {}, {}},
      a::AsmInstruction{"PUSH.C", {a::Cell(127)}, {}, {}},
      a::AsmInstruction{"PUSH.C", {a::Cell(0)}, {}, {}},
      a::AsmInstruction{"PUSH.C", {a::Cell(8)}, {}, {}},
      a::AsmInstruction{"SYSREQ.C", {a::Cell(0)}, {}, {}},
      a::AsmInstruction{"STACK", {a::Cell(12)}, {}, {}},
      a::AsmInstruction{"PUSH.C", {a::Cell(2147483647)}, {}, {}},
      a::AsmInstruction{"PUSH.C", {a::Cell(0)}, {}, {}},
      a::AsmInstruction{"PUSH.C", {a::Cell(8)}, {}, {}},
      a::AsmInstruction{"SYSREQ.C", {a::Cell(1)}, {}, {}},
      a::AsmInstruction{"STACK", {a::Cell(12)}, {}, {}},
      a::AsmInstruction{"RETN", {}, {}, {}}});
  a::Machine machine(a::build_image(assembled.cells, options), variables.natives());
  require(machine.load() == 0, "bytecode fixture load");
  machine.set_string(0, "Bytecode", false);
  auto result = machine.execute(0);
  require(result.first == 0 && result.second == -2147483522 &&
          variables.quest_get("bytecode") == -128, "SYSREQ.C signed32 sum and clamp");
  require(variables.natives().size() == 5, "subset binding count");
  std::cout << "script built-ins API PASS\n";
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc == 2 && std::string(argv[1]) == "--api") { api_tests(); return 0; }
    require(argc == 3, "usage: script-builtins-tests vectors.tsv actual.tsv | --api");
    std::ifstream source(argv[1]);
    std::ofstream trace(argv[2]);
    require(bool(source) && bool(trace), "fixture/trace open");
    Runner runner;
    std::size_t rows = 0;
    std::string line;
    while (std::getline(source, line)) {
      if (line.empty() || line[0] == '#') continue;
      auto fields = split(line, '\t');
      require(fields.size() == 4, "fixture field count");
      auto result = runner.run(fields[0], fields[1]);
      auto state = runner.snapshot();
      ++rows;
      auto actual = fields[0] + '\t' + fields[1] + '\t' + result + '\t' + state;
      trace << actual << '\n';
      if (result != fields[2] || state != fields[3]) {
        auto expected_state = split(fields[3], '|');
        auto actual_state = split(state, '|');
        std::string difference;
        for (std::size_t i = 0; i < actual_state.size(); ++i)
          if (actual_state[i] != expected_state[i]) difference += " state[" + std::to_string(i) + "]";
        throw std::runtime_error("row " + std::to_string(rows) + " " + fields[0] +
                                 " expected " + fields[2] + " got " + result + difference);
      }
    }
    require(source.eof() && rows > 1000, "incomplete fixture input");
    trace.flush();
    require(bool(trace), "trace write");
    std::cout << "script built-ins exact PASS: " << rows << " complete state transitions\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "script built-ins FAIL: " << error.what() << '\n';
    return 1;
  }
}
