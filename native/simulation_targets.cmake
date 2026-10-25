# Shared native target definitions; no Godot dependency or tools/tests/packages.
include_guard(GLOBAL)
set(CHAMPIONS_SIMULATION_ROOT "${CMAKE_CURRENT_LIST_DIR}")
# Standalone RTA quest/profile script built-ins; remaining engine commands refuse.
add_library(champions-script-builtins "${CHAMPIONS_SIMULATION_ROOT}/src/script_builtins.cpp")
target_link_libraries(champions-script-builtins PUBLIC champions-amx champions-core champions-objects champions-creature-hp)
if(MSVC)
    target_compile_options(champions-script-builtins PRIVATE /W4 /utf-8)
else()
    target_compile_options(champions-script-builtins PRIVATE -Wall -Wextra -Wpedantic)
endif()
# RTA Python-contract game rules have no Godot, parser or viewer dependency.
# All arithmetic uses integer patterns, including software double precision.
add_library(champions-core "${CHAMPIONS_SIMULATION_ROOT}/src/core_arithmetic.cpp" "${CHAMPIONS_SIMULATION_ROOT}/src/core_math.cpp" "${CHAMPIONS_SIMULATION_ROOT}/src/core_state.cpp")
target_include_directories(champions-core PUBLIC "${CHAMPIONS_SIMULATION_ROOT}/include")
if(MSVC)
    target_compile_options(champions-core PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-core PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()

add_library(champions-animation-decoder "${CHAMPIONS_SIMULATION_ROOT}/src/animation_decoder.cpp")
target_include_directories(champions-animation-decoder PUBLIC "${CHAMPIONS_SIMULATION_ROOT}/include")
target_link_libraries(champions-animation-decoder PUBLIC champions-core)
if(MSVC)
    target_compile_options(champions-animation-decoder PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-animation-decoder PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()

# Cache-bearing lifecycle over canonical decoder cells; no Session or viewer.
add_library(champions-animation-runtime "${CHAMPIONS_SIMULATION_ROOT}/src/animation_runtime.cpp")
target_include_directories(champions-animation-runtime PUBLIC "${CHAMPIONS_SIMULATION_ROOT}/include")
target_link_libraries(champions-animation-runtime PUBLIC champions-animation-decoder)
if(MSVC)
    target_compile_options(champions-animation-runtime PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-animation-runtime PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()

# Cache-free RTA animation timing; no presentation, pose or decoder dependency.
add_library(champions-animation-clock "${CHAMPIONS_SIMULATION_ROOT}/src/animation_clock.cpp")
target_include_directories(champions-animation-clock PUBLIC "${CHAMPIONS_SIMULATION_ROOT}/include")
target_link_libraries(champions-animation-clock PUBLIC champions-core)
if(MSVC)
    target_compile_options(champions-animation-clock PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-animation-clock PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()

add_library(champions-items "${CHAMPIONS_SIMULATION_ROOT}/src/items.cpp" "${CHAMPIONS_SIMULATION_ROOT}/src/item_generation.cpp" "${CHAMPIONS_SIMULATION_ROOT}/src/item_names.cpp")
target_include_directories(champions-items PUBLIC "${CHAMPIONS_SIMULATION_ROOT}/include")
target_link_libraries(champions-items PUBLIC champions-core)
if(MSVC)
    target_compile_options(champions-items PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-items PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()

add_library(champions-combat "${CHAMPIONS_SIMULATION_ROOT}/src/combat_common.cpp" "${CHAMPIONS_SIMULATION_ROOT}/src/combat_damage.cpp" "${CHAMPIONS_SIMULATION_ROOT}/src/combat_stats.cpp")
target_include_directories(champions-combat PUBLIC "${CHAMPIONS_SIMULATION_ROOT}/include")
target_link_libraries(champions-combat PUBLIC champions-core)
add_library(champions-combat-items "${CHAMPIONS_SIMULATION_ROOT}/src/combat_items.cpp")
target_link_libraries(champions-combat-items PUBLIC champions-combat champions-items)
foreach(target champions-combat champions-combat-items)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /utf-8 /fp:strict)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
    endif()
endforeach()

add_library(champions-amx "${CHAMPIONS_SIMULATION_ROOT}/src/amx_image.cpp" "${CHAMPIONS_SIMULATION_ROOT}/src/amx_execute.cpp" "${CHAMPIONS_SIMULATION_ROOT}/src/amx_host.cpp")
target_include_directories(champions-amx PUBLIC "${CHAMPIONS_SIMULATION_ROOT}/include")
foreach(target champions-amx)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /utf-8 /fp:strict)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
    endif()
endforeach()

add_library(champions-netwire "${CHAMPIONS_SIMULATION_ROOT}/src/netwire_codec.cpp" "${CHAMPIONS_SIMULATION_ROOT}/src/netwire_session.cpp" "${CHAMPIONS_SIMULATION_ROOT}/src/netwire_tick.cpp")
target_include_directories(champions-netwire PUBLIC "${CHAMPIONS_SIMULATION_ROOT}/include")
target_link_libraries(champions-netwire PUBLIC champions-core)
if(MSVC)
    target_compile_options(champions-netwire PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-netwire PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()

# Supplied post-fixup RTA terrain contract; loading and Session movement are separate.
add_library(champions-world-queries "${CHAMPIONS_SIMULATION_ROOT}/src/world_terrain.cpp" "${CHAMPIONS_SIMULATION_ROOT}/src/world_movement.cpp" "${CHAMPIONS_SIMULATION_ROOT}/src/world_raycast.cpp")
target_include_directories(champions-world-queries PUBLIC "${CHAMPIONS_SIMULATION_ROOT}/include")
target_link_libraries(champions-world-queries PUBLIC champions-core)
if(MSVC)
    target_compile_options(champions-world-queries PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-world-queries PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()

# Standalone published object contract; Session integration is separate.
add_library(champions-objects "${CHAMPIONS_SIMULATION_ROOT}/src/objects_lifetime.cpp" "${CHAMPIONS_SIMULATION_ROOT}/src/objects_scheduler.cpp" "${CHAMPIONS_SIMULATION_ROOT}/src/objects_queries.cpp")
target_include_directories(champions-objects PUBLIC "${CHAMPIONS_SIMULATION_ROOT}/include")
target_link_libraries(champions-objects PUBLIC champions-core)
if(MSVC)
    target_compile_options(champions-objects PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-objects PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()

# Complete standalone RTA creature slot-2 contract; storage adapters are separate.
add_library(champions-creature-hp "${CHAMPIONS_SIMULATION_ROOT}/src/creature_hp.cpp")
target_include_directories(champions-creature-hp PUBLIC "${CHAMPIONS_SIMULATION_ROOT}/include")
target_link_libraries(champions-creature-hp PUBLIC champions-core)
if(MSVC)
    target_compile_options(champions-creature-hp PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-creature-hp PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()

# Original RTA creature collision/water step; callbacks and Session wiring are explicit.
add_library(champions-creature-movement "${CHAMPIONS_SIMULATION_ROOT}/src/creature_movement.cpp")
target_include_directories(champions-creature-movement PUBLIC "${CHAMPIONS_SIMULATION_ROOT}/include")
target_link_libraries(champions-creature-movement PUBLIC champions-creature-hp)
if(MSVC)
    target_compile_options(champions-creature-movement PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-creature-movement PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()

# Supplied-grid RTA water query/test/disturb with an explicit creature callback adapter.
add_library(champions-creature-water "${CHAMPIONS_SIMULATION_ROOT}/src/creature_water.cpp")
target_include_directories(champions-creature-water PUBLIC "${CHAMPIONS_SIMULATION_ROOT}/include")
target_link_libraries(champions-creature-water PUBLIC champions-creature-movement)
if(MSVC)
    target_compile_options(champions-creature-water PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-creature-water PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()

# Explicit owned combat-to-Creature storage; shared World/Session integration is separate.
add_library(champions-combat-creature "${CHAMPIONS_SIMULATION_ROOT}/src/combat_creature.cpp")
target_include_directories(champions-combat-creature PUBLIC "${CHAMPIONS_SIMULATION_ROOT}/include")
target_link_libraries(champions-combat-creature PUBLIC champions-combat champions-creature-hp)
if(MSVC)
    target_compile_options(champions-combat-creature PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-combat-creature PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()

# Explicit shared World handle pool with a readonly raw-memory graph view.
add_library(champions-raw-handles "${CHAMPIONS_SIMULATION_ROOT}/src/raw_handles.cpp")
target_include_directories(champions-raw-handles PUBLIC "${CHAMPIONS_SIMULATION_ROOT}/include")
target_link_libraries(champions-raw-handles PUBLIC champions-objects champions-creature-hp)
if(MSVC)
    target_compile_options(champions-raw-handles PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-raw-handles PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()

# Bounded unrelocated RTA world terrain projection, using the existing query core.
add_library(champions-terrain-file "${CHAMPIONS_SIMULATION_ROOT}/src/terrain_file.cpp")
target_include_directories(champions-terrain-file PUBLIC "${CHAMPIONS_SIMULATION_ROOT}/include")
target_link_libraries(champions-terrain-file PUBLIC champions-world-queries)
if(MSVC)
    target_compile_options(champions-terrain-file PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-terrain-file PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()

# Experimental composition host with an optional owned-World scheduler adapter.
add_library(champions-session "${CHAMPIONS_SIMULATION_ROOT}/src/session.cpp")
target_include_directories(champions-session PUBLIC "${CHAMPIONS_SIMULATION_ROOT}/include")
target_link_libraries(champions-session PUBLIC champions-animation-runtime champions-animation-clock champions-netwire champions-amx champions-combat-items champions-objects champions-combat-creature champions-raw-handles champions-world-queries champions-creature-movement)
if(MSVC)
    target_compile_options(champions-session PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-session PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()

# Authored single-actor command policy, independent of presentation.
add_library(champions-session-control "${CHAMPIONS_SIMULATION_ROOT}/src/session_control.cpp")
target_include_directories(champions-session-control PUBLIC "${CHAMPIONS_SIMULATION_ROOT}/include")
target_link_libraries(champions-session-control PUBLIC champions-session)
if(MSVC)
    target_compile_options(champions-session-control PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-session-control PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()

# Authored bounded native replay checkpoints, independent of presentation.
add_library(champions-native-save "${CHAMPIONS_SIMULATION_ROOT}/src/native_save.cpp")
target_link_libraries(champions-native-save PUBLIC champions-session-control)
if(MSVC)
    target_compile_options(champions-native-save PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-native-save PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()

set(CHAMPIONS_SIMULATION_TARGETS champions-core champions-animation-clock champions-animation-decoder champions-items champions-combat champions-combat-items champions-amx champions-netwire champions-world-queries champions-objects champions-creature-hp champions-creature-movement champions-combat-creature champions-raw-handles champions-terrain-file champions-session champions-session-control champions-native-save)

include("${CHAMPIONS_SIMULATION_ROOT}/messages_targets.cmake")

# Standalone bounded original RTA player entry/scripted tick and input helpers.
add_library(champions-player "${CHAMPIONS_SIMULATION_ROOT}/src/player.cpp")
target_include_directories(champions-player PUBLIC "${CHAMPIONS_SIMULATION_ROOT}/include")
target_link_libraries(champions-player PUBLIC champions-creature-hp)
if(MSVC)
    target_compile_options(champions-player PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-player PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()
list(APPEND CHAMPIONS_SIMULATION_TARGETS champions-player)

# Bounded RTA monsters-1 state/touch and part-shedding contract; no scene wiring.
add_library(champions-monsters-1 "${CHAMPIONS_SIMULATION_ROOT}/src/monsters_1.cpp")
target_include_directories(champions-monsters-1 PUBLIC "${CHAMPIONS_SIMULATION_ROOT}/include")
target_link_libraries(champions-monsters-1 PUBLIC champions-creature-hp champions-creature-movement)
if(MSVC)
    target_compile_options(champions-monsters-1 PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-monsters-1 PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()
list(APPEND CHAMPIONS_SIMULATION_TARGETS champions-monsters-1)

# Bounded RTA Skelight state contract; no scene/AI dependency.
add_library(champions-arena-monsters "${CHAMPIONS_SIMULATION_ROOT}/src/arena_monsters.cpp")
target_include_directories(champions-arena-monsters PUBLIC "${CHAMPIONS_SIMULATION_ROOT}/include")
target_link_libraries(champions-arena-monsters PUBLIC champions-creature-hp)
if(MSVC)
    target_compile_options(champions-arena-monsters PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-arena-monsters PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()
list(APPEND CHAMPIONS_SIMULATION_TARGETS champions-arena-monsters)

# Isolated campaign WarBoar/Minotaur actor boundary; no play-scene binding.
add_library(champions-campaign-actors "${CHAMPIONS_SIMULATION_ROOT}/src/campaign_actors.cpp")
target_include_directories(champions-campaign-actors PUBLIC "${CHAMPIONS_SIMULATION_ROOT}/include")
# The GDExtension compiles the same archive/resource-manager sources as
# champions-archive-binding-core and sets CHAMPIONS_ACTORS_ARCHIVE to it.
if(NOT CHAMPIONS_ACTORS_ARCHIVE)
  set(CHAMPIONS_ACTORS_ARCHIVE champions-archive)
endif()
target_link_libraries(champions-campaign-actors PUBLIC champions-objects champions-creature-hp ${CHAMPIONS_ACTORS_ARCHIVE} champions-combat)
if(MSVC)
    target_compile_options(champions-campaign-actors PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-campaign-actors PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()
list(APPEND CHAMPIONS_SIMULATION_TARGETS champions-campaign-actors)

# AntQueen, Skeleton and Soul constructors, set-states and run prologues
# (research-combat-ctors e9777eb) on the arena model.
add_library(champions-combat-ctors "${CHAMPIONS_SIMULATION_ROOT}/src/combat_ctors.cpp"
    "${CHAMPIONS_SIMULATION_ROOT}/src/combat_ctors_2.cpp"
    "${CHAMPIONS_SIMULATION_ROOT}/src/combat_ctors_3.cpp"
    "${CHAMPIONS_SIMULATION_ROOT}/src/combat_ctors_soul.cpp"
    "${CHAMPIONS_SIMULATION_ROOT}/src/combat_weapon_holder.cpp"
    "${CHAMPIONS_SIMULATION_ROOT}/src/combat_death_ctors.cpp"
    "${CHAMPIONS_SIMULATION_ROOT}/src/combat_phprop_ctors.cpp"
    "${CHAMPIONS_SIMULATION_ROOT}/src/combat_rigidbody.cpp"
    "${CHAMPIONS_SIMULATION_ROOT}/src/combat_hit_ctors.cpp"
    "${CHAMPIONS_SIMULATION_ROOT}/src/combat_staticfire.cpp"
    "${CHAMPIONS_SIMULATION_ROOT}/src/combat_minion_place.cpp"
    "${CHAMPIONS_SIMULATION_ROOT}/src/combat_phprop_run.cpp"
    "${CHAMPIONS_SIMULATION_ROOT}/src/combat_gold.cpp"
    "${CHAMPIONS_SIMULATION_ROOT}/src/combat_prop_dtors.cpp"
    "${CHAMPIONS_SIMULATION_ROOT}/src/combat_attach_matrix.cpp"
    "${CHAMPIONS_SIMULATION_ROOT}/src/combat_antwave.cpp")
target_include_directories(champions-combat-ctors PUBLIC "${CHAMPIONS_SIMULATION_ROOT}/include")
target_link_libraries(champions-combat-ctors PUBLIC champions-arena-monsters)
if(MSVC)
    target_compile_options(champions-combat-ctors PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-combat-ctors PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()
list(APPEND CHAMPIONS_SIMULATION_TARGETS champions-combat-ctors)

include("${CHAMPIONS_SIMULATION_ROOT}/skills_a_targets.cmake")

# Bounded RTA skills 35-68 common statistics/passives and base random machinery.
add_library(champions-skills-b "${CHAMPIONS_SIMULATION_ROOT}/src/skills_b.cpp")
target_include_directories(champions-skills-b PUBLIC "${CHAMPIONS_SIMULATION_ROOT}/include")
target_link_libraries(champions-skills-b PUBLIC champions-creature-hp)
if(MSVC)
    target_compile_options(champions-skills-b PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-skills-b PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()
list(APPEND CHAMPIONS_SIMULATION_TARGETS champions-skills-b)

# Standalone RTA skills 69-101 statistics and six passive helpers; casts refuse.
add_library(champions-skills-c "${CHAMPIONS_SIMULATION_ROOT}/src/skills_c.cpp")
target_include_directories(champions-skills-c PUBLIC "${CHAMPIONS_SIMULATION_ROOT}/include")
target_link_libraries(champions-skills-c PUBLIC champions-creature-hp)
if(MSVC)
    target_compile_options(champions-skills-c PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-skills-c PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()
list(APPEND CHAMPIONS_SIMULATION_TARGETS champions-skills-c)

# Existing player bodies plus closed Session recovery policy.
add_library(champions-session-recovery "${CHAMPIONS_SIMULATION_ROOT}/src/session_recovery.cpp")
target_include_directories(champions-session-recovery PUBLIC "${CHAMPIONS_SIMULATION_ROOT}/include")
target_link_libraries(champions-session-recovery PUBLIC champions-player)
if(MSVC)
    target_compile_options(champions-session-recovery PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-session-recovery PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()
target_link_libraries(champions-session PUBLIC champions-session-recovery)
list(APPEND CHAMPIONS_SIMULATION_TARGETS champions-session-recovery)

# Authored closed Controller animation policy; no new reference port.
target_sources(champions-session-control PRIVATE "${CHAMPIONS_SIMULATION_ROOT}/src/control_anim_cache.cpp")

include("${CHAMPIONS_SIMULATION_ROOT}/own_machines_targets.cmake")
include("${CHAMPIONS_SIMULATION_ROOT}/combat_runtime_targets.cmake")

# Explicit authored multi-actor fixture over the released combat adapter.
add_library(champions-combat-play "${CHAMPIONS_SIMULATION_ROOT}/src/combat_play.cpp")
target_link_libraries(champions-combat-play PUBLIC champions-combat-runtime champions-session-control)
if(MSVC)
    target_compile_options(champions-combat-play PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-combat-play PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()
list(APPEND CHAMPIONS_SIMULATION_TARGETS champions-combat-play)

include("${CHAMPIONS_SIMULATION_ROOT}/creature_anim_targets.cmake")

target_link_libraries(champions-combat-runtime PUBLIC champions-own-machines champions-creature-anim champions-objects)

# Borrowed production Player callbacks; the Full fixture retains its own
# explicit synthetic services. No World ownership is changed here.
add_library(champions-player-services "${CHAMPIONS_SIMULATION_ROOT}/src/player_services.cpp")
target_include_directories(champions-player-services PUBLIC "${CHAMPIONS_SIMULATION_ROOT}/include")
target_link_libraries(champions-player-services PUBLIC champions-combat-runtime champions-world-queries champions-creature-anim champions-creature-water)
if(MSVC)
  target_compile_options(champions-player-services PRIVATE /W4 /utf-8 /fp:strict)
else()
  target_compile_options(champions-player-services PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()
list(APPEND CHAMPIONS_SIMULATION_TARGETS champions-player-services)

target_sources(champions-combat-play PRIVATE "${CHAMPIONS_SIMULATION_ROOT}/src/play_animation.cpp")
target_link_libraries(champions-combat-play PUBLIC champions-creature-anim)

# Lockstep session over the ported wire model. Loopback only.
add_library(champions-netplay "${CHAMPIONS_SIMULATION_ROOT}/src/netplay.cpp")
target_include_directories(champions-netplay PUBLIC "${CHAMPIONS_SIMULATION_ROOT}/include")
target_link_libraries(champions-netplay PUBLIC champions-combat-runtime champions-netwire champions-world-queries)
if(MSVC)
    target_compile_options(champions-netplay PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-netplay PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()
list(APPEND CHAMPIONS_SIMULATION_TARGETS champions-netplay)
