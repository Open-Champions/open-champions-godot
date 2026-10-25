cmake_minimum_required(VERSION 3.21)
project(champions_native_data VERSION 0.1.0 LANGUAGES CXX)

# C++17 retains std::string UTF-8 filesystem conversion across all targets.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CHAMPIONS_SOURCE_COMMIT "unrecorded" CACHE STRING "Source revision recorded in binary packages")
set(CHAMPIONS_TEST_PLATFORM "${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}" CACHE STRING "Platform identity for exact traces")

include("${CMAKE_CURRENT_SOURCE_DIR}/simulation_targets.cmake")
# RTA cosmetic state stays independent of Godot and authoritative simulation.
add_library(champions-native-fx src/fx/fx.cpp src/fx/fx_json.cpp src/lighting/lighting.cpp src/lighting/level_lights.cpp)
target_include_directories(champions-native-fx PUBLIC include)
if(MSVC)
    target_compile_options(champions-native-fx PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-native-fx PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()
add_executable(champions-amx-validate tools/amx_validate.cpp)
add_executable(champions-animation-decoder-validate tools/animation_decoder_validate.cpp)
target_link_libraries(champions-animation-decoder-validate PRIVATE champions-animation-decoder)
if(MSVC)
    target_compile_options(champions-animation-decoder-validate PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-animation-decoder-validate PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()
if(MINGW)
    target_link_options(champions-animation-decoder-validate PRIVATE -municode)
endif()
target_link_libraries(champions-amx-validate PRIVATE champions-amx)
if(MSVC)
    target_compile_options(champions-amx-validate PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-amx-validate PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()
if(MINGW)
    target_link_options(champions-amx-validate PRIVATE -municode)
endif()

add_library(champions-archive src/archive.cpp src/resource_manager.cpp)
target_include_directories(champions-archive PUBLIC include)
add_library(champions-game-profiles src/game_profile.cpp)
target_include_directories(champions-game-profiles PUBLIC include)
if(MSVC)
    target_compile_options(champions-game-profiles PRIVATE /W4 /utf-8)
else()
    target_compile_options(champions-game-profiles PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_library(champions-texture src/texture.cpp src/texture_upscale.cpp)
add_executable(champions-texture-fidelity src/texture_fidelity_cli.cpp)
target_link_libraries(champions-texture-fidelity PRIVATE champions-texture champions-archive)
if(MSVC)
    target_compile_options(champions-texture-fidelity PRIVATE /W4 /utf-8)
else()
    target_compile_options(champions-texture-fidelity PRIVATE -Wall -Wextra -Wpedantic)
endif()
if(MINGW)
    target_link_options(champions-texture-fidelity PRIVATE -municode)
endif()
target_include_directories(champions-texture PUBLIC include)
add_library(champions-animation src/animation.cpp)
target_include_directories(champions-animation PUBLIC include)
add_library(champions-model src/model.cpp)
target_include_directories(champions-model PUBLIC include)
add_library(champions-world-geometry src/world_geometry.cpp)
target_include_directories(champions-world-geometry PUBLIC include)
add_library(champions-level-texture src/level_texture/level_texture.cpp src/level_texture/material.cpp src/level_texture/level_draw.cpp)
target_include_directories(champions-level-texture PUBLIC include)
target_link_libraries(champions-level-texture PUBLIC champions-world-geometry)
add_library(champions-water-draw src/water_draw.cpp)
target_include_directories(champions-water-draw PUBLIC include)
target_link_libraries(champions-water-draw PUBLIC champions-core)
if(MSVC)
    target_compile_options(champions-water-draw PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-water-draw PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()
add_executable(champions-level-draw-measure tools/level_draw_measure.cpp)
target_link_libraries(champions-level-draw-measure PRIVATE champions-level-texture champions-archive champions-texture)
# Published WORLD geometry uses binary64 presentation arithmetic. This target
# stays outside the deterministic raw32 game-rule graph and disables contraction.
if(MSVC)
    target_compile_options(champions-world-geometry PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-world-geometry PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()
add_executable(champions-model-forms-validate tools/model_forms_validate.cpp)
target_link_libraries(champions-model-forms-validate PRIVATE champions-model)
if(MSVC)
    target_compile_options(champions-model-forms-validate PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-model-forms-validate PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()
if(MINGW)
    target_link_options(champions-model-forms-validate PRIVATE -municode)
endif()
add_library(champions-skinning src/skinning.cpp)
target_include_directories(champions-skinning PUBLIC include)
add_executable(champions-data src/main.cpp)
target_link_libraries(champions-data PRIVATE champions-archive)
add_executable(champions-data-validate src/validate_data.cpp)
target_link_libraries(champions-data-validate PRIVATE champions-archive champions-animation champions-texture champions-model champions-terrain-file champions-world-geometry)
if(MINGW)
    target_link_options(champions-data PRIVATE -municode)
    target_link_options(champions-data-validate PRIVATE -municode)
endif()

if(MSVC)
    target_compile_options(champions-archive PRIVATE /W4 /utf-8)
    target_compile_options(champions-texture PRIVATE /W4 /utf-8)
    target_compile_options(champions-animation PRIVATE /W4 /utf-8 /fp:strict)
    target_compile_options(champions-skinning PRIVATE /W4 /utf-8 /fp:strict)
    target_compile_options(champions-model PRIVATE /W4 /utf-8)
    target_compile_options(champions-data PRIVATE /W4 /utf-8)
    target_compile_options(champions-data-validate PRIVATE /W4 /utf-8)
else()
    target_compile_options(champions-archive PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(champions-texture PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(champions-animation PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
    target_compile_options(champions-skinning PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
    target_compile_options(champions-model PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(champions-data PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(champions-data-validate PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(champions-campaign-actors-tests tests/campaign_actors_tests.cpp)
    target_link_libraries(champions-campaign-actors-tests PRIVATE champions-campaign-actors champions-items)
    add_test(NAME campaign-actors COMMAND champions-campaign-actors-tests)
    add_executable(champions-resource-manager-tests tests/resource_manager_tests.cpp)
    target_link_libraries(champions-resource-manager-tests PRIVATE champions-archive)
    add_test(NAME resource-manager COMMAND champions-resource-manager-tests)
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/player_state_targets.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/fx2_targets.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/combat_play_effects_targets.cmake")
if(BUILD_TESTING)
    add_executable(champions-native-fx-tests tests/native_fx_tests.cpp)
    target_link_libraries(champions-native-fx-tests PRIVATE champions-native-fx)
    add_test(NAME native-fx-synthetic COMMAND champions-native-fx-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors")
    add_executable(champions-level-lights-tests tests/level_lights_tests.cpp)
    target_link_libraries(champions-level-lights-tests PRIVATE champions-native-fx champions-fx-skills)
    add_test(NAME level-lights COMMAND champions-level-lights-tests)
    add_executable(champions-level-texture-tests tests/level_texture_tests.cpp)
    target_link_libraries(champions-level-texture-tests PRIVATE champions-level-texture)
    target_compile_definitions(champions-level-texture-tests PRIVATE LEVEL_TEXTURE_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/level_texture_synthetic.bin")
    target_compile_definitions(champions-level-texture-tests PRIVATE LEVEL_TEXTURE_SPECIAL="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/level_texture_special.bin")
    add_test(NAME level-texture COMMAND champions-level-texture-tests)
    add_executable(champions-level-draw-tests tests/level_draw_tests.cpp)
    target_link_libraries(champions-level-draw-tests PRIVATE champions-level-texture)
    add_test(NAME level-draw COMMAND champions-level-draw-tests)
    # TEX-6: the automap tile texture at WORLD +0x6c is undocumented (world.md);
    # the measurement tool reports it as refused and never decodes it.
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/level-draw-measure-empty")
    add_test(NAME level-draw-measure-atlas-refused COMMAND champions-level-draw-measure "${CMAKE_CURRENT_BINARY_DIR}/level-draw-measure-empty" "${CMAKE_CURRENT_BINARY_DIR}/level-draw-measure-out")
    set_tests_properties(level-draw-measure-atlas-refused PROPERTIES PASS_REGULAR_EXPRESSION " atlas=undocumented " FAIL_REGULAR_EXPRESSION " atlas=[0-9]")
    add_executable(champions-water-draw-tests tests/water_draw_tests.cpp)
    target_link_libraries(champions-water-draw-tests PRIVATE champions-water-draw champions-level-texture)
    if(MSVC)
        target_compile_options(champions-water-draw-tests PRIVATE /W4 /utf-8 /fp:strict)
    else()
        target_compile_options(champions-water-draw-tests PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
    endif()
    add_test(NAME water-draw COMMAND champions-water-draw-tests)
    add_executable(champions-game-profiles-tests tests/game_profile_tests.cpp)
    target_link_libraries(champions-game-profiles-tests PRIVATE champions-game-profiles champions-archive)
    add_test(NAME game-profiles COMMAND champions-game-profiles-tests)
    add_executable(champions-native-save-tests tests/native_save_tests.cpp)
    target_link_libraries(champions-native-save-tests PRIVATE champions-native-save)
    if(MSVC)
        target_compile_options(champions-native-save-tests PRIVATE /W4 /utf-8 /fp:strict)
    else()
        target_compile_options(champions-native-save-tests PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
    endif()
    add_test(NAME native-save COMMAND champions-native-save-tests)
    add_executable(champions-native-snapshot-tests tests/native_snapshot_tests.cpp)
    target_link_libraries(champions-native-snapshot-tests PRIVATE champions-native-save)
    add_test(NAME native-snapshot COMMAND champions-native-snapshot-tests)
    add_test(NAME native-save-access COMMAND champions-native-snapshot-tests --access-cost)
    add_test(NAME native-save-partition COMMAND champions-native-snapshot-tests --partition)
    add_test(NAME native-save-indexed COMMAND champions-native-snapshot-tests --indexed)

    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    include("${CMAKE_CURRENT_SOURCE_DIR}/messages_tests.cmake")
    add_executable(champions-script-builtins-tests tests/script_builtins_tests.cpp)
    target_link_libraries(champions-script-builtins-tests PRIVATE champions-script-builtins)
    add_test(NAME script-builtins-api COMMAND champions-script-builtins-tests --api)
    add_test(NAME script-builtins-exact COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_script_builtins_vectors.py"
        --native "$<TARGET_FILE:champions-script-builtins-tests>"
        --vectors "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/script_builtins.tsv.gz"
        --manifest "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/script_builtins.json"
        --trace-dir "${CMAKE_CURRENT_BINARY_DIR}/core-traces" --source-commit "${CHAMPIONS_SOURCE_COMMIT}"
        --platform "${CHAMPIONS_TEST_PLATFORM}")
    find_package(Threads REQUIRED)
    add_executable(champions-arena-monsters-tests tests/arena_monsters_tests.cpp)
    target_link_libraries(champions-arena-monsters-tests PRIVATE champions-arena-monsters)
    if(MSVC)
        target_compile_options(champions-arena-monsters-tests PRIVATE /W4 /utf-8)
    else()
        target_compile_options(champions-arena-monsters-tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME arena-monsters-exact COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_arena_monsters_vectors.py"
        "$<TARGET_FILE:champions-arena-monsters-tests>"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/arena_monsters.tsv")
    add_test(NAME simulation-boundary COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/check_simulation_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}/..")
    add_test(NAME core-trace-integrity COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/../tools" -p "test_*core*.py")
    foreach(suite arithmetic math state)
        add_executable(champions-core-${suite}-tests tests/core_${suite}_tests.cpp)
        target_link_libraries(champions-core-${suite}-tests PRIVATE champions-core Threads::Threads)
        if(MSVC)
            target_compile_options(champions-core-${suite}-tests PRIVATE /W4 /utf-8 /fp:strict)
        else()
            target_compile_options(champions-core-${suite}-tests PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
        endif()
        add_test(NAME core-${suite}-exact COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/../tools/run_core_vectors.py"
            --suite ${suite} --exe "$<TARGET_FILE:champions-core-${suite}-tests>"
            --vectors "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/core_${suite}.tsv.gz"
            --manifest "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/core_${suite}.json"
            --trace-dir "${CMAKE_CURRENT_BINARY_DIR}/core-traces"
            --platform "${CHAMPIONS_TEST_PLATFORM}" --source-commit "${CHAMPIONS_SOURCE_COMMIT}")
    endforeach()
    foreach(stem items item_generation item_names)
        string(REPLACE "_" "-" suite "${stem}")
        add_executable(champions-${suite}-tests tests/${stem}_tests.cpp)
        target_link_libraries(champions-${suite}-tests PRIVATE champions-items)
        if(MSVC)
            target_compile_options(champions-${suite}-tests PRIVATE /W4 /utf-8 /fp:strict)
        else()
            target_compile_options(champions-${suite}-tests PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
        endif()
        add_test(NAME ${suite}-exact COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/../tools/run_core_vectors.py"
            --suite ${suite} --exe "$<TARGET_FILE:champions-${suite}-tests>"
            --vectors "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/${stem}.tsv.gz"
            --manifest "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/${stem}.json"
            --trace-dir "${CMAKE_CURRENT_BINARY_DIR}/core-traces"
            --platform "${CHAMPIONS_TEST_PLATFORM}" --source-commit "${CHAMPIONS_SOURCE_COMMIT}")
    endforeach()
    foreach(component common damage stats items)
        set(suite "combat-${component}")
        add_executable(champions-${suite}-tests tests/combat_${component}_tests.cpp)
        if(component STREQUAL "items")
            target_link_libraries(champions-${suite}-tests PRIVATE champions-combat-items)
        else()
            target_link_libraries(champions-${suite}-tests PRIVATE champions-combat)
        endif()
        if(MSVC)
            target_compile_options(champions-${suite}-tests PRIVATE /W4 /utf-8 /fp:strict)
        else()
            target_compile_options(champions-${suite}-tests PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
        endif()
        add_test(NAME ${suite}-exact COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/../tools/run_core_vectors.py"
            --suite ${suite} --exe "$<TARGET_FILE:champions-${suite}-tests>"
            --vectors "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/combat_${component}.tsv.gz"
            --manifest "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/combat_${component}.json"
            --trace-dir "${CMAKE_CURRENT_BINARY_DIR}/core-traces"
            --platform "${CHAMPIONS_TEST_PLATFORM}" --source-commit "${CHAMPIONS_SOURCE_COMMIT}")
    endforeach()
    # The fire entry of a LooseFire touch and its burn tick (research
    # combat-ctors-2 fire-touch.md) on the combat-damage harness.
    add_test(NAME fire-touch-exact COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_fire_touch_vectors.py"
        "$<TARGET_FILE:champions-combat-damage-tests>"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/fire_touch.tsv.gz")
    foreach(component image execute host)
        set(suite "amx-${component}")
        add_executable(champions-${suite}-tests tests/amx_${component}_tests.cpp)
        target_link_libraries(champions-${suite}-tests PRIVATE champions-amx)
        if(MSVC)
            target_compile_options(champions-${suite}-tests PRIVATE /W4 /utf-8 /fp:strict)
        else()
            target_compile_options(champions-${suite}-tests PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
        endif()
        add_test(NAME ${suite}-exact COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/../tools/run_core_vectors.py"
            --suite ${suite} --exe "$<TARGET_FILE:champions-${suite}-tests>"
            --vectors "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/amx_${component}.tsv.gz"
            --manifest "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/amx_${component}.json"
            --trace-dir "${CMAKE_CURRENT_BINARY_DIR}/core-traces"
            --platform "${CHAMPIONS_TEST_PLATFORM}" --source-commit "${CHAMPIONS_SOURCE_COMMIT}")
    endforeach()
    foreach(component codec session tick)
        set(suite "netwire-${component}")
        add_executable(champions-${suite}-tests tests/netwire_${component}_tests.cpp)
        target_link_libraries(champions-${suite}-tests PRIVATE champions-netwire)
        if(MSVC)
            target_compile_options(champions-${suite}-tests PRIVATE /W4 /utf-8 /fp:strict)
        else()
            target_compile_options(champions-${suite}-tests PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
        endif()
        add_test(NAME ${suite}-exact COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/../tools/run_core_vectors.py"
            --suite ${suite} --exe "$<TARGET_FILE:champions-${suite}-tests>"
            --vectors "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/netwire_${component}.tsv.gz"
            --manifest "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/netwire_${component}.json"
            --trace-dir "${CMAKE_CURRENT_BINARY_DIR}/core-traces"
            --platform "${CHAMPIONS_TEST_PLATFORM}" --source-commit "${CHAMPIONS_SOURCE_COMMIT}")
    endforeach()
    add_test(NAME amx-validation-transport COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_amx_validate.py" "$<TARGET_FILE:champions-amx-validate>")
    add_executable(champions-session-tests tests/session_tests.cpp)
    add_executable(champions-session-replay tools/session_replay.cpp)
    foreach(target champions-session-tests champions-session-replay)
        target_link_libraries(${target} PRIVATE champions-session)
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 /utf-8 /fp:strict)
        else()
            target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
        endif()
    endforeach()
    if(MINGW)
        target_link_options(champions-session-replay PRIVATE -municode)
    endif()
    add_test(NAME session-replay-transport COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_session_replay.py" --native "$<TARGET_FILE:champions-session-replay>")
    add_test(NAME session-owned-host COMMAND champions-session-tests)
    add_test(NAME session-replay-exact COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/run_core_vectors.py"
        --suite session-replay --exe "$<TARGET_FILE:champions-session-replay>"
        --vectors "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/session_replay.tsv.gz"
        --manifest "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/session_replay.json"
        --trace-dir "${CMAKE_CURRENT_BINARY_DIR}/core-traces"
        --platform "${CHAMPIONS_TEST_PLATFORM}" --source-commit "${CHAMPIONS_SOURCE_COMMIT}")
    add_executable(champions-objects-tests tests/objects_tests.cpp)
    add_executable(champions-objects-trace tools/objects_trace.cpp)
    foreach(target champions-objects-tests champions-objects-trace)
        target_link_libraries(${target} PRIVATE champions-objects)
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 /utf-8 /fp:strict)
        else()
            target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
        endif()
    endforeach()
    if(MINGW)
        target_link_options(champions-objects-trace PRIVATE -municode)
    endif()
    add_test(NAME objects-runtime-host COMMAND champions-objects-tests)
    add_test(NAME objects-runtime-exact COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/run_core_vectors.py"
        --suite objects-runtime --exe "$<TARGET_FILE:champions-objects-trace>"
        --vectors "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/objects_runtime.tsv.gz"
        --manifest "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/objects_runtime.json"
        --trace-dir "${CMAKE_CURRENT_BINARY_DIR}/core-traces"
        --platform "${CHAMPIONS_TEST_PLATFORM}" --source-commit "${CHAMPIONS_SOURCE_COMMIT}")
    add_test(NAME objects-runtime-transport COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_objects_trace.py" --native "$<TARGET_FILE:champions-objects-trace>")
    add_executable(champions-session-objects-tests tests/session_objects_tests.cpp)
    add_executable(champions-session-objects-trace tools/session_objects_trace.cpp)
    foreach(target champions-session-objects-tests champions-session-objects-trace)
        target_link_libraries(${target} PRIVATE champions-session)
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 /utf-8 /fp:strict)
        else()
            target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
        endif()
    endforeach()
    if(MINGW)
        target_link_options(champions-session-objects-trace PRIVATE -municode)
    endif()
    add_test(NAME session-objects-host COMMAND champions-session-objects-tests)
    add_test(NAME session-objects-exact COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/run_core_vectors.py"
        --suite session-objects --exe "$<TARGET_FILE:champions-session-objects-trace>"
        --vectors "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/session_objects.tsv.gz"
        --manifest "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/session_objects.json"
        --trace-dir "${CMAKE_CURRENT_BINARY_DIR}/core-traces"
        --platform "${CHAMPIONS_TEST_PLATFORM}" --source-commit "${CHAMPIONS_SOURCE_COMMIT}")
    add_test(NAME session-objects-transport COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_session_objects_trace.py" --native "$<TARGET_FILE:champions-session-objects-trace>")
    add_executable(champions-creature-hp-tests tests/creature_hp_tests.cpp)
    add_executable(champions-creature-hp-trace tools/creature_hp_trace.cpp)
    foreach(target champions-creature-hp-tests champions-creature-hp-trace)
        target_link_libraries(${target} PRIVATE champions-creature-hp)
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 /utf-8 /fp:strict)
        else()
            target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
        endif()
    endforeach()
    if(MINGW)
        target_link_options(champions-creature-hp-trace PRIVATE -municode)
    endif()
    add_test(NAME creature-hp-host COMMAND champions-creature-hp-tests)
    add_test(NAME creature-hp-exact COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/run_core_vectors.py"
        --suite creature-hp --exe "$<TARGET_FILE:champions-creature-hp-trace>"
        --vectors "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/creature_hp.tsv.gz"
        --manifest "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/creature_hp.json"
        --trace-dir "${CMAKE_CURRENT_BINARY_DIR}/core-traces"
        --platform "${CHAMPIONS_TEST_PLATFORM}" --source-commit "${CHAMPIONS_SOURCE_COMMIT}")
    add_test(NAME creature-hp-transport COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_creature_hp_trace.py" --native "$<TARGET_FILE:champions-creature-hp-trace>")
    add_executable(champions-creature-movement-tests tests/creature_movement_tests.cpp)
    add_executable(champions-creature-movement-trace tools/creature_movement_trace.cpp)
    foreach(target champions-creature-movement-tests champions-creature-movement-trace)
        target_link_libraries(${target} PRIVATE champions-creature-movement)
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 /utf-8 /fp:strict)
        else()
            target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
        endif()
    endforeach()
    if(MINGW)
        target_link_options(champions-creature-movement-trace PRIVATE -municode)
    endif()
    add_test(NAME creature-movement-host COMMAND champions-creature-movement-tests)
    add_executable(champions-creature-water-tests tests/creature_water_tests.cpp)
    target_link_libraries(champions-creature-water-tests PRIVATE champions-creature-water)
    if(MSVC)
        target_compile_options(champions-creature-water-tests PRIVATE /W4 /utf-8 /fp:strict)
    else()
        target_compile_options(champions-creature-water-tests PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
    endif()
    add_test(NAME creature-water-host COMMAND champions-creature-water-tests)
    add_test(NAME creature-movement-exact COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/run_core_vectors.py"
        --suite creature-movement --exe "$<TARGET_FILE:champions-creature-movement-trace>"
        --vectors "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/creature_movement.tsv.gz"
        --manifest "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/creature_movement.json"
        --trace-dir "${CMAKE_CURRENT_BINARY_DIR}/core-traces"
        --platform "${CHAMPIONS_TEST_PLATFORM}" --source-commit "${CHAMPIONS_SOURCE_COMMIT}")
    add_test(NAME creature-movement-transport COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_creature_movement_trace.py" --native "$<TARGET_FILE:champions-creature-movement-trace>")
    add_executable(champions-creature-turn-tests tests/creature_turn_tests.cpp)
    add_executable(champions-creature-turn-trace tools/creature_turn_trace.cpp)
    foreach(target champions-creature-turn-tests champions-creature-turn-trace)
        target_link_libraries(${target} PRIVATE champions-creature-movement)
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 /utf-8 /fp:strict)
        else()
            target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
        endif()
    endforeach()
    if(MINGW)
        target_link_options(champions-creature-turn-trace PRIVATE -municode)
    endif()
    add_test(NAME creature-turn-host COMMAND champions-creature-turn-tests)
    add_test(NAME creature-turn-exact COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/run_core_vectors.py"
        --suite creature-turn --exe "$<TARGET_FILE:champions-creature-turn-trace>"
        --vectors "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/creature_turn.tsv.gz"
        --manifest "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/creature_turn.json"
        --trace-dir "${CMAKE_CURRENT_BINARY_DIR}/core-traces"
        --platform "${CHAMPIONS_TEST_PLATFORM}" --source-commit "${CHAMPIONS_SOURCE_COMMIT}")
    add_test(NAME creature-turn-transport COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_creature_turn_trace.py" --native "$<TARGET_FILE:champions-creature-turn-trace>")
    add_executable(champions-combat-creature-tests tests/combat_creature_tests.cpp)
    add_executable(champions-combat-creature-trace tools/combat_creature_trace.cpp)
    foreach(target champions-combat-creature-tests champions-combat-creature-trace)
        target_link_libraries(${target} PRIVATE champions-combat-creature)
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 /utf-8 /fp:strict)
        else()
            target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
        endif()
    endforeach()
    if(MINGW)
        target_link_options(champions-combat-creature-trace PRIVATE -municode)
    endif()
    add_test(NAME combat-creature-host COMMAND champions-combat-creature-tests)
    add_test(NAME combat-creature-exact COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/run_core_vectors.py"
        --suite combat-creature --exe "$<TARGET_FILE:champions-combat-creature-trace>"
        --vectors "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/combat_creature.tsv.gz"
        --manifest "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/combat_creature.json"
        --trace-dir "${CMAKE_CURRENT_BINARY_DIR}/core-traces"
        --platform "${CHAMPIONS_TEST_PLATFORM}" --source-commit "${CHAMPIONS_SOURCE_COMMIT}")
    add_test(NAME combat-creature-transport COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_combat_creature_trace.py" --native "$<TARGET_FILE:champions-combat-creature-trace>")
    add_executable(champions-raw-handles-tests tests/raw_handles_tests.cpp)
    add_executable(champions-raw-handles-trace tools/raw_handles_trace.cpp)
    foreach(target champions-raw-handles-tests champions-raw-handles-trace)
        target_link_libraries(${target} PRIVATE champions-raw-handles)
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 /utf-8 /fp:strict)
        else()
            target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
        endif()
    endforeach()
    if(MINGW)
        target_link_options(champions-raw-handles-trace PRIVATE -municode)
    endif()
    add_test(NAME raw-handles-host COMMAND champions-raw-handles-tests)
    add_test(NAME raw-handles-exact COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/run_core_vectors.py"
        --suite raw-handles --exe "$<TARGET_FILE:champions-raw-handles-trace>"
        --vectors "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/raw_handles.tsv.gz"
        --manifest "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/raw_handles.json"
        --trace-dir "${CMAKE_CURRENT_BINARY_DIR}/core-traces"
        --platform "${CHAMPIONS_TEST_PLATFORM}" --source-commit "${CHAMPIONS_SOURCE_COMMIT}")
    add_test(NAME raw-handles-transport COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_raw_handles_trace.py" --native "$<TARGET_FILE:champions-raw-handles-trace>")
    add_executable(champions-session-hp-tests tests/session_hp_tests.cpp)
    add_executable(champions-session-hp-trace tools/session_hp_trace.cpp)
    foreach(target champions-session-hp-tests champions-session-hp-trace)
        target_link_libraries(${target} PRIVATE champions-session)
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 /utf-8 /fp:strict)
        else()
            target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
        endif()
    endforeach()
    if(MINGW)
        target_link_options(champions-session-hp-trace PRIVATE -municode)
    endif()
    add_test(NAME session-hp-host COMMAND champions-session-hp-tests)
    add_test(NAME session-hp-exact COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/run_core_vectors.py"
        --suite session-hp --exe "$<TARGET_FILE:champions-session-hp-trace>"
        --vectors "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/session_hp.tsv.gz"
        --manifest "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/session_hp.json"
        --trace-dir "${CMAKE_CURRENT_BINARY_DIR}/core-traces"
        --platform "${CHAMPIONS_TEST_PLATFORM}" --source-commit "${CHAMPIONS_SOURCE_COMMIT}")
    add_test(NAME session-hp-transport COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_session_hp_trace.py" --native "$<TARGET_FILE:champions-session-hp-trace>")
    add_executable(champions-session-combat-tests tests/session_combat_tests.cpp)
    add_executable(champions-session-combat-trace tools/session_combat_trace.cpp)
    foreach(target champions-session-combat-tests champions-session-combat-trace)
        target_link_libraries(${target} PRIVATE champions-session)
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 /utf-8 /fp:strict)
        else()
            target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
        endif()
    endforeach()
    if(MINGW)
        target_link_options(champions-session-combat-trace PRIVATE -municode)
    endif()
    add_test(NAME session-combat-host COMMAND champions-session-combat-tests)
    add_test(NAME session-combat-exact COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/run_core_vectors.py"
        --suite session-combat --exe "$<TARGET_FILE:champions-session-combat-trace>"
        --vectors "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/session_combat.tsv.gz"
        --manifest "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/session_combat.json"
        --trace-dir "${CMAKE_CURRENT_BINARY_DIR}/core-traces"
        --platform "${CHAMPIONS_TEST_PLATFORM}" --source-commit "${CHAMPIONS_SOURCE_COMMIT}")
    add_test(NAME session-combat-transport COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_session_combat_trace.py" --native "$<TARGET_FILE:champions-session-combat-trace>")
    add_executable(champions-session-terrain-tests tests/session_terrain_tests.cpp)
    add_executable(champions-session-terrain-trace tools/session_terrain_trace.cpp)
    foreach(target champions-session-terrain-tests champions-session-terrain-trace)
        target_link_libraries(${target} PRIVATE champions-session)
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 /utf-8 /fp:strict)
        else()
            target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
        endif()
    endforeach()
    if(MINGW)
        target_link_options(champions-session-terrain-trace PRIVATE -municode)
    endif()
    add_test(NAME session-terrain-host COMMAND champions-session-terrain-tests)
    add_test(NAME session-terrain-exact COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/run_core_vectors.py"
        --suite session-terrain --exe "$<TARGET_FILE:champions-session-terrain-trace>"
        --vectors "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/session_terrain.tsv.gz"
        --manifest "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/session_terrain.json"
        --trace-dir "${CMAKE_CURRENT_BINARY_DIR}/core-traces"
        --platform "${CHAMPIONS_TEST_PLATFORM}" --source-commit "${CHAMPIONS_SOURCE_COMMIT}")
    add_test(NAME session-terrain-transport COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_session_terrain_trace.py" --native "$<TARGET_FILE:champions-session-terrain-trace>")
    add_executable(champions-creature-movement-borrowed-tests tests/creature_movement_borrowed_tests.cpp)
    target_link_libraries(champions-creature-movement-borrowed-tests PRIVATE champions-creature-movement)
    add_executable(champions-session-creature-tests tests/session_creature_tests.cpp)
    add_executable(champions-session-creature-trace tools/session_creature_trace.cpp)
    foreach(target champions-creature-movement-borrowed-tests champions-session-creature-tests champions-session-creature-trace)
        if(NOT target STREQUAL "champions-creature-movement-borrowed-tests")
            target_link_libraries(${target} PRIVATE champions-session)
        endif()
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 /utf-8 /fp:strict)
        else()
            target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
        endif()
    endforeach()
    if(MINGW)
        target_link_options(champions-session-creature-trace PRIVATE -municode)
    endif()
    add_test(NAME creature-movement-borrowed-host COMMAND champions-creature-movement-borrowed-tests)
    add_test(NAME session-creature-host COMMAND champions-session-creature-tests)
    add_test(NAME session-creature-exact COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/run_core_vectors.py"
        --suite session-creature --exe "$<TARGET_FILE:champions-session-creature-trace>"
        --vectors "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/session_creature.tsv.gz"
        --manifest "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/session_creature.json"
        --trace-dir "${CMAKE_CURRENT_BINARY_DIR}/core-traces"
        --platform "${CHAMPIONS_TEST_PLATFORM}" --source-commit "${CHAMPIONS_SOURCE_COMMIT}")
    add_test(NAME session-creature-transport COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_session_creature_trace.py" --native "$<TARGET_FILE:champions-session-creature-trace>")
    add_executable(champions-session-control-tests tests/session_control_tests.cpp)
    target_link_libraries(champions-session-control-tests PRIVATE champions-session-control)
    if(MSVC)
        target_compile_options(champions-session-control-tests PRIVATE /W4 /utf-8 /fp:strict)
    else()
        target_compile_options(champions-session-control-tests PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
    endif()
    add_test(NAME session-control-host COMMAND champions-session-control-tests)
    add_executable(champions-session-control-trace tools/session_control_trace.cpp)
    target_link_libraries(champions-session-control-trace PRIVATE champions-session-control)
    if(MSVC)
        target_compile_options(champions-session-control-trace PRIVATE /W4 /utf-8 /fp:strict)
    else()
        target_compile_options(champions-session-control-trace PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
    endif()
    if(MINGW)
        target_link_options(champions-session-control-trace PRIVATE -municode)
    endif()
    add_test(NAME session-control-exact COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/run_core_vectors.py"
        --suite session-control --exe "$<TARGET_FILE:champions-session-control-trace>"
        --vectors "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/session_control.tsv.gz"
        --manifest "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/session_control.json"
        --trace-dir "${CMAKE_CURRENT_BINARY_DIR}/core-traces"
        --platform "${CHAMPIONS_TEST_PLATFORM}" --source-commit "${CHAMPIONS_SOURCE_COMMIT}")
    add_test(NAME session-control-transport COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_session_control_trace.py" --native "$<TARGET_FILE:champions-session-control-trace>")
    add_executable(champions-session-control-creature-tests tests/session_control_creature_tests.cpp)
    add_executable(champions-session-control-creature-trace tools/session_control_creature_trace.cpp)
    foreach(target champions-session-control-creature-tests champions-session-control-creature-trace)
        target_link_libraries(${target} PRIVATE champions-session-control)
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 /utf-8 /fp:strict)
        else()
            target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
        endif()
    endforeach()
    if(MINGW)
        target_link_options(champions-session-control-creature-trace PRIVATE -municode)
    endif()
    add_executable(champions-frame-clock-tests tests/frame_clock_tests.cpp)
    target_link_libraries(champions-frame-clock-tests PRIVATE champions-session-control)
    add_test(NAME frame-clock-traces COMMAND champions-frame-clock-tests)
    add_test(NAME session-control-creature-host COMMAND champions-session-control-creature-tests)
    add_test(NAME session-control-creature-exact COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/run_core_vectors.py"
        --suite session-control-creature --exe "$<TARGET_FILE:champions-session-control-creature-trace>"
        --vectors "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/session_control_creature.tsv.gz"
        --manifest "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/session_control_creature.json"
        --trace-dir "${CMAKE_CURRENT_BINARY_DIR}/core-traces"
        --platform "${CHAMPIONS_TEST_PLATFORM}" --source-commit "${CHAMPIONS_SOURCE_COMMIT}")
    add_test(NAME session-control-creature-transport COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_session_control_creature_trace.py" --native "$<TARGET_FILE:champions-session-control-creature-trace>")
    add_executable(champions-terrain-file-tests tests/terrain_file_tests.cpp)
    add_executable(champions-terrain-file-trace tools/terrain_file_trace.cpp)
    foreach(target champions-terrain-file-tests champions-terrain-file-trace)
        target_link_libraries(${target} PRIVATE champions-terrain-file)
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 /utf-8 /fp:strict)
        else()
            target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
        endif()
    endforeach()
    if(MINGW)
        target_link_options(champions-terrain-file-trace PRIVATE -municode)
    endif()
    add_test(NAME terrain-file-host COMMAND champions-terrain-file-tests)
    add_test(NAME terrain-file-exact COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/run_core_vectors.py"
        --suite terrain-file --exe "$<TARGET_FILE:champions-terrain-file-trace>"
        --vectors "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/terrain_file.tsv.gz"
        --manifest "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/terrain_file.json"
        --trace-dir "${CMAKE_CURRENT_BINARY_DIR}/core-traces"
        --platform "${CHAMPIONS_TEST_PLATFORM}" --source-commit "${CHAMPIONS_SOURCE_COMMIT}")
    add_test(NAME terrain-file-transport COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_terrain_file_trace.py" --native "$<TARGET_FILE:champions-terrain-file-trace>")
    add_executable(champions-world-queries-tests tests/world_queries_tests.cpp)
    add_executable(champions-world-queries-trace tools/world_queries_trace.cpp)
    foreach(target champions-world-queries-tests champions-world-queries-trace)
        target_link_libraries(${target} PRIVATE champions-world-queries)
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 /utf-8 /fp:strict)
        else()
            target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
        endif()
    endforeach()
    if(MINGW)
        target_link_options(champions-world-queries-trace PRIVATE -municode)
    endif()
    add_test(NAME world-queries-host COMMAND champions-world-queries-tests)
    add_test(NAME world-queries-exact COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/run_core_vectors.py"
        --suite world-queries --exe "$<TARGET_FILE:champions-world-queries-trace>"
        --vectors "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/world_queries.tsv.gz"
        --manifest "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/world_queries.json"
        --trace-dir "${CMAKE_CURRENT_BINARY_DIR}/core-traces"
        --platform "${CHAMPIONS_TEST_PLATFORM}" --source-commit "${CHAMPIONS_SOURCE_COMMIT}")
    add_test(NAME world-queries-transport COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_world_queries_trace.py" --native "$<TARGET_FILE:champions-world-queries-trace>")
    add_test(NAME model-forms-validation-transport COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_model_forms_validate.py" --native "$<TARGET_FILE:champions-model-forms-validate>")
    add_executable(champions-world-raycast-tests tests/world_raycast_tests.cpp)
    target_link_libraries(champions-world-raycast-tests PRIVATE champions-world-queries)
    if(MSVC)
        target_compile_options(champions-world-raycast-tests PRIVATE /W4 /utf-8 /fp:strict)
    else()
        target_compile_options(champions-world-raycast-tests PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
    endif()
    add_test(NAME world-raycast-host COMMAND champions-world-raycast-tests)
    add_executable(champions-world-geometry-tests tests/world_geometry_tests.cpp)
    add_executable(champions-world-geometry-trace tools/world_geometry_trace.cpp)
    foreach(target champions-world-geometry-tests champions-world-geometry-trace)
        target_link_libraries(${target} PRIVATE champions-world-geometry)
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 /utf-8 /fp:strict)
        else()
            target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
        endif()
    endforeach()
    if(MINGW)
        target_link_options(champions-world-geometry-trace PRIVATE -municode)
    endif()
    add_test(NAME world-geometry-host COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_world_geometry_api.py" "$<TARGET_FILE:champions-world-geometry-tests>")
    add_test(NAME world-geometry-exact COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/run_core_vectors.py"
        --suite world-geometry --exe "$<TARGET_FILE:champions-world-geometry-trace>"
        --vectors "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/world_geometry.tsv.gz"
        --manifest "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/world_geometry.json"
        --trace-dir "${CMAKE_CURRENT_BINARY_DIR}/core-traces"
        --platform "${CHAMPIONS_TEST_PLATFORM}" --source-commit "${CHAMPIONS_SOURCE_COMMIT}")
    add_test(NAME world-geometry-transport COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_world_geometry_trace.py" --native "$<TARGET_FILE:champions-world-geometry-trace>")
    add_executable(champions-animation-clock-api-tests tests/animation_clock_tests.cpp)
    add_executable(champions-animation-clock-trace tools/animation_clock_trace.cpp)
    foreach(target champions-animation-clock-api-tests champions-animation-clock-trace)
        target_link_libraries(${target} PRIVATE champions-animation-clock)
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 /utf-8 /fp:strict)
        else()
            target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
        endif()
    endforeach()
    if(MINGW)
        target_link_options(champions-animation-clock-trace PRIVATE -municode)
    endif()
    add_test(NAME animation-clock-api COMMAND champions-animation-clock-api-tests)
    add_test(NAME animation-clock-exact COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/run_core_vectors.py"
        --suite animation-clock --exe "$<TARGET_FILE:champions-animation-clock-trace>"
        --vectors "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/animation_clock.tsv.gz"
        --manifest "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/animation_clock.json"
        --trace-dir "${CMAKE_CURRENT_BINARY_DIR}/core-traces"
        --platform "${CHAMPIONS_TEST_PLATFORM}" --source-commit "${CHAMPIONS_SOURCE_COMMIT}")
    add_test(NAME animation-clock-transport COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_animation_clock_trace.py" --native "$<TARGET_FILE:champions-animation-clock-trace>")
    add_executable(champions-session-turn-tests tests/session_turn_tests.cpp)
    target_link_libraries(champions-session-turn-tests PRIVATE champions-session)
    if(MSVC)
        target_compile_options(champions-session-turn-tests PRIVATE /W4 /utf-8 /fp:strict)
    else()
        target_compile_options(champions-session-turn-tests PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
    endif()
    add_executable(champions-session-turn-trace tools/session_turn_trace.cpp)
    target_link_libraries(champions-session-turn-trace PRIVATE champions-session)
    if(MSVC)
        target_compile_options(champions-session-turn-trace PRIVATE /W4 /utf-8 /fp:strict)
    else()
        target_compile_options(champions-session-turn-trace PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
    endif()
    if(MINGW)
        target_link_options(champions-session-turn-trace PRIVATE -municode)
    endif()
    add_test(NAME session-turn-host COMMAND champions-session-turn-tests)
    add_test(NAME session-turn-exact COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/run_core_vectors.py"
        --suite session-turn --exe "$<TARGET_FILE:champions-session-turn-trace>"
        --vectors "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/session_turn.tsv.gz"
        --manifest "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/session_turn.json"
        --trace-dir "${CMAKE_CURRENT_BINARY_DIR}/core-traces"
        --platform "${CHAMPIONS_TEST_PLATFORM}" --source-commit "${CHAMPIONS_SOURCE_COMMIT}")
    add_test(NAME session-turn-transport COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_session_turn_trace.py" --native "$<TARGET_FILE:champions-session-turn-trace>")
    include("${CMAKE_CURRENT_SOURCE_DIR}/session_anim_cache_targets.cmake")
    add_executable(champions-session-animation-tests tests/session_animation_tests.cpp)
    add_executable(champions-session-animation-trace tools/session_animation_trace.cpp)
    foreach(target champions-session-animation-tests champions-session-animation-trace)
        target_link_libraries(${target} PRIVATE champions-session)
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 /utf-8 /fp:strict)
        else()
            target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
        endif()
    endforeach()
    if(MINGW)
        target_link_options(champions-session-animation-trace PRIVATE -municode)
    endif()
    add_test(NAME session-animation-host COMMAND champions-session-animation-tests)
    add_test(NAME session-animation-exact COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/run_core_vectors.py"
        --suite session-animation --exe "$<TARGET_FILE:champions-session-animation-trace>"
        --vectors "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/session_animation.tsv.gz"
        --manifest "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/session_animation.json"
        --trace-dir "${CMAKE_CURRENT_BINARY_DIR}/core-traces"
        --platform "${CHAMPIONS_TEST_PLATFORM}" --source-commit "${CHAMPIONS_SOURCE_COMMIT}")
    add_test(NAME session-animation-transport COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_session_animation_trace.py" --native "$<TARGET_FILE:champions-session-animation-trace>")
    add_executable(champions-animation-runtime-api-tests tests/animation_runtime_tests.cpp)
    add_executable(champions-animation-runtime-trace tools/animation_runtime_trace.cpp)
    foreach(target champions-animation-runtime-api-tests champions-animation-runtime-trace)
        target_link_libraries(${target} PRIVATE champions-animation-runtime)
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 /utf-8 /fp:strict)
        else()
            target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
        endif()
    endforeach()
    if(MINGW)
        target_link_options(champions-animation-runtime-trace PRIVATE -municode)
    endif()
    add_test(NAME animation-runtime-api COMMAND champions-animation-runtime-api-tests)
    add_test(NAME animation-runtime-exact COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/run_core_vectors.py"
        --suite animation-runtime --exe "$<TARGET_FILE:champions-animation-runtime-trace>"
        --vectors "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/animation_runtime.tsv.gz"
        --manifest "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/animation_runtime.json"
        --trace-dir "${CMAKE_CURRENT_BINARY_DIR}/core-traces"
        --platform "${CHAMPIONS_TEST_PLATFORM}" --source-commit "${CHAMPIONS_SOURCE_COMMIT}")
    add_test(NAME animation-runtime-transport COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_animation_runtime_trace.py" --native "$<TARGET_FILE:champions-animation-runtime-trace>")
    add_executable(champions-animation-decoder-api-tests tests/animation_decoder_tests.cpp)
    add_executable(champions-animation-decoder-trace tools/animation_decoder_trace.cpp)
    foreach(target champions-animation-decoder-api-tests champions-animation-decoder-trace)
        target_link_libraries(${target} PRIVATE champions-animation-decoder)
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 /utf-8 /fp:strict)
        else()
            target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
        endif()
    endforeach()
    if(MINGW)
        target_link_options(champions-animation-decoder-trace PRIVATE -municode)
    endif()
    add_test(NAME animation-decoder-api COMMAND champions-animation-decoder-api-tests)
    add_test(NAME animation-decoder-exact COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/run_core_vectors.py"
        --suite animation-decoder --exe "$<TARGET_FILE:champions-animation-decoder-trace>"
        --vectors "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/animation_decoder.tsv.gz"
        --manifest "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/animation_decoder.json"
        --trace-dir "${CMAKE_CURRENT_BINARY_DIR}/core-traces"
        --platform "${CHAMPIONS_TEST_PLATFORM}" --source-commit "${CHAMPIONS_SOURCE_COMMIT}")
    add_test(NAME animation-decoder-transport COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_animation_decoder_trace.py" --native "$<TARGET_FILE:champions-animation-decoder-trace>")
    add_test(NAME animation-decoder-private-input COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_animation_decoder_validate.py" "$<TARGET_FILE:champions-animation-decoder-validate>")
    include("${CMAKE_CURRENT_SOURCE_DIR}/skills_a_tests.cmake")
    include("${CMAKE_CURRENT_SOURCE_DIR}/skill_kernel_targets.cmake")
    add_test(NAME private-validation-report COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_validate_data.py" "$<TARGET_FILE:champions-data-validate>")
    # Authored local-pose numeric policy: no fast-math or fused operations.
    add_executable(champions-animation-trace tests/animation_trace.cpp)
    target_link_libraries(champions-animation-trace PRIVATE champions-animation champions-skinning champions-model)
    if(MSVC)
        target_compile_options(champions-animation-trace PRIVATE /W4 /utf-8 /fp:strict)
    else()
        target_compile_options(champions-animation-trace PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
    endif()
    add_test(NAME animation-pose-trace COMMAND champions-animation-trace --self-test)
    add_executable(champions-animation-tests tests/animation_tests.cpp)
    target_link_libraries(champions-animation-tests PRIVATE champions-animation)
    if(MSVC)
        target_compile_options(champions-animation-tests PRIVATE /W4 /utf-8)
    else()
        target_compile_options(champions-animation-tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME animation-skeleton COMMAND champions-animation-tests)
    add_executable(champions-skinning-tests tests/skinning_tests.cpp)
    target_link_libraries(champions-skinning-tests PRIVATE champions-skinning champions-animation champions-model)
    if(MSVC)
        target_compile_options(champions-skinning-tests PRIVATE /W4 /utf-8 /fp:strict)
    else()
        target_compile_options(champions-skinning-tests PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
    endif()
    add_test(NAME skinning-positions COMMAND champions-skinning-tests)
    add_executable(champions-model-tests tests/model_tests.cpp)
    target_link_libraries(champions-model-tests PRIVATE champions-model)
    if(MSVC)
        target_compile_options(champions-model-tests PRIVATE /W4 /utf-8)
    else()
        target_compile_options(champions-model-tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    if(MINGW)
        target_link_options(champions-model-tests PRIVATE -municode)
    endif()
    add_test(NAME model-parser COMMAND champions-model-tests)
    add_test(NAME create-model-fixtures COMMAND champions-model-tests --write-fixtures "${CMAKE_CURRENT_BINARY_DIR}/model fixtures")
    add_executable(champions-texture-tests tests/texture_tests.cpp)
    target_link_libraries(champions-texture-tests PRIVATE champions-texture)
    if(MSVC)
        target_compile_options(champions-texture-tests PRIVATE /W4 /utf-8)
    else()
        target_compile_options(champions-texture-tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    if(MINGW)
        target_link_options(champions-texture-tests PRIVATE -municode)
    endif()
    add_test(NAME texture-parser COMMAND champions-texture-tests)
    add_test(NAME create-texture-fixtures COMMAND champions-texture-tests --write-fixtures "${CMAKE_CURRENT_BINARY_DIR}/texture fixtures")
    add_executable(champions-data-tests tests/archive_tests.cpp)
    target_link_libraries(champions-data-tests PRIVATE champions-archive)
    if(MSVC)
        target_compile_options(champions-data-tests PRIVATE /W4 /utf-8)
    else()
        target_compile_options(champions-data-tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    if(MINGW)
        target_link_options(champions-data-tests PRIVATE -municode)
    endif()
    add_test(NAME archive-parser COMMAND champions-data-tests)
    set(FIXTURE_DIR "${CMAKE_CURRENT_BINARY_DIR}/test data")
    add_test(NAME create-fixtures COMMAND champions-data-tests --write-fixtures "${FIXTURE_DIR}")
    set_tests_properties(create-fixtures PROPERTIES FIXTURES_SETUP data-fixtures)
    add_test(NAME cli-nested COMMAND champions-data list "${FIXTURE_DIR}/nested.GOB" "${FIXTURE_DIR}/empty.LMP")
    set_tests_properties(cli-nested PROPERTIES FIXTURES_REQUIRED data-fixtures PASS_REGULAR_EXPRESSION "lmp-entry")
    add_test(NAME cli-unicode-path COMMAND champions-data list "${FIXTURE_DIR}/Chämpions 测试.LMP")
    set_tests_properties(cli-unicode-path PROPERTIES FIXTURES_REQUIRED data-fixtures PASS_REGULAR_EXPRESSION "lmp-entry")
    add_test(NAME cli-invalid COMMAND champions-data list "${FIXTURE_DIR}/invalid.LMP")
    set_tests_properties(cli-invalid PROPERTIES FIXTURES_REQUIRED data-fixtures WILL_FAIL TRUE)
    add_test(NAME cli-help COMMAND champions-data --help)
    add_test(NAME cli-missing-arguments COMMAND champions-data list)
    set_tests_properties(cli-missing-arguments PROPERTIES WILL_FAIL TRUE)
endif()

# Binary packages contain only the inspector and authored documentation.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/build-info.txt"
    "scope=Archive metadata inspector; not a playable game\n"
    "source_commit=${CHAMPIONS_SOURCE_COMMIT}\n"
    "system=${CMAKE_SYSTEM_NAME}\n"
    "system_processor=${CMAKE_SYSTEM_PROCESSOR}\n"
    "generator_platform=${CMAKE_GENERATOR_PLATFORM}\n"
    "osx_architectures=${CMAKE_OSX_ARCHITECTURES}\n"
    "compiler=${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}\n")
install(TARGETS champions-data champions-data-validate RUNTIME DESTINATION .)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/../LICENSE"
    "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
    "${CMAKE_CURRENT_BINARY_DIR}/build-info.txt" DESTINATION .)
set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_NAME "champions-data")
set(CPACK_PACKAGE_VENDOR "Open Champions contributors")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Champions archive metadata inspector; not a playable game")
set(CPACK_PACKAGE_CHECKSUM "SHA256")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/../LICENSE")
set(CPACK_VERBATIM_VARIABLES YES)
# CI uses CPackConfig.cmake and install rules, never source-directory packaging.
set(CPACK_SOURCE_GENERATOR "")
include(CPack)

if(BUILD_TESTING)
    add_executable(champions-player-tests tests/player_tests.cpp)
    add_executable(champions-player-trace tests/player_trace.cpp)
    target_link_libraries(champions-player-tests PRIVATE champions-player)
    target_link_libraries(champions-player-trace PRIVATE champions-player)
    add_test(NAME player-host COMMAND champions-player-tests)
    add_test(NAME player-vector-harness COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_player_vector_harness.py")
    add_test(NAME player-exact COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_player_vectors.py"
        --native "$<TARGET_FILE:champions-player-trace>"
        --vectors "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/player.tsv.gz")
    # 274,304 full-state vectors exceed 240 s on the arm64 HUD worktree.
    set_tests_properties(player-exact PROPERTIES TIMEOUT 900)
    add_executable(champions-monsters-1-trace tools/monsters_1_trace.cpp)
    target_link_libraries(champions-monsters-1-trace PRIVATE champions-monsters-1)
    add_executable(champions-monsters-1-tests tests/monsters_1_tests.cpp)
    target_link_libraries(champions-monsters-1-tests PRIVATE champions-monsters-1)
    add_test(NAME monsters-1-unit COMMAND champions-monsters-1-tests)
    add_test(NAME monsters-1-exact COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/run_monsters_1_vectors.py"
        --exe "$<TARGET_FILE:champions-monsters-1-trace>"
        --vectors "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/monsters_1.tsv.gz"
        --manifest "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/monsters_1.json"
        --scratch "${CMAKE_CURRENT_BINARY_DIR}/monsters-1-exact")
    set_tests_properties(monsters-1-exact PROPERTIES TIMEOUT 240)
    add_executable(champions-skills-b-tests tests/skills_b_tests.cpp)
    target_link_libraries(champions-skills-b-tests PRIVATE champions-skills-b)
    if(MSVC)
        target_compile_options(champions-skills-b-tests PRIVATE /W4 /utf-8 /fp:strict)
    else()
        target_compile_options(champions-skills-b-tests PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
    endif()
    add_test(NAME skills-b-host COMMAND champions-skills-b-tests)
    add_test(NAME skills-b-exact COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_skills_b_vectors.py"
        --exe "$<TARGET_FILE:champions-skills-b-tests>"
        --vectors "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/skills_b.tsv.gz")
    add_test(NAME skills-b-transport COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_skills_b_vectors.py"
        --exe "$<TARGET_FILE:champions-skills-b-tests>"
        --vectors "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/skills_b.tsv.gz" --transport)
    set_tests_properties(skills-b-exact skills-b-transport PROPERTIES TIMEOUT 150)
    add_executable(champions-skills-c-api-tests tests/skills_c_tests.cpp)
    target_link_libraries(champions-skills-c-api-tests PRIVATE champions-skills-c)
    add_test(NAME skills-c-api COMMAND champions-skills-c-api-tests)
    add_executable(champions-skills-c-trace tools/skills_c_trace.cpp)
    target_link_libraries(champions-skills-c-trace PRIVATE champions-skills-c)
    add_test(NAME skills-c-exact COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_skills_c_vectors.py"
        --exe "$<TARGET_FILE:champions-skills-c-trace>"
        --vectors "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/skills_c.tsv.gz"
        --manifest "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/skills_c.json")
    # Complete-state corpus streaming and traces share CPU with other worktrees.
    set_tests_properties(skills-c-exact PROPERTIES TIMEOUT 1800)
    add_executable(champions-texture-upscale-tests tests/texture_upscale_tests.cpp)
    target_link_libraries(champions-texture-upscale-tests PRIVATE champions-texture)
    add_test(NAME texture-upscale COMMAND champions-texture-upscale-tests)
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/ui_font_targets.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/ui_layout_targets.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/menus_targets.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/session_recovery_targets.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/session_facing_targets.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/horsemen_mount_targets.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/control_anim_cache_targets.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/play_session_targets.cmake")
# Serialized RTA inventory picture-cache payload; independent of Godot.
add_library(champions-inventory-icons src/inventory_icons.cpp src/inventory_icon_assoc.cpp)
target_link_libraries(champions-menus PUBLIC champions-inventory-icons)
target_include_directories(champions-inventory-icons PUBLIC include)
if(MSVC)
    target_compile_options(champions-inventory-icons PRIVATE /W4 /utf-8)
else()
    target_compile_options(champions-inventory-icons PRIVATE -Wall -Wextra -Wpedantic)
endif()
if(BUILD_TESTING)
    add_executable(champions-inventory-icons-tests tests/inventory_icons_tests.cpp)
    target_link_libraries(champions-inventory-icons-tests PRIVATE champions-inventory-icons)
    add_test(NAME inventory-icons COMMAND champions-inventory-icons-tests)
    add_executable(champions-inventory-icon-assoc-tests tests/inventory_icon_assoc_tests.cpp)
    target_link_libraries(champions-inventory-icon-assoc-tests PRIVATE champions-inventory-icons)
    add_test(NAME inventory-icon-assoc COMMAND champions-inventory-icon-assoc-tests)
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/combat_runtime_test_targets.cmake")
if(BUILD_TESTING)
    add_executable(champions-combat-play-tests tests/combat_play_tests.cpp)
    target_link_libraries(champions-combat-play-tests PRIVATE champions-combat-play)
    add_test(NAME combat-play COMMAND champions-combat-play-tests)
    add_executable(champions-play-spells-tests tests/play_spells_tests.cpp)
    target_link_libraries(champions-play-spells-tests PRIVATE champions-combat-play)
    add_test(NAME play-spells COMMAND champions-play-spells-tests)
    add_executable(champions-combat-free-spot-tests tests/combat_free_spot_tests.cpp)
    target_link_libraries(champions-combat-free-spot-tests PRIVATE champions-combat-play)
    add_test(NAME combat-free-spot-host COMMAND champions-combat-free-spot-tests)
    add_test(NAME combat-free-spot-exact COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_combat_free_spot_vectors.py"
        "$<TARGET_FILE:champions-combat-free-spot-tests>"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/combat_free_spot.txt.gz")
    add_executable(champions-netplay-tests tests/netplay_tests.cpp)
    target_link_libraries(champions-netplay-tests PRIVATE champions-netplay champions-archive)
    if(MSVC)
        target_compile_options(champions-netplay-tests PRIVATE /W4 /utf-8 /fp:strict)
    else()
        target_compile_options(champions-netplay-tests PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
    endif()
    add_test(NAME netplay COMMAND champions-netplay-tests)
    set_tests_properties(netplay PROPERTIES TIMEOUT 900)
    add_executable(champions-potion-ctors-tests tests/potion_ctors_tests.cpp)
    target_link_libraries(champions-potion-ctors-tests PRIVATE champions-combat-runtime)
    add_test(NAME potion-ctors-exact COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_potion_ctors_vectors.py"
        "$<TARGET_FILE:champions-potion-ctors-tests>"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/potion_ctors.txt.gz")
    add_executable(champions-combat-ctors-tests tests/combat_ctors_tests.cpp)
    target_link_libraries(champions-combat-ctors-tests PRIVATE champions-combat-ctors)
    add_test(NAME combat-ctors-exact COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_combat_ctors_vectors.py"
        "$<TARGET_FILE:champions-combat-ctors-tests>"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/combat_ctors.txt.gz")
    add_executable(champions-combat-ctors-2-tests tests/combat_ctors_2_tests.cpp)
    target_link_libraries(champions-combat-ctors-2-tests PRIVATE champions-combat-ctors)
    add_executable(champions-combat-ctors-3-tests tests/combat_ctors_3_tests.cpp)
    target_link_libraries(champions-combat-ctors-3-tests PRIVATE champions-combat-ctors)
    add_test(NAME combat-ctors-3-reference COMMAND champions-combat-ctors-3-tests)
    add_test(NAME combat-ctors-2-exact COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_combat_ctors_2_vectors.py"
        "$<TARGET_FILE:champions-combat-ctors-2-tests>"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/combat_ctors_2.txt.gz")
    add_executable(champions-playable-combat-fx-tests tests/playable_combat_fx_tests.cpp)
    target_include_directories(champions-playable-combat-fx-tests PRIVATE src)
    target_link_libraries(champions-playable-combat-fx-tests PRIVATE champions-combat-play-effects)
    add_test(NAME playable-combat-fx COMMAND champions-playable-combat-fx-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/fx_skill_recipes_synthetic.json")
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/level_flow_targets.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/arena_modes_targets.cmake")
# Counts-only real-level lockstep measure. Not a CTest: it needs a private disc.
add_executable(champions-netplay-level-measure tools/netplay_level_measure.cpp)
target_link_libraries(champions-netplay-level-measure PRIVATE champions-netplay champions-level-flow champions-terrain-file)
if(MSVC)
    target_compile_options(champions-netplay-level-measure PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-netplay-level-measure PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/fixture_save_targets.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/creature_anim_tests.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/own_machines_tests.cmake")

if(BUILD_TESTING)
    add_executable(champions-animated-combat-play-tests tests/animated_combat_play_tests.cpp)
    target_link_libraries(champions-animated-combat-play-tests PRIVATE champions-combat-play)
    add_test(NAME animated-combat-play COMMAND champions-animated-combat-play-tests)
endif()
# Portable audio contracts and synthetic vectors.
include("${CMAKE_CURRENT_SOURCE_DIR}/audio_targets.cmake")

if(BUILD_TESTING)
    add_executable(champions-playable-audio-tests tests/playable_audio_tests.cpp)
    target_link_libraries(champions-playable-audio-tests PRIVATE champions-native-audio champions-creature-anim)
    add_test(NAME playable-audio COMMAND champions-playable-audio-tests)
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/real_data_targets.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/script_commands_targets.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/campaign_fight_targets.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/ps2_save_targets.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/frontend_targets.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/ui_text_gaps_tests.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/hud_extras_targets.cmake")
if(BUILD_TESTING)
    add_executable(champions-hub-services-tests tests/hub_services_tests.cpp)
    target_include_directories(champions-hub-services-tests PRIVATE include)
    add_test(NAME hub-services COMMAND champions-hub-services-tests)
endif()

# Exact-vector corpora larger than 5 MB are not committed (see
# tests/vectors/README.md). Their tests are registered but disabled, and show as
# "Not Run (Disabled)", until the corpus is regenerated locally.
if(BUILD_TESTING)
    set(CHAMPIONS_OPTIONAL_VECTORS
        "arena-monsters-exact=tests/vectors/arena_monsters.tsv"
        "script-builtins-exact=tests/vectors/script_builtins.tsv.gz"
        "player-exact=tests/vectors/player.tsv.gz"
        "monsters-1-exact=tests/vectors/monsters_1.tsv.gz"
        "skills-a-exact=tests/vectors/skills_a.tsv.gz"
        "skills-b-exact=tests/vectors/skills_b.tsv.gz"
        "skills-b-transport=tests/vectors/skills_b.tsv.gz"
        "skills-c-exact=tests/vectors/skills_c.tsv.gz"
        "session-facing-exact=tests/vectors/session_facing.tsv.gz"
        "combat-runtime-2-exact=tests/vectors/combat_runtime_2.txt.gz"
        "dialog=tests/dialog_vectors.tsv")
    foreach(entry IN LISTS CHAMPIONS_OPTIONAL_VECTORS)
        string(FIND "${entry}" "=" split)
        string(SUBSTRING "${entry}" 0 ${split} test_name)
        math(EXPR split "${split} + 1")
        string(SUBSTRING "${entry}" ${split} -1 vector_path)
        if(TEST ${test_name} AND NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${vector_path}")
            set_tests_properties(${test_name} PROPERTIES DISABLED TRUE)
            message(STATUS "${test_name}: ${vector_path} absent, test disabled (tests/vectors/README.md)")
        endif()
    endforeach()
endif()
