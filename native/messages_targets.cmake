# Standalone RTA game-message transport subset. No Godot or other port dependency.
add_library(champions-messages "${CHAMPIONS_SIMULATION_ROOT}/src/messages.cpp")
target_include_directories(champions-messages PUBLIC "${CHAMPIONS_SIMULATION_ROOT}/include")
if(MSVC)
    target_compile_options(champions-messages PRIVATE /W4 /utf-8)
else()
    target_compile_options(champions-messages PRIVATE -Wall -Wextra -Wpedantic)
endif()
list(APPEND CHAMPIONS_SIMULATION_TARGETS champions-messages)
