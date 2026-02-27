CMAKE_MINIMUM_REQUIRED(VERSION 3.24)
project(Flotilla LANGUAGES CXX)

if(WIN32)
    set(CMAKE_C_COMPILER cl)
    set(CMAKE_CXX_COMPILER cl)
elseif(UNIX)
    set(CMAKE_C_COMPILER gcc)
    set(CMAKE_CXX_COMPILER g++)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED on)

# grpc configuration
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)

# find code generators
find_program(PROTOC_EXECUTABLE protoc REQUIRED)
find_program(GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin REQUIRED)

# Define our input and expected output files
set(PROTO_FILE "${CMAKE_CURRENT_SOURCE_DIR}/src/proto/service.proto")
set(PROTO_SRC "${CMAKE_CURRENT_BINARY_DIR}/service.pb.cc")
set(PROTO_HDR "${CMAKE_CURRENT_BINARY_DIR}/service.pb.h")
set(GRPC_SRC "${CMAKE_CURRENT_BINARY_DIR}/service.grpc.pb.cc")
set(GRPC_HDR "${CMAKE_CURRENT_BINARY_DIR}/service.grpc.pb.h")

# Instruct CMake how to compile the .proto file
add_custom_command(
    OUTPUT ${PROTO_SRC} ${PROTO_HDR} ${GRPC_SRC} ${GRPC_HDR}
    COMMAND ${PROTOC_EXECUTABLE}
    ARGS --grpc_out="${CMAKE_CURRENT_BINARY_DIR}"
         --cpp_out="${CMAKE_CURRENT_BINARY_DIR}"
         -I "${CMAKE_CURRENT_SOURCE_DIR}/src/proto"
         --plugin=protoc-gen-grpc="${GRPC_CPP_PLUGIN_EXECUTABLE}"
         "${PROTO_FILE}"
    DEPENDS "${PROTO_FILE}"
)

set(SOURCE_FILES
    src/main.cpp
    src/server/server.cpp
)

# Add the generated gRPC and Protobuf files
add_executable(${PROJECT_NAME} 
    ${SOURCE_FILES} 
    ${PROTO_SRC} 
    ${PROTO_HDR} 
    ${GRPC_SRC} 
    ${GRPC_HDR}
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    gRPC::grpc++
    protobuf::libprotobuf
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
