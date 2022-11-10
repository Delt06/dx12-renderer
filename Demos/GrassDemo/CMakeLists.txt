cmake_minimum_required(VERSION 3.8.0)

project("GrassDemo")

set(HEADER_FILES
        include/GrassDemo.h
        )

set(SOURCE_FILES
        src/main.cpp
        src/GrassDemo.cpp
        )

set(SHADER_FILES_VERTEX
        )

set(SHADER_FILES_PIXEL
        )

set(SHADER_FILES_COMPUTE
        )

set(TARGET_NAME GrassDemo)
include(${CMAKE_SOURCE_DIR}/CMakeIncludes/ProjectBase.cmake)
include(${CMAKE_SOURCE_DIR}/CMakeIncludes/DemoBase.cmake)

target_link_libraries(${TARGET_NAME}
        PUBLIC Framework
        )
