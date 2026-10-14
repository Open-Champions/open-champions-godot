if(BUILD_TESTING)
  add_executable(champions-session-recovery-tests tests/session_recovery_tests.cpp)
  target_link_libraries(champions-session-recovery-tests PRIVATE champions-native-save)
  add_executable(champions-session-recovery-trace tests/session_recovery_trace.cpp)
  target_link_libraries(champions-session-recovery-trace PRIVATE champions-native-save)
  add_test(NAME session-recovery-api COMMAND champions-session-recovery-tests)
  add_test(NAME session-recovery-exact COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_session_recovery_vectors.py"
    --exe "$<TARGET_FILE:champions-session-recovery-trace>"
    --vectors "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/session_recovery.tsv.gz")
endif()
