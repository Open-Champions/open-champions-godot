# Nightmare mount contract: the research model's cases replayed on the arena model.
if(BUILD_TESTING)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  add_executable(champions-horsemen-mount-tests tests/horsemen_mount_tests.cpp)
  target_link_libraries(champions-horsemen-mount-tests PRIVATE champions-arena-monsters)
  if(MSVC)
    target_compile_options(champions-horsemen-mount-tests PRIVATE /W4 /utf-8)
  else()
    target_compile_options(champions-horsemen-mount-tests PRIVATE -Wall -Wextra -Wpedantic)
  endif()
  add_test(NAME horsemen-mount-contract COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../tools/run_horsemen_mount_vectors.py"
    --exe "$<TARGET_FILE:champions-horsemen-mount-tests>"
    --vectors "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/horsemen_mount.tsv.gz"
    --manifest "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/horsemen_mount.json")
endif()
