cmake_minimum_required(VERSION 3.8.0)

project("GrassDemo")

set(HEADER_FILES
        include/GrassDemo.h
        include/CBuffers.h
        include/MultiConstantBuffer.h
        )

set(SOURCE_FILES
        src/main.cpp
        src/GrassDemo.cpp
        src/MultiConstantBuffer.cpp
        )

set(SHADER_FILES_VERTEX
        shaders/Grass_VS.hlsl
        )

set(SHADER_FILES_PIXEL
        shaders/Grass_PS.hlsl
        )

set(SHADER_FILES_COMPUTE
        shaders/CullGrass_CS.hlsl
        )

set(TARGET_NAME GrassDemo)
include(${CMAKE_SOURCE_DIR}/CMakeIncludes/ProjectBase.cmake)
include(${CMAKE_SOURCE_DIR}/CMakeIncludes/DemoBase.cmake)

target_link_libraries(${TARGET_NAME}
        PUBLIC Framework
        )
