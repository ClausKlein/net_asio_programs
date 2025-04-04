include(FetchContent)

# we use boost::ut
FetchContent_Declare(
    ut
    GIT_TAG v2.3.1
    GIT_REPOSITORY https://github.com/boost-ext/ut.git
    FIND_PACKAGE_ARGS 2.3.1 NAMES ut
    EXCLUDE_FROM_ALL
    SYSTEM
)

# TODO(CK): We still use googletest too! But will be changed to Boost::ut
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
FetchContent_MakeAvailable(googletest ut)

# add_library(base64c STATIC)
# NO! target_sources(base64c PRIVATE base64.c
#   PUBLIC FILE_SET HEADERS BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} FILES base64.h)

target_sources(
    rrcp_helper
    PRIVATE ${CMAKE_SOURCE_DIR}/Base64.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_SOURCE_DIR}
        FILES ${CMAKE_SOURCE_DIR}/Base64.hpp
)
target_link_libraries(
    rrcp_helper # Not needed: PUBLIC Boost::beast
)

add_executable(RRCP-test RRCP-test.cpp)
target_link_libraries(RRCP-test PRIVATE rrcp_helper Boost::ut)
add_test(NAME RRCP-test COMMAND RRCP-test)

add_executable(Base64-test Base64-test.cpp)
target_link_libraries(Base64-test PRIVATE rrcp_helper GTest::gtest_main)
add_test(NAME Base64-test COMMAND Base64-test)
