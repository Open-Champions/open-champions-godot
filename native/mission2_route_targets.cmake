# Mission 2 hub-return list and the round-6 jump-gate portal
# (mission2_portal). The live session does not link this: the play caller
# was not re-established. Included after champions-quests and the level-flow
# session adapter exist.
if(NOT TARGET champions-quests)
  return()
endif()
if(NOT TARGET champions-mission2-route)
  add_library(champions-mission2-route "${CMAKE_CURRENT_LIST_DIR}/src/mission2_route.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/mission2_portal.cpp")
  target_include_directories(champions-mission2-route PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include")
  target_link_libraries(champions-mission2-route PUBLIC champions-quests champions-session-level-flow)
  set_target_properties(champions-mission2-route PROPERTIES POSITION_INDEPENDENT_CODE ON)
  if(MSVC)
    target_compile_options(champions-mission2-route PRIVATE /W4 /utf-8 /fp:strict)
  else()
    target_compile_options(champions-mission2-route PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
  endif()
endif()
if(BUILD_TESTING AND NOT TARGET champions-mission2-route-tests)
  add_executable(champions-mission2-route-tests "${CMAKE_CURRENT_LIST_DIR}/tests/mission2_route_tests.cpp")
  # ps2-save is linked for the +0x134 cross-check against pack_jump only.
  target_link_libraries(champions-mission2-route-tests PRIVATE champions-mission2-route champions-ps2-save)
  if(MSVC)
    target_compile_options(champions-mission2-route-tests PRIVATE /W4 /utf-8 /fp:strict)
  else()
    target_compile_options(champions-mission2-route-tests PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
  endif()
  add_test(NAME mission2-route COMMAND champions-mission2-route-tests)
endif()
