cmake_minimum_required(VERSION 3.21)
project(champions_native_data VERSION 0.1.0 LANGUAGES CXX)

# C++17 retains std::string UTF-8 filesystem conversion across all targets.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CHAMPIONS_SOURCE_COMMIT "unrecorded" CACHE STRING "Source revision recorded in binary packages")
set(CHAMPIONS_TEST_PLATFORM "${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}" CACHE STRING "Platform identity for exact traces")

# RTA Python-contract game rules have no Godot, parser or viewer dependency.
# All arithmetic uses integer patterns, including software double precision.
add_library(champions-core src/core_arithmetic.cpp src/core_math.cpp src/core_state.cpp)
target_include_directories(champions-core PUBLIC include)
if(MSVC)
    target_compile_options(champions-core PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-core PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()

add_library(champions-items src/items.cpp src/item_generation.cpp src/item_names.cpp)
target_include_directories(champions-items PUBLIC include)
target_link_libraries(champions-items PUBLIC champions-core)
if(MSVC)
    target_compile_options(champions-items PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-items PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()

add_library(champions-combat src/combat_common.cpp src/combat_damage.cpp src/combat_stats.cpp)
target_include_directories(champions-combat PUBLIC include)
target_link_libraries(champions-combat PUBLIC champions-core)
add_library(champions-combat-items src/combat_items.cpp)
target_link_libraries(champions-combat-items PUBLIC champions-combat champions-items)
foreach(target champions-combat champions-combat-items)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /utf-8 /fp:strict)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
    endif()
endforeach()

add_library(champions-amx src/amx_image.cpp src/amx_execute.cpp src/amx_host.cpp)
target_include_directories(champions-amx PUBLIC include)
add_executable(champions-amx-validate tools/amx_validate.cpp)
target_link_libraries(champions-amx-validate PRIVATE champions-amx)
foreach(target champions-amx champions-amx-validate)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /utf-8 /fp:strict)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
    endif()
endforeach()
if(MINGW)
    target_link_options(champions-amx-validate PRIVATE -municode)
endif()

add_library(champions-netwire src/netwire_codec.cpp src/netwire_session.cpp src/netwire_tick.cpp)
target_include_directories(champions-netwire PUBLIC include)
target_link_libraries(champions-netwire PUBLIC champions-core)
if(MSVC)
    target_compile_options(champions-netwire PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-netwire PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()

# Supplied post-fixup RTA terrain contract; loading and Session movement are separate.
add_library(champions-world-queries src/world_terrain.cpp src/world_movement.cpp)
target_include_directories(champions-world-queries PUBLIC include)
target_link_libraries(champions-world-queries PUBLIC champions-core)
if(MSVC)
    target_compile_options(champions-world-queries PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-world-queries PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()

# Standalone published object contract; Session integration is separate.
add_library(champions-objects src/objects_lifetime.cpp src/objects_scheduler.cpp src/objects_queries.cpp)
target_include_directories(champions-objects PUBLIC include)
target_link_libraries(champions-objects PUBLIC champions-core)
if(MSVC)
    target_compile_options(champions-objects PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-objects PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()

# Complete standalone RTA creature slot-2 contract; storage adapters are separate.
add_library(champions-creature-hp src/creature_hp.cpp)
target_include_directories(champions-creature-hp PUBLIC include)
target_link_libraries(champions-creature-hp PUBLIC champions-core)
if(MSVC)
    target_compile_options(champions-creature-hp PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-creature-hp PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()

# Explicit owned combat-to-Creature storage; shared World/Session integration is separate.
add_library(champions-combat-creature src/combat_creature.cpp)
target_include_directories(champions-combat-creature PUBLIC include)
target_link_libraries(champions-combat-creature PUBLIC champions-combat champions-creature-hp)
if(MSVC)
    target_compile_options(champions-combat-creature PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-combat-creature PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()

# Explicit shared World handle pool with a readonly raw-memory graph view.
add_library(champions-raw-handles src/raw_handles.cpp)
target_include_directories(champions-raw-handles PUBLIC include)
target_link_libraries(champions-raw-handles PUBLIC champions-objects champions-creature-hp)
if(MSVC)
    target_compile_options(champions-raw-handles PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-raw-handles PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()

# Bounded unrelocated RTA world terrain projection, using the existing query core.
add_library(champions-terrain-file src/terrain_file.cpp)
target_include_directories(champions-terrain-file PUBLIC include)
target_link_libraries(champions-terrain-file PUBLIC champions-world-queries)
if(MSVC)
    target_compile_options(champions-terrain-file PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-terrain-file PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()

# Experimental composition host with an optional owned-World scheduler adapter.
add_library(champions-session src/session.cpp)
target_include_directories(champions-session PUBLIC include)
target_link_libraries(champions-session PUBLIC champions-netwire champions-amx champions-combat-items champions-objects champions-combat-creature champions-raw-handles champions-world-queries)
if(MSVC)
    target_compile_options(champions-session PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-session PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()

add_library(champions-archive src/archive.cpp)
target_include_directories(champions-archive PUBLIC include)
add_library(champions-texture src/texture.cpp)
target_include_directories(champions-texture PUBLIC include)
add_library(champions-animation src/animation.cpp)
target_include_directories(champions-animation PUBLIC include)
add_library(champions-model src/model.cpp)
target_include_directories(champions-model PUBLIC include)
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
target_link_libraries(champions-data-validate PRIVATE champions-archive champions-animation champions-texture champions-model champions-terrain-file)
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
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    find_package(Threads REQUIRED)
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
