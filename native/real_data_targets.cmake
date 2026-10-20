# Counts-only measurement over an explicitly selected private directory.
add_executable(champions-real-data-measure src/real_data_measure.cpp)
target_link_libraries(champions-real-data-measure PRIVATE
    champions-archive champions-animation champions-animation-decoder
    champions-model champions-skinning champions-texture champions-ui-font
    champions-ui-layout champions-inventory-icons champions-native-audio
    champions-level-texture champions-world-geometry champions-terrain-file
    champions-level-flow champions-native-fx champions-fx-skills)
