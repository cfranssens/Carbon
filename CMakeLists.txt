cmake_minimum_required(VERSION 3.14)
project(Carbon VERSION 0.25.0 LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CARBON_LTO "Enable link-time optimization" ON)
set(CARBON_PGO "OFF" CACHE STRING "Profile-guided optimization phase: OFF, GEN (instrumented build) or USE (optimize with collected profile)")
set_property(CACHE CARBON_PGO PROPERTY STRINGS OFF GEN USE)
set(CARBON_PGO_DIR "${CMAKE_BINARY_DIR}/pgo" CACHE PATH "Where profile data is written / read")
 
add_library(Carbon INTERFACE)
target_compile_features(Carbon INTERFACE cxx_std_11)
 
set_target_properties(Carbon PROPERTIES CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
target_include_directories(Carbon INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(Carbon INTERFACE
  # $<$<CXX_COMPILER_ID:GNU,Clang>:-ffreestanding -fno-exceptions -fno-rtti>
  $<$<CXX_COMPILER_ID:MSVC>:/Zc:wchar_t>
)
 
add_executable(carbon-dev src/main.cpp)
target_link_libraries(carbon-dev PRIVATE Carbon)
target_compile_features(carbon-dev PRIVATE cxx_std_11)
 
set_target_properties(carbon-dev PROPERTIES
  CXX_STANDARD 11
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF)
 
target_compile_options(carbon-dev PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-O3 -march=native -pipe -Wall -Wextra>
    $<$<CXX_COMPILER_ID:MSVC>:/O2 /Oi /Gy /W4>
)
 
if(CARBON_LTO)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT carbon_ipo_ok OUTPUT carbon_ipo_msg LANGUAGES CXX)
  if(carbon_ipo_ok)
    set_property(TARGET carbon-dev PROPERTY INTERPROCEDURAL_OPTIMIZATION ON)
  else()
    message(WARNING "LTO requested but not supported: ${carbon_ipo_msg}")
  endif()
endif()

if(NOT CARBON_PGO STREQUAL "OFF")
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CARBON_PGO STREQUAL "GEN")
      target_compile_options(carbon-dev PRIVATE -fprofile-generate=${CARBON_PGO_DIR} -fprofile-update=atomic)
      target_link_options(carbon-dev PRIVATE -fprofile-generate=${CARBON_PGO_DIR})
      add_custom_target(carbon-pgo-train
        COMMAND $<TARGET_FILE:carbon-dev>
        DEPENDS carbon-dev
        COMMENT "Running PGO training workload")
    elseif(CARBON_PGO STREQUAL "USE")
      target_compile_options(carbon-dev PRIVATE -fprofile-use=${CARBON_PGO_DIR} -fprofile-correction -Wmissing-profile)
      target_link_options(carbon-dev PRIVATE -fprofile-use=${CARBON_PGO_DIR})
    endif()
 
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    find_program(LLVM_PROFDATA NAMES llvm-profdata llvm-profdata-20 llvm-profdata-19 llvm-profdata-18 llvm-profdata-17)
    if(CARBON_PGO STREQUAL "GEN")
      target_compile_options(carbon-dev PRIVATE -fprofile-generate)
      target_link_options(carbon-dev PRIVATE -fprofile-generate)
      if(NOT LLVM_PROFDATA)
        message(FATAL_ERROR "CARBON_PGO=GEN with Clang needs llvm-profdata to merge the raw profile")
      endif()
      add_custom_target(carbon-pgo-train
        COMMAND ${CMAKE_COMMAND} -E env LLVM_PROFILE_FILE=${CARBON_PGO_DIR}/carbon.profraw $<TARGET_FILE:carbon-dev>
        COMMAND ${LLVM_PROFDATA} merge -output=${CARBON_PGO_DIR}/carbon.profdata ${CARBON_PGO_DIR}/carbon.profraw
        DEPENDS carbon-dev
        COMMENT "Running PGO training workload and merging profile")
    elseif(CARBON_PGO STREQUAL "USE")
      if(NOT EXISTS "${CARBON_PGO_DIR}/carbon.profdata")
        message(FATAL_ERROR "No profile at ${CARBON_PGO_DIR}/carbon.profdata: build with -DCARBON_PGO=GEN and run the carbon-pgo-train target first")
      endif()
      target_compile_options(carbon-dev PRIVATE -fprofile-use=${CARBON_PGO_DIR}/carbon.profdata)
      target_link_options(carbon-dev PRIVATE -fprofile-use=${CARBON_PGO_DIR}/carbon.profdata)
    endif()
 
  else()
    message(WARNING "CARBON_PGO is only wired up for GCC and Clang; ignoring for ${CMAKE_CXX_COMPILER_ID}")
  endif()
endif()

