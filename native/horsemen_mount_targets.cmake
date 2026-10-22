# Nightmare mount contract: the research model's cases replayed on the arena model.
if(BUILD_TESTING)
  add_executable(champions-horsemen-mount-tests tests/horsemen_mount_tests.cpp)
  target_link_libraries(champions-horsemen-mount-tests PRIVATE champions-arena-monsters)
  if(MSVC)
    target_compile_options(champions-horsemen-mount-tests PRIVATE /W4 /utf-8)
  else()
    target_compile_options(champions-horsemen-mount-tests PRIVATE -Wall -Wextra -Wpedantic)
  endif()
  add_test(NAME horsemen-mount-contract COMMAND champions-horsemen-mount-tests
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/horsemen_mount.tsv")
endif()
