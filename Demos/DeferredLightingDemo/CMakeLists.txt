cmake_minimum_required(VERSION 3.8.0)

project("DeferredLightingDemo")

set(HEADER_FILES
        include/DeferredLightingDemo.h
        include/Reflections.h
        include/Ssao.h
        include/Taa.h
        include/TaaCBuffer.h

        include/HDR/AutoExposure.h
        include/HDR/ToneMapping.h

        include/PBR/BrdfIntegration.h
        include/PBR/DiffuseIrradiance.h
        include/PBR/PreFilterEnvironment.h

        include/SSR/Ssr.h
        include/SSR/SsrBlurPass.h
        include/SSR/SsrTrace.h
        )

set(SOURCE_FILES
        src/main.cpp
        src/DeferredLightingDemo.cpp

        src/Reflections.cpp
        src/Ssao.cpp
        src/Taa.cpp
        src/TaaCBuffer.cpp

        src/HDR/AutoExposure.cpp
        src/HDR/ToneMapping.cpp

        src/PBR/BrdfIntegration.cpp
        src/PBR/DiffuseIrradiance.cpp
        src/PBR/PreFilterEnvironment.cpp

        src/SSR/Ssr.cpp
        src/SSR/SsrBlurPass.cpp
        src/SSR/SsrTrace.cpp
        )

set(SHADER_FILES_VERTEX
        shaders/DeferredLightingDemo_GBuffer_VS.hlsl
        shaders/DeferredLightingDemo_LightBuffer_Common_VS.hlsl
        shaders/DeferredLightingDemo_LightBuffer_Directional_VS.hlsl
        shaders/DeferredLightingDemo_LightBuffer_LightStencil_VS.hlsl
        shaders/DeferredLightingDemo_Skybox_VS.hlsl
        shaders/IBL_CubeMapSideBlit_VS.hlsl
        shaders/IBL_PreFilterEnvironment_VS.hlsl
        )

set(SHADER_FILES_PIXEL
        shaders/DeferredLightingDemo_GBuffer_PS.hlsl
        shaders/DeferredLightingDemo_LightBuffer_Capsule_PS.hlsl
        shaders/DeferredLightingDemo_LightBuffer_Directional_PS.hlsl
        shaders/DeferredLightingDemo_LightBuffer_LightStencil_PS.hlsl
        shaders/DeferredLightingDemo_LightBuffer_Point_PS.hlsl
        shaders/DeferredLightingDemo_LightBuffer_Reflections_PS.hlsl
        shaders/DeferredLightingDemo_LightBuffer_Spot_PS.hlsl
        shaders/DeferredLightingDemo_Skybox_PS.hlsl
        shaders/HDR_ToneMapping_PS.hlsl
        shaders/IBL_BRDFIntegration_PS.hlsl
        shaders/IBL_DiffuseIrradiance_PS.hlsl
        shaders/IBL_PreFilterEnvironment_PS.hlsl
        shaders/SSAO_Blur_PS.hlsl
        shaders/SSAO_PS.hlsl
        shaders/SSR_Trace_PS.hlsl
        shaders/SSR_BlurPass_PS.hlsl
        shaders/TAA_Resolve_PS.hlsl
        )

set(SHADER_FILES_COMPUTE
        shaders/HDR_AutoExposure_AverageLuminanceHistogram_CS.hlsl
        shaders/HDR_AutoExposure_BuildLuminanceHistogram_CS.hlsl
        )

set(TARGET_NAME DeferredLightingDemo)
include(${CMAKE_SOURCE_DIR}/CMakeIncludes/ProjectBase.cmake)
include(${CMAKE_SOURCE_DIR}/CMakeIncludes/DemoBase.cmake)

target_link_libraries(${TARGET_NAME}
        PUBLIC Framework
        )
