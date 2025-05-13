cmake_minimum_required(VERSION 3.25...4.0)

project(Base64-examples VERSION 0.1.1 LANGUAGES CXX)

find_package(Poco 1.14 COMPONENTS Foundation REQUIRED)
find_package(Threads)

if(NOT TARGET Boost::headers)
    find_package(Boost 1.71 COMPONENTS headers REQUIRED HINTS $ENV{HOME}/.local)
endif()
if(NOT TARGET fmt::fmt-header-only)
    find_package(fmt 11 REQUIRED HINTS $ENV{HOME}/.local)
endif()

enable_testing()

add_executable(base64decode base64decode.cpp)
target_link_libraries(base64decode PUBLIC Poco::Foundation)

add_executable(base64encode base64encode.cpp)
target_link_libraries(base64encode PUBLIC Poco::Foundation)

if(UNIX)
    add_test(
        NAME base64-test
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test.sh
        WORKING_DIRECTORY ${CMAKE__CURRENT_BINARY_DIR}
    )
endif()

if(PROJECT_IS_TOP_LEVEL)
    return()
endif()

do_test(base64decode --help input)
do_test(base64encode --help input)

if(APPLE AND NOT ENABLE_TEST_COVERAGE)
    add_executable(async_tcp_client_v20 async_tcp_client_v20.cpp)
    target_link_libraries(
        async_tcp_client_v20
        PUBLIC Boost::headers fmt::fmt-header-only
    )
    do_test(async_tcp_client_v20 --help Usage)

    add_executable(async_tcp_client async_tcp_client.cpp)
    target_link_libraries(
        async_tcp_client
        PUBLIC Boost::headers fmt::fmt-header-only
    )
    do_test(async_tcp_client --help Usage)

    add_executable(blocking_tcp_echo_server blocking_tcp_echo_server.cpp)
    target_link_libraries(blocking_tcp_echo_server PUBLIC Boost::headers)
    # FIXME: do_test(blocking_tcp_echo_server port Usage)
endif()
