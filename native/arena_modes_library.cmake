# The arena library alone. Included from level_flow_targets.cmake (level_flow
# forwards the arena respawn pick to it) and from arena_modes_targets.cmake,
# so the native tree and the GDExtension both define it once.
set(CHAMPIONS_ARENA_MODES_ROOT "${CMAKE_CURRENT_LIST_DIR}")
if(NOT TARGET champions-arena-modes)
  add_library(champions-arena-modes
    "${CHAMPIONS_ARENA_MODES_ROOT}/src/arena_modes.cpp"
    "${CHAMPIONS_ARENA_MODES_ROOT}/src/arena_executable.cpp")
  target_include_directories(champions-arena-modes PUBLIC "${CHAMPIONS_ARENA_MODES_ROOT}/include")
  target_link_libraries(champions-arena-modes PUBLIC champions-core)
  set_target_properties(champions-arena-modes PROPERTIES POSITION_INDEPENDENT_CODE ON)
  if(MSVC)
    target_compile_options(champions-arena-modes PRIVATE /W4 /utf-8 /fp:strict)
  else()
    target_compile_options(champions-arena-modes PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
  endif()
endif()
