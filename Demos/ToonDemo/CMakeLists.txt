cmake_minimum_required(VERSION 3.8.0)

project("ToonDemo")

set(HEADER_FILES
        include/ToonDemo.h
        include/CBuffers.h
        )

set(SOURCE_FILES
        src/ToonDemo.cpp
        src/main.cpp
        )

set(SHADER_FILES_VERTEX
        shaders/Toon_VS.hlsl
        shaders/DepthOnly_VS.hlsl
        )

set(SHADER_FILES_PIXEL
        shaders/Toon_PS.hlsl
        shaders/DepthOnly_PS.hlsl
        )

set(SHADER_FILES_COMPUTE
        )

set(TARGET_NAME ToonDemo)
include(${CMAKE_SOURCE_DIR}/CMakeIncludes/ProjectBase.cmake)
include(${CMAKE_SOURCE_DIR}/CMakeIncludes/DemoBase.cmake)

target_link_libraries(${TARGET_NAME}
        PUBLIC Framework
        )
