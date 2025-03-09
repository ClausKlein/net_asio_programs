cmake_minimum_required(VERSION 3.28...4.0)

project(RRCP-client VERSION 0.1.0 LANGUAGES CXX)

find_package(Boost 1.87 COMPONENTS asio REQUIRED HINTS ../../../../stagedir)

add_library(rrcp_helper STATIC rrcp_helper.cpp rrcp_helper.hpp)

add_executable(blocking_tcp_echo_client blocking_tcp_echo_client.cpp)
target_link_libraries(blocking_tcp_echo_client PRIVATE rrcp_helper PUBLIC Boost::asio)

add_executable(blocking_tcp_echo_server blocking_tcp_echo_server.cpp)
target_link_libraries(blocking_tcp_echo_server PRIVATE rrcp_helper PUBLIC Boost::asio)

add_executable(rrcp_client rrcp_client.cpp rrcp_message.hpp)
target_link_libraries(rrcp_client PUBLIC Boost::asio)

add_executable(async_client async_client.cpp)
target_link_libraries(async_client PUBLIC Boost::asio)

add_executable(async_tcp_client async_tcp_client.cpp)
target_link_libraries(async_tcp_client PRIVATE rrcp_helper PUBLIC Boost::asio)
