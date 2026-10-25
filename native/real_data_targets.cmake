# Counts-only measurement over an explicitly selected private directory.
add_executable(champions-real-data-measure src/real_data_measure.cpp)
target_link_libraries(champions-real-data-measure PRIVATE
    champions-archive champions-animation champions-animation-decoder
    champions-model champions-skinning champions-texture champions-ui-font
    champions-ui-layout champions-inventory-icons champions-native-audio
    champions-level-texture champions-world-geometry champions-terrain-file
    champions-level-flow champions-native-fx champions-fx-skills)
if(BUILD_TESTING)
    # Public checks need no game files: bad usage and an empty folder both refuse.
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/real-data-empty")
    add_test(NAME real-data-measure-usage COMMAND champions-real-data-measure --game norrath .)
    add_test(NAME real-data-measure-empty COMMAND champions-real-data-measure --game rta "${CMAKE_CURRENT_BINARY_DIR}/real-data-empty")
    set_tests_properties(real-data-measure-usage real-data-measure-empty PROPERTIES WILL_FAIL TRUE)
endif()

# WORLD -> water services and level-flow moving pieces (presentation adapters).
add_library(champions-real-world src/real_world.cpp)
target_include_directories(champions-real-world PUBLIC include)
target_link_libraries(champions-real-world PUBLIC
    champions-world-geometry champions-terrain-file champions-level-flow
    champions-creature-water)
if(BUILD_TESTING)
    add_executable(champions-player-services-tests tests/player_services_tests.cpp)
    target_link_libraries(champions-player-services-tests PRIVATE
        champions-player-services champions-real-world champions-archive)
    add_test(NAME player-services COMMAND champions-player-services-tests)
endif()
if(MSVC)
    target_compile_options(champions-real-world PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-real-world PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()
add_library(champions-model-texture-bind src/model_texture_bind.cpp)
target_include_directories(champions-model-texture-bind PUBLIC include)
target_link_libraries(champions-model-texture-bind PUBLIC champions-model champions-texture)
if(MSVC)
    target_compile_options(champions-model-texture-bind PRIVATE /W4 /utf-8 /fp:strict)
else()
    target_compile_options(champions-model-texture-bind PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()
target_link_libraries(champions-real-data-measure PRIVATE champions-real-world champions-model-texture-bind)
if(BUILD_TESTING)
    add_executable(champions-real-world-tests tests/real_world_tests.cpp)
    target_link_libraries(champions-real-world-tests PRIVATE champions-real-world)
    add_test(NAME real-world-adapter COMMAND champions-real-world-tests)
    add_executable(champions-model-texture-bind-tests tests/model_texture_bind_tests.cpp)
    target_link_libraries(champions-model-texture-bind-tests PRIVATE champions-model-texture-bind)
    add_test(NAME model-texture-bind COMMAND champions-model-texture-bind-tests)
endif()
