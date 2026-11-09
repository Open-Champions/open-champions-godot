# Level-up and automap producers (docs/LEVELUP_HUD.md). Included by
# menus_targets.cmake, so the native tree and the GDExtension both have it.
include_guard(GLOBAL)
add_library(champions-levelup-hud "${CMAKE_CURRENT_LIST_DIR}/src/levelup_hud.cpp")
target_include_directories(champions-levelup-hud PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include")
target_link_libraries(champions-levelup-hud PUBLIC champions-core)
set_target_properties(champions-levelup-hud PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_options(champions-levelup-hud PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-levelup-hud PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()
if(BUILD_TESTING AND CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_CURRENT_LIST_DIR)
    add_executable(champions-levelup-hud-tests "${CMAKE_CURRENT_LIST_DIR}/tests/levelup_hud_tests.cpp")
    target_link_libraries(champions-levelup-hud-tests PRIVATE champions-levelup-hud
        champions-ui-layout-dynamic champions-strings)
    add_test(NAME levelup-hud COMMAND champions-levelup-hud-tests)
endif()
