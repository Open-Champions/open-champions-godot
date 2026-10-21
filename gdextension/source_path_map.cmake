# Keep local checkout paths out of release libraries, including paths expanded
# by __FILE__ in the native core and godot-cpp headers. This only changes
# diagnostic file names; it does not change game logic or binary interfaces.
if(CMAKE_CXX_COMPILER_ID MATCHES "^(Clang|GNU)$")
    file(REAL_PATH "${CMAKE_CURRENT_LIST_DIR}/.." CHAMPIONS_SOURCE_ROOT)
    file(REAL_PATH "${GODOT_CPP_DIR}" CHAMPIONS_GODOT_CPP_ROOT)
    file(REAL_PATH "${CMAKE_BINARY_DIR}" CHAMPIONS_BUILD_ROOT)
    add_compile_options(
        "-ffile-prefix-map=${CHAMPIONS_SOURCE_ROOT}=open-champions"
        "-ffile-prefix-map=${CHAMPIONS_GODOT_CPP_ROOT}=godot-cpp"
        "-ffile-prefix-map=${CHAMPIONS_BUILD_ROOT}=build")
endif()
