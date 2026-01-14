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
  step03_connection_pool
  VERSION 1.0.0
  LANGUAGES CXX
  DESCRIPTION "Step 03: Connection Pool")

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
find_package(Threads REQUIRED)

# SQLite wrapper library (from Step 02)
add_library(sqlite_wrapper STATIC ../step02-sqlite-foundation/src/database.cpp
                                  ../step02-sqlite-foundation/src/statement.cpp)

target_include_directories(
  sqlite_wrapper PUBLIC ${CMAKE_SOURCE_DIR}/../step02-sqlite-foundation/include)

target_link_libraries(sqlite_wrapper PUBLIC unofficial::sqlite3::sqlite3
                                            spdlog::spdlog fmt::fmt)

# Connection pool library
add_library(connection_pool STATIC src/connection_pool.cpp)

target_include_directories(connection_pool
                           PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(connection_pool PUBLIC sqlite_wrapper Threads::Threads)

target_compile_options(
  connection_pool PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra
                          -Wpedantic> $<$<CXX_COMPILER_ID:MSVC>:/W4>)

# Demo executable
add_executable(step03_demo src/main.cpp)

target_link_libraries(step03_demo PRIVATE connection_pool
                                          nlohmann_json::nlohmann_json)

# Tests
add_executable(step03_tests tests/pool_test.cpp)

target_link_libraries(step03_tests PRIVATE connection_pool
                                           Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(step03_tests)

message(STATUS "")
message(STATUS "=== Step 03: Connection Pool ===")
message(STATUS "")
