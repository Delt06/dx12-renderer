﻿cmake_minimum_required(VERSION 3.8.0)

project("ToonDemo")

set(HEADER_FILES
        include/ToonDemo.h
        include/CBuffers.h
        include/OutlinePass.h
        include/FXAAPass.h
        include/DirectionalLightShadowsPass.h
        include/SSAOPass.h
        include/DownsamplePass.h
        )

set(SOURCE_FILES
        src/main.cpp
        src/ToonDemo.cpp
        src/OutlinePass.cpp
        src/FXAAPass.cpp
        src/DirectionalLightShadowsPass.cpp
        src/SSAOPass.cpp
        src/DownsamplePass.cpp
        )

set(SHADER_FILES_VERTEX
        shaders/Toon_VS.hlsl
        shaders/DepthNormals_VS.hlsl
        shaders/ShadowCaster_VS.hlsl
        )

set(SHADER_FILES_PIXEL
        shaders/Toon_PS.hlsl
        shaders/DepthNormals_PS.hlsl
        shaders/ShadowCaster_PS.hlsl
        shaders/Outline_PS.hlsl
        shaders/FXAA_PS.hlsl
        shaders/VarianceShadows_GaussianBlur_PS.hlsl
        shaders/SSAO_PS.hlsl
        shaders/SSAO_Blur_PS.hlsl
        shaders/Downsample_PS.hlsl
        )

set(SHADER_FILES_COMPUTE
        )

set(TARGET_NAME ToonDemo)
include(${CMAKE_SOURCE_DIR}/CMakeIncludes/ProjectBase.cmake)
include(${CMAKE_SOURCE_DIR}/CMakeIncludes/DemoBase.cmake)

target_link_libraries(${TARGET_NAME}
        PUBLIC Framework
        )
