cmake_minimum_required(VERSION 3.10)

add_library(camadapt STATIC)

target_include_directories(camadapt
    PUBLIC
        "$<BUILD_INTERFACE:${CAMPARAMS_PUBLIC_HEADERS_DIR}>"
        "$<INSTALL_INTERFACE:include>"
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CAMPARAMS_3RDPARTY_DIR}
)

target_sources(camadapt PRIVATE
    FileIo.cpp
    ImageTransform.cpp
    ParamTransform.cpp
    ConvertJsonGen.cpp
    ConvertJsonValidate.cpp
    YamlSerialization.cpp
    CamParamsAPI.cpp
)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR OR NOT DEFINED CMAKE_SOURCE_DIR)
    install(TARGETS camadapt ARCHIVE DESTINATION lib)
    install(DIRECTORY ${CAMPARAMS_PUBLIC_HEADERS_DIR}/camadapt DESTINATION include)
endif()