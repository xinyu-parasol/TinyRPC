protobuf_generate_cpp(USER_PROTO_SRCS USER_PROTO_HDRS
    ${CMAKE_SOURCE_DIR}/demo/user.proto
)

add_executable(client main.cc ${USER_PROTO_SRCS})
target_link_libraries(client PUBLIC tinyrpc_core ${LIBS})
target_include_directories(client PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_compile_options(client PRIVATE -std=c++14 -Wall)
