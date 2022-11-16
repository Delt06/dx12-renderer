cmake_minimum_required(VERSION 3.8.0)

project("Framework")

set(HEADER_FILES
        "include/Framework/Aabb.h"
        
        "include/Framework/Bone.h"
        "include/Framework/BoundingSphere.h"
        
        "include/Framework/Mesh.h"
        "include/Framework/Model.h"
        
        "include/Framework/GameObject.h"
        "include/Framework/Light.h"
        "include/Framework/MatricesCb.h"
        "include/Framework/ModelLoader.h"
        "include/Framework/Animation.h"
        "include/Framework/GraphicsSettings.h"
        "include/Framework/DemoMain.h"
        "include/Framework/Bloom.h"
        "include/Framework/BloomPrefilter.h"
        "include/Framework/BloomDownsample.h"
        "include/Framework/BloomUpsample.h" 
        "include/Framework/CommonRootSignature.h" 
        "include/Framework/PipelineStateBuilder.h" 
        "include/Framework/Shader.h" 
        "include/Framework/Material.h" 
        "include/Framework/ShaderResourceView.h" 
        "include/Framework/ShaderBlob.h"
        "include/Framework/UnorderedAccessView.h"
        "include/Framework/ComputeShader.h"
        "include/Framework/MSAADepthResolvePass.h"
        "include/Framework/SSAOUtils.h"
        "include/Framework/TAA.h"
)

set(SOURCE_FILES
        "src/Aabb.cpp"
        
        "src/Bone.cpp"
        "src/BoundingSphere.cpp"
        "src/Framework.cpp"
        "src/Mesh.cpp"
        "src/Model.cpp"
        
        "src/GameObject.cpp"
        "src/Light.cpp"
        "src/MatricesCb.cpp"
        "src/ModelLoader.cpp"
        "src/Animation.cpp"
        "src/Bloom.cpp"
        "src/BloomPrefilter.cpp"
        "src/BloomDownsample.cpp"
        "src/BloomUpsample.cpp" 
        "src/CommonRootSignature.cpp" 
        "src/PipelineStateBuilder.cpp" 
        "src/Shader.cpp" 
        "src/Material.cpp" 
        "src/ShaderBlob.cpp"
        "src/ComputeShader.cpp"
        "src/MSAADepthResolvePass.cpp"
        "src/SSAOUtils.cpp"
        "src/TAA.cpp"
        )

set(SHADER_FILES_VERTEX
        "shaders/Blit_VS.hlsl"
        )

set(SHADER_FILES_PIXEL
        "shaders/Blit_PS.hlsl"
        "shaders/Bloom_Prefilter_PS.hlsl"
        "shaders/Bloom_Downsample_PS.hlsl"
        "shaders/TAA_Resolve_PS.hlsl"
        )

set(SHADER_FILES_COMPUTE
        "shaders/MSAADepthResolve_CS.hlsl"
        )

set(TARGET_NAME Framework)
set(SHADERS_OUTPUT_HEADERS TRUE)
include(${CMAKE_SOURCE_DIR}/CMakeIncludes/ProjectBase.cmake)

# Add source to this project's executable.
add_library(${TARGET_NAME} STATIC ${HEADER_FILES} ${SOURCE_FILES} ${SHADER_FILES} ${SHADERS_HEADER_FILES})

include(${CMAKE_SOURCE_DIR}/CMakeIncludes/ProjectBasePost.cmake)

target_include_directories(${TARGET_NAME}
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/Framework
        )

target_link_libraries(${TARGET_NAME}
        PUBLIC DX12Library
        )