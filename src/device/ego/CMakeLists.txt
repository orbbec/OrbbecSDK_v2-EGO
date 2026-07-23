# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.10)

target_sources(
    ${OB_TARGET_DEVICE}
    PRIVATE  ${CMAKE_CURRENT_LIST_DIR}/EGODevice.hpp
             ${CMAKE_CURRENT_LIST_DIR}/EGODevice.cpp
             ${CMAKE_CURRENT_LIST_DIR}/EGOColorVideoSensor.hpp
             ${CMAKE_CURRENT_LIST_DIR}/EGOColorVideoSensor.cpp
             ${CMAKE_CURRENT_LIST_DIR}/EGOStreamStrategy.hpp
             ${CMAKE_CURRENT_LIST_DIR}/EGOStreamStrategy.cpp
             ${CMAKE_CURRENT_LIST_DIR}/EGONetDevice.hpp
             ${CMAKE_CURRENT_LIST_DIR}/EGONetDevice.cpp
             ${CMAKE_CURRENT_LIST_DIR}/EGODeviceInfo.hpp
             ${CMAKE_CURRENT_LIST_DIR}/EGODeviceInfo.cpp
             ${CMAKE_CURRENT_LIST_DIR}/EGOAlgParamManager.hpp
             ${CMAKE_CURRENT_LIST_DIR}/EGOAlgParamManager.cpp
             ${CMAKE_CURRENT_LIST_DIR}/boot/EGOBootDevice.hpp
             ${CMAKE_CURRENT_LIST_DIR}/boot/EGOBootDevice.cpp
             ${CMAKE_CURRENT_LIST_DIR}/boot/EGOBootDeviceInfo.hpp
             ${CMAKE_CURRENT_LIST_DIR}/boot/EGOBootDeviceInfo.cpp
             ${CMAKE_CURRENT_LIST_DIR}/boot/EGOBinFirmwareUpdater.hpp
             ${CMAKE_CURRENT_LIST_DIR}/boot/EGOBinFirmwareUpdater.cpp
             ${CMAKE_CURRENT_LIST_DIR}/boot/EGOBinPackage.hpp
             ${CMAKE_CURRENT_LIST_DIR}/boot/EGOBinPackage.cpp
             ${CMAKE_CURRENT_LIST_DIR}/boot/EGOMd5.hpp
             ${CMAKE_CURRENT_LIST_DIR}/boot/EGOMd5.cpp
        )
