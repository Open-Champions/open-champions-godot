# The mission 1-3 creature decision kernel library alone (research-campaign-combat
# fbb2e1d). Included from simulation_targets.cmake (blood1 actors),
# campaign_fight_targets.cmake (mission-2 actors) and creature_ai_m1_3_targets.cmake,
# so the native tree and the GDExtension both define it once.
if(NOT TARGET champions-creature-ai-m1-3)
  add_library(champions-creature-ai-m1-3 "${CMAKE_CURRENT_LIST_DIR}/src/creature_ai_m1_3.cpp")
  target_include_directories(champions-creature-ai-m1-3 PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include")
  target_link_libraries(champions-creature-ai-m1-3 PUBLIC champions-core)
  set_target_properties(champions-creature-ai-m1-3 PROPERTIES POSITION_INDEPENDENT_CODE ON)
  if(MSVC)
    target_compile_options(champions-creature-ai-m1-3 PRIVATE /W4 /utf-8 /fp:strict)
  else()
    target_compile_options(champions-creature-ai-m1-3 PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
  endif()
endif()
