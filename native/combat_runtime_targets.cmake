add_library(champions-combat-runtime
    "${CHAMPIONS_SIMULATION_ROOT}/src/combat_runtime_world.cpp"
    "${CHAMPIONS_SIMULATION_ROOT}/src/combat_runtime_services.cpp"
    "${CHAMPIONS_SIMULATION_ROOT}/src/combat_runtime_queries.cpp"
    "${CHAMPIONS_SIMULATION_ROOT}/src/combat_runtime_damage.cpp"
    "${CHAMPIONS_SIMULATION_ROOT}/src/combat_runtime_player.cpp"
    "${CHAMPIONS_SIMULATION_ROOT}/src/session_combat_runtime_adapter.cpp")
target_link_libraries(champions-combat-runtime PUBLIC champions-arena-monsters champions-creature-movement champions-combat)
if(MSVC)
    target_compile_options(champions-combat-runtime PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-combat-runtime PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()
list(APPEND CHAMPIONS_SIMULATION_TARGETS champions-combat-runtime)
