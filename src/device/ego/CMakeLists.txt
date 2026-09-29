# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.10)

target_sources(
    ${OB_TARGET_DEVICE}
    PRIVATE  ${CMAKE_CURRENT_LIST_DIR}/EGODevice.hpp
             ${CMAKE_CURRENT_LIST_DIR}/EGODevice.cpp
             ${CMAKE_CURRENT_LIST_DIR}/EgoProDevice.hpp
             ${CMAKE_CURRENT_LIST_DIR}/EgoProDevice.cpp
             ${CMAKE_CURRENT_LIST_DIR}/EgoProEC130Device.hpp
             ${CMAKE_CURRENT_LIST_DIR}/EgoProEC130Device.cpp
             ${CMAKE_CURRENT_LIST_DIR}/EGOVideoSensor.hpp
             ${CMAKE_CURRENT_LIST_DIR}/EGOVideoSensor.cpp
             ${CMAKE_CURRENT_LIST_DIR}/EGOStreamStrategy.hpp
             ${CMAKE_CURRENT_LIST_DIR}/EGOStreamStrategy.cpp
             ${CMAKE_CURRENT_LIST_DIR}/EGOProColorDemuxer.hpp
             ${CMAKE_CURRENT_LIST_DIR}/EGOProColorDemuxer.cpp
             ${CMAKE_CURRENT_LIST_DIR}/EGOProColorDemuxerInterface.hpp
             ${CMAKE_CURRENT_LIST_DIR}/EGOProEC130ColorDemuxer.hpp
             ${CMAKE_CURRENT_LIST_DIR}/EGOProEC130ColorDemuxer.cpp
             ${CMAKE_CURRENT_LIST_DIR}/EGONetDevice.hpp
             ${CMAKE_CURRENT_LIST_DIR}/EGONetDevice.cpp
             ${CMAKE_CURRENT_LIST_DIR}/EgoProNetDevice.hpp
             ${CMAKE_CURRENT_LIST_DIR}/EgoProNetDevice.cpp
             ${CMAKE_CURRENT_LIST_DIR}/EgoProEC130NetDevice.hpp
             ${CMAKE_CURRENT_LIST_DIR}/EgoProEC130NetDevice.cpp
             ${CMAKE_CURRENT_LIST_DIR}/UMIFingerDevice.hpp
             ${CMAKE_CURRENT_LIST_DIR}/UMIFingerDevice.cpp
             ${CMAKE_CURRENT_LIST_DIR}/UMIFingerNetDevice.hpp
             ${CMAKE_CURRENT_LIST_DIR}/UMIFingerNetDevice.cpp
             ${CMAKE_CURRENT_LIST_DIR}/EGOStreamDemuxer.hpp
             ${CMAKE_CURRENT_LIST_DIR}/EGOStreamDemuxer.cpp
             ${CMAKE_CURRENT_LIST_DIR}/EGODepthDevice.hpp
             ${CMAKE_CURRENT_LIST_DIR}/EGODepthDevice.cpp
             ${CMAKE_CURRENT_LIST_DIR}/EGODepthDepthWorkModeManager.hpp
             ${CMAKE_CURRENT_LIST_DIR}/EGODepthDepthWorkModeManager.cpp
             ${CMAKE_CURRENT_LIST_DIR}/EGODepthPresetManager.hpp
             ${CMAKE_CURRENT_LIST_DIR}/EGODepthPresetManager.cpp
             ${CMAKE_CURRENT_LIST_DIR}/EGODepthNetDevice.hpp
             ${CMAKE_CURRENT_LIST_DIR}/EGODepthNetDevice.cpp
             ${CMAKE_CURRENT_LIST_DIR}/EGODeviceInfo.hpp
             ${CMAKE_CURRENT_LIST_DIR}/EGODeviceInfo.cpp
             ${CMAKE_CURRENT_LIST_DIR}/EGOAlgParamManager.hpp
             ${CMAKE_CURRENT_LIST_DIR}/EGOAlgParamManager.cpp
             ${CMAKE_CURRENT_LIST_DIR}/WristDevice.hpp
             ${CMAKE_CURRENT_LIST_DIR}/WristDevice.cpp
             ${CMAKE_CURRENT_LIST_DIR}/WristNetDevice.hpp
             ${CMAKE_CURRENT_LIST_DIR}/WristNetDevice.cpp
             ${CMAKE_CURRENT_LIST_DIR}/WristDeviceInfo.hpp
             ${CMAKE_CURRENT_LIST_DIR}/WristDeviceInfo.cpp
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
             ${CMAKE_CURRENT_LIST_DIR}/boot/EgoProBoadDeviceInfo.hpp
             ${CMAKE_CURRENT_LIST_DIR}/boot/EgoProBoadDeviceInfo.cpp
             ${CMAKE_CURRENT_LIST_DIR}/boot/EgoProRkBootDevice.hpp
             ${CMAKE_CURRENT_LIST_DIR}/boot/EgoProRkBootDevice.cpp
             ${CMAKE_CURRENT_LIST_DIR}/boot/EgoProRkFirmwareUpdater.hpp
             ${CMAKE_CURRENT_LIST_DIR}/boot/EgoProRkFirmwareUpdater.cpp
             ${CMAKE_CURRENT_LIST_DIR}/boot/IRockusbUpgradeBackend.hpp
             ${CMAKE_CURRENT_LIST_DIR}/boot/RockusbDllBackend.hpp
             ${CMAKE_CURRENT_LIST_DIR}/boot/RockusbDllBackend.cpp
             ${CMAKE_CURRENT_LIST_DIR}/boot/EgoProUpgradeApi.h
        )


add_subdirectory(rockchip_upgrade/egopro_dll)
