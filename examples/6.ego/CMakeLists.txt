# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.10)
project(ob_ego_sample)

add_executable(${PROJECT_NAME} ego_sample.cpp)
add_executable(ob_ego_fw_log_export ego_fw_log_export.cpp)
add_executable(ob_ego_recording_directory ego_recording_directory.cpp)

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 11)
set_property(TARGET ob_ego_fw_log_export PROPERTY CXX_STANDARD 11)
set_property(TARGET ob_ego_recording_directory PROPERTY CXX_STANDARD 11)
target_link_libraries(${PROJECT_NAME} ob::${OB_SDK_LIB_NAME} ob::examples::utils)
target_link_libraries(ob_ego_fw_log_export ob::${OB_SDK_LIB_NAME} ob::examples::utils)
target_link_libraries(ob_ego_recording_directory ob::${OB_SDK_LIB_NAME} ob::examples::utils)

# Cross-platform audio playback backend.
if(WIN32)
    target_link_libraries(${PROJECT_NAME} winmm)
else()
    # Linux / arm64: use ALSA when available (libasound). Falls back to no audio playback otherwise.
    find_package(ALSA QUIET)
    if(ALSA_FOUND)
        target_compile_definitions(${PROJECT_NAME} PRIVATE OB_EGO_HAVE_ALSA)
        target_include_directories(${PROJECT_NAME} PRIVATE ${ALSA_INCLUDE_DIRS})
        target_link_libraries(${PROJECT_NAME} ${ALSA_LIBRARIES})
    else()
        message(STATUS "ALSA not found; ob_ego_sample audio playback disabled (video still works).")
    endif()
endif()

set_target_properties(${PROJECT_NAME} ob_ego_fw_log_export ob_ego_recording_directory PROPERTIES FOLDER "examples")
if(MSVC)
    set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    set_target_properties(ob_ego_fw_log_export PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    set_target_properties(ob_ego_recording_directory PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
endif()

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
install(TARGETS ob_ego_fw_log_export RUNTIME DESTINATION bin)
install(TARGETS ob_ego_recording_directory RUNTIME DESTINATION bin)
