# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.
#
# Built by default with the SDK. Disable with -DOB_BUILD_EGOPRO_RK_UPGRADE_DLL=OFF.
# Requires the full rockchip_upgrade source tree (this directory's parent).

cmake_minimum_required(VERSION 3.10)

option(OB_BUILD_EGOPRO_RK_UPGRADE_DLL "Build the EgoPro RockUSB upgrade static library" ON)
if(NOT OB_BUILD_EGOPRO_RK_UPGRADE_DLL)
    return()
endif()

# RockUSB firmware upgrade uses the host USB stack.  The SDK's iOS build
# deliberately does not build the USB PAL/libusb target, and iOS cannot use
# the macOS IOKit-based libusb backend in any case.
if(IOS)
    message(STATUS "EgoPro RockUSB firmware upgrade is not supported on iOS; skipping EgoProRkUpgrade")
    return()
endif()

project(EgoProRkUpgrade CXX)

set(RK_SRC_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)

# Shared, license-neutral library sources (main.cpp is the CLI front-end and is
# intentionally excluded).
set(RK_SOURCES
    ${RK_SRC_ROOT}/CRC.cpp
    ${RK_SRC_ROOT}/MD5Checksum.cpp
    ${RK_SRC_ROOT}/RKAndroidDevice.cpp
    ${RK_SRC_ROOT}/RKBoot.cpp
    ${RK_SRC_ROOT}/RKDevice.cpp
    ${RK_SRC_ROOT}/RKImage.cpp
    ${RK_SRC_ROOT}/RKLog.cpp
    ${RK_SRC_ROOT}/RKUpgrade.cpp
)

if(WIN32)
    list(APPEND RK_SOURCES
        ${RK_SRC_ROOT}/os/win/platform.cpp
        ${RK_SRC_ROOT}/os/win/RKComm.cpp
        ${RK_SRC_ROOT}/os/win/RKDisk.cpp
        ${RK_SRC_ROOT}/os/win/RKScan.cpp
    )
elseif(APPLE)
    list(APPEND RK_SOURCES
        ${RK_SRC_ROOT}/os/mac/platform.cpp
        ${RK_SRC_ROOT}/os/mac/RKComm.cpp
        ${RK_SRC_ROOT}/os/mac/RKScan.cpp
    )
else()
    list(APPEND RK_SOURCES
        ${RK_SRC_ROOT}/os/linux/platform.cpp
        ${RK_SRC_ROOT}/os/linux/RKComm.cpp
        ${RK_SRC_ROOT}/os/linux/RKDisk.cpp
        ${RK_SRC_ROOT}/os/linux/RKScan.cpp
    )
endif()

add_library(EgoProRkUpgrade STATIC
    ${CMAKE_CURRENT_LIST_DIR}/EgoProUpgradeApi.cpp
    ${CMAKE_CURRENT_LIST_DIR}/rk_cli_globals.cpp
    ${RK_SOURCES}
)

set_target_properties(EgoProRkUpgrade PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
    CXX_VISIBILITY_PRESET hidden
    POSITION_INDEPENDENT_CODE ON
)

# The reused Rockchip upgrade-tool sources are legacy third-party code and are
# not warning-clean under the SDK's global /W4 /WX (see cmake/compiler_flags.cmake).
# Disable warnings-as-errors for this target only, mirroring how the 3rdparty/*
# libraries strip /WX. /WX- follows the inherited /WX on the MSVC command line,
# so it wins.
if(MSVC)
    target_compile_options(EgoProRkUpgrade PRIVATE /WX-)
endif()

target_include_directories(EgoProRkUpgrade PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${RK_SRC_ROOT}
)

if(WIN32)
    target_link_libraries(EgoProRkUpgrade PRIVATE setupapi)
else()
    # Always use the libusb built from 3rdparty/libusb.  Apart from avoiding a
    # second libusb implementation, this also propagates its header directory
    # to the legacy Rockchip sources on both Linux and macOS.
    # src/platform creates this target before src/device is configured.
    if(NOT TARGET libusb::libusb)
        message(FATAL_ERROR "EgoProRkUpgrade requires the SDK bundled libusb target (libusb::libusb)")
    endif()
    target_link_libraries(EgoProRkUpgrade PRIVATE libusb::libusb)

    # Android's bionic libc provides pthread APIs directly; it has no separate
    # libpthread to link. Desktop Linux still needs an explicit pthread link.
    if(NOT APPLE AND NOT ANDROID)
        target_link_libraries(EgoProRkUpgrade PRIVATE pthread)
    endif()
endif()

# Android RockUSB enumeration must reuse the SDK's Java-authorized USB
# handles; the desktop paths remain self-contained in the legacy scanner.
if(ANDROID)
    target_link_libraries(EgoProRkUpgrade PRIVATE ob::platform)
endif()
