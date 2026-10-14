# Closed authored Controller policy; historical suite contracts are unchanged.
if(BUILD_TESTING)
  foreach(kind tests trace legacy-bytes)
    if(kind STREQUAL "legacy-bytes")
      set(source tests/control_anim_cache_legacy_bytes.cpp)
    elseif(kind STREQUAL "trace")
      set(source tests/control_anim_cache_trace.cpp)
    else()
      set(source tests/control_anim_cache_tests.cpp)
    endif()
    add_executable(champions-control-anim-cache-${kind} ${source})
    target_link_libraries(champions-control-anim-cache-${kind} PRIVATE champions-native-save)
    if(MSVC)
      target_compile_options(champions-control-anim-cache-${kind} PRIVATE /W4 /utf-8 /fp:strict)
    else()
      target_compile_options(champions-control-anim-cache-${kind} PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
    endif()
  endforeach()
  if(MINGW)
    target_link_options(champions-control-anim-cache-trace PRIVATE -municode)
    target_link_options(champions-control-anim-cache-legacy-bytes PRIVATE -municode)
  endif()
  add_test(NAME control_anim_cache-legacy-bytes COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_control_anim_cache_legacy_bytes.py"
    --exe "$<TARGET_FILE:champions-control-anim-cache-legacy-bytes>")
  add_test(NAME control_anim_cache-api COMMAND champions-control-anim-cache-tests)
  add_test(NAME control_anim_cache-exact COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_control_anim_cache_vectors.py"
    --exe "$<TARGET_FILE:champions-control-anim-cache-trace>"
    --vectors "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/control_anim_cache.tsv.gz")
  add_test(NAME control_anim_cache-provenance COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_control_anim_cache_contract.py")
endif()
