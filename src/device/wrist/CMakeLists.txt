# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.10)

target_sources(
    ${OB_TARGET_DEVICE}
    PRIVATE  ${CMAKE_CURRENT_LIST_DIR}/WristDevice.hpp
             ${CMAKE_CURRENT_LIST_DIR}/WristDevice.cpp
             ${CMAKE_CURRENT_LIST_DIR}/WristNetDevice.hpp
             ${CMAKE_CURRENT_LIST_DIR}/WristNetDevice.cpp
             ${CMAKE_CURRENT_LIST_DIR}/WristDeviceInfo.hpp
             ${CMAKE_CURRENT_LIST_DIR}/WristDeviceInfo.cpp
        )
