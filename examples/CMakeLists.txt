cmake_minimum_required(VERSION 3.28...4.0)

project(Base64-examples VERSION 0.1.0 LANGUAGES CXX)

find_package(Poco 1.14 COMPONENTS Foundation REQUIRED)

add_executable(base64decode base64decode.cpp)
target_link_libraries(base64decode PUBLIC Poco::Foundation)

add_executable(base64encode base64encode.cpp)
target_link_libraries(base64encode PUBLIC Poco::Foundation)

if(DEFINED do_test)
    do_test(base64decode --help stdin)
    do_test(base64encode --help stdin)
endif()
