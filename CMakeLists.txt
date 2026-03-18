cmake_minimum_required(VERSION 3.16)
project(Carbon LANGUAGES CXX)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(USE_PGO "Use Profile Guided Optimization" ON)

if(USE_PGO)
    # Two-phase PGO
    option(PGO_GENERATE "Generate PGO profile data" OFF)
    option(PGO_USE "Use existing PGO profile data" OFF)

    if(PGO_GENERATE)
        message(STATUS "PGO: Generating profile data")
        add_compile_options(-fprofile-generate)
        add_link_options(-fprofile-generate)
    elseif(PGO_USE)
        message(STATUS "PGO: Using profile data")
        add_compile_options(-fprofile-use -fprofile-correction)
        add_link_options(-fprofile-use -fprofile-correction)
    endif()
endif()

# SIMD / optimization flags
if (MSVC)
    add_compile_options(/O2 /Oi /GL /arch:AVX2 /fp:fast)
    else()
    add_compile_options(-O3 -D_GNU_SOURCE -march=haswell -mtune=haswell -mavx2 -mfma -funroll-loops -ffast-math)
endif()

# SIMPLE VERSION - just add to your existing compile options
add_compile_options(
    -O3
    -D_GNU_SOURCE
    -march=haswell
    -mtune=haswell
    -mavx2
    -mfma
    -funroll-loops
    -ffast-math
    "--param=max-inline-insns-single=200"
    "--param=max-inline-insns-auto=100"

    -g
    -save-temps=obj
    -fverbose-asm
    -Wa,-gdwarf-2
)


# Include directory
include_directories(${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/third_party)

# Grab all .cpp files recursively
file(GLOB_RECURSE ENGINE_SOURCES ${CMAKE_SOURCE_DIR}/src/*.cpp)
find_package(PkgConfig REQUIRED)
find_package(SDL2 REQUIRED)
pkg_check_modules(XXHASH REQUIRED libxxhash)
pkg_check_modules(HWLOC REQUIRED hwloc)

# ====== 1. MAIN TARGET ======
add_executable(carbon ${ENGINE_SOURCES})
target_include_directories(carbon PRIVATE ${XXHASH_INCLUDE_DIRS} ${SDL_INCLUDE_DIRS} ${HWLOC_INCLUDE_DIRS})
target_link_libraries(carbon SDL2::SDL2 ${XXHASH_LIBRARIES} ${HWLOC_LIBRARIES})


# ====== 3. LTO FOR ALL TARGETS ======
include(CheckIPOSupported)
check_ipo_supported(RESULT result)
if(result)
    set_property(TARGET carbon PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    # Enable LTO for inline targets too
    foreach(limit IN LISTS INLINE_THRESHOLDS)
        set_property(TARGET carbon_inline_${limit} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    endforeach()
endif()


