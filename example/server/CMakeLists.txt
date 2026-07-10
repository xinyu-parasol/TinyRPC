protobuf_generate_cpp(USER_PROTO_SRCS USER_PROTO_HDRS
    ${CMAKE_SOURCE_DIR}/demo/user.proto
)

add_executable(server main.cc ${USER_PROTO_SRCS})
target_link_libraries(server PUBLIC tinyrpc_core ${LIBS})
target_include_directories(server PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_compile_options(server PRIVATE -std=c++14 -Wall)
