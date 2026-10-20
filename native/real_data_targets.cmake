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
