# Mission 1-3 creature decision kernel (research-campaign-combat fbb2e1d) and
# its synthetic tests. Included by the native tree.
include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/creature_ai_m1_3_library.cmake")
if(BUILD_TESTING)
    add_executable(champions-creature-ai-m1-3-tests "${CMAKE_CURRENT_LIST_DIR}/tests/creature_ai_m1_3_tests.cpp")
    target_link_libraries(champions-creature-ai-m1-3-tests PRIVATE champions-creature-ai-m1-3)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME creature-ai-m1-3 COMMAND "${Python3_EXECUTABLE}"
             "${CMAKE_CURRENT_LIST_DIR}/../tools/test_creature_ai_m1_3_vectors.py"
             --exe "$<TARGET_FILE:champions-creature-ai-m1-3-tests>"
             --vectors "${CMAKE_CURRENT_LIST_DIR}/tests/vectors/creature_ai_m1_3.tsv.gz"
             --work "${CMAKE_CURRENT_BINARY_DIR}/creature_ai_m1_3")
endif()
