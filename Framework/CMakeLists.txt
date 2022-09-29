cmake_minimum_required(VERSION 3.8.0)

project("Framework")

set(HEADER_FILES
        "include/Framework/Aabb.h"
        "include/Framework/Blit.h"
        "include/Framework/Bone.h"
        "include/Framework/BoundingSphere.h"
        "include/Framework/EffectBase.h"
        "include/Framework/Mesh.h"
        "include/Framework/Model.h"
        "include/Framework/Shader.h"
        "include/Framework/GameObject.h"
        "include/Framework/Light.h"
        "include/Framework/MatricesCb.h"
        "include/Framework/ModelLoader.h"
        "include/Framework/MaterialBase.h"
        "include/Framework/Animation.h"
        "include/Framework/GraphicsSettings.h"
        "include/Framework/DemoMain.h"
        "include/Framework/Bloom.h"
        "include/Framework/BloomPrefilter.h"
        "include/Framework/BloomDownsample.h"
        "include/Framework/BloomUpsample.h")

set(SOURCE_FILES
        "src/Aabb.cpp"
        "src/Blit.cpp"
        "src/Bone.cpp"
        "src/BoundingSphere.cpp"
        "src/Framework.cpp"
        "src/Mesh.cpp"
        "src/Model.cpp"
        "src/Shader.cpp"
        "src/GameObject.cpp"
        "src/Light.cpp"
        "src/MatricesCb.cpp"
        "src/ModelLoader.cpp"
        "src/Animation.cpp"
        "src/Bloom.cpp"
        "src/BloomPrefilter.cpp"
        "src/BloomDownsample.cpp"
        "src/BloomUpsample.cpp")

set(SHADER_FILES_VERTEX
        "shaders/Blit_VS.hlsl"
        )

set(SHADER_FILES_PIXEL
        "shaders/Blit_PS.hlsl"
        "shaders/Bloom_Prefilter_PS.hlsl"
        "shaders/Bloom_Downsample_PS.hlsl"
        )

set(SHADER_FILES_COMPUTE

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