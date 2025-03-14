cmake_minimum_required(VERSION 3.28...4.0)

project(RRCP-client VERSION 0.1.0 LANGUAGES CXX)

# ---- add dependencies ----

find_package(Boost 1.87 COMPONENTS asio REQUIRED HINTS $ENV{HOME}/.local)
find_package(fmt 11.1.4 REQUIRED HINTS $ENV{HOME}/.local)

# ---- default settings ----

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    if(APPLE)
        execute_process(
            OUTPUT_VARIABLE LLVM_PREFIX
            COMMAND brew --prefix llvm@19
            COMMAND_ECHO STDOUT
        )
        string(STRIP ${LLVM_PREFIX} LLVM_PREFIX)
    elseif(LINUX)
        set(LLVM_PREFIX $ENV{LLVM_ROOT})
    endif()

    add_compile_options(-fexperimental-library)
    add_link_options(-L${LLVM_PREFIX}/lib/c++ -lc++experimental)
endif()

# ---- code coverage ----

option(ENABLE_TEST_COVERAGE ON)
if(ENABLE_TEST_COVERAGE)
    message(STATUS "ENABLE_TEST_COVERAGE is set!")
    add_compile_options(-O0 -g -fprofile-arcs -ftest-coverage)
    add_link_options(-fprofile-arcs -ftest-coverage)
endif()

add_executable(async_tcp_echo_server async_tcp_echo_server.cpp)
target_link_libraries(async_tcp_echo_server PUBLIC Boost::asio)

add_library(rrcp_helper STATIC rrcp_helper.cpp rrcp_helper.hpp)

add_executable(async_tcp_echo_client async_tcp_echo_client.cpp)
target_link_libraries(
    async_tcp_echo_client
    PRIVATE rrcp_helper
    PUBLIC Boost::asio fmt::fmt-header-only
)

add_executable(blocking_tcp_echo_client blocking_tcp_echo_client.cpp)
target_link_libraries(
    blocking_tcp_echo_client
    PRIVATE rrcp_helper
    PUBLIC Boost::asio
)

add_executable(blocking_tcp_echo_server blocking_tcp_echo_server.cpp)
target_link_libraries(
    blocking_tcp_echo_server
    PRIVATE rrcp_helper
    PUBLIC Boost::asio
)

add_executable(rrcp_client rrcp_client.cpp rrcp_message.hpp)
target_link_libraries(rrcp_client PRIVATE rrcp_helper PUBLIC Boost::asio)

add_executable(timer timer.cpp)
target_link_libraries(timer PUBLIC Boost::asio)

add_executable(async_client async_client.cpp)
target_link_libraries(async_client PUBLIC Boost::asio)

add_executable(async_tcp_client async_tcp_client.cpp)
target_link_libraries(async_tcp_client PRIVATE rrcp_helper PUBLIC Boost::asio)
