#include "control_anim_cache_trace.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
namespace T = champions::test::control_anim_cache_trace;
namespace R = champions::simulation::rta;
namespace C = R::session_control;
namespace N = R::native_save;
using Action = std::pair<std::string, unsigned>;
std::vector<Action> actions(const std::string &name) {
  if (name == "full_cap") {
    std::vector<Action> a{{"start", 1}, {"advance", 21}};
    for (unsigned i = 0; i < 129; ++i)
      a.push_back({"advance", 0});
    return a;
  }
  if (name == "key_failure" || name == "history_failure")
    return {{"start", 1},
            {"advance", 0},
            {"advance", 0},
            {"advance", 0},
            {"advance", 0}};
  if (name == "reset")
    return {{"start", 1}, {"advance", 1}, {"advance", 0}, {"reset", 0},
            {"start", 2}, {"advance", 4}, {"advance", 0}};
  if (name == "paused_pending")
    return {{"start", 1},      {"advance", 21}, {"advance", 0},
            {"pause", 1},      {"restart", 2},  {"checkpoint", 0},
            {"checkpoint", 1}, {"advance", 31}, {"pause", 0},
            {"advance", 0},    {"advance", 0},  {"stopclip", 2},
            {"advance", 0},    {"advance", 0}};
  if (name != "lifecycle")
    throw std::runtime_error("unknown control_anim_cache recipe");
  return {{"start", 1},    {"advance", 21}, {"checkpoint", 0}, {"advance", 0},
          {"start", 2},    {"advance", 5},  {"start", 3},      {"advance", 0},
          {"stopclip", 1}, {"advance", 0},  {"checkpoint", 1}, {"restart", 1},
          {"advance", 0},  {"advance", 0},  {"stopclip", 1},   {"advance", 0},
          {"advance", 0}};
}
#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
#else
int main(int argc, char **argv) {
#endif
  try {
    if (argc != 3)
      throw std::runtime_error("input/output required");
    std::ifstream in{std::filesystem::path(argv[1])};
    std::ofstream out{std::filesystem::path(argv[2])};
    std::string line;
    while (std::getline(in, line)) {
      auto at = line.find('\t');
      if (at == std::string::npos)
        throw std::runtime_error("recipe columns");
      auto name = line.substr(0, at);
      unsigned f = std::stoul(line.substr(at + 1));
      if (f > 15)
        throw std::runtime_error("config range");
      auto cfg = T::config(f);
      if (name == "key_failure")
        cfg.animation_cache->key_records = 1;
      if (name == "history_failure")
        cfg.animation_cache->history = 3;
      auto scene = std::make_unique<N::Scene>(cfg);
      const auto start = T::control(scene->snapshot());
      auto previous = T::parsed(start);
      std::vector<std::string> steps;
      auto commands = actions(name);
      for (std::size_t k = 0; k < commands.size(); ++k) {
        const auto &[a, v] = commands[k];
        if (a == "start" || a == "stopclip" || a == "restart")
          scene->queue_animation(a == "start" ? C::AnimationCommandKind::Start
                                 : a == "stopclip"
                                     ? C::AnimationCommandKind::Stop
                                     : C::AnimationCommandKind::Restart,
                                 v);
        else if (a == "checkpoint")
          scene =
              N::Scene::load(v ? scene->save_snapshot() : scene->save(), "rta");
        else
          champions::test::facing_trace::action(*scene, a, v);
        if (name == "full_cap" && k + 1 < commands.size())
          continue;
        auto current = T::parsed(T::control(scene->snapshot()));
        steps.push_back(T::sequence(
            {T::sequence({T::text(a), T::n(v)}), T::delta(previous, current)}));
        previous = current;
      }
      out << line << '\t' << T::sequence({start, T::sequence(steps)}) << '\n';
    }
    if (!in.eof() || !out)
      throw std::runtime_error("trace IO");
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
