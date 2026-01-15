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

project(step10_complete_system LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)

# jwt-cpp is header-only, include from vcpkg
set(JWT_CPP_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/vcpkg_installed/arm64-osx/include")

set(PROTO_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/proto)
set(PROTO_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(GLOB PROTO_FILES "${PROTO_SRC_DIR}/*.proto")

# Resolve protoc and plugin
get_target_property(PROTOC_EXECUTABLE protobuf::protoc
                    IMPORTED_LOCATION_RELEASE)
if(NOT PROTOC_EXECUTABLE)
  get_target_property(PROTOC_EXECUTABLE protobuf::protoc IMPORTED_LOCATION)
endif()
if(NOT PROTOC_EXECUTABLE)
  find_program(PROTOC_EXECUTABLE protoc REQUIRED)
endif()

get_target_property(GRPC_CPP_PLUGIN gRPC::grpc_cpp_plugin
                    IMPORTED_LOCATION_RELEASE)
if(NOT GRPC_CPP_PLUGIN)
  get_target_property(GRPC_CPP_PLUGIN gRPC::grpc_cpp_plugin IMPORTED_LOCATION)
endif()
if(NOT GRPC_CPP_PLUGIN)
  find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin REQUIRED)
endif()

set(PROTO_SRCS)
set(PROTO_HDRS)
foreach(proto_file ${PROTO_FILES})
  get_filename_component(proto_name ${proto_file} NAME_WE)
  set(pb_src ${PROTO_OUTPUT_DIR}/${proto_name}.pb.cc)
  set(pb_hdr ${PROTO_OUTPUT_DIR}/${proto_name}.pb.h)
  set(grpc_src ${PROTO_OUTPUT_DIR}/${proto_name}.grpc.pb.cc)
  set(grpc_hdr ${PROTO_OUTPUT_DIR}/${proto_name}.grpc.pb.h)

  add_custom_command(
    OUTPUT ${pb_src} ${pb_hdr} ${grpc_src} ${grpc_hdr}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PROTO_OUTPUT_DIR}
    COMMAND
      ${PROTOC_EXECUTABLE} --proto_path ${PROTO_SRC_DIR} --cpp_out
      ${PROTO_OUTPUT_DIR} --experimental_allow_proto3_optional ${proto_file}
    COMMAND
      ${PROTOC_EXECUTABLE} --proto_path ${PROTO_SRC_DIR} --grpc_out
      ${PROTO_OUTPUT_DIR} --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
      ${proto_file}
    DEPENDS ${proto_file}
    VERBATIM)

  list(APPEND PROTO_SRCS ${pb_src} ${grpc_src})
  list(APPEND PROTO_HDRS ${pb_hdr} ${grpc_hdr})
endforeach()

add_custom_target(proto_gen DEPENDS ${PROTO_SRCS} ${PROTO_HDRS})

add_library(proto_lib ${PROTO_SRCS} ${PROTO_HDRS})
add_dependencies(proto_lib proto_gen)

target_include_directories(proto_lib PUBLIC ${PROTO_OUTPUT_DIR})

target_link_libraries(proto_lib PUBLIC protobuf::libprotobuf gRPC::grpc++)

# Database library
add_library(db_lib
    src/db/database.cpp
    src/db/statement.cpp
    src/db/schema_initializer.cpp)

target_include_directories(db_lib PUBLIC include)

target_link_libraries(db_lib PUBLIC
    spdlog::spdlog
    fmt::fmt
    unofficial::sqlite3::sqlite3)

# Repository library
add_library(repository_lib
    src/repository/user_repository.cpp
    src/repository/tenant_repository.cpp)

target_include_directories(repository_lib PUBLIC include)

target_link_libraries(repository_lib PUBLIC db_lib spdlog::spdlog)

# Auth library
add_library(auth_lib
    src/auth/authorization_service.cpp
    src/auth/jwt_validator.cpp
    src/auth/policy_engine.cpp
    src/auth/role_repository.cpp)

target_include_directories(auth_lib PUBLIC include ${JWT_CPP_INCLUDE_DIR})

target_link_libraries(auth_lib PUBLIC
    db_lib
    spdlog::spdlog
    fmt::fmt
    nlohmann_json::nlohmann_json)

# Service library
add_library(service_lib
    src/services/user_service.cpp
    src/services/tenant_service.cpp
    src/services/mapper.cpp
    src/services/auth_service.cpp
    src/services/health_service.cpp)
add_dependencies(service_lib proto_gen)

target_include_directories(service_lib PUBLIC include ${PROTO_OUTPUT_DIR})

target_link_libraries(service_lib PUBLIC
    proto_lib
    repository_lib
    auth_lib
    spdlog::spdlog
    fmt::fmt)

# Interceptor library
add_library(interceptor_lib
    src/interceptors/base_interceptor.cpp
    src/interceptors/logging_interceptor.cpp
    src/interceptors/auth_interceptor.cpp
    src/interceptors/tenant_interceptor.cpp
    src/interceptors/interceptor_factory.cpp)
add_dependencies(interceptor_lib proto_gen)

target_include_directories(interceptor_lib PUBLIC include ${PROTO_OUTPUT_DIR})

target_link_libraries(interceptor_lib PUBLIC
    proto_lib
    auth_lib
    spdlog::spdlog
    fmt::fmt)

# Handler library
add_library(handler_lib
    src/handlers/user_handler.cpp
    src/handlers/tenant_handler.cpp
    src/handlers/health_handler.cpp)
add_dependencies(handler_lib proto_gen)

target_include_directories(handler_lib PUBLIC include ${PROTO_OUTPUT_DIR})

target_link_libraries(handler_lib PUBLIC
    proto_lib
    service_lib
    auth_lib
    spdlog::spdlog
    fmt::fmt)

# Configuration library
add_library(config_lib src/config/server_config.cpp)

target_include_directories(config_lib PUBLIC include)

target_link_libraries(config_lib PUBLIC spdlog::spdlog
                                        nlohmann_json::nlohmann_json)

# Main server executable
add_executable(step10_server src/main.cpp)
add_dependencies(step10_server proto_gen)

target_include_directories(step10_server PRIVATE include ${PROTO_OUTPUT_DIR})

target_link_libraries(
  step10_server
  PRIVATE config_lib
          db_lib
          repository_lib
          auth_lib
          interceptor_lib
          handler_lib
          service_lib
          proto_lib
          spdlog::spdlog
          fmt::fmt)

# Tests
enable_testing()

find_package(Catch2 3 CONFIG REQUIRED)

add_executable(grpc_complete_system_test tests/grpc_interceptor_test.cpp)
add_dependencies(grpc_complete_system_test proto_gen)

target_include_directories(grpc_complete_system_test
                           PRIVATE include ${PROTO_OUTPUT_DIR})

target_link_libraries(
  grpc_complete_system_test
  PRIVATE proto_lib
          service_lib
          auth_lib
          db_lib
          Catch2::Catch2WithMain
          gRPC::grpc++
          spdlog::spdlog)

add_test(NAME GrpcCompleteSystemTest COMMAND grpc_complete_system_test)

# Repository tests
add_executable(repository_test tests/repository_test.cpp)

target_include_directories(repository_test PRIVATE include)

target_link_libraries(
  repository_test
  PRIVATE db_lib
          repository_lib
          Catch2::Catch2WithMain
          spdlog::spdlog)

add_test(NAME RepositoryTest COMMAND repository_test)

message(STATUS "")
message(STATUS "=== Step 10: Complete System ===")
message(STATUS "Proto output: ${PROTO_OUTPUT_DIR}")
message(STATUS "")
