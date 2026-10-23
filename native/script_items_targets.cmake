# Item, gold and monster-builder callees of the level-script commands
# (research-items 49bc578, docs/SCRIPT_ITEMS.md). Included from
# script_commands_targets.cmake, so the native tree and the GDExtension both
# define it.
include_guard(GLOBAL)
set(CHAMPIONS_SCRIPT_ITEMS_ROOT "${CMAKE_CURRENT_LIST_DIR}")
add_library(champions-script-items
    "${CHAMPIONS_SCRIPT_ITEMS_ROOT}/src/script_items.cpp"
    "${CHAMPIONS_SCRIPT_ITEMS_ROOT}/src/monster_build.cpp"
    "${CHAMPIONS_SCRIPT_ITEMS_ROOT}/src/net_launch.cpp")
target_include_directories(champions-script-items PUBLIC "${CHAMPIONS_SCRIPT_ITEMS_ROOT}/include")
target_link_libraries(champions-script-items PUBLIC champions-script-builtins champions-messages champions-items champions-core)
set_target_properties(champions-script-items PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_options(champions-script-items PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-script-items PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()

# Disc counts only (call sites per command); reads the owner's disc copy.
if(TARGET champions-archive AND NOT TARGET champions-script-items-measure)
    add_executable(champions-script-items-measure "${CHAMPIONS_SCRIPT_ITEMS_ROOT}/src/script_items_measure.cpp")
    target_link_libraries(champions-script-items-measure PRIVATE champions-amx champions-archive)
    if(MSVC)
        target_compile_options(champions-script-items-measure PRIVATE /W4 /utf-8)
    else()
        target_compile_options(champions-script-items-measure PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()
