cmake_minimum_required(VERSION 3.8.0)

project("Framework")

set(HEADER_FILES
        include/Framework/Aabb.h
        include/Framework/Blit.h
        include/Framework/Bone.h
        include/Framework/BoundingSphere.h
        include/Framework/EffectBase.h
        include/Framework/Mesh.h
        include/Framework/Model.h
        include/Framework/Shader.h
        )

set(HEADER_FILES
        src/Aabb.cpp
        src/Blit.cpp
        src/Bone.cpp
        src/BoundingSphere.cpp
        src/Framework.cpp
        src/Mesh.cpp
        src/Model.cpp
        src/Shader.cpp
        )

set(SHADER_FILES_VERTEX
        shaders/Blit_VS.hlsl
        )

set(SHADER_FILES_PIXEL
        ./shaders/Blit_PS.hlsl
        )

set(SHADER_FILES_COMPUTE

        )

set(TARGET_NAME Framework)
include(${CMAKE_CURRENT_SOURCE_DIR}/../CMakeIncludes/ProjectBase.cmake)

# Add source to this project's executable.
add_library(${TARGET_NAME} STATIC ${HEADER_FILES} ${SOURCE_FILES} ${SHADER_FILES} ${SHADERS_HEADER_FILES})

include(${CMAKE_CURRENT_SOURCE_DIR}/../CMakeIncludes/ProjectBasePost.cmake)

target_include_directories(${TARGET_NAME}
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/Framework
        )

target_link_libraries(${TARGET_NAME}
        PUBLIC DX12Library
        )