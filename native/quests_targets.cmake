# Quest and mission campaign. Included from the end of
# script_commands_targets.cmake, so the native tree and the GDExtension both
# define it; the host links champions-quests by name.
set(CHAMPIONS_QUESTS_ROOT "${CMAKE_CURRENT_LIST_DIR}")
if(NOT TARGET champions-quests)
  add_library(champions-quests
    "${CHAMPIONS_QUESTS_ROOT}/src/quests.cpp"
    "${CHAMPIONS_QUESTS_ROOT}/src/quests_flow.cpp"
    "${CHAMPIONS_QUESTS_ROOT}/src/quests_bridge.cpp"
    "${CHAMPIONS_QUESTS_ROOT}/src/quest_log.cpp")
  target_include_directories(champions-quests PUBLIC "${CHAMPIONS_QUESTS_ROOT}/include")
  target_link_libraries(champions-quests PUBLIC champions-level-flow champions-script-builtins)
  set_target_properties(champions-quests PROPERTIES POSITION_INDEPENDENT_CODE ON)
  if(MSVC)
    target_compile_options(champions-quests PRIVATE /W4 /utf-8 /fp:strict)
  else()
    target_compile_options(champions-quests PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
  endif()
endif()

if(BUILD_TESTING AND TARGET champions-script-commands AND NOT TARGET champions-quests-tests)
  add_executable(champions-quests-tests "${CHAMPIONS_QUESTS_ROOT}/tests/quests_tests.cpp")
  target_link_libraries(champions-quests-tests PRIVATE champions-script-commands)
  if(MSVC)
    target_compile_options(champions-quests-tests PRIVATE /W4 /utf-8 /fp:strict)
  else()
    target_compile_options(champions-quests-tests PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
  endif()
  add_test(NAME quests COMMAND champions-quests-tests
    "${CHAMPIONS_QUESTS_ROOT}/tests/quests_vectors.tsv"
    "${CHAMPIONS_QUESTS_ROOT}/tests/quests_contract.json")
endif()

# Quest log: list order and writers, the page's text calls and the player
# record copy of the quest span (research-quest-log 965ec52 quest-log.md 6).
if(BUILD_TESTING AND TARGET champions-script-commands AND NOT TARGET champions-quest-log-tests)
  add_executable(champions-quest-log-tests "${CHAMPIONS_QUESTS_ROOT}/tests/quest_log_tests.cpp")
  target_link_libraries(champions-quest-log-tests PRIVATE champions-script-commands)
  if(MSVC)
    target_compile_options(champions-quest-log-tests PRIVATE /W4 /utf-8 /fp:strict)
  else()
    target_compile_options(champions-quest-log-tests PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
  endif()
  add_test(NAME quest-log COMMAND champions-quest-log-tests)
endif()

if(TARGET champions-script-commands AND TARGET champions-archive AND NOT TARGET champions-quests-measure)
  add_executable(champions-quests-measure "${CHAMPIONS_QUESTS_ROOT}/src/quests_measure.cpp")
  target_link_libraries(champions-quests-measure PRIVATE champions-script-commands champions-archive)
  if(MSVC)
    target_compile_options(champions-quests-measure PRIVATE /W4 /utf-8 /fp:strict)
  else()
    target_compile_options(champions-quests-measure PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
  endif()
endif()
