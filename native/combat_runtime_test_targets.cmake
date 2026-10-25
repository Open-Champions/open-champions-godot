if(BUILD_TESTING)
    add_executable(champions-horsemen-runtime-tests tests/horsemen_runtime_tests.cpp)
    target_link_libraries(champions-horsemen-runtime-tests PRIVATE champions-combat-play)
    add_test(NAME horsemen-runtime-fight COMMAND champions-horsemen-runtime-tests)
endif()
if(BUILD_TESTING)
    add_executable(champions-combat-runtime-tests tests/combat_runtime_tests.cpp)
    target_link_libraries(champions-combat-runtime-tests PRIVATE champions-combat-runtime)
    add_test(NAME combat-runtime-host COMMAND champions-combat-runtime-tests)
    add_test(NAME combat-runtime-exact COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_combat_runtime_vectors.py"
        "$<TARGET_FILE:champions-combat-runtime-tests>"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/combat_runtime.txt.gz")
    set_tests_properties(combat-runtime-exact PROPERTIES TIMEOUT 240)
endif()
if(BUILD_TESTING)
    add_executable(champions-combat-runtime-2-tests tests/combat_runtime_2_tests.cpp)
    target_link_libraries(champions-combat-runtime-2-tests PRIVATE champions-combat-runtime)
    add_test(NAME combat-runtime-2-host COMMAND champions-combat-runtime-2-tests)
    add_test(NAME combat-runtime-2-exact COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_combat_runtime_2_vectors.py"
        "$<TARGET_FILE:champions-combat-runtime-2-tests>"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/combat_runtime_2.txt.gz")
    set_tests_properties(combat-runtime-2-exact PROPERTIES TIMEOUT 600)
endif()
if(BUILD_TESTING)
    add_executable(champions-combat-runtime-3-tests tests/combat_runtime_3_tests.cpp)
    target_link_libraries(champions-combat-runtime-3-tests PRIVATE champions-combat-runtime)
    add_test(NAME combat-runtime-3-host COMMAND champions-combat-runtime-3-tests)
    add_test(NAME combat-runtime-3-exact COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_combat_runtime_3_vectors.py"
        "$<TARGET_FILE:champions-combat-runtime-3-tests>"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/combat_runtime_3.txt.gz")
    set_tests_properties(combat-runtime-3-exact PROPERTIES TIMEOUT 600)
endif()
if(BUILD_TESTING)
    add_executable(champions-combat-runtime-4-tests tests/combat_runtime_4_tests.cpp)
    target_link_libraries(champions-combat-runtime-4-tests PRIVATE champions-combat-runtime)
    add_test(NAME combat-runtime-4-host COMMAND champions-combat-runtime-4-tests)
    add_test(NAME combat-runtime-4-exact COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_combat_runtime_4_vectors.py"
        "$<TARGET_FILE:champions-combat-runtime-4-tests>"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/combat_runtime_4.txt.gz")
    set_tests_properties(combat-runtime-4-exact PROPERTIES TIMEOUT 600)
endif()
if(BUILD_TESTING)
    add_executable(champions-combat-runtime-5-tests tests/combat_runtime_5_tests.cpp)
    target_link_libraries(champions-combat-runtime-5-tests PRIVATE champions-combat-runtime)
    add_test(NAME combat-runtime-5-host COMMAND champions-combat-runtime-5-tests)
    add_test(NAME combat-runtime-5-exact COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_combat_runtime_5_vectors.py"
        "$<TARGET_FILE:champions-combat-runtime-5-tests>"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/combat_runtime_5.txt.gz")
    set_tests_properties(combat-runtime-5-exact PROPERTIES TIMEOUT 600)
endif()
if(BUILD_TESTING)
    add_executable(champions-combat-runtime-6-tests tests/combat_runtime_6_tests.cpp)
    target_link_libraries(champions-combat-runtime-6-tests PRIVATE champions-combat-runtime)
    add_test(NAME combat-runtime-6-host COMMAND champions-combat-runtime-6-tests)
    add_test(NAME combat-runtime-6-exact COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_combat_runtime_6_vectors.py"
        "$<TARGET_FILE:champions-combat-runtime-6-tests>"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/combat_runtime_6.txt.gz")
    set_tests_properties(combat-runtime-6-exact PROPERTIES TIMEOUT 600)
endif()
if(BUILD_TESTING)
    add_executable(champions-player-wading-tests tests/player_wading_tests.cpp)
    target_link_libraries(champions-player-wading-tests PRIVATE champions-creature-water)
    if(MSVC)
        target_compile_options(champions-player-wading-tests PRIVATE /W4 /utf-8 /fp:strict)
    else()
        target_compile_options(champions-player-wading-tests PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
    endif()
    add_test(NAME player-wading-exact COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_player_wading_vectors.py"
        "$<TARGET_FILE:champions-player-wading-tests>"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/player_wading.txt.gz")
    set_tests_properties(player-wading-exact PROPERTIES TIMEOUT 120)
endif()
if(BUILD_TESTING)
    add_executable(champions-player-wading-runtime-tests tests/player_wading_runtime_tests.cpp)
    target_link_libraries(champions-player-wading-runtime-tests PRIVATE champions-combat-runtime champions-creature-water)
    add_test(NAME player-wading-runtime COMMAND champions-player-wading-runtime-tests)
endif()
