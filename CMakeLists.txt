cmake_minimum_required(VERSION 3.16)
project(Carbon LANGUAGES CXX)

# Require C++20 (adjust if you need C++23)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# SIMD / optimization flags
if (MSVC)
    add_compile_options(/O2 /Oi /GL /arch:AVX2 /fp:fast)
else()
    add_compile_options(-O3 -march=native -ffast-math -funroll-loops)
endif()

# Include directory
include_directories(${CMAKE_SOURCE_DIR}/include)

# Grab all .cpp files recursively
file(GLOB_RECURSE ENGINE_SOURCES ${CMAKE_SOURCE_DIR}/src/*.cpp)

# Build the main library/executable
add_executable(engine ${ENGINE_SOURCES})

# Optional: enable interprocedural optimization (LTO) if compiler supports it
include(CheckIPOSupported)
check_ipo_supported(RESULT result)
if(result)
    set_property(TARGET engine PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()
