cmake_minimum_required(VERSION 3.28...4.0)

project(RRCP-client VERSION 0.1.0 LANGUAGES CXX C)

# ---- add dependencies ----

find_package(Boost 1.87 COMPONENTS asio REQUIRED HINTS $ENV{HOME}/.local)
find_package(fmt 11 REQUIRED HINTS $ENV{HOME}/.local)

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

option(ENABLE_TEST_COVERAGE "Compile with test-coverage flags" ON)
if(ENABLE_TEST_COVERAGE AND CMAKE_BUILD_TYPE)
    message(WARNING "ENABLE_TEST_COVERAGE is set!")
    add_compile_options(-O0 -g -fprofile-arcs -ftest-coverage)
    add_link_options(-fprofile-arcs -ftest-coverage)
    # FIXME: add_compile_definitions(TARGET_CODE_COVERAGE)
endif()

# ---- ctest ----

enable_testing()

function(do_test target arg result)
    add_test(NAME ${target}${arg} COMMAND ${target} ${arg})
    set_tests_properties(
        ${target}${arg}
        PROPERTIES PASS_REGULAR_EXPRESSION ${result}
    )
endfunction()

add_executable(async_tcp_echo_server async_tcp_echo_server.cpp)
target_link_libraries(async_tcp_echo_server PUBLIC Boost::asio)
do_test(async_tcp_echo_server "" port)

add_library(rrcp_helper STATIC rrcp_helper.cpp rrcp_helper.hpp)

add_executable(async_tcp_echo_client async_tcp_echo_client.cpp)
target_link_libraries(
    async_tcp_echo_client
    PRIVATE rrcp_helper
    PUBLIC Boost::asio fmt::fmt-header-only
)
do_test(async_tcp_echo_client --help Usage)

add_executable(blocking_tcp_echo_client blocking_tcp_echo_client.cpp)
target_link_libraries(
    blocking_tcp_echo_client
    PRIVATE rrcp_helper
    PUBLIC Boost::asio fmt::fmt-header-only
)
do_test(blocking_tcp_echo_client --help Usage)

# add_executable(async_tcp_client_v20 async_tcp_client_v20.cpp)
# target_link_libraries(async_tcp_client_v20 PUBLIC Boost::asio)
# do_test(async_tcp_client_v20 --help Usage)

# add_executable(async_tcp_client async_tcp_client.cpp)
# target_link_libraries(async_tcp_client PUBLIC Boost::asio)
# do_test(async_tcp_client --help Usage)

# add_executable(blocking_tcp_echo_server blocking_tcp_echo_server.cpp)
# target_link_libraries(blocking_tcp_echo_server PUBLIC Boost::asio)
#XXX do_test(blocking_tcp_echo_server port Usage)

add_executable(rrcp_client rrcp_client.cpp rrcp_message.hpp)
target_link_libraries(rrcp_client PRIVATE rrcp_helper PUBLIC Boost::asio)
do_test(rrcp_client --help Usage)

if(APPLE)
    add_executable(timer timer.cpp)
    target_link_libraries(timer PUBLIC Boost::asio)
    add_test(NAME timer COMMAND timer)

    add_executable(async_client async_client.cpp)
    target_link_libraries(async_client PUBLIC Boost::asio)
    add_test(NAME async_client COMMAND async_client)
endif()

add_executable(rrcp_async_tcp_client rrcp_async_tcp_client.cpp)
target_link_libraries(
    rrcp_async_tcp_client
    PRIVATE rrcp_helper
    PUBLIC Boost::asio fmt::fmt-header-only
)
do_test(rrcp_async_tcp_client --help Usage)

# we need googletest
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_TAG v1.16.0
    GIT_REPOSITORY https://github.com/google/googletest.git
    FIND_PACKAGE_ARGS 1.16.0 NAMES GTest COMPONENTS gmain_main
    EXCLUDE_FROM_ALL
    SYSTEM
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_library(base64c STATIC)
target_sources(base64c PRIVATE base64.c PUBLIC FILE_SET HEADERS FILES base64.h)

add_library(Base64 STATIC)
target_sources(
    Base64
    PRIVATE Base64.cpp
    PUBLIC FILE_SET HEADERS FILES Base64.hpp
)

add_executable(Base64-test tests/Base64-test.cpp)
target_link_libraries(Base64-test PRIVATE Base64 base64c GTest::gtest_main)
# FIXME: add_test(NAME Base64-test COMMAND Base64-test)
