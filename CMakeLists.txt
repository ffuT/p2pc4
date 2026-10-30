cmake_minimum_required(VERSION 3.25)
project(connect4 LANGUAGES CXX)

if(WIN32)
    target_link_libraries(connect4 PRIVATE ws2_32)
    target_compile_definitions(connect4 PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

find_package(protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)

add_library(rendezvous_proto STATIC proto/rendezvous.proto)
target_link_libraries(rendezvous_proto PUBLIC protobuf::libprotobuf gRPC::grpc++)
target_include_directories(rendezvous_proto PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_compile_features(rendezvous_proto PUBLIC cxx_std_20)

protobuf_generate(TARGET rendezvous_proto
    IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/proto)

protobuf_generate(TARGET rendezvous_proto
    LANGUAGE grpc
    GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc
    PLUGIN "protoc-gen-grpc=\$<TARGET_FILE:gRPC::grpc_cpp_plugin>"
    IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/proto)

include(FetchContent)
FetchContent_Declare(ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/ftxui
  GIT_TAG v7.0.3
)
FetchContent_MakeAvailable(ftxui)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE})
add_executable(connect4 src/client/main.cpp src/client/net.cpp)

if(UNIX)
    target_link_options(connect4 PRIVATE $<$<CONFIG:Release>:-static-libstdc++ -static-libgcc>)
endif()

target_compile_features(connect4 PRIVATE cxx_std_20)
target_link_libraries(connect4 PRIVATE rendezvous_proto)
target_link_libraries(connect4 PRIVATE ftxui::ftxui)

set_target_properties(rendezvous_proto connect4 PROPERTIES CXX_EXTENSIONS OFF)
