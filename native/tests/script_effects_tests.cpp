#include "champions/script_builtins.hpp"
#include "champions/script_effects.hpp"
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace fx = champions::simulation::rta::script_effects;
namespace builtins = champions::simulation::rta::builtins;

namespace {
int failed = 0;
void check(bool ok, const std::string &what) {
  if (ok)
    return;
  std::cerr << "FAIL " << what << '\n';
  ++failed;
}
std::uint32_t hex_of(const std::string &text) {
  return static_cast<std::uint32_t>(std::stoul(text, nullptr, 16));
}
std::vector<std::uint32_t> hex_list(const std::string &text) {
  std::vector<std::uint32_t> out;
  std::stringstream in(text);
  std::string item;
  while (std::getline(in, item, ','))
    if (!item.empty())
      out.push_back(hex_of(item));
  return out;
}
struct Store {
  std::uint32_t address = 0;
  std::size_t size = 0;
  std::uint32_t value = 0;
};
struct ExpectCall {
  std::string name;
  std::vector<std::uint32_t> args;
  std::vector<std::uint32_t> fargs;
};

std::optional<std::uint32_t> run_op(fx::Effects &body, const std::vector<std::string> &words) {
  const auto &op = words[0];
  auto arg = [&](std::size_t i) { return hex_of(words[i]); };
  if (op == "scale") {
    std::uint32_t scales[5] = {arg(2), arg(3), arg(4), arg(5), arg(6)};
    return body.scale_stats(arg(1), scales);
  }
  if (op == "charm")
    return body.set_charm(arg(1), arg(2), arg(3));
  if (op == "slot34") {
    body.slot34_set_home_range(arg(1), arg(2));
    return std::nullopt;
  }
  if (op == "slot27") {
    body.slot27_set_state(arg(1), arg(2));
    return std::nullopt;
  }
  if (op == "message")
    return body.show_message(arg(1), arg(2), arg(3), arg(4));
  if (op == "help")
    return body.show_help(arg(1), arg(2), arg(3));
  if (op == "fade")
    return body.set_fade(arg(1));
  if (op == "step")
    return body.step_fade(arg(1));
  if (op == "compass") {
    const std::uint32_t pos[3] = {arg(1), arg(2), arg(3)};
    return body.set_compass(pos, arg(4));
  }
  if (op == "bust")
    return body.set_bust(arg(1), arg(2), arg(3), arg(4));
  if (op == "frozen")
    return body.set_plays_while_frozen(arg(1));
  if (op == "queue")
    return body.queue_stop(arg(1));
  if (op == "music")
    return body.music_play_level(arg(1), arg(2));
  throw std::runtime_error("unknown effect op " + op);
}

void poke(builtins::Memory &memory, const Store &store) {
  if (store.size == 1)
    memory.w8(store.address, store.value);
  else if (store.size == 2)
    memory.w16(store.address, store.value);
  else if (store.size == 4)
    memory.w32(store.address, store.value);
  else
    throw std::runtime_error("unsupported poke width");
}
std::uint32_t peek(const builtins::Memory &memory, const Store &store) {
  if (store.size == 1)
    return memory.u8(store.address);
  if (store.size == 2)
    return memory.u16(store.address);
  if (store.size == 4)
    return memory.u32(store.address);
  throw std::runtime_error("unsupported peek width");
}

void run_case(const std::string &name,
              const std::vector<std::pair<std::string, std::uint32_t>> &stubs,
              const std::string &local,
              const std::vector<Store> &pres, const std::vector<Store> &posts,
              const std::vector<Store> &keeps, const std::vector<std::string> &op,
              const std::optional<std::uint32_t> &expect,
              const std::vector<ExpectCall> &calls) {
  builtins::Memory memory;
  fx::Effects body(memory);
  // ref_script_effects.Trace.call answers a call without a stub with 0.
  body.default_return = 0;
  for (const auto &stub : stubs)
    body.stubs[stub.first] = stub.second;
  if (local == "eq0")
    body.local_slot = [](std::uint32_t slot) { return slot == 0u ? 1u : 0u; };
  for (const auto &store : pres)
    poke(memory, store);
  memory.writes.clear();
  std::optional<std::uint32_t> got;
  try {
    got = run_op(body, op);
  } catch (const std::exception &error) {
    check(false, name + " threw " + error.what());
    return;
  }
  if (expect)
    check(got && *got == *expect, name + " return");
  else
    check(!got, name + " has no return");
  check(memory.writes.size() == posts.size(), name + " store count");
  const auto stores = std::min(memory.writes.size(), posts.size());
  for (std::size_t i = 0; i < stores; ++i) {
    check(memory.writes[i].first == posts[i].address && memory.writes[i].second == posts[i].size,
          name + " store address");
    check(peek(memory, posts[i]) == posts[i].value, name + " store value");
  }
  for (const auto &keep : keeps)
    check(peek(memory, keep) == keep.value, name + " unchanged word");
  check(body.calls.size() == calls.size(), name + " call count");
  const auto n = std::min(body.calls.size(), calls.size());
  for (std::size_t i = 0; i < n; ++i) {
    check(body.calls[i].name == calls[i].name, name + " call " + calls[i].name);
    check(body.calls[i].args == calls[i].args, name + " call args " + calls[i].name);
    check(body.calls[i].fargs == calls[i].fargs, name + " call fargs " + calls[i].name);
  }
}
} // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "usage: script-effects-tests VECTORS\n";
    return 2;
  }
  std::ifstream in(argv[1]);
  if (!in) {
    std::cerr << "vector file missing\n";
    return 2;
  }
  std::string line;
  std::string name;
  bool open = false;
  int cases = 0;
  std::vector<std::pair<std::string, std::uint32_t>> stubs;
  std::string local;
  std::vector<Store> pres, posts, keeps;
  std::vector<std::string> op;
  std::optional<std::uint32_t> expect;
  bool saw_ret = false;
  std::vector<ExpectCall> calls;
  auto reset = [&]() {
    stubs.clear();
    local.clear();
    pres.clear();
    posts.clear();
    keeps.clear();
    op.clear();
    expect.reset();
    saw_ret = false;
    calls.clear();
  };
  auto words_of = [](const std::string &text) {
    std::stringstream in_line(text);
    std::vector<std::string> words;
    std::string word;
    while (in_line >> word)
      words.push_back(word);
    return words;
  };
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#')
      continue;
    const auto words = words_of(line);
    if (words.empty())
      continue;
    if (words[0] == "case") {
      check(!open, "case started before the previous one ended");
      open = true;
      reset();
      name = words.size() > 1 ? words[1] : "";
      continue;
    }
    check(open, "directive outside a case");
    if (!open)
      continue;
    if (words[0] == "stub")
      stubs.emplace_back(words[1], hex_of(words[2]));
    else if (words[0] == "local")
      local = words[1];
    else if (words[0] == "pre" || words[0] == "post" || words[0] == "keep") {
      Store store{hex_of(words[1]), static_cast<std::size_t>(std::stoul(words[2])), hex_of(words[3])};
      if (words[0] == "pre")
        pres.push_back(store);
      else if (words[0] == "post")
        posts.push_back(store);
      else
        keeps.push_back(store);
    } else if (words[0] == "op")
      op.assign(words.begin() + 1, words.end());
    else if (words[0] == "ret") {
      saw_ret = true;
      expect = hex_of(words[1]);
    } else if (words[0] == "call") {
      ExpectCall call;
      call.name = words[1];
      std::string rest;
      for (std::size_t i = 2; i < words.size(); ++i)
        rest += (rest.empty() ? "" : " ") + words[i];
      const auto slash = rest.find('/');
      const auto args = slash == std::string::npos ? rest : rest.substr(0, slash);
      const auto fargs = slash == std::string::npos ? std::string() : rest.substr(slash + 1);
      call.args = hex_list(args);
      call.fargs = hex_list(fargs);
      calls.push_back(call);
    } else if (words[0] == "end") {
      check(!op.empty(), name + " has an op");
      if (!saw_ret)
        expect.reset();
      run_case(name, stubs, local, pres, posts, keeps, op, expect, calls);
      open = false;
      ++cases;
    } else
      check(false, name + " unknown directive " + words[0]);
  }
  check(!open, "file ended inside a case");
  check(cases > 0, "the vector file has cases");
  if (failed) {
    std::cerr << failed << " failure(s)\n";
    return 1;
  }
  std::cout << "script-effects ok, " << cases << " cases\n";
  return 0;
}
