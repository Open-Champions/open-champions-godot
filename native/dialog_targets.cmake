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
  target_link_libraries(champions-dialog-tests PRIVATE champions-script-commands)
  if(MSVC)
    target_compile_options(champions-dialog-tests PRIVATE /W4 /utf-8 /fp:strict)
  else()
    target_compile_options(champions-dialog-tests PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
  endif()
  add_test(NAME dialog COMMAND champions-dialog-tests
    "${CHAMPIONS_DIALOG_ROOT}/tests/dialog_vectors.tsv"
    "${CHAMPIONS_DIALOG_ROOT}/tests/dialog_contract.json")
endif()

if(TARGET champions-dialog AND TARGET champions-script-commands AND TARGET champions-archive
   AND TARGET champions-native-audio AND TARGET champions-ui-layout-dynamic AND NOT TARGET champions-dialog-census)
  add_executable(champions-dialog-census "${CHAMPIONS_DIALOG_ROOT}/src/dialog_census.cpp")
  target_link_libraries(champions-dialog-census PRIVATE champions-script-commands champions-archive
    champions-native-audio champions-ui-layout-dynamic)
  if(MSVC)
    target_compile_options(champions-dialog-census PRIVATE /W4 /utf-8 /fp:strict)
  else()
    target_compile_options(champions-dialog-census PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
  endif()
endif()

# The dialog gaps (docs/DIALOG.md "The dialog gaps"): the conversation camera, the
# remaining dialog_isOpen sites, the box without box.tex, the cursor glyph and the
# object factory's property fallback.
if(TARGET champions-dialog AND NOT TARGET champions-dialog-gaps)
  add_library(champions-dialog-gaps "${CHAMPIONS_DIALOG_ROOT}/src/dialog_gaps.cpp")
  target_include_directories(champions-dialog-gaps PUBLIC "${CHAMPIONS_DIALOG_ROOT}/include")
  target_link_libraries(champions-dialog-gaps PUBLIC champions-dialog champions-core)
  set_target_properties(champions-dialog-gaps PROPERTIES POSITION_INDEPENDENT_CODE ON)
  if(MSVC)
    target_compile_options(champions-dialog-gaps PRIVATE /W4 /utf-8 /fp:strict)
  else()
    target_compile_options(champions-dialog-gaps PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
  endif()
endif()

if(BUILD_TESTING AND TARGET champions-dialog-gaps AND NOT TARGET champions-dialog-gaps-tests)
  add_executable(champions-dialog-gaps-tests "${CHAMPIONS_DIALOG_ROOT}/tests/dialog_gaps_tests.cpp")
  target_link_libraries(champions-dialog-gaps-tests PRIVATE champions-dialog-gaps)
  if(MSVC)
    target_compile_options(champions-dialog-gaps-tests PRIVATE /W4 /utf-8 /fp:strict)
  else()
    target_compile_options(champions-dialog-gaps-tests PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
  endif()
  add_test(NAME dialog_gaps COMMAND champions-dialog-gaps-tests
    "${CHAMPIONS_DIALOG_ROOT}/tests/dialog_gaps_vectors.tsv"
    "${CHAMPIONS_DIALOG_ROOT}/tests/dialog_gaps_contract.json")
endif()
