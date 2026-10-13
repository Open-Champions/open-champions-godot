add_executable(champions-messages-tests tests/messages_tests.cpp)
add_executable(champions-messages-trace tools/messages_trace.cpp)
foreach(target champions-messages-tests champions-messages-trace)
    target_link_libraries(${target} PRIVATE champions-messages)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
add_test(NAME messages-api COMMAND champions-messages-tests)
add_test(NAME messages-exact COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_messages_vectors.py"
    --native "$<TARGET_FILE:champions-messages-trace>")
add_test(NAME messages-integrity COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_messages_vectors.py"
    --native "$<TARGET_FILE:champions-messages-trace>" --integrity)
set_tests_properties(messages-exact PROPERTIES TIMEOUT 150)
