# Cross-compile x86_64 Linux from macOS with Zig's bundled glibc 2.28 sysroot.
# Matches the official Godot 4.5.2 Linux export template's maximum GLIBC symbol.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(_CHAMPIONS_ZIG "${CMAKE_CURRENT_LIST_DIR}")
set(CMAKE_C_COMPILER "${_CHAMPIONS_ZIG}/zig-cc-linux-x64")
set(CMAKE_CXX_COMPILER "${_CHAMPIONS_ZIG}/zig-cxx-linux-x64")
set(CMAKE_AR "${_CHAMPIONS_ZIG}/zig-ar" CACHE FILEPATH "Zig archiver" FORCE)
set(CMAKE_RANLIB "${_CHAMPIONS_ZIG}/zig-ranlib" CACHE FILEPATH "Zig ranlib" FORCE)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
