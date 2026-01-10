cmake_minimum_required(VERSION 3.16)

project(openwheel-gadget)

find_package(ECM 5.240 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

find_package(KF6 REQUIRED COMPONENTS KPackage I18n)

kpackage_install_package(package org.openwheel.gadget)
