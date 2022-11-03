cmake_minimum_required(VERSION 3.8.0)

project("LightingDemo")

set(HEADER_FILES
        include/LightingDemo.h

        include/CBuffers.h
        include/DirectionalLightShadowPassPso.h
        include/ParticleSystem.h
        include/ParticleSystemPso.h
        include/PointLightPass.h
        include/PointLightShadowPassPso.h
        include/PostFxPso.h
        include/Scene.h
        include/SceneRenderer.h
        include/ShadowPassPsoBase.h
        include/SpotLightShadowPassPso.h
        )

set(SOURCE_FILES
        src/LightingDemo.cpp
        src/main.cpp

        src/DirectionalLightShadowPassPso.cpp
        src/ParticleSystem.cpp
        src/ParticleSystemPso.cpp
        src/PointLightPass.cpp
        src/PointLightShadowPassPso.cpp
        src/PostFxPso.cpp
        src/Scene.cpp
        src/SceneRenderer.cpp
        src/ShadowPassPsoBase.cpp
        src/SpotLightShadowPassPso.cpp
        )

set(SHADER_FILES_VERTEX
        shaders/LightingDemo_Blit_VS.hlsl
        shaders/LightingDemo_ParticleSystem_VS.hlsl
        shaders/LightingDemo_PointLight_VS.hlsl
        shaders/LightingDemo_ShadowCaster_VS.hlsl
        shaders/LightingDemo_VS.hlsl
        )

set(SHADER_FILES_PIXEL
        shaders/LightingDemo_ParticleSystem_PS.hlsl
        shaders/LightingDemo_PointLight_PS.hlsl
        shaders/LightingDemo_PostFX_PS.hlsl
        shaders/LightingDemo_PS.hlsl
        shaders/LightingDemo_ShadowCaster_PS.hlsl
        )

set(SHADER_FILES_COMPUTE
        )

set(TARGET_NAME LightingDemo)
include(${CMAKE_SOURCE_DIR}/CMakeIncludes/ProjectBase.cmake)
include(${CMAKE_SOURCE_DIR}/CMakeIncludes/DemoBase.cmake)

target_link_libraries(${TARGET_NAME}
        PUBLIC Framework
        )
