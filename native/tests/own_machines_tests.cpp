#include "champions/own_machines.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace champions::simulation::rta::own_machines;
namespace {
unsigned checks = 0;
void check(bool v, const std::string &message) {
  ++checks;
  if (!v)
    throw std::runtime_error(message);
}
bool same(const Tick &a, const Tick &b) {
  return a.requests == b.requests && a.calls == b.calls && a.words == b.words;
}
View load_case(std::istream &in, std::string &name, unsigned &state) {
  View v;
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#')
      continue;
    std::istringstream row(line);
    std::string op;
    row >> op;
    if (op == "CASE") {
      row >> name >> state;
      return v;
    }
  }
  name.clear();
  return v;
}
Tick load_expect(std::istream &in, View &v) {
  Tick want;
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#')
      continue;
    std::istringstream row(line);
    std::string op;
    row >> op;
    if (op == "END")
      return want;
    if (op == "F") {
      std::string key;
      row >> key;
      if (key == "clock")
        row >> v.clock;
      else if (key == "timer")
        row >> v.timer;
      else if (key == "flight")
        row >> v.flight;
      else if (key == "queue")
        row >> v.queue;
      else if (key == "anim")
        row >> v.anim;
      else if (key == "events")
        row >> v.events;
      else if (key == "rand")
        row >> v.rand;
      else if (key == "fid_time")
        row >> v.fid_time;
      else if (key == "z")
        row >> v.z;
      else if (key == "stuck")
        row >> v.stuck;
      else if (key == "cflags")
        row >> v.cflags;
      else if (key == "kind")
        row >> v.kind;
      else if (key == "class_flags")
        row >> v.class_flags;
      else if (key == "frame")
        row >> v.frame;
      else if (key == "serial")
        row >> v.serial;
      else if (key == "seen")
        row >> v.seen;
      else if (key == "away")
        row >> v.away;
      else if (key == "see")
        row >> v.see;
      else if (key == "enemy")
        row >> v.enemy;
      else if (key == "ally")
        row >> v.ally;
      else if (key == "rooted")
        row >> v.rooted;
      else if (key == "ready")
        row >> v.ready;
      else if (key == "stunned")
        row >> v.stunned;
      else if (key == "arrived")
        row >> v.arrived;
      else if (key == "demon")
        row >> v.demon;
      else if (key == "path")
        row >> v.path;
      else if (key == "target")
        row >> v.target;
      else if (key == "owner")
        row >> v.owner;
      else if (key == "idle")
        row >> v.idle;
      else if (key == "move")
        row >> v.move;
      else if (key == "fidget_a")
        row >> v.fidget_a;
      else if (key == "fidget_b")
        row >> v.fidget_b;
      else if (key == "flee_clip")
        row >> v.flee_clip;
      else if (key == "snap")
        row >> v.snap;
      else
        throw std::runtime_error("unknown own-machine field " + key);
    } else if (op == "SEQ") {
      std::string key;
      row >> key;
      int n;
      while (row >> n)
        v.seq[key].push_back(n);
    } else if (op == "REQ") {
      std::uint32_t n;
      row >> n;
      want.requests.push_back(n);
    } else if (op == "CALL") {
      std::string name;
      row >> name;
      want.calls.push_back(name);
    } else if (op == "WORD") {
      std::uint32_t off, val;
      row >> off >> val;
      want.words[off] = val;
    } else
      throw std::runtime_error("unknown own-machine row " + op);
  }
  throw std::runtime_error("own-machine case did not end");
}
} // namespace
int main(int argc, char **argv) try {
  if (argc != 2)
    throw std::runtime_error("own-machine vector path required");
  std::ifstream in(argv[1]);
  if (!in)
    throw std::runtime_error("own-machine vectors missing");
  unsigned cases = 0;
  for (;;) {
    std::string name;
    unsigned state = 0;
    auto view = load_case(in, name, state);
    if (name.empty())
      break;
    auto want = load_expect(in, view);
    auto got = step(name, state, view);
    if (!same(got, want)) {
      std::ostringstream why;
      why << name << " state " << state << " diverged";
      throw std::runtime_error(why.str());
    }
    ++cases;
    ++checks;
  }
  check(cases == 253, "own-machine vector count");
  View ctx;
  ctx.seq["v25"] = {0, 1};
  ctx.queue = 1;
  ctx.clock = 100;
  ctx.timer = 5000;
  auto saved = ctx.seq["v25"];
  step("AntQueen", 0, ctx);
  check(ctx.seq["v25"] == saved, "context sequence consumed");
  auto second = step("AntQueen", 0, ctx);
  check(second.requests == std::vector<std::uint32_t>{3} &&
            second.calls ==
                std::vector<std::string>{"aq_ground", "v25", "v25", "v27"},
        "antqueen second sight");
  View fidget;
  fidget.fidget_a = true;
  fidget.move = true;
  fidget.clock = 1000;
  fidget.fid_time = 0;
  fidget.timer = 0;
  auto blocked = step("Critter", 0, fidget);
  check(blocked.requests.empty() &&
            blocked.calls ==
                std::vector<std::string>{"move_by", "anim_bio", "rand"},
        "critter fidget blocks wander");
  View mermaid;
  mermaid.clock = 100;
  mermaid.timer = 5000;
  mermaid.arrived = true;
  auto ignored = step("Mermaid", 1, mermaid);
  check(ignored.requests.empty() &&
            ignored.calls == std::vector<std::string>{"move_by", "move_dest"},
        "mermaid ignores arrival");
  View soul;
  soul.frame = 0;
  soul.demon = true;
  soul.clock = 100;
  soul.timer = 5000;
  check(step("Soul", 0, soul).requests == std::vector<std::uint32_t>{0x18},
        "soul alarm phase");
  auto held = step("Soul", 0x18, soul);
  check(held.requests.empty() &&
            held.calls == std::vector<std::string>{"move_by", "demon"},
        "soul holds the demon state");
  soul.frame = 1;
  soul.demon = false;
  check(step("Soul", 0x18, soul).requests.empty(), "soul off-phase demon");
  View shot;
  shot.anim = 0;
  shot.timer = 1;
  auto empty = step("Skeleton", 8, shot);
  check(empty.requests == std::vector<std::uint32_t>{0, 3} &&
            empty.calls == std::vector<std::string>{"v27", "v27"} &&
            empty.words.at(0xf4) == 0,
        "skeleton empty shot then countdown");
  View rise;
  rise.anim = 0x80;
  check(step("Skelight", 0x17, rise).calls ==
            std::vector<std::string>{"v27", "fx"},
        "skelight rise order");
  rise.anim = 0;
  check(step("Skeleton", 0x17, rise).calls ==
            std::vector<std::string>{"fx", "v27"},
        "skeleton rise order");
  View noisy;
  noisy.seen = true;
  noisy.target = true;
  check(step("Natasla", 0, noisy).calls.empty() &&
            step("Natasla", 0, noisy).requests.empty(),
        "natasla decides nothing");
  check(!root_clears_speed(0x3f000000), "half does not clear speed");
  check(root_clears_speed(0x3f000001), "one ulp above half clears speed");
  check(root_clears_speed(0xbf000001), "one ulp below minus half clears speed");
  check(!root_clears_speed(0), "zero root does not clear speed");
  check(speed_word_is_integer(0) && speed_word_is_integer(0x3f800000) &&
            speed_word_is_integer(0x42480000),
        "integer speeds");
  check(!speed_word_is_integer(0x3f000000) &&
            !speed_word_is_integer(0x3f000001),
        "non-integer speeds");
  std::cout << checks << " own-machine checks passed\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << e.what() << "\n";
  return 1;
}
