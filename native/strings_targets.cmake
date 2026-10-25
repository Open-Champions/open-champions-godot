# String table, wstr_format and the HUD text-command state (docs/STRINGS.md).
# Included by script_commands_targets.cmake, so the native tree and the
# GDExtension both have the target.
include_guard(GLOBAL)
set(CHAMPIONS_STRINGS_ROOT "${CMAKE_CURRENT_LIST_DIR}")
add_library(champions-strings "${CHAMPIONS_STRINGS_ROOT}/src/strings.cpp")
target_include_directories(champions-strings PUBLIC "${CHAMPIONS_STRINGS_ROOT}/include")
target_link_libraries(champions-strings PUBLIC champions-core)
set_target_properties(champions-strings PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
  target_compile_options(champions-strings PRIVATE /W4 /utf-8 /fp:strict)
else()
  target_compile_options(champions-strings PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()

if(BUILD_TESTING AND TARGET champions-archive)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  add_executable(champions-strings-tests "${CHAMPIONS_STRINGS_ROOT}/tests/strings_tests.cpp")
  target_link_libraries(champions-strings-tests PRIVATE champions-strings)
  if(MSVC)
    target_compile_options(champions-strings-tests PRIVATE /W4 /utf-8 /fp:strict)
  else()
    target_compile_options(champions-strings-tests PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
  endif()
  add_test(NAME strings-exact COMMAND "${Python3_EXECUTABLE}"
      "${CHAMPIONS_STRINGS_ROOT}/../tools/run_strings_vectors.py"
      --exe "$<TARGET_FILE:champions-strings-tests>"
      --vectors "${CHAMPIONS_STRINGS_ROOT}/tests/vectors/strings.tsv.gz"
      --manifest "${CHAMPIONS_STRINGS_ROOT}/tests/vectors/strings.json")
endif()

# Counts-only disc census of the string tables and the five commands' calls.
if(TARGET champions-archive AND TARGET champions-amx)
  add_executable(champions-strings-census "${CHAMPIONS_STRINGS_ROOT}/src/strings_census.cpp")
  target_link_libraries(champions-strings-census PRIVATE champions-strings champions-archive champions-amx)
  if(MSVC)
    target_compile_options(champions-strings-census PRIVATE /W4 /utf-8)
  else()
    target_compile_options(champions-strings-census PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endif()
