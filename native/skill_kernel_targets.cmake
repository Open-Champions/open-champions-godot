# Header-only RTA primitives; adapters and their existing dependencies own state.
add_executable(champions-skill-kernel-tests tests/skill_kernel_tests.cpp)
target_link_libraries(champions-skill-kernel-tests PRIVATE champions-skills-a champions-skills-b champions-skills-c)
if(MSVC)
    target_compile_options(champions-skill-kernel-tests PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-skill-kernel-tests PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()
add_test(NAME skill-kernel-adapters COMMAND champions-skill-kernel-tests)
add_test(NAME skill-kernel-exact COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_skill_kernel_vectors.py"
    --exe "$<TARGET_FILE:champions-skill-kernel-tests>"
    --vectors "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/skill_kernel.tsv.gz")

add_test(NAME skills-range-kernel-exact COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_skills_range_kernel_vectors.py"
    --exe "$<TARGET_FILE:champions-skill-kernel-tests>"
    --vectors "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/skills_range_kernel.tsv.gz")

add_test(NAME skills-handle-kernel-adapters COMMAND champions-skill-kernel-tests --handle-host)
add_test(NAME skills-handle-kernel-exact COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_skills_handle_kernel_vectors.py"
    --exe "$<TARGET_FILE:champions-skill-kernel-tests>"
    --vectors "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/skills_handle_kernel.tsv.gz")

add_test(NAME skills-spell-kernel-adapters COMMAND champions-skill-kernel-tests --spell-host)
add_test(NAME skills-spell-kernel-exact COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_skills_spell_kernel_vectors.py"
    --exe "$<TARGET_FILE:champions-skill-kernel-tests>"
    --vectors "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/skills_spell_kernel.tsv.gz")
