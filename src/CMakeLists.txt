file(GLOB SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cc)

set(PROTO_FILES ${CMAKE_CURRENT_SOURCE_DIR}/header.proto)
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

add_library(tinyrpc_core STATIC ${SRC_FILES} ${PROTO_SRCS})
target_link_libraries(tinyrpc_core PUBLIC protobuf)
target_include_directories(tinyrpc_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
)
target_compile_options(tinyrpc_core PRIVATE -std=c++14 -Wall)
