add_library(champions-own-machines
    "${CHAMPIONS_SIMULATION_ROOT}/src/own_machines.cpp")
target_include_directories(champions-own-machines PUBLIC "${CHAMPIONS_SIMULATION_ROOT}/include")
if(MSVC)
  target_compile_options(champions-own-machines PRIVATE /W4 /utf-8 /fp:strict)
else()
  target_compile_options(champions-own-machines PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()
list(APPEND CHAMPIONS_SIMULATION_TARGETS champions-own-machines)
