cmake_minimum_required(VERSION 3.16)
project(openwheel-daemon C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(DBUS dbus-1)

if(NOT DBUS_FOUND)
    message(WARNING "DBus not found - daemon will not be built")
    return()
endif()

# Source files
set(DAEMON_SRCS
    hidreader.c
    helpers.c
    keymap.c
    uinput.c
    dbus_methods.c
)

set(DAEMON_HEADERS
    openwheel.h
    helpers.h
    keymap.h
    uinput.h
    dbus_methods.h
)

# Create executable
add_executable(asus_wheel ${DAEMON_SRCS} ${DAEMON_HEADERS})

# Include directories
target_include_directories(asus_wheel PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${DBUS_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(asus_wheel PRIVATE
    ${DBUS_LIBRARIES}
)

# Compile options
target_compile_options(asus_wheel PRIVATE
    ${DBUS_CFLAGS_OTHER}
    -Wall
    -Wextra
)

# Installation
install(TARGETS asus_wheel
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT daemon
)

# Install headers (for development)
install(FILES ${DAEMON_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/openwheel
    COMPONENT daemon-dev
)

# Install udev rule for /dev/uinput and ASUS Dial hidraw access.
# Relative destination — CMake prepends CMAKE_INSTALL_PREFIX at install time,
# so `cmake --install build --prefix /usr` → /usr/lib/udev/rules.d/
install(FILES ${CMAKE_SOURCE_DIR}/udev/99-openwheel.rules
    DESTINATION lib/udev/rules.d
    COMPONENT daemon
)

# Generate and install the systemd user service from the template so that the
# ExecStart path matches the configured CMAKE_INSTALL_FULL_BINDIR.
configure_file(
    ${CMAKE_SOURCE_DIR}/systemd/openwheel-daemon.service.in
    ${CMAKE_CURRENT_BINARY_DIR}/openwheel-daemon.service
    @ONLY
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/openwheel-daemon.service
    DESTINATION lib/systemd/user
    COMPONENT daemon
)