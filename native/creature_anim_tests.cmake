if(BUILD_TESTING)
  add_executable(champions-creature-anim-tests tests/creature_anim_tests.cpp)
  target_link_libraries(champions-creature-anim-tests PRIVATE champions-creature-anim)
  add_test(NAME creature-anim-api COMMAND champions-creature-anim-tests)
  add_executable(champions-creature-anim-trace tests/creature_anim_trace.cpp)
  target_link_libraries(champions-creature-anim-trace PRIVATE champions-creature-anim)
  add_test(NAME creature-anim-exact COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_creature_anim_vectors.py"
    --exe "$<TARGET_FILE:champions-creature-anim-trace>")
  set_tests_properties(creature-anim-exact PROPERTIES TIMEOUT 600)
endif()
if(BUILD_TESTING)
  add_executable(champions-creature-dispatch-trace tests/creature_dispatch_trace.cpp)
  target_link_libraries(champions-creature-dispatch-trace PRIVATE champions-creature-anim)
  add_test(NAME creature-dispatch-exact COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_creature_dispatch.py"
    --exe "$<TARGET_FILE:champions-creature-dispatch-trace>")
  set_tests_properties(creature-dispatch-exact PROPERTIES TIMEOUT 600)
endif()
if(BUILD_TESTING)
  add_executable(champions-creature-dispatch-tests tests/creature_dispatch_tests.cpp)
  target_link_libraries(champions-creature-dispatch-tests PRIVATE champions-creature-anim)
  add_test(NAME creature-dispatch-api COMMAND champions-creature-dispatch-tests)
endif()
