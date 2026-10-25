add_executable(champions-skills-a-trace tools/skills_a_trace.cpp)
add_executable(champions-skills-a-tests tests/skills_a_tests.cpp)
add_executable(champions-skills-a-ctor-tests tests/skills_a_ctor_tests.cpp)
target_link_libraries(champions-skills-a-ctor-tests PRIVATE champions-skills-a)
foreach(target champions-skills-a-trace champions-skills-a-tests)
    target_link_libraries(${target} PRIVATE champions-skills-a)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /utf-8 /fp:strict)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
    endif()
endforeach()
add_test(NAME skills-a-api COMMAND champions-skills-a-tests)
add_test(NAME skills-a-ctor COMMAND champions-skills-a-ctor-tests)
add_test(NAME skills-a-exact COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_skills_a_vectors.py"
    --native "$<TARGET_FILE:champions-skills-a-trace>"
    --vectors "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/skills_a.tsv.gz"
    --manifest "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/skills_a.json"
    --trace-dir "${CMAKE_CURRENT_BINARY_DIR}/core-traces")
set_tests_properties(skills-a-exact PROPERTIES TIMEOUT 300)
