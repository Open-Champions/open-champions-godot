// Replays tools/generate_horsemen_mount_vectors.py output: the memory rig of research
// tools/test_ref_horsemen_mount.py run through tools/ref_horsemen_mount.py (research
// t3/research-horsemen-mount 397c35c). Synthetic bytes only; no game data.
// Also checks the native-only host refusals the research leaves Unknown.
#include "champions/arena_monsters.hpp"
#include <cstdint>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace A = champions::simulation::rta::arena_monsters;
using A::Value;
using Vec3 = A::Vec3;
namespace {
constexpr std::uint32_t RIDER = 0x10000, MOUNT = 0x11000;
std::vector<std::string> split(const std::string &s, char sep) {
  std::vector<std::string> out;
  std::string cur;
  for (char c : s) {
    if (c == sep) { out.push_back(cur); cur.clear(); } else cur += c;
  }
  out.push_back(cur);
  return out;
}
void load_bytes(A::Memory &mem, const std::string &text) {
  if (text == "-") return;
  for (const auto &pair : split(text, ',')) {
    const auto at = pair.find(':');
    mem.poke(std::uint32_t(std::stoul(pair.substr(0, at))), 1, std::stoul(pair.substr(at + 1)));
  }
}
std::string val(const Value &v) {
  if (auto p = std::get_if<std::int64_t>(&v)) return "i" + std::to_string(std::uint64_t(*p));
  if (auto p = std::get_if<Vec3>(&v)) {
    std::string s = "v";
    for (std::size_t k = 0; k < 3; ++k) s += (k ? "," : "") + std::to_string((*p)[k]);
    return s;
  }
  return "?";
}
struct Answer {
  std::string kind;
  std::vector<std::int64_t> list;
  std::int64_t fallback = 0;
  std::map<std::int64_t, std::int64_t> map;
  std::size_t used = 0;
};
} // namespace

int main(int argc, char **argv) {
  if (argc != 2) { std::cerr << "usage: horsemen-mount-tests expected-cases < vectors.tsv\n"; return 2; }
  const auto expected_cases = std::stoull(argv[1]);
  std::istream &in = std::cin;
  std::string line, linked_bytes, unlinked_bytes;
  std::size_t cases = 0, failed = 0;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') continue;
    auto f = split(line, '\t');
    if (f[0] == "L" && f.size() == 2) { linked_bytes = f[1]; continue; }
    if (f[0] == "U" && f.size() == 2) { unlinked_bytes = f[1]; continue; }
    if (f.size() != 10) { std::cerr << "bad line\n"; return 2; }
    const std::string &name = f[0], &op = f[1];
    A::Memory mem;
    load_bytes(mem, f[4] == "L" ? linked_bytes : unlinked_bytes);
    load_bytes(mem, f[5]);
    A::Model m(mem);
    m.reference_defaults = true;
    m.nightmare_native = true;
    std::map<std::string, Answer> answers;
    if (f[3] != "-")
      for (const auto &spec : split(f[3], '|')) {
        const auto eq = spec.find('='), colon = spec.find(':', eq);
        const auto key = spec.substr(0, eq), kind = spec.substr(eq + 1, colon - eq - 1), rest = spec.substr(colon + 1);
        Answer a;
        a.kind = kind;
        if (kind == "const") a.fallback = std::stoll(rest);
        else if (kind == "seq") for (const auto &x : split(rest, ',')) a.list.push_back(std::stoll(x));
        else {
          std::string table = rest;
          if (kind == "arg0") { const auto c2 = rest.find(':'); a.fallback = std::stoll(rest.substr(0, c2)); table = rest.substr(c2 + 1); }
          for (const auto &p : split(table, ','))
            if (!p.empty()) { const auto g = p.find('>'); a.map[std::stoll(p.substr(0, g))] = std::stoll(p.substr(g + 1)); }
        }
        answers[key] = a;
      }
    for (auto &kv : answers) {
      auto *a = &kv.second;
      m.callbacks[kv.first] = [a, &mem](A::Model &, const std::vector<Value> &args) -> Value {
        if (a->kind == "const") return std::int64_t(a->fallback);
        if (a->kind == "seq") {
          const auto v = a->list[std::min(a->used, a->list.size() - 1)];
          ++a->used;
          return std::int64_t(v);
        }
        if (a->kind == "arg0") {
          const auto key = std::get<std::int64_t>(args.at(0));
          const auto it = a->map.find(key);
          return std::int64_t(it == a->map.end() ? a->fallback : it->second);
        }
        // link: handle_link(head, obj, obj) stores the node of obj in the head word
        const auto head = std::uint32_t(std::get<std::int64_t>(args.at(0)));
        const auto it = a->map.find(std::get<std::int64_t>(args.at(1)));
        mem.poke(head, 4, it == a->map.end() ? 0 : std::uint64_t(it->second));
        return std::monostate{};
      };
    }
    std::string result = "n";
    std::string error;
    try {
      const auto arg = [&](std::size_t k) { return std::uint32_t(std::stoull(split(f[2], ',')[k])); };
      m.in_reference_mode([&] {
        if (op == "nm_set_state") m.nightmare_set_state(MOUNT, arg(0));
        else if (op == "nm_run") m.nightmare_run(MOUNT);
        else if (op == "rider_run") m.horsemen_run(RIDER);
        else if (op == "rider_set_state") m.horsemen_set_state(RIDER, arg(0));
        else if (op == "nm_ctor") m.nightmare_ctor(MOUNT, 0, {0, 0, 0}, arg(0), arg(1));
        else if (op == "hm_ctor") m.horsemen_ctor(RIDER, 0, {0, 0, 0}, 0, 0x9000, arg(0), 1);
        else if (op == "pick") result = std::to_string(m.nightmare_pick_charge_target(MOUNT));
        else if (op == "touch") {
          std::vector<std::uint32_t> t;
          if (f[2] != "-") for (const auto &x : split(f[2], ',')) t.push_back(std::uint32_t(std::stoull(x)));
          m.nightmare_on_touch_list(MOUNT, t);
        } else if (op == "hit_terrain") m.nightmare_on_hit_terrain(MOUNT);
        else if (op == "draw") m.nightmare_draw(MOUNT);
        else if (op == "angle") result = std::to_string(m.angle_to_target(MOUNT));
        else if (op == "seat") m.nightmare_seat_transform(MOUNT, arg(0));
        else throw std::runtime_error("unknown op " + op);
      });
    } catch (const std::exception &e) {
      error = e.what();
    }
    std::string writes, calls;
    for (const auto &w : m.writes)
      writes += (writes.empty() ? "" : ";") + std::to_string(w.address) + "," + std::to_string(w.size) + "," + std::to_string(w.value);
    for (const auto &c : m.calls) {
      std::string s = "c" + c.name + "(";
      for (std::size_t k = 0; k < c.args.size(); ++k) s += (k ? ";" : "") + val(c.args[k]);
      calls += (calls.empty() ? "" : ";") + s + ")";
    }
    if (writes.empty()) writes = "-";
    if (calls.empty()) calls = "-";
    ++cases;
    const bool ok = error.empty() && writes == f[6] && calls == f[7] && std::to_string(m.draws) == f[8] && result == f[9];
    if (!ok) {
      ++failed;
      if (failed <= 5) {
        std::cerr << "MISMATCH " << name << (error.empty() ? "" : " error: " + error) << "\n";
        if (writes != f[6]) std::cerr << "  writes\n   got " << writes.substr(0, 400) << "\n   exp " << f[6].substr(0, 400) << "\n";
        if (calls != f[7]) std::cerr << "  calls\n   got " << calls.substr(0, 600) << "\n   exp " << f[7].substr(0, 600) << "\n";
        if (std::to_string(m.draws) != f[8] || result != f[9]) std::cerr << "  draws " << m.draws << "/" << f[8] << " result " << result << "/" << f[9] << "\n";
      }
    }
  }
  if (cases != expected_cases || cases == 0) {
    std::cerr << "vector count " << cases << " != manifest " << expected_cases << "\n";
    return 1;
  }
  // Native-only refusals (the research leaves these Unknown / the host's choice).
  {
    A::Memory mem;
    A::Model m(mem);
    mem.poke(0x4d8ae0, 4, 0x7fffffff);
    mem.poke(0x4d8af0 + 0x43d8, 4, 0);
    bool refused = false;
    try {
      m.in_reference_mode([&] { m.nightmare_pick_charge_target(MOUNT); });
    } catch (const A::Error &) { refused = true; }
    ++cases;
    if (!refused) { ++failed; std::cerr << "MISMATCH picker accepted an unbounded player count\n"; }
  }
  {
    // Without the opt-in, Horsemen_ctor still hands the mount to the host as a call-out.
    A::Memory mem;
    A::Model m(mem);
    m.callbacks["pool_new"] = [](A::Model &, const std::vector<Value> &) -> Value { return std::int64_t(MOUNT); };
    bool saw = false;
    for (auto name : {"Nightmare_ctor"}) m.callbacks[name] = [&](A::Model &, const std::vector<Value> &) -> Value { saw = true; return std::monostate{}; };
    m.reference_defaults = true;
    m.in_reference_mode([&] { m.horsemen_ctor(RIDER, 0, {0, 0, 0}, 0, 0x9000, 1, 1); });
    ++cases;
    if (!saw) { ++failed; std::cerr << "MISMATCH call-out lost without opt-in\n"; }
  }
  std::cout << "horsemen-mount: " << (cases - failed) << "/" << cases << " cases match\n";
  return failed ? 1 : 0;
}
