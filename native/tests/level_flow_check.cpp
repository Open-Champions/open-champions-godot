#include "champions/level_flow.hpp"
#include <string>

// Test-only cold failure body. Assertion conditions, counts, expression text
// and source line are retained; the generated reference corpus is untouched.
[[noreturn]] void level_flow_check_failed(const char *expression, int line) {
  throw champions::simulation::rta::level_flow::Error(
      std::string("line ") + std::to_string(line) + ": " + expression);
}
