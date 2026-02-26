cmake_minimum_required(VERSION 3.16)

project(openwheel-gadget VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Try to find Qt6 first
find_package(Qt6 QUIET COMPONENTS Core Gui Widgets Qml Quick DBus QuickControls2)

if(NOT Qt6_FOUND)
    message(STATUS "Qt6 not found, trying Qt5...")
    find_package(Qt5 5.15 QUIET COMPONENTS Core Gui Widgets Qml Quick DBus QuickControls2)
    if(NOT Qt5_FOUND)
        message(WARNING "Neither Qt6 nor Qt5 found - gadget will not be built")
        return()
    endif()
    set(QT_VERSION_MAJOR 5)
else()
    set(QT_VERSION_MAJOR 6)
    message(STATUS "Using Qt6")
endif()

# Try to find KDE Frameworks (optional for basic build)
if(QT_VERSION_MAJOR EQUAL 6)
    find_package(ECM QUIET NO_MODULE)
    if(ECM_FOUND)
        set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
        include(KDEInstallDirs)
        include(KDECMakeSettings)
        include(KDECompilerSettings NO_POLICY_SCOPE)

        find_package(KF6 QUIET COMPONENTS
            Config
            CoreAddons
            I18n
            WindowSystem
            DBusAddons
        )

        if(KF6_FOUND)
            set(HAVE_KF6 TRUE)
            message(STATUS "KDE Frameworks 6 found")
        endif()
    endif()
endif()

# Check for X11
find_package(X11)
if(X11_FOUND)
    find_package(X11 COMPONENTS XTest)
    if(X11_XTest_FOUND)
        set(HAVE_X11 TRUE)
        add_definitions(-DHAVE_X11)
        message(STATUS "X11 with XTest support enabled")
    else()
        message(WARNING "XTest not found - keyboard/mouse simulation will not be available")
    endif()
else()
    message(WARNING "X11 not found - keyboard/mouse simulation will not be available")
endif()

# Source files
set(openwheel_gadget_SRCS
    src/main.cpp
    src/core/dialcontroller.cpp
    src/core/rotationhandler.cpp
    src/core/actionexecutor.cpp
    src/profiles/profile.cpp
    src/profiles/profilemanager.cpp
    src/profiles/applicationmatcher.cpp
    src/dbus/dbusinterface.cpp
)

# Create executable
add_executable(openwheel-gadget ${openwheel_gadget_SRCS})

target_include_directories(openwheel-gadget PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link Qt libraries
if(QT_VERSION_MAJOR EQUAL 6)
    target_link_libraries(openwheel-gadget
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Qml
        Qt6::Quick
        Qt6::DBus
        Qt6::QuickControls2
    )
else()
    target_link_libraries(openwheel-gadget
        Qt5::Core
        Qt5::Gui
        Qt5::Widgets
        Qt5::Qml
        Qt5::Quick
        Qt5::DBus
        Qt5::QuickControls2
    )
endif()

# Link KDE Frameworks if available
if(HAVE_KF6)
    target_link_libraries(openwheel-gadget
        KF6::ConfigCore
        KF6::CoreAddons
        KF6::I18n
        KF6::WindowSystem
        KF6::DBusAddons
    )
else()
    message(WARNING "Building without KDE Frameworks - some features will be limited")
    # Add a define so we can conditionally compile KF-dependent code
    target_compile_definitions(openwheel-gadget PRIVATE NO_KDE_FRAMEWORKS)
endif()

# Link X11 libraries if available
if(HAVE_X11)
    target_link_libraries(openwheel-gadget
        ${X11_LIBRARIES}
        ${X11_XTest_LIB}
    )
endif()

# Installation
install(TARGETS openwheel-gadget
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT gadget
)

# Install profiles
install(DIRECTORY ${CMAKE_SOURCE_DIR}/profiles/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/openwheel/profiles
    COMPONENT data
    FILES_MATCHING PATTERN "*.json"
)

# Install desktop file if it exists
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/openwheel-gadget.desktop)
    install(FILES openwheel-gadget.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
        COMPONENT data
    )
endif()

# Install icon if it exists
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/openwheel-gadget.png)
    install(FILES openwheel-gadget.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/128x128/apps
        COMPONENT data
    )
endif()

# Install headers for development
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/openwheel-gadget
    COMPONENT gadget-dev
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

message(STATUS "")
message(STATUS "Gadget Configuration Summary:")
message(STATUS "  Qt version: ${QT_VERSION_MAJOR}")
message(STATUS "  KDE Frameworks: ${HAVE_KF6}")
message(STATUS "  X11 support: ${HAVE_X11}")
message(STATUS "")
