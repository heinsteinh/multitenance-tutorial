cmake_minimum_required(VERSION 3.21)

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
  step02_sqlite_foundation
  VERSION 1.0.0
  LANGUAGES CXX
  DESCRIPTION "Step 02: SQLite Foundation")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find packages
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(Catch2 CONFIG REQUIRED)

# Database library
add_library(sqlite_wrapper STATIC src/database.cpp src/statement.cpp)

target_include_directories(sqlite_wrapper
                           PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(sqlite_wrapper PUBLIC unofficial::sqlite3::sqlite3
                                            spdlog::spdlog fmt::fmt)

target_compile_options(
  sqlite_wrapper PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra
                         -Wpedantic> $<$<CXX_COMPILER_ID:MSVC>:/W4>)

# Demo executable
add_executable(step02_demo src/main.cpp)

target_link_libraries(step02_demo PRIVATE sqlite_wrapper
                                          nlohmann_json::nlohmann_json)

# Tests
add_executable(step02_tests tests/database_test.cpp)

target_link_libraries(step02_tests PRIVATE sqlite_wrapper
                                           Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(step02_tests)

message(STATUS "")
message(STATUS "=== Step 02: SQLite Foundation ===")
message(STATUS "")
