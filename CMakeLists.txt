cmake_minimum_required(VERSION 3.16)

project(carbon LANGUAGES CXX)

option(USE_PGO "Enable profile guided optimization" OFF)
option(PGO_GENERATE "Generate PGO profile data" OFF)
option(PGO_USE "Use existing PGO profile data" OFF)


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()


file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
)

add_executable(carbon ${ENGINE_SOURCES})

target_include_directories(carbon PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/third_party"
)

if(MSVC)

    target_compile_options(carbon PRIVATE
        /W4
        $<$<CONFIG:Release>:
            /O2
            /Oi
            /GL
            /fp:fast
        >
    )

else()

    target_compile_options(carbon PRIVATE
        -Wall
        -Wextra
        -Wpedantic

        $<$<CONFIG:Debug>:
            -g
            -O0
        >

        $<$<CONFIG:Release>:
            -O3
            -ffast-math
            -funroll-loops
            -DNDEBUG
        >
    )

endif()

if(USE_PGO)

    if(PGO_GENERATE)
        message(STATUS "PGO: generating profile data")

        target_compile_options(carbon PRIVATE -fprofile-generate)
        target_link_options(carbon PRIVATE -fprofile-generate)

    elseif(PGO_USE)
        message(STATUS "PGO: using profile data")

        target_compile_options(carbon PRIVATE
            -fprofile-use
            -fprofile-correction
        )

        target_link_options(carbon PRIVATE
            -fprofile-use
            -fprofile-correction
        )
    endif()

endif()



include(CheckIPOSupported)

check_ipo_supported(RESULT ipo_supported)

if(ipo_supported)
    set_property(TARGET carbon PROPERTY
        INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
    )
endif()



if(EMSCRIPTEN)

    message(STATUS "Configuring for Emscripten")

    set_target_properties(carbon PROPERTIES SUFFIX ".html")

    target_compile_options(carbon PRIVATE
        -sUSE_SDL=2
    )

    target_link_options(carbon PRIVATE
        -sUSE_SDL=2
        -sWASM=1
        -sALLOW_MEMORY_GROWTH=1
        -sASSERTIONS=1
        -sASYNCIFY
    )

endif()
