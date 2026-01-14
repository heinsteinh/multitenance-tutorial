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
  step06_grpc_basics
  VERSION 1.0.0
  LANGUAGES CXX
  DESCRIPTION "Step 06: gRPC Basics")

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
find_package(gRPC CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)

# Get protobuf and grpc plugin executables
get_target_property(PROTOC_EXECUTABLE protobuf::protoc
                    IMPORTED_LOCATION_RELEASE)
if(NOT PROTOC_EXECUTABLE)
  get_target_property(PROTOC_EXECUTABLE protobuf::protoc IMPORTED_LOCATION)
endif()
get_target_property(GRPC_CPP_PLUGIN gRPC::grpc_cpp_plugin
                    IMPORTED_LOCATION_RELEASE)
if(NOT GRPC_CPP_PLUGIN)
  get_target_property(GRPC_CPP_PLUGIN gRPC::grpc_cpp_plugin IMPORTED_LOCATION)
endif()

# Use command directly if not found
if(NOT PROTOC_EXECUTABLE)
  find_program(PROTOC_EXECUTABLE protoc REQUIRED)
endif()
if(NOT GRPC_CPP_PLUGIN)
  find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin REQUIRED)
endif()

# Proto files
set(PROTO_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/proto/common.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/proto/tenant.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/proto/user.proto)

# Output directory for generated files
set(PROTO_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${PROTO_OUTPUT_DIR})

# Generate protobuf and grpc files
set(GENERATED_SOURCES)
set(GENERATED_HEADERS)

foreach(PROTO_FILE ${PROTO_FILES})
  get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)

  set(PB_CC ${PROTO_OUTPUT_DIR}/${PROTO_NAME}.pb.cc)
  set(PB_H ${PROTO_OUTPUT_DIR}/${PROTO_NAME}.pb.h)
  set(GRPC_CC ${PROTO_OUTPUT_DIR}/${PROTO_NAME}.grpc.pb.cc)
  set(GRPC_H ${PROTO_OUTPUT_DIR}/${PROTO_NAME}.grpc.pb.h)

  add_custom_command(
    OUTPUT ${PB_CC} ${PB_H} ${GRPC_CC} ${GRPC_H}
    COMMAND
      ${PROTOC_EXECUTABLE} --cpp_out=${PROTO_OUTPUT_DIR}
      --grpc_out=${PROTO_OUTPUT_DIR} --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
      -I${CMAKE_CURRENT_SOURCE_DIR}/proto ${PROTO_FILE}
    DEPENDS ${PROTO_FILE}
    COMMENT "Generating protobuf/grpc code for ${PROTO_NAME}.proto")

  list(APPEND GENERATED_SOURCES ${PB_CC} ${GRPC_CC})
  list(APPEND GENERATED_HEADERS ${PB_H} ${GRPC_H})
endforeach()

# Proto library
add_library(proto_lib STATIC ${GENERATED_SOURCES})

target_include_directories(proto_lib PUBLIC ${PROTO_OUTPUT_DIR}
                                            ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(proto_lib PUBLIC gRPC::grpc++ protobuf::libprotobuf)

# Suppress warnings in generated code
target_compile_options(
  proto_lib PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-Wno-unused-parameter
                    -Wno-array-bounds> $<$<CXX_COMPILER_ID:MSVC>:/W0>)

# SQLite wrapper library (from Step 02)
add_library(sqlite_wrapper STATIC ../step02-sqlite-foundation/src/database.cpp
                                  ../step02-sqlite-foundation/src/statement.cpp)

target_include_directories(
  sqlite_wrapper PUBLIC ${CMAKE_SOURCE_DIR}/../step02-sqlite-foundation/include)

target_link_libraries(sqlite_wrapper PUBLIC unofficial::sqlite3::sqlite3
                                            spdlog::spdlog fmt::fmt)

# Connection pool library (from Step 03)
add_library(connection_pool STATIC
            ../step03-connection-pool/src/connection_pool.cpp)

target_include_directories(
  connection_pool PUBLIC ${CMAKE_SOURCE_DIR}/../step03-connection-pool/include)

target_link_libraries(connection_pool PUBLIC sqlite_wrapper Threads::Threads)

# Tenant management (from Step 05)
add_library(
  tenant_lib STATIC ../step05-tenant-management/src/tenant_context.cpp
                    ../step05-tenant-management/src/tenant_manager.cpp)

target_include_directories(
  tenant_lib PUBLIC ${CMAKE_SOURCE_DIR}/../step05-tenant-management/include
                    ${CMAKE_SOURCE_DIR}/../step04-repository-pattern/include)

target_link_libraries(tenant_lib PUBLIC connection_pool)

# Server executable
add_executable(step06_server src/server.cpp)

target_link_libraries(step06_server PRIVATE proto_lib tenant_lib
                                            nlohmann_json::nlohmann_json)

# Client executable
add_executable(step06_client src/client.cpp)

target_link_libraries(
  step06_client PRIVATE proto_lib nlohmann_json::nlohmann_json fmt::fmt
                        spdlog::spdlog)

# Demo executable
add_executable(step06_demo src/main.cpp)

target_link_libraries(step06_demo PRIVATE proto_lib tenant_lib
                                          nlohmann_json::nlohmann_json)

# Test executable
add_executable(step06_test tests/grpc_test.cpp)

target_include_directories(step06_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                                               ${PROTO_OUTPUT_DIR})

target_link_libraries(step06_test PRIVATE proto_lib Catch2::Catch2WithMain)

# Register test
enable_testing()
add_test(NAME grpc_tests COMMAND step06_test)

message(STATUS "")
message(STATUS "=== Step 06: gRPC Basics ===")
message(STATUS "Proto output: ${PROTO_OUTPUT_DIR}")
message(STATUS "")
