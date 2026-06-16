# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.10)

if(OB_BUILD_WIN32)
    # Windows: UAC microphone is captured through WASAPI (usbaudio.sys exposes a standard endpoint).
    target_sources(${OB_TARGET_PAL} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/WasapiUacDevicePort.hpp")
    target_sources(${OB_TARGET_PAL} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/WasapiUacDevicePort.cpp")
else()
    # Linux / Android / macOS: read the UAC isochronous IN endpoint directly via libusb.
    target_sources(${OB_TARGET_PAL} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/ObLibUacDevicePort.hpp")
    target_sources(${OB_TARGET_PAL} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/ObLibUacDevicePort.cpp")
endif()
