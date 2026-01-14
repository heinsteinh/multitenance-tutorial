cmake_minimum_required(VERSION 3.21)

# vcpkg toolchain must be set before project()
if(DEFINED ENV{VCPKG_ROOT})
  set(CMAKE_TOOLCHAIN_FILE
      "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
      CACHE STRING "Vcpkg toolchain file")
elseif(EXISTS "${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
  set(CMAKE_TOOLCHAIN_FILE
      "${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
      CACHE STRING "Vcpkg toolchain file")
endif()

project(
  step01_vcpkg_demo
  VERSION 1.0.0
  LANGUAGES CXX
  DESCRIPTION "Step 01: vcpkg Setup Demo")

# C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find packages installed by vcpkg
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)

# Create executable
add_executable(step01_demo src/main.cpp)

# Link libraries
target_link_libraries(step01_demo PRIVATE spdlog::spdlog
                                          nlohmann_json::nlohmann_json fmt::fmt)

# Compiler warnings
target_compile_options(
  step01_demo PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
                      $<$<CXX_COMPILER_ID:MSVC>:/W4>)

# Print configuration summary
message(STATUS "")
message(STATUS "=== Step 01: vcpkg Setup Demo ===")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Toolchain: ${CMAKE_TOOLCHAIN_FILE}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "")
