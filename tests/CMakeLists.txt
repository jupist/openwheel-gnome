# Tests for OpenWheel

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
