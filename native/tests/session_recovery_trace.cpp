#include "session_recovery_trace.hpp"
#include "champions/native_save.hpp"
#include "session_recovery_direct.hpp"
#include "session_recovery_recipes.hpp"
#include <fstream>
#include <iostream>
namespace T = champions::test::recovery_trace;
namespace Q = champions::test::recovery_recipes;
namespace N = champions::simulation::rta::native_save;
#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
#else
int main(int argc, char **argv) {
#endif
  try {
    if (argc != 3)
      throw std::runtime_error("input and output paths required");
    if (std::filesystem::weakly_canonical(argv[1]) ==
        std::filesystem::weakly_canonical(argv[2]))
      throw std::runtime_error("input/output alias");
    std::ifstream input{std::filesystem::path(argv[1])};
    std::ofstream out{std::filesystem::path(argv[2])};
    std::string line;
    while (std::getline(input, line)) {
      auto cols = T::split(line, '\t');
      if (cols.size() != 3)
        throw std::runtime_error("recipe columns");
      auto f = T::integer(cols[1]), v = T::integer(cols[2]);
      if (f < 0 || f >= 16 || v < 0 || v > 13)
        throw std::runtime_error("recipe range");
      if (cols[0].rfind("direct_", 0) == 0) {
        if (v)
          throw std::runtime_error("direct variant must be zero");
        out << line << '\t'
            << champions::test::recovery_direct::run(cols[0], unsigned(f))
            << '\n';
        continue;
      }
      N::Scene scene(Q::config(unsigned(f), unsigned(v)));
      auto initial = scene.snapshot();
      auto previous = T::parsed(T::control(initial));
      std::vector<std::string> steps;
      for (const auto &[a, value] : Q::actions(cols[0])) {
        if (a == "advance")
          scene.advance(value);
        else if (a == "pause")
          scene.set_paused(value);
        else if (a == "release")
          scene.release_input();
        else if (a == "reset")
          scene.reset();
        else
          scene.stop();
        const auto current = T::parsed(T::control(scene.snapshot()));
        // Same lossless state delta transport as existing Session traces.
        steps.push_back(T::sequence({T::sequence({T::text(a), T::n(value)}),
                                     T::delta(previous, current)}));
        previous = current;
      }
      out << line << '\t'
          << T::sequence({T::control(initial), T::sequence(steps)}) << '\n';
    }
    if (!input.eof() || !out)
      throw std::runtime_error("trace IO failure");
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
