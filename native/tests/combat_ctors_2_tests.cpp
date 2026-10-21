// Replays native/tests/vectors/combat_ctors_2.txt.gz
// (tools/generate_combat_ctors_2_vectors.py, research ref_antqueen_run and
// ref_combat_ctors2 at 5b92837): AntQueen_run in every state with
// AntQueen_pickFlyDest, Skeleton_isEnemy, the three take-damage slots and the six
// inline destructors on an authored image. Every call-out is answered with the
// reference's recorded answer and compared by name and arguments; each operation's
// stored bytes, shared draw count and result are compared exactly.
#include "champions/combat_ctors.hpp"
#include <cstdlib>
#include <deque>
#include <fstream>
#include <iostream>
#include <sstream>

namespace A = champions::simulation::rta::arena_monsters;
namespace CT = champions::simulation::rta::combat_ctors;
using champions::simulation::rta::Raw;

namespace {
int failures = 0;
std::size_t compared = 0;
void check(bool ok, const std::string &what) {
  ++compared;
  if (!ok) {
    if (++failures < 30)
      std::cerr << "FAIL " << what << "\n";
  }
}
// The generator's canonical text: integers, n, (...) tuples, [...] lists.
struct Node {
  enum Kind { Int, None, Tuple, List } kind = None;
  std::int64_t value = 0;
  std::vector<Node> items;
};
Node parse(const std::string &s, std::size_t &i) {
  Node n;
  if (s[i] == 'n') {
    ++i;
    return n;
  }
  if (s[i] == '(' || s[i] == '[') {
    n.kind = s[i] == '(' ? Node::Tuple : Node::List;
    char close = s[i] == '(' ? ')' : ']';
    ++i;
    while (s[i] != close) {
      n.items.push_back(parse(s, i));
      if (s[i] == ',')
        ++i;
    }
    ++i;
    return n;
  }
  std::size_t j = i;
  if (s[j] == '-')
    ++j;
  while (j < s.size() && std::isdigit(static_cast<unsigned char>(s[j])))
    ++j;
  n.kind = Node::Int;
  auto digits = s.substr(i, j - i);
  n.value = digits[0] == '-' ? std::stoll(digits) : std::int64_t(std::stoull(digits));
  i = j;
  return n;
}
Node parse(const std::string &s) {
  std::size_t i = 0;
  return parse(s, i);
}
std::string text(const A::Value &v);
std::string text_result(const A::CallbackResult &r) {
  using K = A::CallbackResult::Kind;
  switch (r.kind) {
  case K::none:
    return "n";
  case K::integer:
    return std::to_string(r.negative ? -std::int64_t(r.magnitude) : std::int64_t(r.magnitude));
  case K::text: {
    static const char *hex = "0123456789abcdef";
    std::string out = "s";
    for (unsigned char ch : r.data)
      out += std::string{hex[ch >> 4], hex[ch & 15]};
    return out;
  }
  case K::tuple:
  case K::list: {
    std::string out = r.kind == K::tuple ? "(" : "[";
    for (std::size_t k = 0; k < r.elements.size(); ++k)
      out += (k ? "," : "") + text_result(r.elements[k]);
    return out + (r.kind == K::tuple ? ")" : "]");
  }
  default:
    return "?";
  }
}
std::string text(const A::Value &v) {
  if (auto p = std::get_if<std::int64_t>(&v))
    return std::to_string(*p);
  if (std::holds_alternative<std::monostate>(v))
    return "n";
  if (auto p = std::get_if<A::Vec3>(&v))
    return "(" + std::to_string((*p)[0]) + "," + std::to_string((*p)[1]) + "," +
           std::to_string((*p)[2]) + ")";
  if (auto p = std::get_if<A::Objects>(&v)) {
    std::string out = "[";
    for (std::size_t k = 0; k < p->values.size(); ++k)
      out += (k ? "," : "") + std::to_string(p->values[k]);
    return out + "]";
  }
  if (auto p = std::get_if<A::CallbackResult>(&v))
    return text_result(*p);
  return text_result(A::callback_value(v));
}
A::Vec3 vec(const Node &n) {
  if (n.kind != Node::Tuple || n.items.size() != 3)
    throw std::runtime_error("vector answer expected");
  return {Raw(n.items[0].value), Raw(n.items[1].value), Raw(n.items[2].value)};
}
// The recorded answer as the value the native body consumes.
A::Value answer(const std::string &name, const Node &n) {
  if (n.kind == Node::None)
    return std::monostate{};
  if (n.kind == Node::Int)
    return std::int64_t(n.value);
  if (name == "balance_lookup") {
    A::Balance b;
    b.found = n.items.at(0).value;
    for (unsigned k = 0; k < 5; ++k)
      b.factors[k] = Raw(n.items.at(k + 1).value);
    return b;
  }
  if (name == "model_getSamplePoint") {
    A::Sample s;
    s.point = vec(n.items.at(0));
    s.second = vec(n.items.at(1));
    return s;
  }
  if (name == "world_slideMove") {
    A::Slide sl;
    sl.hit = Raw(n.items.at(0).value);
    sl.normal = vec(n.items.at(1));
    return sl;
  }
  if (n.kind == Node::List) {
    A::Objects o;
    for (auto &e : n.items)
      o.values.push_back(Raw(e.value));
    return o;
  }
  if (n.kind == Node::Tuple && n.items.size() == 3)
    return vec(n);
  throw std::runtime_error("unhandled answer for " + name);
}
struct Expected {
  std::string name, args;
  Node answer;
};
std::vector<std::pair<std::uint32_t, std::uint32_t>> parse_pairs(std::istringstream &in) {
  std::vector<std::pair<std::uint32_t, std::uint32_t>> out;
  std::string item;
  while (in >> item) {
    auto colon = item.find(':');
    out.push_back({std::uint32_t(std::stoul(item.substr(0, colon), nullptr, 16)),
                   std::uint32_t(std::stoul(item.substr(colon + 1), nullptr, 16))});
  }
  return out;
}
struct Case {
  std::string name, cls;
  std::vector<std::pair<std::uint32_t, std::uint32_t>> init;
  std::vector<std::string> lines;
};
void run(const Case &k) {
  A::Memory mem;
  for (auto &p : k.init)
    mem.poke(p.first, 1, p.second);
  A::Model model(mem);
  CT::install(model);
  CT::install_round2(model);
  std::deque<Expected> queue;
  std::string where;
  auto handler = [&](const std::string &name) {
    return [&, name](A::Model &m, const std::vector<A::Value> &args) -> A::Value {
      std::string got = "(";
      for (std::size_t i = 0; i < args.size(); ++i)
        got += (i ? "," : "") + text(args[i]);
      got += ")";
      if (queue.empty()) {
        check(false, where + ": unexpected call " + name + " " + got);
        throw std::runtime_error("unexpected call");
      }
      auto e = queue.front();
      queue.pop_front();
      check(e.name == name && e.args == got,
            where + ": call " + name + " " + got + " expected " + e.name + " " + e.args);
      if (name == "GameObject_ctor")
        m.mem.poke(Raw(std::get<std::int64_t>(args[0])) + 0x24, 4,
                   Raw(std::get<std::int64_t>(args[2])));
      return answer(name, e.answer);
    };
  };
  for (const auto &line : k.lines) {
    if (line.rfind("CALL ", 0) == 0) {
      std::istringstream in(line.substr(5));
      std::string name;
      in >> name;
      if (!model.callbacks.count(name))
        model.callbacks[name] = handler(name);
    }
  }
  // Slot 26 (turning) as the combat World runs it: a call-out answered by the
  // base body 0x10bb08, which the reference runs inside its model (not recorded).
  model.external_slot26 = true;
  model.callbacks["Creature.v26"] = [](A::Model &m, const std::vector<A::Value> &args) {
    std::vector<A::Value> rest(args.begin() + 1, args.end());
    return m.vcall(CT::Address(std::get<std::int64_t>(args.at(0))), 26, std::move(rest));
  };
  // A navigation request of a state (Creature_moveToTarget without a target,
  // Creature_moveToDest on arrival) reaches slot 27 as a call-out here, as in
  // the combat World; the class set-state answers it.
  model.callbacks["Creature.v27"] = [](A::Model &m, const std::vector<A::Value> &args) {
    std::vector<A::Value> rest(args.begin() + 1, args.end());
    return m.vcall(CT::Address(std::get<std::int64_t>(args.at(0))), 27, std::move(rest));
  };
  const CT::Address c = 0x100000;
  std::size_t i = 0;
  while (i < k.lines.size()) {
    const auto &line = k.lines[i++];
    if (line.rfind("POKE ", 0) == 0) {
      std::istringstream in(line.substr(5));
      std::string a, n, v;
      in >> a >> n >> v;
      mem.poke(std::uint32_t(std::stoul(a, nullptr, 16)), std::stoul(n, nullptr, 16),
               std::stoull(v, nullptr, 16));
      continue;
    }
    if (line.rfind("OP ", 0) != 0)
      throw std::runtime_error("operation expected: " + line);
    std::istringstream head(line.substr(3));
    std::string op, rest;
    head >> op;
    std::getline(head, rest);
    rest.erase(0, rest.find_first_not_of(' '));
    auto args = parse(rest);
    where = k.name + " " + op + " " + rest;
    queue.clear();
    std::vector<std::pair<std::uint32_t, std::uint32_t>> diff;
    std::uint64_t draws = 0;
    std::string result, refused;
    bool has_result = false, has_refused = false;
    while (i < k.lines.size() && k.lines[i].rfind("OP ", 0) != 0 &&
           k.lines[i].rfind("POKE ", 0) != 0) {
      const auto &l = k.lines[i++];
      if (l.rfind("CALL ", 0) == 0) {
        std::istringstream in(l.substr(5));
        Expected e;
        std::string answer_text;
        in >> e.name >> e.args >> answer_text;
        e.answer = parse(answer_text);
        queue.push_back(e);
      } else if (l.rfind("DIFF", 0) == 0) {
        std::istringstream in(l.substr(4));
        diff = parse_pairs(in);
      } else if (l.rfind("DRAWS ", 0) == 0) {
        draws = std::stoull(l.substr(6));
      } else if (l.rfind("RESULT ", 0) == 0) {
        has_result = true;
        result = l.substr(7);
      } else if (l.rfind("REFUSED", 0) == 0) {
        has_refused = true;
      }
    }
    auto before = mem.bytes();
    auto draws_before = model.draws;
    bool threw = false;
    std::string got_text;
    try {
      auto a = [&](unsigned n) { return Raw(args.items.at(n).value); };
      auto o = [&](unsigned n) { return A::Value(std::int64_t(args.items.at(n).value)); };
      if (op == "ctor") {
        if (k.cls == "antqueen")
          CT::antqueen_ctor(model, c, a(0), vec(args.items.at(1)), a(2), a(3));
        else if (k.cls == "skeleton")
          CT::skeleton_ctor(model, c, vec(args.items.at(0)), a(1), a(2), a(3), a(4));
        else
          CT::soul_ctor(model, c, a(0), vec(args.items.at(1)), a(2), a(3), a(4));
      } else if (op == "set") {
        CT::antqueen_set_state(model, c, a(0));
      } else if (op == "run") {
        CT::antqueen_run(model, c);
      } else if (op == "fly") {
        auto p = CT::antqueen_pick_fly_dest(model, c);
        got_text = "(" + std::to_string(p[0]) + "," + std::to_string(p[1]) + "," +
                   std::to_string(p[2]) + ")";
      } else if (op == "enemy") {
        got_text = text(model.vcall(c, 20, {o(0)}));
      } else if (op == "damage") {
        auto v = model.vcall(c, 2, {o(0), o(1), o(2)});
        if (!std::holds_alternative<std::monostate>(v))
          got_text = text(v);
      } else if (op == "dtor") {
        auto v = model.vcall(c, 1, {o(0)});
        if (!std::holds_alternative<std::monostate>(v))
          got_text = text(v);
      } else {
        throw std::runtime_error("unknown operation " + op);
      }
    } catch (const A::Unsupported &e) {
      threw = true;
      if (!has_refused)
        check(false, where + ": refused " + e.what());
    } catch (const std::exception &e) {
      check(false, where + ": threw " + e.what());
      return;
    }
    check(threw == has_refused, where + ": refusal mismatch");
    check(queue.empty(), where + ": " + std::to_string(queue.size()) + " calls not made");
    std::vector<std::pair<std::uint32_t, std::uint32_t>> got;
    for (const auto &p : mem.bytes()) {
      auto b = before.find(p.first);
      if (b == before.end() || b->second != p.second)
        got.push_back({p.first, p.second});
    }
    if (got != diff) {
      std::ostringstream o;
      o << where << ": stored bytes differ (" << got.size() << " vs " << diff.size() << ")";
      for (std::size_t n = 0; n < std::max(got.size(), diff.size()) && n < 400; ++n) {
        bool same = n < got.size() && n < diff.size() && got[n] == diff[n];
        if (same)
          continue;
        o << "\n   native " << std::hex
          << (n < got.size() ? std::to_string(got[n].first) + ":" + std::to_string(got[n].second) : "-")
          << " reference "
          << (n < diff.size() ? std::to_string(diff[n].first) + ":" + std::to_string(diff[n].second) : "-")
          << std::dec;
        break;
      }
      check(false, o.str());
    } else {
      ++compared;
    }
    check(model.draws - draws_before == draws, where + ": shared draws");
    if (!threw)
      check(got_text == (has_result ? result : std::string()),
            where + ": result '" + got_text + "' expected '" + result + "'");
    model.calls.clear();
    model.writes.clear();
    model.events.clear();
    if (has_refused)
      break;
  }
}
} // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "usage: combat_ctors_2_tests vectors.txt\n";
    return 2;
  }
  std::ifstream in(argv[1]);
  std::string line;
  std::vector<Case> cases;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#')
      continue;
    if (line.rfind("CASE ", 0) == 0) {
      cases.push_back({});
      cases.back().name = line.substr(5);
    } else if (line.rfind("CLASS ", 0) == 0) {
      cases.back().cls = line.substr(6);
    } else if (line.rfind("MEM ", 0) == 0) {
      std::istringstream s(line.substr(4));
      cases.back().init = parse_pairs(s);
    } else {
      cases.back().lines.push_back(line);
    }
  }
  for (const auto &k : cases)
    run(k);
  std::cout << cases.size() << " cases, " << compared << " comparisons, " << failures
            << " failures\n";
  return failures || cases.empty() ? 1 : 0;
}
