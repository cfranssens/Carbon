cmake_minimum_required(VERSION 3.16)
project(Carbon LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=znver3 -mtune=znver3 -ffast-math -funroll-loops -fomit-frame-pointer")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ENABLE_IPO "Enable Link-Time Optimization" ON)

add_library(carbon INTERFACE)
target_include_directories(carbon INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")


target_compile_definitions(carbon INTERFACE THREAD_COUNT=16)
target_compile_definitions(carbon INTERFACE LOG2_CHUNK_CAP=6)
target_compile_definitions(carbon INTERFACE ALLOCATOR_CHUNK_CAP=64)
target_compile_definitions(carbon INTERFACE PAGEBLOCK_SIZE=512*1024)
target_compile_definitions(carbon INTERFACE ECS_MAX_COMPONENTS=256)
target_compile_definitions(carbon INTERFACE MAX_QUERY_COMPONENTS=16)
target_compile_definitions(carbon INTERFACE ECS_MAX_WORLDS=8)

add_executable(carbon_bench main.cpp)
target_link_libraries(carbon_bench PRIVATE carbon)

target_link_options(carbon_bench PRIVATE -static-libstdc++ -static-libgcc)


