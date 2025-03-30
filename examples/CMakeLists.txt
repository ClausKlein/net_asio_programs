cmake_minimum_required(VERSION 3.25...4.0)

project(Base64-examples VERSION 0.1.0 LANGUAGES CXX)

find_package(Poco 1.14 COMPONENTS Foundation REQUIRED)

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
