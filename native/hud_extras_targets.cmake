# HUD extras (docs/HUD_EXTRAS.md): status XP bar and pending rectangles,
# starting/respawn current values, gold counter, potion notices, selector input,
# minimap projector geometry. Replays research t3/research-hud-gaps vectors.
include_guard(GLOBAL)
add_library(champions-hud-extras src/hud_extras.cpp)
target_include_directories(champions-hud-extras PUBLIC include)
target_link_libraries(champions-hud-extras PUBLIC champions-ui-layout-dynamic champions-core)
set_target_properties(champions-hud-extras PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
  target_compile_options(champions-hud-extras PRIVATE /W4 /utf-8 /fp:strict)
else()
  target_compile_options(champions-hud-extras PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()
if(BUILD_TESTING)
  add_executable(champions-hud-extras-tests tests/hud_extras_tests.cpp)
  target_link_libraries(champions-hud-extras-tests PRIVATE champions-hud-extras)
  add_test(NAME hud-extras COMMAND champions-hud-extras-tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors")
endif()
