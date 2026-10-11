// Authored single-peer controls. Every state field is computed from native
// execution; expected columns are compared only after execution.
#include "../tests/session_creature_trace.hpp"
#include "champions/session_control.hpp"
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
namespace {
using namespace champions::test::session_trace;
namespace Control = R::session_control;
const std::array<std::string, 9> names{"movement", "edges",    "pause",
                                       "focus",    "reset",    "limit",
                                       "stop",     "supplied", "invalid_input"};
bool floating(const std::string &name) {
  return name == "movement" || name == "edges" || name == "supplied";
}
unsigned variants(const std::string &name) {
  return name == "movement"                           ? 4
         : name == "supplied"                         ? 3
         : name == "edges" || name == "invalid_input" ? 2
                                                      : 1;
}
using Action = std::pair<std::string, unsigned>;
std::vector<Action> commands(const std::string &name, unsigned variant) {
  if (name == "supplied")
    return {{"advance", 17}, {"advance", 17}, {"advance", 0}, {"advance", 0},
            {"reset", 0},    {"advance", 16}, {"advance", 0}};
  if (name == "invalid_input")
    return {{"advance", 17}, {"advance", variant ? UINT32_MAX : 32u},
            {"advance", 0},  {"pause", 1},
            {"advance", 32}, {"pause", 0},
            {"advance", 16}, {"advance", 0}};
  if (name == "movement") {
    auto mask = std::array<unsigned, 4>{1, 2, 9, 15}.at(variant);
    return {
        {"advance", mask}, {"advance", mask}, {"advance", 0}, {"advance", 0}};
  }
  if (name == "edges") {
    auto mask = variant ? 16u : 17u;
    return {{"advance", mask},
            {"advance", mask},
            {"advance", 0},
            {"advance", 16},
            {"advance", 0}};
  }
  if (name == "pause")
    return {{"advance", 17}, {"pause", 1},   {"advance", 31}, {"advance", 31},
            {"pause", 0},    {"advance", 0}, {"advance", 16}, {"advance", 0}};
  if (name == "focus")
    return {{"advance", 17},
            {"release", 0},
            {"advance", 0},
            {"advance", 18},
            {"advance", 0}};
  if (name == "reset")
    return {{"advance", 17},
            {"advance", 0},
            {"reset", 0},
            {"advance", 16},
            {"advance", 0}};
  if (name == "limit")
    return {{"advance", 17}, {"advance", 17}, {"advance", 17},
            {"advance", 0},  {"release", 0},  {"pause", 1}};
  if (name == "stop")
    return {{"advance", 17}, {"advance", 0}, {"stop", 0},
            {"advance", 31}, {"release", 0}, {"reset", 0}};
  throw std::runtime_error("unknown scenario");
}
std::string control_snapshot(const Control::Snapshot &s) {
  return sequence(
      {sequence({text(s.config.profile),
                 n(static_cast<unsigned>(s.config.model)), n(s.config.mul_bias),
                 n(s.config.div_nearest), n(s.config.shared_seed),
                 n(s.config.max_ready_ticks), n(unsigned(s.config.movement))}),
       level_snapshot(s.initial_terrain),
       n(s.supplied_terrain),
       champions::test::creature_trace::observation(s.session),
       n(s.actor_serial),
       n(s.actor_address),
       n(s.mover),
       n(s.hp),
       n(s.held_input),
       integers(s.pending_damage_frames),
       n(s.minimum_input_frame),
       n(s.submissions),
       n(s.ready_ticks),
       n(s.damage_events),
       n(s.paused),
       n(s.stopped),
       n(s.failed),
       n(s.busy),
       text(s.phase),
       text(s.error)});
}
std::string observation(const Control::Observation &o) {
  return sequence({integers(o.position), n(o.vz), n(o.radius), n(o.hp),
                   n(o.frame), n(o.submissions), n(o.ready_ticks),
                   n(o.damage_events), n(o.paused), n(o.stopped), n(o.failed),
                   text(o.error), n(unsigned(o.movement)),
                   n(o.ready_tick_limit)});
}
R::world_queries::LevelData supplied_terrain(unsigned variant) {
  namespace W = R::world_queries;
  W::LevelData d;
  d.tg_x0 = d.tg_y0 = -2;
  d.tg_w = d.tg_h = 4;
  d.grid.assign(16, {0});
  W::HeightField h;
  h.x0 = h.y0 = -64;
  h.w = h.h = 129;
  h.samples.assign(129 * 129, 0);
  if (variant < 2) {
    h.lo = h.hi = variant == 0 ? 3 : -3;
    std::fill(h.samples.begin(), h.samples.end(), std::int16_t(h.lo * 16));
  } else
    for (std::size_t y = 0; y < 129; ++y)
      h.samples[y * 129 + 65] = 1;
  d.heightfields.push_back(std::move(h));
  d.elements.push_back(W::make_element(
      0, -256, -256, 256, 256, 0, variant == 2 ? 4 : 0, 0, 0, 0, 32767, 0));
  return d;
}
std::pair<std::string, std::string>
scenario(const std::string &name, std::int64_t cfg, const std::string &args) {
  auto a = split(args, ';');
  if (a.size() != 2 || cfg < 0 || cfg > 15)
    throw std::runtime_error("invalid scenario/configuration");
  auto id = integer(a[0]), v = integer(a[1]);
  std::int64_t counter = 0;
  bool valid = false;
  for (const auto &op : names)
    for (unsigned c = 0; c < (floating(op) ? 16u : 1u); ++c)
      for (unsigned k = 0; k < variants(op); ++k) {
        if (counter == id)
          valid = name == op && cfg == c && v == k;
        ++counter;
      }
  if (!valid)
    throw std::runtime_error("inconsistent scenario identity");
  Control::Config config;
  config.model = static_cast<R::Fpu::Model>(cfg / 4);
  config.mul_bias = cfg & 2;
  config.div_nearest = cfg & 1;
  config.max_ready_ticks = name == "limit" ? 2 : 512;
  config.movement = Control::MovementPolicy::DryCreature;
  std::optional<R::world_queries::LevelData> level;
  if (name == "supplied")
    level = supplied_terrain(unsigned(v));
  Control::Controller c(config, level);
  auto initial = control_snapshot(c.snapshot());
  auto previous = parsed(initial);
  std::vector<std::string> steps;
  for (const auto &[action, value] : commands(name, unsigned(v))) {
    bool refused = false;
    try {
      if (action == "advance")
        c.advance(value);
      else if (action == "pause")
        c.set_paused(value != 0);
      else if (action == "release")
        c.release_input();
      else if (action == "reset")
        c.reset();
      else if (action == "stop")
        c.stop();
      else
        throw std::runtime_error("unknown action");
    } catch (const Control::Error &) {
      if (name != "invalid_input" || action != "advance" || !(value & ~31u))
        throw;
      refused = true;
    }
    auto after = parsed(control_snapshot(c.snapshot()));
    auto change = delta(previous, after);
    auto reconstructed = previous;
    reconstructed = apply_delta(reconstructed, change);
    if (encoded(reconstructed) != encoded(after))
      throw std::runtime_error("lossless control delta reconstruction failed");
    if (refused && encoded(previous) != encoded(after))
      throw std::runtime_error("refused input mutated state");
    const auto observed = observation(c.observation());
    steps.push_back(sequence(
        {sequence({text(action), n(value)}),
         refused ? sequence({text("refused"), observed}) : observed, change}));
    previous = std::move(after);
  }
  return {"ok", sequence({initial, sequence(steps)})};
}
// Pinned independently generated corpus coverage and full computed trace
// digest.
constexpr std::uint64_t expected_digest = 0x9e62b0fba31eff0cULL;
constexpr std::size_t expected_rows = 151;
int run(const std::vector<std::filesystem::path> &args) {
  delta_anchors();
  if (args.size() < 1 || args.size() > 2)
    throw std::runtime_error(
        "usage: session-control-creature-trace VECTORS.tsv [TRACE.tsv]");
  constexpr std::uintmax_t max_fixture = 64ULL * 1024 * 1024;
  constexpr std::size_t max_line = 1024 * 1024;
  if (!std::filesystem::is_regular_file(args[0]) ||
      std::filesystem::file_size(args[0]) > max_fixture)
    throw std::runtime_error(
        "fixture is missing, not regular, or exceeds budget");
  if (args.size() == 2) {
    if (std::filesystem::weakly_canonical(args[0]) ==
            std::filesystem::weakly_canonical(args[1]) ||
        (std::filesystem::exists(args[1]) &&
         std::filesystem::equivalent(args[0], args[1])))
      throw std::runtime_error("trace must not overwrite fixture");
  }
  std::ifstream input(args[0], std::ios::binary);
  if (!input)
    throw std::runtime_error("cannot open fixture");
  std::ofstream output;
  if (args.size() == 2) {
    output.open(args[1], std::ios::binary);
    if (!output)
      throw std::runtime_error("cannot open trace");
  }
  auto line = [&]() {
    std::string value;
    for (char ch; input.get(ch);) {
      if (ch == '\n')
        return value;
      if (value.size() == max_line)
        throw std::runtime_error("fixture line exceeds budget");
      if (static_cast<unsigned char>(ch) > 127 || ch == '\r' || ch == '\0')
        throw std::runtime_error("fixture must be ASCII with LF separators");
      value += ch;
    }
    if (!value.empty())
      throw std::runtime_error("fixture ends without newline");
    return value;
  };
  if (line() != "# session_control_creature_vectors_v1")
    throw std::runtime_error("fixture schema mismatch");
  std::set<std::string> seen;
  std::size_t rows = 0;
  std::uint64_t digest = 14695981039346656037ULL;
  while (input.peek() != std::char_traits<char>::eof()) {
    const auto columns = split(line(), '\t');
    if (columns.size() != 5)
      throw std::runtime_error("fixture requires five columns");
    const auto key = columns[0] + '\t' + columns[1] + '\t' + columns[2];
    if (!seen.insert(key).second)
      throw std::runtime_error("duplicate scenario");
    const auto actual = scenario(columns[0], integer(columns[1]), columns[2]);
    if (actual.first != columns[3] || actual.second != columns[4]) {
      std::cerr << "session-control-creature mismatch at row " << rows + 1
                << " (" << columns[0] << ")\n";
      return 1;
    }
    const auto record = key + '\t' + actual.first + '\t' + actual.second + '\n';
    if (args.size() == 2) {
      output << record;
      if (!output)
        throw std::runtime_error("trace write failed");
    }
    for (unsigned char ch : record) {
      digest ^= ch;
      digest *= 1099511628211ULL;
    }
    if (++rows > 65536)
      throw std::runtime_error("fixture row budget exceeded");
  }
  if (input.bad())
    throw std::runtime_error("fixture read failed");
  if (rows != expected_rows || digest != expected_digest)
    throw std::runtime_error(
        "incomplete or changed session-control-creature corpus");
  if (args.size() == 2) {
    output.flush();
    if (!output)
      throw std::runtime_error("trace write failed");
  }
  std::cout << rows
            << " exact session-control-creature vectors passed; FNV1a64="
            << std::hex << digest << '\n';
  return 0;
}
int entry(const std::vector<std::filesystem::path> &args) {
  try {
    champions::test::creature_trace::value_anchors();
    return run(args);
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
} // namespace
#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
#else
int main(int argc, char **argv) {
#endif
  std::vector<std::filesystem::path> args;
  for (int i = 1; i < argc; ++i)
    args.emplace_back(argv[i]);
  return entry(args);
}
