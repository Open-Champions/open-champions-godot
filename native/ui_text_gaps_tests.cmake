# Replay test of the UI text gaps port (docs/UI_TEXT_GAPS.md).
set(CHAMPIONS_UI_TEXT_GAPS_ROOT "${CMAKE_CURRENT_LIST_DIR}")
if(BUILD_TESTING AND TARGET champions-strings AND TARGET champions-ui-layout)
  add_executable(champions-ui-text-gaps-tests "${CHAMPIONS_UI_TEXT_GAPS_ROOT}/tests/ui_text_gaps_tests.cpp")
  target_link_libraries(champions-ui-text-gaps-tests PRIVATE champions-ui-text-gaps champions-strings champions-ui-layout)
  if(MSVC)
    target_compile_options(champions-ui-text-gaps-tests PRIVATE /W4 /utf-8 /fp:strict)
  else()
    target_compile_options(champions-ui-text-gaps-tests PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
  endif()
  add_test(NAME ui-text-gaps-exact COMMAND champions-ui-text-gaps-tests
    "${CHAMPIONS_UI_TEXT_GAPS_ROOT}/tests/vectors/ui_text_gaps.tsv")
endif()
