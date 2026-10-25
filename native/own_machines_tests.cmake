if(BUILD_TESTING)
  add_executable(champions-own-machines-tests tests/own_machines_tests.cpp)
  target_link_libraries(champions-own-machines-tests PRIVATE champions-own-machines)
  add_test(NAME own-machines-exact COMMAND champions-own-machines-tests
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/own_machines.txt")
endif()
