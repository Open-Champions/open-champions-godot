# Playable integration (wave 11): native-fx-2 EffectSimulation inside the opt-in
# combat play session. Outside the simulation boundary list on purpose: the
# effect libraries use binary64 for unrounded emitter inputs.
include_guard(GLOBAL)
add_library(champions-combat-play-effects "${CMAKE_CURRENT_LIST_DIR}/src/combat_play_effects.cpp")
target_include_directories(champions-combat-play-effects PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src")
target_link_libraries(champions-combat-play-effects PUBLIC champions-combat-play champions-fx-sim)
if(MSVC)
    target_compile_options(champions-combat-play-effects PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-combat-play-effects PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()
