add_library(champions-combat-runtime
    "${CHAMPIONS_SIMULATION_ROOT}/src/combat_runtime_world.cpp"
    "${CHAMPIONS_SIMULATION_ROOT}/src/combat_runtime_horsemen.cpp"
    "${CHAMPIONS_SIMULATION_ROOT}/src/combat_runtime_services.cpp"
    "${CHAMPIONS_SIMULATION_ROOT}/src/combat_runtime_queries.cpp"
    "${CHAMPIONS_SIMULATION_ROOT}/src/combat_runtime_terrain.cpp"
    "${CHAMPIONS_SIMULATION_ROOT}/src/combat_runtime_damage.cpp"
    "${CHAMPIONS_SIMULATION_ROOT}/src/combat_runtime_player.cpp"
    "${CHAMPIONS_SIMULATION_ROOT}/src/combat_runtime_api.cpp"
    "${CHAMPIONS_SIMULATION_ROOT}/src/combat_runtime_adopt.cpp"
    "${CHAMPIONS_SIMULATION_ROOT}/src/combat_runtime_move.cpp"
    "${CHAMPIONS_SIMULATION_ROOT}/src/combat_runtime_melee.cpp"
    "${CHAMPIONS_SIMULATION_ROOT}/src/combat_runtime_defence.cpp"
    "${CHAMPIONS_SIMULATION_ROOT}/src/combat_runtime_tick.cpp"
    "${CHAMPIONS_SIMULATION_ROOT}/src/combat_runtime_ranged.cpp"
    "${CHAMPIONS_SIMULATION_ROOT}/src/combat_runtime_monsters.cpp"
    "${CHAMPIONS_SIMULATION_ROOT}/src/combat_runtime_spells.cpp"
    "${CHAMPIONS_SIMULATION_ROOT}/src/session_combat_runtime_adapter.cpp"
    "${CHAMPIONS_SIMULATION_ROOT}/src/combat_runtime_effect_hooks.cpp"
    "${CHAMPIONS_SIMULATION_ROOT}/src/combat_runtime_own.cpp"
    "${CHAMPIONS_SIMULATION_ROOT}/src/combat_runtime_round3.cpp"
    "${CHAMPIONS_SIMULATION_ROOT}/src/combat_runtime_round4.cpp"
    "${CHAMPIONS_SIMULATION_ROOT}/src/combat_runtime_round5.cpp"
    "${CHAMPIONS_SIMULATION_ROOT}/src/combat_runtime_potions.cpp"
    "${CHAMPIONS_SIMULATION_ROOT}/src/combat_runtime_round6.cpp")
target_link_libraries(champions-combat-runtime PUBLIC champions-arena-monsters champions-combat-ctors champions-creature-movement champions-combat champions-player champions-world-queries)
if(MSVC)
    target_compile_options(champions-combat-runtime PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-combat-runtime PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()
list(APPEND CHAMPIONS_SIMULATION_TARGETS champions-combat-runtime)
