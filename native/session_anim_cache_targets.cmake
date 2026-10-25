# Additive exact/API registrations for the explicit owned Session cache policy.
add_executable(champions-session-anim-cache-tests tests/session_anim_cache_tests.cpp)
add_executable(champions-session-anim-cache-trace tools/session_anim_cache_trace.cpp)
foreach(target champions-session-anim-cache-tests champions-session-anim-cache-trace)
  target_link_libraries(${target} PRIVATE champions-session)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /utf-8 /fp:strict)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
  endif()
endforeach()
if(MINGW)
  target_link_options(champions-session-anim-cache-trace PRIVATE -municode)
endif()
add_test(NAME session-anim-cache-api COMMAND champions-session-anim-cache-tests)
add_test(NAME session-anim-cache-exact COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/../tools/run_core_vectors.py"
  --suite session-anim-cache --exe "$<TARGET_FILE:champions-session-anim-cache-trace>"
  --vectors "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/session_anim_cache.tsv.gz"
  --manifest "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/session_anim_cache.json"
  --trace-dir "${CMAKE_CURRENT_BINARY_DIR}/core-traces"
  --platform "${CHAMPIONS_TEST_PLATFORM}" --source-commit "${CHAMPIONS_SOURCE_COMMIT}")

add_test(NAME session-anim-cache-provenance COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_session_anim_cache_contract.py")
add_test(NAME session-anim-cache-transport COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_session_anim_cache_trace.py"
  --native "$<TARGET_FILE:champions-session-anim-cache-trace>")
add_test(NAME session-anim-cache-generation COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_session_anim_cache_generation.py")
