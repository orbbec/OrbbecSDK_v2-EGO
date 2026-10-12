cmake_minimum_required(VERSION 3.10)

set(CAMPARAMS_PUBLIC_HEADERS_DIR ${CMAKE_CURRENT_LIST_DIR}/src/include)
set(CAMPARAMS_3RDPARTY_DIR ${CMAKE_CURRENT_LIST_DIR}/src/3rdparty)

add_subdirectory(src/src)

# remove warning as error
get_target_property(compile_options camadapt COMPILE_OPTIONS)
list(REMOVE_ITEM compile_options "-Werror" "/WX" "-Wpedantic")
set_target_properties(camadapt PROPERTIES COMPILE_OPTIONS "${compile_options}")

add_library(camadapt::camadapt ALIAS camadapt)
ob_source_group(camadapt::camadapt)
set_target_properties(camadapt PROPERTIES FOLDER "dependencies")
