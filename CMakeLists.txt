cmake_minimum_required(VERSION 3.25)
project(connect4 LANGUAGES CXX)

find_package(protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)

# A library whose only "source" is the .proto; the build generates the real .cc files
add_library(rendezvous_proto STATIC proto/rendezvous.proto)
target_link_libraries(rendezvous_proto PUBLIC protobuf::libprotobuf gRPC::grpc++)
target_include_directories(rendezvous_proto PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_compile_features(rendezvous_proto PUBLIC cxx_std_20)

# Pass 1: messages -> rendezvous.pb.h / .pb.cc
protobuf_generate(TARGET rendezvous_proto
    IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/proto)

# Pass 2: service -> rendezvous.grpc.pb.h / .grpc.pb.cc (via the gRPC plugin)
protobuf_generate(TARGET rendezvous_proto
    LANGUAGE grpc
    GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc
    PLUGIN "protoc-gen-grpc=\$<TARGET_FILE:gRPC::grpc_cpp_plugin>"
    IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/proto)

    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../../bin)   

add_executable(connect4 src/client/main.cpp)

target_compile_features(connect4 PRIVATE cxx_std_20)
target_link_libraries(connect4 PRIVATE rendezvous_proto)

set_target_properties(rendezvous_proto connect4 PROPERTIES CXX_EXTENSIONS OFF)