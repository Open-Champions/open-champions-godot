include_guard(GLOBAL)
set(CHAMPIONS_ARENA_MODES_ROOT "${CMAKE_CURRENT_LIST_DIR}")

include("${CMAKE_CURRENT_LIST_DIR}/arena_modes_library.cmake")

if(BUILD_TESTING)
  add_executable(champions-arena-modes-tests "${CHAMPIONS_ARENA_MODES_ROOT}/tests/arena_modes_tests.cpp")
  target_link_libraries(champions-arena-modes-tests PRIVATE champions-arena-modes)
  add_test(NAME arena-modes-exact COMMAND champions-arena-modes-tests)
  add_executable(champions-arena-modes-damage-tests "${CHAMPIONS_ARENA_MODES_ROOT}/tests/arena_modes_damage_tests.cpp")
  target_link_libraries(champions-arena-modes-damage-tests PRIVATE champions-arena-modes champions-combat)
  add_test(NAME arena-modes-damage COMMAND champions-arena-modes-damage-tests)
endif()

# Counts only. Not a CTest: it reads a private disc and prints no names.
# Captures go to a directory the caller names outside the tracked tree.
add_executable(champions-arena-measure "${CHAMPIONS_ARENA_MODES_ROOT}/tools/arena_measure.cpp")
target_link_libraries(champions-arena-measure PRIVATE champions-arena-modes champions-level-flow champions-objects champions-terrain-file)
if(MSVC)
  target_compile_options(champions-arena-measure PRIVATE /W4 /utf-8 /fp:strict)
else()
  target_compile_options(champions-arena-measure PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()
