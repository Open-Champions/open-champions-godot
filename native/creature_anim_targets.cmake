# Isolated pose/root/SmallSpider implementation; existing Runtime is untouched.
add_library(champions-creature-anim
    "${CHAMPIONS_SIMULATION_ROOT}/src/creature_anim/runtime.cpp"
    "${CHAMPIONS_SIMULATION_ROOT}/src/creature_anim/pose.cpp"
    "${CHAMPIONS_SIMULATION_ROOT}/src/creature_anim/sound.cpp"
    "${CHAMPIONS_SIMULATION_ROOT}/src/creature_anim/creature.cpp"
    "${CHAMPIONS_SIMULATION_ROOT}/src/creature_anim/dispatch.cpp"
    "${CHAMPIONS_SIMULATION_ROOT}/src/creature_anim/dispatch_data.cpp"
    "${CHAMPIONS_SIMULATION_ROOT}/src/creature_anim/player_dispatch.cpp")
target_link_libraries(champions-creature-anim PUBLIC champions-animation-decoder champions-monsters-1)
target_include_directories(champions-creature-anim PUBLIC "${CHAMPIONS_SIMULATION_ROOT}/include")
if(MSVC)
  target_compile_options(champions-creature-anim PRIVATE /W4 /utf-8 /fp:strict)
else()
  target_compile_options(champions-creature-anim PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()
list(APPEND CHAMPIONS_SIMULATION_TARGETS champions-creature-anim)
