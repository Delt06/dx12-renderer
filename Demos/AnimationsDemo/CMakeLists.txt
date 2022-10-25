cmake_minimum_required(VERSION 3.8.0)

project("AnimationsDemo")

set(HEADER_FILES
        include/AnimationsDemo.h
        )

set(SOURCE_FILES
        src/AnimationsDemo.cpp
        src/main.cpp
        )

set(SHADER_FILES_VERTEX
        shaders/AnimationsDemo_Bone_VS.hlsl
        shaders/AnimationsDemo_VS.hlsl
        )

set(SHADER_FILES_PIXEL
        shaders/AnimationsDemo_Bone_PS.hlsl
        shaders/AnimationsDemo_PS.hlsl
        )

set(SHADER_FILES_COMPUTE
        )

set(TARGET_NAME AnimationsDemo)
include(${CMAKE_SOURCE_DIR}/CMakeIncludes/ProjectBase.cmake)
include(${CMAKE_SOURCE_DIR}/CMakeIncludes/DemoBase.cmake)

target_link_libraries(${TARGET_NAME}
        PUBLIC Framework
        )
