// Authored Session-owned isolated terrain replay. Expected values are produced
// independently by the pinned Python contracts; this executable computes every
// output field.
#include "champions/session.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "../tests/session_snapshot_trace.hpp"

namespace {
using namespace champions::test::session_trace;
namespace W = R::world_queries;
R::Raw I(int v) { return R::from_int(std::uint32_t(v)); }
W::LevelData fixture(unsigned kind) {
  W::LevelData d;
  d.pg_x0 = -3;
  d.pg_y0 = 7;
  d.pg_w = 2;
  d.pg_h = 1;
  d.path_cells = {0x12345678, 0xfedcba98};
  if (!kind)
    return d;
  d.tg_x0 = d.tg_y0 = -1;
  d.tg_w = d.tg_h = 2;
  std::array<int, 25> heights{}, flags{};
  int lo = 0, hi = 0;
  if (kind == 2) {
    for (unsigned i = 0; i < 25; ++i)
      flags[i] = i % 5 == 3 ? 1 : i % 5 == 4 ? 2 : 0;
  } else if (kind == 3) {
    for (unsigned i = 0; i < 25; ++i) {
      heights[i] = std::array<int, 5>{-2000, 0, 11, 12, 30}[i % 5];
      flags[i] = i % 8;
    }
    lo = -2000;
    hi = 30;
  } else if (kind == 6) {
    for (unsigned i = 0; i < 25; ++i) {
      heights[i] = i % 6 == 0 ? 2000 : -4;
      flags[i] = i % 6 == 0 ? 1 : 0;
    }
    lo = -4;
    hi = 2000;
  } else {
    for (unsigned i = 0; i < 25; ++i)
      heights[i] = 4 + i % 5;
    lo = 4;
    hi = 8;
  }
  W::HeightField field;
  field.x0 = field.y0 = -2;
  field.w = field.h = 5;
  field.lo = lo;
  field.hi = hi;
  field.offset = 0x400;
  for (unsigned i = 0; i < 25; ++i)
    field.samples.push_back(
        static_cast<std::int16_t>(heights[i] * 16 + flags[i]));
  W::HeightField unused;
  unused.x0 = 11;
  unused.y0 = -12;
  unused.w = unused.h = 1;
  unused.lo = unused.hi = -2000;
  unused.samples = {-32000};
  unused.offset = 0x400;
  d.heightfields = {field, unused};
  auto cosine = kind == 4   ? 0
                : kind == 5 ? 23170
                            : 32767,
       sine = kind == 4   ? 32767
              : kind == 5 ? -23170
                          : 0;
  d.elements = {W::make_element(77, -16, -16, 16, 16, 0,
                                4 | (kind == 4 ? 1 : 0), 0, 0, 0, cosine, sine),
                W::make_element(77, -16, -16, 16, 16, 0, 4, 4, 0, 3, 32767, 0),
                W::make_element(-9, -16, -16, 16, 16, 1, 6, 0, 0, 0, 32767, 0)};
  d.grid = {{0, 1, 0, 2}, {-3, 1}, {1, 0}, {-1, 0, 1}};
  d.pieces = {0, 1, -2};
  if (kind == 5) {
    d.elements[0].f_max_x = I(1);
    d.elements[0].f_min_y = I(-1);
  }
  if (kind == 7) {
    d.stamp = UINT32_MAX;
    d.elements[0].stamp = 0;
    d.elements[1].stamp = UINT32_MAX;
    d.elements[2].stamp = 7;
  }
  return d;
}
const std::vector<std::string> names{
    "ready",        "threshold",     "slide",     "grid",
    "blocked",      "delayed_hold",  "frozen",    "hp",
    "second_actor", "destroy_reuse", "full_pool", "missing_grid",
    "post_failure", "end_failure",   "opposite"};
bool floating(const std::string &name) {
  return name == "ready" || name == "threshold" || name == "slide" ||
         name == "grid" || name == "delayed_hold" || name == "frozen" ||
         name == "hp" || name == "opposite";
}
unsigned variants(const std::string &name) {
  if (name == "blocked")
    return 3;
  return name == "ready" || name == "threshold" || name == "slide" ||
                 name == "grid" || name == "delayed_hold" || name == "hp" ||
                 name == "second_actor" || name == "destroy_reuse"
             ? 2
             : 1;
}
N::ByteVector ready_packet(const S::Config &c) {
  N::Machine remote(2, 1, N::Fpu(c.model, c.mul_bias, c.div_nearest));
  for (unsigned f = 1; f <= 12; ++f)
    remote.peers[1].set_record(f,
                               N::rec12(f * 17,
                                        {128 + f, 64 + f, 255 - f, f * 9},
                                        {f * 11, f * 13}, f * 123, f & 1),
                               0, 0);
  return remote.build_packet(0, 1, 12, N::PKT_RAW_RELAY, 1, nullptr, true).data;
}
struct TaskFailure : std::runtime_error {
  TaskFailure() : std::runtime_error("authored task failure") {}
};
struct Replay {
  S::Config config;
  std::string name;
  unsigned variant;
  S::Session session;
  O::GameObject *actor = nullptr;
  S::MoverId mover = 0;
  S::HpId hp = 0;
  std::vector<std::string> events, observations;
  unsigned moves = 0;
  static S::Config settings(unsigned c, unsigned v, unsigned local) {
    S::Config r;
    r.model = static_cast<R::Fpu::Model>(c / 4);
    r.mul_bias = c & 2;
    r.div_nearest = c & 1;
    r.peers = 2;
    r.local = local;
    r.network_mode = 1;
    r.seed = (v ? 0xdeadbeef : 1) ^ 0xa5a5a5a5;
    return r;
  }
  unsigned ready_count() const {
    const auto s = session.snapshot();
    return unsigned(
        std::count_if(s.events.begin(), s.events.end(), [](const auto &e) {
          return e.kind == "task" && e.name == "runObjects";
        }));
  }
  Replay(unsigned cfg, unsigned v, std::string scenario, unsigned local = 0)
      : config(settings(cfg, v, local)), name(std::move(scenario)), variant(v),
        session(config) {
    S::ObjectBindings bindings;
    bindings.on_run = {"authored_on_run", [this](O::GameObject &o) {
                         effect("run", {n(o.serial)});
                         o.tag = o.tag.value_or(0) + 1;
                       }};
    bindings.on_destroy = {
        "authored_on_destroy", [this](O::GameObject &o, std::int64_t flags) {
          session.with_world([&](auto &w) { w.destroy(o, flags); });
        }};
    bindings.is_frozen = {"authored_is_frozen",
                          [this] { return name == "frozen"; }};
    bindings.res_find = {"authored_res_find",
                         [](const auto &, const auto &) { return 0u; }};
    bindings.end_of_tick = {"authored_end_of_tick", [this] {
                              effect("end");
                              if (name == "end_failure")
                                throw TaskFailure();
                            }};
    session.enable_objects(v ? 0xdeadbeef : 1, bindings);
    for (unsigned i = 0; i < 3; ++i)
      session.register_object_task(
          0xf2000001 + i,
          std::array<std::string, 3>{"terrain_pre", "terrain_move",
                                     "terrain_post"}[i],
          20 + 10 * i, 0, [this, i](auto &) { task(i); });
    session.with_world([&](O::World &w) {
      actor =
          w.new_object(0x400, {I(name == "grid" && variant == 0 ? 255 : 1),
                               I(std::min(variant, 1u)),
                               I(name == "ready" && variant == 1 ? 0 : 12)});
      actor->flags = 32;
      actor->tag = 0;
      actor->vz = 0;
      w.run_every_frame(*actor);
      if (name == "grid" && variant == 1)
        w.add_to_always_draw(*actor);
    });
    if (name == "hp") {
      session.enable_hp();
      hp = session.bind_hp(actor->serial, actor->addr, {0x42c80000, 0, {}});
      session.seed_hp_backing(hp, {{0xc8, 4, 0},
                                   {0x231, 1, 0},
                                   {0x2dc, 4, 0},
                                   {0x2e0, 2, 0},
                                   {0x304, 4, 0},
                                   {0x3b0, 2, 0}});
    }
    auto level = fixture(name == "slide" ? (variant == 0 ? 3 : 4) : 2);
    if (name != "slide") {
      std::fill(level.heightfields[0].samples.begin(),
                level.heightfields[0].samples.end(), 0);
      level.heightfields[0].lo = level.heightfields[0].hi = 0;
      level.elements[1].z = 0;
    }
    session.enable_terrain(level);
    mover = session.bind_isolated_mover(actor->serial, actor->addr);
  }
  void effect(const std::string &label, std::vector<std::string> fields = {}) {
    auto s = session.snapshot();
    std::vector<std::string> row{text(label)};
    row.insert(row.end(), fields.begin(), fields.end());
    row.insert(row.end(), {n(s.world->frame), n(s.network.net_frame),
                           n(s.world->shared_seed)});
    events.push_back(sequence(row));
  }
  W::Vec3 velocity() const {
    if (name == "threshold")
      return {variant == 0 ? 0u : 0x3f000001, 0, 0};
    if (name == "slide")
      return {I(variant == 0 ? 120 : -120), I(30), 0};
    if (name == "grid")
      return {I(120), 0, 0};
    return {variant == 0 ? I(30) : 0x3f000001, I(-7), 0};
  }
  void task(unsigned i) {
    effect(std::array<std::string, 3>{"terrain_pre", "terrain_move",
                                      "terrain_post"}[i]);
    auto ready = ready_count();
    if (i == 0) {
      if (name == "second_actor" && ready == 1)
        session.with_world([&](auto &w) {
          auto *b = w.new_object(0x400, {I(2), I(3), 0});
          if (variant)
            w.move_off_grid(*b);
          effect("second", {n(b->serial)});
        });
      if (name == "destroy_reuse" && ready == 2)
        session.with_world([&](auto &w) {
          w.destroy(*actor, 3);
          effect("destroy", {n(actor->serial)});
          if (variant) {
            auto *b = w.new_object(0x400, {I(9), 0, 0});
            effect("replacement", {n(b->serial), n(b->addr)});
          }
        });
      if (name == "full_pool" && ready == 1)
        session.with_world([](auto &w) {
          auto count = w.handles.count_free();
          for (std::size_t j = 0; j < count; ++j)
            w.handles.add(w.always_draw, nullptr);
        });
      if (name == "missing_grid" && ready == 1)
        session.with_world([&](auto &w) { w.handles.clear(actor->heads[2]); });
      if (hp && variant == 0)
        session.take_damage(hp, 0x40000000, {}, 0x40);
    } else if (i == 1) {
      const auto result = session.move_isolated(mover, velocity());
      ++moves;
      observations.push_back(
          sequence({text("move"), sequence({integers(result.position),
                                            n(result.vz), n(result.bits)})}));
    } else {
      if (hp && variant == 1)
        session.take_damage(hp, 0x40000000, {}, 0x40);
      if (name == "post_failure") {
        session.with_world([](auto &w) { w.shared.draw(); });
        throw TaskFailure();
      }
    }
  }
  std::string summary(const S::Snapshot &s) {
    std::vector<std::string> health;
    if (s.hp)
      for (const auto &o : s.hp->adapter.objects)
        health.push_back(n(o.value.hp));
    return sequence({n(ready_count()), n(moves), n(s.network.net_frame),
                     n(s.world->frame), integers(actor->pos), n(actor->vz),
                     n(s.failed), n(s.world->shared_seed), sequence(health)});
  }
  std::pair<std::string, std::vector<N::Sent>>
  step(unsigned k, const std::vector<N::ByteVector> &inbox, Value &previous) {
    N::ByteVector sample{1,
                         0,
                         std::uint8_t(128 + k),
                         std::uint8_t(64 + 3 * k),
                         std::uint8_t(255 - k),
                         std::uint8_t(17 * k),
                         std::uint8_t(31 * k),
                         std::uint8_t(47 * k)};
    std::string result;
    std::vector<N::Sent> sent;
    bool hold = name == "delayed_hold" && variant == 1;
    try {
      auto r = session.submit(sample, k * 1234, k & 1, hold && k == 0 ? 4 : 1,
                              (name == "blocked" && variant == 2) ||
                                  (hold && k == 0),
                              hold && k > 0, k, inbox);
      sent = r.sent;
      result = sequence({sends(sent), n(r.held), n(r.steps)});
    } catch (const TaskFailure &) {
      result = sequence({text("error"), text("TaskFailure")});
    } catch (const R::HandleError &) {
      result = sequence({text("error"), text("HandleError")});
    } catch (const O::ObjectError &) {
      result = sequence({text("error"), text("ObjectError")});
    } catch (const S::Error &) {
      result = sequence({text("error"), text("Refusal")});
    }
    auto snap = session.snapshot();
    auto current = parsed(snapshot(snap));
    auto patch = delta(previous, current);
    if (encoded(apply_delta(previous, patch)) != encoded(current))
      throw std::runtime_error("computed delta is not lossless");
    previous = std::move(current);
    return {sequence({result, summary(snap), patch}), sent};
  }
  std::string run() {
    if (name == "blocked" && variant == 1)
      session.set_level_pending(true);
    auto previous = parsed(snapshot(session.snapshot()));
    auto initial = encoded(previous);
    std::vector<std::string> steps;
    for (unsigned k = 0; k < 4; ++k) {
      if (name == "blocked" && variant == 1 && k == 2)
        session.set_level_pending(false);
      bool delayed = name == "delayed_hold" && variant == 0,
           no_ready = name == "blocked" && variant == 0;
      std::vector<N::ByteVector> inbox;
      if ((delayed && k == 2) || (!delayed && !no_ready && k == 0))
        inbox.push_back(ready_packet(config));
      steps.push_back(step(k, inbox, previous).first);
    }
    return sequence(
        {initial, sequence(steps), sequence(events), sequence(observations)});
  }
};
std::string opposite(unsigned cfg) {
  std::array<std::unique_ptr<Replay>, 2> peers{
      std::make_unique<Replay>(cfg, 0, "opposite", 0),
      std::make_unique<Replay>(cfg, 0, "opposite", 1)};
  std::vector<std::string> initial, steps;
  std::array<Value, 2> previous;
  std::array<std::vector<N::ByteVector>, 2> queues;
  std::array<std::map<std::int64_t, std::string>, 2> completed;
  for (unsigned i = 0; i < 2; ++i) {
    previous[i] = parsed(snapshot(peers[i]->session.snapshot()));
    initial.push_back(encoded(previous[i]));
  }
  const std::array<unsigned, 12> order{0, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1};
  for (unsigned turn = 0; turn < order.size(); ++turn) {
    auto which = order[turn];
    auto &o = *peers[which];
    std::vector<N::ByteVector> inbox;
    if (turn >= 3) {
      inbox = queues[which];
      queues[which].clear();
    }
    auto old = o.ready_count();
    auto step = o.step(turn, inbox, previous[which]);
    for (const auto &p : step.second)
      queues.at(std::size_t(p.peer)).push_back(p.data);
    if (o.ready_count() > old) {
      auto s = o.session.snapshot();
      completed[which][s.network.net_frame] =
          sequence({world_snapshot(*s.world),
                    encoded(parsed(snapshot(s)).children.at(1))});
    }
    steps.push_back(sequence({n(which), step.first}));
  }
  std::vector<std::string> equal, q;
  for (const auto &[frame, value] : completed[0])
    if (completed[1].count(frame)) {
      const bool match = value == completed[1].at(frame);
      if (!match)
        throw std::runtime_error("opposite completed gameplay state mismatch");
      equal.push_back(sequence({n(frame), n(match)}));
    }
  if (equal.empty())
    throw std::runtime_error("opposite replay lacks common completed tick");
  for (const auto &row : queues) {
    std::vector<std::string> payloads;
    for (const auto &x : row)
      payloads.push_back(bytes(x));
    q.push_back(sequence(payloads));
  }
  return sequence(
      {sequence(initial), sequence(steps),
       sequence({sequence(peers[0]->events), sequence(peers[1]->events)}),
       sequence({sequence(q), sequence(equal)})});
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
  R::ScopedFpu context(
      R::Fpu(static_cast<R::Fpu::Model>(cfg / 4), cfg & 2, cfg & 1));
  if (name == "opposite")
    return {"ok", opposite(unsigned(cfg))};
  Replay r(unsigned(cfg), unsigned(v), name);
  return {"ok", r.run()};
}

// Pinned independently generated corpus coverage and full computed trace
// digest.
constexpr std::uint64_t expected_digest = 0xcf466aaff33afd8aULL;
constexpr std::size_t expected_rows = 235;
int run(const std::vector<std::filesystem::path> &args) {
  delta_anchors();
  if (args.size() < 1 || args.size() > 2)
    throw std::runtime_error(
        "usage: session-terrain-trace VECTORS.tsv [TRACE.tsv]");
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
  if (line() != "# session_terrain_vectors_v1")
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
      std::cerr << "session-terrain mismatch at row " << rows + 1 << " ("
                << columns[0] << ")\n";
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
    throw std::runtime_error("incomplete or changed session-terrain corpus");
  if (args.size() == 2) {
    output.flush();
    if (!output)
      throw std::runtime_error("trace write failed");
  }
  std::cout << rows
            << " exact session-terrain vectors passed; FNV1a64=" << std::hex
            << digest << '\n';
  return 0;
}
void check_terrain_observation_guard() {
  auto state = S::Session().snapshot();
  const auto before = snapshot(state);
  state.terrain.emplace();
  const auto active = snapshot(state);
  if (active == before)
    throw std::runtime_error("terrain extension omitted");
  ++state.terrain->level.data.stamp;
  if (snapshot(state) == active)
    throw std::runtime_error("terrain stamp omitted");
  state.terrain.reset();
  if (snapshot(state) != before)
    throw std::runtime_error("absent terrain observation changed");
}
int entry(const std::vector<std::filesystem::path> &args) {
  try {
    check_terrain_observation_guard();
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
