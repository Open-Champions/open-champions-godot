# Opt-in level-script host. Links the existing binder, level flow and FX sim.
include_guard(GLOBAL)
set(CHAMPIONS_SCRIPT_COMMANDS_ROOT "${CMAKE_CURRENT_LIST_DIR}")
add_library(champions-script-commands
    "${CHAMPIONS_SCRIPT_COMMANDS_ROOT}/src/script_commands.cpp"
    "${CHAMPIONS_SCRIPT_COMMANDS_ROOT}/src/script_effects.cpp"
    "${CHAMPIONS_SCRIPT_COMMANDS_ROOT}/src/script_dialog.cpp")
target_include_directories(champions-script-commands PUBLIC "${CHAMPIONS_SCRIPT_COMMANDS_ROOT}/include")
target_link_libraries(champions-script-commands PUBLIC
    champions-script-builtins champions-level-flow champions-fx-sim champions-quests champions-dialog)
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

# The host links champions-quests; define it here so every build that includes
# this file (native and the GDExtension) has the target.
include("${CHAMPIONS_SCRIPT_COMMANDS_ROOT}/quests_targets.cmake")

# NPC dialog machine; the host links champions-dialog by name.
include("${CHAMPIONS_SCRIPT_COMMANDS_ROOT}/dialog_targets.cmake")
