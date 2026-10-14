if(BUILD_TESTING)
  add_executable(champions-session-facing-tests tests/session_facing_tests.cpp)
  target_link_libraries(champions-session-facing-tests PRIVATE champions-native-save)
  add_executable(champions-session-facing-trace tests/session_facing_trace.cpp)
  target_link_libraries(champions-session-facing-trace PRIVATE champions-native-save)
  add_test(NAME session-facing-api COMMAND champions-session-facing-tests)
  add_test(NAME session-facing-exact COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_session_facing_vectors.py"
    --exe "$<TARGET_FILE:champions-session-facing-trace>"
    --vectors "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/session_facing.tsv.gz")
endif()
