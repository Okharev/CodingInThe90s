# Minimum CMake version required
cmake_minimum_required(VERSION 4.0)

# Project definition
project(MyC23Project LANGUAGES C)

# Set the C standard to C23
set(CMAKE_C_STANDARD 23)

set (CMAKE_C_FLAGS "-march=native -mtune=native -ffast-math -fopenmp-simd -lm")

# Include the FetchContent module for managing dependencies
include(FetchContent)

# --- Dependency Declarations ---

# 1. simde (SIMD Everywhere) - A header-only library
FetchContent_Declare(
        simde
        GIT_REPOSITORY https://github.com/simd-everywhere/simde.git
        GIT_TAG v0.8.2 # Use a specific, stable version
)

# 2. raylib - A simple and easy-to-use library for videogames programming
FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 5.5 # Use a specific, stable version
)

# 3. flecs - A fast entity component system
FetchContent_Declare(
        flecs
        GIT_REPOSITORY https://github.com/SanderMertens/flecs.git
        GIT_TAG v4.1.1 # Use a specific, stable version
)

# 4. cglm - Highly optimized graphics math for C
FetchContent_Declare(
        cglm
        GIT_REPOSITORY https://github.com/recp/cglm.git
        GIT_TAG v0.9.6 # Use a specific, stable version
)

# --- Make Dependencies Available ---

FetchContent_MakeAvailable(simde raylib flecs cglm)

# --- Project Sources ---

add_executable(MyC23Project src/main.c)

# --- Linking Libraries ---

# For header-only libraries, we just need to add their include directories.
# FetchContent_<dependency>_SOURCE_DIR variables are created by FetchContent_MakeAvailable
target_include_directories(MyC23Project PRIVATE
        ${simde_SOURCE_DIR}
        ${cglm_SOURCE_DIR}/include
)

# For libraries that need to be compiled, we link against the targets they create.
# These target names are typically defined in the library's own CMakeLists.txt.
target_link_libraries(MyC23Project PRIVATE
        raylib
        flecs
)

# On some platforms, raylib requires linking additional system libraries.
if(WIN32)
    target_link_libraries(MyC23Project PRIVATE opengl32 gdi32 shell32 winmm)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(MyC23Project PRIVATE -lm -ldl -lrt -lX11)
endif()