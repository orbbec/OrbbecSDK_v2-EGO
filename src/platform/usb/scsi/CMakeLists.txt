# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

target_sources(${OB_TARGET_PAL} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/ScsiDevicePort.hpp)

if(OB_BUILD_WIN32)
    target_sources(${OB_TARGET_PAL} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/win/WinScsiDevicePort.hpp
        ${CMAKE_CURRENT_LIST_DIR}/win/WinScsiDevicePort.cpp)
endif()

if(OB_BUILD_LINUX OR OB_BUILD_ANDROID)
    target_sources(${OB_TARGET_PAL} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/linux/LinuxScsiDevicePort.hpp
        ${CMAKE_CURRENT_LIST_DIR}/linux/LinuxScsiDevicePort.cpp)
endif()
