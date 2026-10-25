#include "session_facing_direct.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
namespace T = champions::test::facing_trace;
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
      auto c = T::split(line, '\t');
      if (c.size() != 4)
        throw std::runtime_error("recipe columns");
      auto f = T::integer(c[1]), v = T::integer(c[2]), r = T::integer(c[3]);
      if (f < 0 || f >= 16 || v < 0 || v > 6 || r < 0 || r > 1)
        throw std::runtime_error("recipe range");
      if (c[0].rfind("direct_", 0) == 0) {
        out << line << '\t' << champions::test::facing_direct::run(c[0], f)
            << '\n';
        continue;
      }
      auto scene =
          std::make_unique<champions::simulation::rta::native_save::Scene>(
              T::config(f, v, r));
      const auto start = T::control(scene->snapshot());
      auto previous = T::parsed(start);
      std::vector<std::string> steps;
      const auto commands = T::actions(c[0]);
      for (std::size_t k = 0; k < commands.size(); ++k) {
        auto [a, value] = commands[k];
        if (a == "checkpoint")
          scene = champions::simulation::rta::native_save::Scene::load(
              value ? scene->save_snapshot() : scene->save(), "rta");
        else
          T::action(*scene, a, value);
        if (c[0] == "limit" && k + 1 < commands.size())
          continue;
        auto current = T::parsed(T::control(scene->snapshot()));
        steps.push_back(T::sequence({T::sequence({T::text(a), T::n(value)}),
                                     T::delta(previous, current)}));
        previous = std::move(current);
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
