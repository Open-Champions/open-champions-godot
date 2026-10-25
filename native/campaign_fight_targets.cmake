# The first real enemy fight: one shared World with the session Player and the
# WarBoar-family actors the level factory finishes. Included by the native tree
# and by the GDExtension after script_commands_targets.cmake.
include_guard(GLOBAL)
set(CHAMPIONS_FIGHT_ARCHIVE "")
if(TARGET champions-model)
    list(APPEND CHAMPIONS_FIGHT_ARCHIVE champions-model)
endif()
if(TARGET champions-archive)
    list(APPEND CHAMPIONS_FIGHT_ARCHIVE champions-archive)
elseif(TARGET champions-archive-binding-core)
    list(APPEND CHAMPIONS_FIGHT_ARCHIVE champions-archive-binding-core)
endif()
if(TARGET champions-script-commands AND TARGET champions-campaign-actors AND
   TARGET champions-player-services AND TARGET champions-combat-runtime AND
   TARGET champions-player-state AND TARGET champions-items AND CHAMPIONS_FIGHT_ARCHIVE)
    add_library(champions-campaign-fight "${CMAKE_CURRENT_LIST_DIR}/src/campaign_fight.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/campaign_rewards.cpp")
    target_include_directories(champions-campaign-fight PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include")
    target_link_libraries(champions-campaign-fight PUBLIC
        champions-script-commands champions-campaign-actors champions-player-services
        champions-combat-runtime champions-creature-anim champions-player-state champions-items
        ${CHAMPIONS_FIGHT_ARCHIVE})
    set_target_properties(champions-campaign-fight PROPERTIES POSITION_INDEPENDENT_CODE ON)
    if(MSVC)
        target_compile_options(champions-campaign-fight PRIVATE /W4 /utf-8 /fp:strict)
    else()
        target_compile_options(champions-campaign-fight PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
    endif()
    # Counts-only private-disc probe (never prints paths, names or bytes).
    if(TARGET champions-archive)
        add_executable(champions-campaign-fight-probe "${CMAKE_CURRENT_LIST_DIR}/src/campaign_fight_probe.cpp")
        target_link_libraries(champions-campaign-fight-probe PRIVATE champions-campaign-fight)
    endif()
endif()
if(BUILD_TESTING AND TARGET champions-campaign-actors AND TARGET champions-combat-runtime)
    add_executable(champions-campaign-fight-adopt-tests "${CMAKE_CURRENT_LIST_DIR}/tests/campaign_fight_adopt_tests.cpp")
    target_include_directories(champions-campaign-fight-adopt-tests PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src")
    target_link_libraries(champions-campaign-fight-adopt-tests PRIVATE champions-campaign-actors champions-combat-runtime)
    add_test(NAME campaign-fight-adopt COMMAND champions-campaign-fight-adopt-tests)
endif()
if(BUILD_TESTING AND TARGET champions-campaign-fight)
    add_executable(champions-campaign-rewards-tests "${CMAKE_CURRENT_LIST_DIR}/tests/campaign_rewards_tests.cpp")
    target_link_libraries(champions-campaign-rewards-tests PRIVATE champions-campaign-fight)
    add_test(NAME campaign-rewards COMMAND champions-campaign-rewards-tests)
endif()
