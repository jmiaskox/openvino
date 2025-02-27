# Copyright (C) 2018-2020 Intel Corporation
# SPDX-License-Identifier: Apache-2.0
#

set(TARGET_NAME ie_docs_snippets)

if(CMAKE_COMPILER_IS_GNUCXX)
    ie_add_compiler_flags(-Wall)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR OV_COMPILER_IS_CLANG)
    ie_add_compiler_flags(-Wno-unused-variable)
    if(CMAKE_COMPILER_IS_GNUCXX)
        ie_add_compiler_flags(-Wno-unused-variable -Wno-unused-but-set-variable)
    endif()
endif()

file(GLOB SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
                  "${CMAKE_CURRENT_SOURCE_DIR}/gpu/*.cpp")

# remove GPU remote snippets if OpenCL hasn't been found
if (NOT TARGET OpenCL::OpenCL)
    list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/gpu/context_sharing_va.cpp"
                             "${CMAKE_CURRENT_SOURCE_DIR}/gpu/context_sharing.cpp"
                             "${CMAKE_CURRENT_SOURCE_DIR}/gpu/preprocessing.cpp"
                             "${CMAKE_CURRENT_SOURCE_DIR}/gpu/queue_sharing.cpp"
                             "${CMAKE_CURRENT_SOURCE_DIR}/gpu/remote_objects_creation.cpp")
endif()

# try to find VA libraries
if(NOT ANDROID)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_search_module(libva QUIET IMPORTED_TARGET libva)
        if(libva_FOUND)
            message(STATUS "${PKG_CONFIG_EXECUTABLE}: libva (${libva_VERSION}) is found at ${libva_PREFIX}")
        endif()
    endif()
endif()

# remove OpenCV related sources
find_package(OpenCV QUIET COMPONENTS core imgcodecs)
if(NOT OpenCV_FOUND)
    list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/ShapeInference.cpp")
endif()

# requires mfxFrameSurface1 and MSS API
list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/dldt_optimization_guide2.cpp"
                         "${CMAKE_CURRENT_SOURCE_DIR}/dldt_optimization_guide3.cpp"
                         "${CMAKE_CURRENT_SOURCE_DIR}/dldt_optimization_guide4.cpp")

# build separatelly as ov_integration_snippet and ov_integration_snippet_c
list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.c"
                         "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

# create a static library

add_library(${TARGET_NAME} STATIC ${SOURCES})
target_include_directories(${TARGET_NAME} PRIVATE "${OpenVINO_SOURCE_DIR}/src/inference/include/ie"
                                                  "${OpenVINO_SOURCE_DIR}/src/inference/include"
                                                  "${OpenVINO_SOURCE_DIR}/src/inference/dev_api"
                                                  "${OpenVINO_SOURCE_DIR}/src/core/include"
                                                  "${OpenVINO_SOURCE_DIR}/src/bindings/c/include"
                                                  "${OpenVINO_SOURCE_DIR}/src/common/transformations/include"
                                                  "${OpenVINO_SOURCE_DIR}/src/common/preprocessing/src"
                                                  "${OpenVINO_SOURCE_DIR}/src/common/util/include"
                                                  "${OpenVINO_SOURCE_DIR}/src/common/low_precision_transformations/include"
                                                  "${OpenVINO_SOURCE_DIR}/src/frontends/common/include"
                                                  "${OpenVINO_SOURCE_DIR}/src/core/template_extension/new/")

if(TARGET OpenCL::OpenCL)
    target_link_libraries(${TARGET_NAME} PRIVATE OpenCL::OpenCL)

    if(libva_FOUND)
        target_compile_definitions(${TARGET_NAME} PRIVATE ENABLE_LIBVA)
        target_link_libraries(${TARGET_NAME} PRIVATE PkgConfig::libva)
    endif()

    if(WIN32)
        target_compile_definitions(${TARGET_NAME} PRIVATE ENABLE_DX11)
        target_link_libraries(${TARGET_NAME} PRIVATE d3d11 dxgi)
    endif()
endif()

if(OpenCV_FOUND)
    target_include_directories(${TARGET_NAME} SYSTEM PRIVATE ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(${TARGET_NAME} PRIVATE opencv_core)
endif()

# ov_ncc_naming_style(FOR_TARGET "${TARGET_NAME}"
#                     SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
#                     ADDITIONAL_INCLUDE_DIRECTORIES
#                         $<TARGET_PROPERTY:openvino::runtime,INTERFACE_INCLUDE_DIRECTORIES>)

# Detect OpenVINO
find_package(OpenVINO QUIET
             PATHS "${CMAKE_BINARY_DIR}"
             NO_DEFAULT_PATH)
if(NOT OpenVINO_FOUND)
    set(OpenVINO_DIR ${CMAKE_BINARY_DIR})
endif()

#
# Example
#

set(TARGET_NAME "ov_integration_snippet")
# [cmake:integration_example_cpp]
cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 11)

find_package(OpenVINO REQUIRED)

add_executable(${TARGET_NAME} src/main.cpp)

target_link_libraries(${TARGET_NAME} PRIVATE openvino::runtime)

# [cmake:integration_example_cpp]

set(TARGET_NAME_C "ov_integration_snippet_c")
# [cmake:integration_example_c]
cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 11)

find_package(OpenVINO REQUIRED)

add_executable(${TARGET_NAME_C} src/main.c)

target_link_libraries(${TARGET_NAME_C} PRIVATE openvino::runtime::c)

# [cmake:integration_example_c]
