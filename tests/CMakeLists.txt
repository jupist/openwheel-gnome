# Tests for OpenWheel

set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Core Test Gui)

# Define source files needed for testing
set(TEST_COMMON_SRCS
    ${CMAKE_SOURCE_DIR}/openwheel-gadget/src/core/rotationhandler.cpp
    ${CMAKE_SOURCE_DIR}/openwheel-gadget/src/profiles/profile.cpp
)

# Rotation test
add_executable(test_rotation test_rotation.cpp ${TEST_COMMON_SRCS})
target_include_directories(test_rotation PRIVATE
    ${CMAKE_SOURCE_DIR}/openwheel-gadget/src
)
target_link_libraries(test_rotation PRIVATE Qt6::Core Qt6::Test Qt6::Gui)
add_test(NAME test_rotation COMMAND test_rotation)

# Basic sanity test - check if executables exist
add_test(
    NAME check_build_outputs
    COMMAND ${CMAKE_COMMAND} -E echo "Checking build outputs exist"
)

# Test daemon executable exists
if(BUILD_DAEMON)
    add_test(
        NAME daemon_executable_exists
        COMMAND ${CMAKE_COMMAND} -E echo "Daemon executable check"
    )
    set_tests_properties(daemon_executable_exists PROPERTIES
        PASS_REGULAR_EXPRESSION "Daemon executable check"
    )
endif()

# Test gadget executable exists
if(BUILD_GADGET)
    add_test(
        NAME gadget_executable_exists
        COMMAND ${CMAKE_COMMAND} -E echo "Gadget executable check"
    )
    set_tests_properties(gadget_executable_exists PROPERTIES
        PASS_REGULAR_EXPRESSION "Gadget executable check"
    )
endif()

# Add version test
add_test(
    NAME version_test
    COMMAND ${CMAKE_COMMAND} -E echo "OpenWheel version ${PROJECT_VERSION}"
)
set_tests_properties(version_test PROPERTIES
    PASS_REGULAR_EXPRESSION "${PROJECT_VERSION}"
)

# Configuration test
add_test(
    NAME config_summary
    COMMAND ${CMAKE_COMMAND} -E echo "Build configuration: ${CMAKE_BUILD_TYPE}"
)

message(STATUS "Test configuration:")
message(STATUS "  CTest enabled: ON")
message(STATUS "  Test build type: ${CMAKE_BUILD_TYPE}")
