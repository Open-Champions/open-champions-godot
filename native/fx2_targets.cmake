# native-fx-2: exact (executed-contract) RTA effects. No Godot dependency.
# Each part is its own library so a part can be built and tested alone.
include_guard(GLOBAL)
set(CHAMPIONS_FX2_ROOT "${CMAKE_CURRENT_LIST_DIR}")
function(champions_fx2_options target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /utf-8 /fp:strict)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
    endif()
endfunction()

# Pools, definitions, emitters, decals and fire props (ref_particles, ref_decals, ref_fx_props).
add_library(champions-fx-exact
    "${CHAMPIONS_FX2_ROOT}/src/fx/exact_particles.cpp"
    "${CHAMPIONS_FX2_ROOT}/src/fx/exact_decals.cpp"
    "${CHAMPIONS_FX2_ROOT}/src/fx/exact_props.cpp")
target_include_directories(champions-fx-exact PUBLIC "${CHAMPIONS_FX2_ROOT}/include")
target_link_libraries(champions-fx-exact PUBLIC champions-core)
champions_fx2_options(champions-fx-exact)

# What the effect drawers send to the GS (ref_fx_render): particles, groups, trails, beams, glows.
add_library(champions-fx-render "${CHAMPIONS_FX2_ROOT}/src/fx/render.cpp")
target_include_directories(champions-fx-render PUBLIC "${CHAMPIONS_FX2_ROOT}/include")
target_link_libraries(champions-fx-render PUBLIC champions-core)
champions_fx2_options(champions-fx-render)

# One-shot, death and status recipes (ref_fx_recipes).
add_library(champions-fx-recipes "${CHAMPIONS_FX2_ROOT}/src/fx/recipes.cpp")
target_include_directories(champions-fx-recipes PUBLIC "${CHAMPIONS_FX2_ROOT}/include")
target_link_libraries(champions-fx-recipes PUBLIC champions-core)
champions_fx2_options(champions-fx-recipes)

# Skill effect classes (ref_fx_skills): call-outs, recipe rows, the 21 round-2 functions.
add_library(champions-fx-skills
    "${CHAMPIONS_FX2_ROOT}/src/fx/skills_fx.cpp"
    "${CHAMPIONS_FX2_ROOT}/src/fx/skill_recipes_json.cpp")
target_include_directories(champions-fx-skills PUBLIC "${CHAMPIONS_FX2_ROOT}/include")
target_link_libraries(champions-fx-skills PUBLIC champions-core)
champions_fx2_options(champions-fx-skills)

# Simulation-side effect API (generators, damage, light effects) and the renderer state.
add_library(champions-fx-sim "${CHAMPIONS_FX2_ROOT}/src/fx/simulation.cpp")
target_include_directories(champions-fx-sim PUBLIC "${CHAMPIONS_FX2_ROOT}/include")
target_link_libraries(champions-fx-sim PUBLIC champions-fx-exact champions-fx-render
    champions-fx-recipes champions-fx-skills champions-native-fx)
champions_fx2_options(champions-fx-sim)

if(BUILD_TESTING)
    foreach(part exact render recipes skills sim)
        add_executable(champions-fx-${part}-tests "${CHAMPIONS_FX2_ROOT}/tests/fx_${part}_tests.cpp")
        target_link_libraries(champions-fx-${part}-tests PRIVATE champions-fx-${part})
        champions_fx2_options(champions-fx-${part}-tests)
        if(part STREQUAL "exact")
            target_link_libraries(champions-fx-exact-tests PRIVATE champions-native-fx)
        endif()
        add_test(NAME fx-${part} COMMAND champions-fx-${part}-tests
            "${CHAMPIONS_FX2_ROOT}/tests/vectors")
        set_tests_properties(fx-${part} PROPERTIES LABELS "fx;native-fx-2")
    endforeach()
endif()
