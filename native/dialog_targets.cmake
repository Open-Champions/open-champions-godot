# NPC dialog machine (docs/DIALOG.md). Included from the end of
# script_commands_targets.cmake, so the native tree and the GDExtension both
# define it; the script host links champions-dialog by name.
set(CHAMPIONS_DIALOG_ROOT "${CMAKE_CURRENT_LIST_DIR}")
if(NOT TARGET champions-dialog)
  add_library(champions-dialog "${CHAMPIONS_DIALOG_ROOT}/src/dialog.cpp")
  target_include_directories(champions-dialog PUBLIC "${CHAMPIONS_DIALOG_ROOT}/include")
  target_link_libraries(champions-dialog PUBLIC champions-core)
  set_target_properties(champions-dialog PROPERTIES POSITION_INDEPENDENT_CODE ON)
  if(MSVC)
    target_compile_options(champions-dialog PRIVATE /W4 /utf-8 /fp:strict)
  else()
    target_compile_options(champions-dialog PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
  endif()
endif()

if(BUILD_TESTING AND TARGET champions-script-commands AND NOT TARGET champions-dialog-tests)
  add_executable(champions-dialog-tests "${CHAMPIONS_DIALOG_ROOT}/tests/dialog_tests.cpp")
  target_link_libraries(champions-dialog-tests PRIVATE champions-dialog champions-script-commands)
  if(MSVC)
    target_compile_options(champions-dialog-tests PRIVATE /W4 /utf-8 /fp:strict)
  else()
    target_compile_options(champions-dialog-tests PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
  endif()
  add_test(NAME dialog COMMAND champions-dialog-tests
    "${CHAMPIONS_DIALOG_ROOT}/tests/dialog_vectors.tsv"
    "${CHAMPIONS_DIALOG_ROOT}/tests/dialog_contract.json")
endif()
