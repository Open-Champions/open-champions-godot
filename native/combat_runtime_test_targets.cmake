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
