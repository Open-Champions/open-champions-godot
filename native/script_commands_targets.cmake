# Opt-in level-script host. Links the existing binder, level flow and FX sim.
include_guard(GLOBAL)
set(CHAMPIONS_SCRIPT_COMMANDS_ROOT "${CMAKE_CURRENT_LIST_DIR}")
# world_objectFactory as a contract (research t3/research-script-session 6841b8a).
add_library(champions-level-factory "${CHAMPIONS_SCRIPT_COMMANDS_ROOT}/src/level_factory.cpp")
target_include_directories(champions-level-factory PUBLIC "${CHAMPIONS_SCRIPT_COMMANDS_ROOT}/include")
set_target_properties(champions-level-factory PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_options(champions-level-factory PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-level-factory PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()
include("${CHAMPIONS_SCRIPT_COMMANDS_ROOT}/script_items_targets.cmake")
include("${CHAMPIONS_SCRIPT_COMMANDS_ROOT}/strings_targets.cmake")
include("${CHAMPIONS_SCRIPT_COMMANDS_ROOT}/ui_text_gaps_targets.cmake")
add_library(champions-script-commands "${CHAMPIONS_SCRIPT_COMMANDS_ROOT}/src/script_commands.cpp"
    "${CHAMPIONS_SCRIPT_COMMANDS_ROOT}/src/script_commands_objects.cpp"
    "${CHAMPIONS_SCRIPT_COMMANDS_ROOT}/src/script_effects.cpp"
    "${CHAMPIONS_SCRIPT_COMMANDS_ROOT}/src/script_dialog.cpp")
target_include_directories(champions-script-commands PUBLIC "${CHAMPIONS_SCRIPT_COMMANDS_ROOT}/include")
target_link_libraries(champions-script-commands PUBLIC
    champions-script-builtins champions-level-flow champions-fx-sim champions-quests
    champions-level-factory champions-monsters-1 champions-arena-monsters champions-script-items champions-dialog
    champions-dialog-gaps champions-strings champions-ui-text-gaps
    champions-combat champions-terrain-file)
if(TARGET champions-archive)
    target_link_libraries(champions-script-commands PUBLIC champions-archive)
endif()
# The native build provides champions-native-audio. The Godot extension compiles
# the same file as champions-native-audio-binding-core and links it after this
# static library, so the Playback references resolve there.
if(TARGET champions-native-audio)
    target_link_libraries(champions-script-commands PUBLIC champions-native-audio)
endif()


set_target_properties(champions-script-commands PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_options(champions-script-commands PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-script-commands PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()

if(TARGET champions-archive)
add_executable(champions-script-commands-measure "${CHAMPIONS_SCRIPT_COMMANDS_ROOT}/src/script_commands_measure.cpp")
target_link_libraries(champions-script-commands-measure PRIVATE champions-script-commands champions-archive)
if(MSVC)
    target_compile_options(champions-script-commands-measure PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-script-commands-measure PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()
endif()

if(BUILD_TESTING AND TARGET champions-archive)
    add_executable(champions-script-commands-tests "${CHAMPIONS_SCRIPT_COMMANDS_ROOT}/tests/script_commands_tests.cpp")
    target_link_libraries(champions-script-commands-tests PRIVATE champions-script-commands)
    if(MSVC)
        target_compile_options(champions-script-commands-tests PRIVATE /W4 /utf-8 /fp:strict)
    else()
        target_compile_options(champions-script-commands-tests PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
    endif()
    add_executable(champions-level-factory-tests "${CHAMPIONS_SCRIPT_COMMANDS_ROOT}/tests/level_factory_tests.cpp")
    target_link_libraries(champions-level-factory-tests PRIVATE champions-level-factory)
    if(MSVC)
        target_compile_options(champions-level-factory-tests PRIVATE /W4 /utf-8 /fp:strict)
    else()
        target_compile_options(champions-level-factory-tests PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
    endif()
    add_test(NAME level-factory COMMAND champions-level-factory-tests
        "${CHAMPIONS_SCRIPT_COMMANDS_ROOT}/tests/vectors/level_factory.tsv")
    add_executable(champions-level-objects-tests "${CHAMPIONS_SCRIPT_COMMANDS_ROOT}/tests/level_objects_tests.cpp")
    target_link_libraries(champions-level-objects-tests PRIVATE champions-script-commands champions-combat-runtime)
    if(MSVC)
        target_compile_options(champions-level-objects-tests PRIVATE /W4 /utf-8 /fp:strict)
    else()
        target_compile_options(champions-level-objects-tests PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
    endif()
    add_test(NAME level-objects COMMAND champions-level-objects-tests
        "${CHAMPIONS_SCRIPT_COMMANDS_ROOT}/tests/vectors/level_objects.tsv")
    add_test(NAME script-commands COMMAND champions-script-commands-tests
        "${CHAMPIONS_SCRIPT_COMMANDS_ROOT}/tests/vectors/script_commands.tsv"
        "${CHAMPIONS_SCRIPT_COMMANDS_ROOT}/tests/vectors/script_host.txt")
    add_executable(champions-script-effects-tests "${CHAMPIONS_SCRIPT_COMMANDS_ROOT}/tests/script_effects_tests.cpp")
    target_link_libraries(champions-script-effects-tests PRIVATE champions-script-commands)
    if(MSVC)
        target_compile_options(champions-script-effects-tests PRIVATE /W4 /utf-8 /fp:strict)
    else()
        target_compile_options(champions-script-effects-tests PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
    endif()
    add_test(NAME script-effects COMMAND champions-script-effects-tests
        "${CHAMPIONS_SCRIPT_COMMANDS_ROOT}/tests/vectors/script_effects.tsv")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/script-commands-empty")
    add_test(NAME script-commands-measure-usage COMMAND champions-script-commands-measure --game norrath .)
    add_test(NAME script-commands-measure-empty COMMAND champions-script-commands-measure --game rta
        "${CMAKE_CURRENT_BINARY_DIR}/script-commands-empty")
    set_tests_properties(script-commands-measure-usage PROPERTIES WILL_FAIL TRUE)
    set_tests_properties(script-commands-measure-empty PROPERTIES PASS_REGULAR_EXPRESSION "scripts_started 0")
endif()

if(BUILD_TESTING AND NOT TARGET champions-script-items-tests)
    add_executable(champions-script-items-tests "${CHAMPIONS_SCRIPT_COMMANDS_ROOT}/tests/script_items_tests.cpp")
    target_link_libraries(champions-script-items-tests PRIVATE champions-script-commands champions-script-items)
    if(MSVC)
        target_compile_options(champions-script-items-tests PRIVATE /W4 /utf-8 /fp:strict)
    else()
        target_compile_options(champions-script-items-tests PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
    endif()
    add_test(NAME script-items COMMAND champions-script-items-tests
        "${CHAMPIONS_SCRIPT_COMMANDS_ROOT}/tests/vectors/script_items.txt"
        "${CHAMPIONS_SCRIPT_COMMANDS_ROOT}/tests/script_items_contract.json"
        "${CHAMPIONS_SCRIPT_COMMANDS_ROOT}/tests/vectors/give_random.txt")
endif()

if(BUILD_TESTING AND NOT TARGET champions-item-drops-tests)
    add_executable(champions-item-drops-tests "${CHAMPIONS_SCRIPT_COMMANDS_ROOT}/tests/item_drops_tests.cpp")
    target_link_libraries(champions-item-drops-tests PRIVATE champions-script-commands champions-script-items)
    if(MSVC)
        target_compile_options(champions-item-drops-tests PRIVATE /W4 /utf-8 /fp:strict)
    else()
        target_compile_options(champions-item-drops-tests PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
    endif()
    add_test(NAME item-drops COMMAND champions-item-drops-tests
        "${CHAMPIONS_SCRIPT_COMMANDS_ROOT}/tests/vectors/item_drops.txt")
endif()

if(BUILD_TESTING AND NOT TARGET champions-net-launch-tests)
    add_executable(champions-net-launch-tests "${CHAMPIONS_SCRIPT_COMMANDS_ROOT}/tests/net_launch_tests.cpp")
    target_link_libraries(champions-net-launch-tests PRIVATE champions-script-items)
    if(MSVC)
        target_compile_options(champions-net-launch-tests PRIVATE /W4 /utf-8)
    else()
        target_compile_options(champions-net-launch-tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME net-launch COMMAND champions-net-launch-tests
        "${CHAMPIONS_SCRIPT_COMMANDS_ROOT}/tests/vectors/net_launch.txt")
endif()

# The host links champions-quests; define it here so every build that includes
# this file (native and the GDExtension) has the target.
include("${CHAMPIONS_SCRIPT_COMMANDS_ROOT}/quests_targets.cmake")

# NPC dialog machine; the host links champions-dialog by name.
include("${CHAMPIONS_SCRIPT_COMMANDS_ROOT}/dialog_targets.cmake")
