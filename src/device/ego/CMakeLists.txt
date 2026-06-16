# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.10)

target_sources(
    ${OB_TARGET_DEVICE}
    PRIVATE  ${CMAKE_CURRENT_LIST_DIR}/EGODevice.hpp
             ${CMAKE_CURRENT_LIST_DIR}/EGODevice.cpp
             ${CMAKE_CURRENT_LIST_DIR}/EGONetDevice.hpp
             ${CMAKE_CURRENT_LIST_DIR}/EGONetDevice.cpp
             ${CMAKE_CURRENT_LIST_DIR}/EGODeviceInfo.hpp
             ${CMAKE_CURRENT_LIST_DIR}/EGODeviceInfo.cpp
        )
