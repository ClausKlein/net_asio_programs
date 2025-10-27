cmake_minimum_required(VERSION 3.25...4.2)

project(RRCP-client VERSION 0.1.1 LANGUAGES CXX)

# ---- add dependencies ----

include(FetchContent)

find_package(Threads)
if(NOT TARGET Boost::headers)
    find_package(
        Boost
        1.71
        COMPONENTS headers signals2
        REQUIRED
        HINTS $ENV{HOME}/.local
    )
endif()

FetchContent_Declare(
    fmt
    GIT_TAG 12.0.0
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    EXCLUDE_FROM_ALL
    SYSTEM
    FIND_PACKAGE_ARGS 12.0.0 NAMES fmt HINTS $ENV{HOME}/.local
)

FetchContent_MakeAvailable(fmt)

# ---- default settings ----

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    if(APPLE)
        execute_process(
            OUTPUT_VARIABLE LLVM_PREFIX
            COMMAND brew --prefix llvm
            COMMAND_ECHO STDOUT
        )
        string(STRIP ${LLVM_PREFIX} LLVM_PREFIX)
        set(CMAKE_CXX_STANDARD 20)
    elseif(LINUX)
        set(LLVM_PREFIX $ENV{LLVM_ROOT})
    endif()

    add_compile_options(-fexperimental-library)
    add_link_options(-L${LLVM_PREFIX}/lib/c++ -lc++experimental)
else()
    set(CMAKE_CXX_STANDARD 17)
endif()

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- code coverage ----

option(BUILD_TESTING "Build ctest" ${PROJECT_IS_TOP_LEVEL})
option(BUILD_EXAMPLES "Compile examples too" NO)
option(
    ENABLE_TEST_COVERAGE
    "Compile with test-coverage flags"
    ${PROJECT_IS_TOP_LEVEL}
)
if(ENABLE_TEST_COVERAGE AND CMAKE_BUILD_TYPE STREQUAL Debug)
    message(WARNING "ENABLE_TEST_COVERAGE is set!")
    add_compile_options(-O0 -g -fprofile-arcs -ftest-coverage)
    add_link_options(-fprofile-arcs -ftest-coverage)
    # FIXME: add_compile_definitions(TARGET_CODE_COVERAGE)
endif()

# ---- ctest ----

enable_testing()

function(do_test target arg result)
    if(BUILD_TESTING)
        add_test(NAME ${target}${arg} COMMAND ${target} ${arg})
        set_tests_properties(
            ${target}${arg}
            PROPERTIES PASS_REGULAR_EXPRESSION ${result}
        )
    endif()
endfunction()

# ---- server needed for tests ----

add_executable(async_tcp_echo_server examples/async_tcp_echo_server.cpp)
target_link_libraries(
    async_tcp_echo_server
    PUBLIC Threads::Threads Boost::headers
)
do_test(async_tcp_echo_server "" port)

# ---- rrcp class sources and helpers as a library ----

add_library(rrcp_helper STATIC)
target_sources(
    rrcp_helper
    PRIVATE rrcp_helper.cpp
    PUBLIC FILE_SET HEADERS FILES async_rrcp_client.hpp rrcp_helper.hpp
)
target_link_libraries(
    rrcp_helper
    PUBLIC Threads::Threads Boost::headers fmt::fmt-header-only
)

add_executable(async_tcp_echo_client examples/async_tcp_echo_client.cpp)
target_link_libraries(async_tcp_echo_client PRIVATE rrcp_helper)
do_test(async_tcp_echo_client --help Usage)

add_executable(blocking_tcp_echo_client examples/blocking_tcp_echo_client.cpp)
target_link_libraries(blocking_tcp_echo_client PRIVATE rrcp_helper)
do_test(blocking_tcp_echo_client --help Usage)

add_executable(rrcp_client rrcp_client.cpp rrcp_message.hpp)
target_link_libraries(rrcp_client PRIVATE rrcp_helper)
do_test(rrcp_client --help Usage)

if(APPLE AND BUILD_EXAMPLES)
    # TODO(CK): mv to examples too!
    add_executable(timer timer.cpp)
    target_link_libraries(
        timer
        PUBLIC Threads::Threads Boost::headers fmt::fmt-header-only
    )
    add_test(NAME timer COMMAND timer)
endif()

# ---- rrcp client class usage examples main  ----

add_executable(rrcp_async_tcp_client rrcp_async_tcp_client.cpp)
target_link_libraries(rrcp_async_tcp_client PRIVATE rrcp_helper)
do_test(rrcp_async_tcp_client --help Usage)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

if(APPLE AND BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
