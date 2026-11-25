cmake_minimum_required(VERSION 3.14)
project(Carbon VERSION 0.25.0 LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(Carbon INTERFACE)
target_compile_features(Carbon INTERFACE cxx_std_11) 
set_target_properties(Carbon PROPERTIES CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
target_include_directories(Carbon INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(Carbon INTERFACE
  # $<$<CXX_COMPILER_ID:GNU,Clang>:-ffreestanding -fno-exceptions -fno-rtti>
  $<$<CXX_COMPILER_ID:MSVC>:/Zc:wchar_t>
)
 
add_executable(carbon-dev src/main.cpp)

find_package(EnTT CONFIG REQUIRED)
find_package(flecs CONFIG REQUIRED)
find_package(Threads REQUIRED)

target_link_libraries(carbon-dev PRIVATE Carbon) # EnTT::EnTT flecs::flecs Threads::Threads)
target_compile_features(carbon-dev PRIVATE cxx_std_11)
 
set_target_properties(carbon-dev PROPERTIES
  CXX_STANDARD 11
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF)
 
target_compile_options(carbon-dev PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-O3 -march=native -pipe -Wall -Wextra>
    $<$<CXX_COMPILER_ID:MSVC>:/O2 /Oi /Gy /W4>
)




