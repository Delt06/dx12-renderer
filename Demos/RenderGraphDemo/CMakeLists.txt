cmake_minimum_required(VERSION 3.8.0)

project("RenderGraphDemo")

set(HEADER_FILES
        include/RenderContext.h
        include/RenderGraphDemo.h
        include/RenderGraphRoot.h
        include/RenderMetadata.h
        include/RenderPass.h
        include/ResourceDescription.h
        include/ResourceId.h
        include/ResourceIds.User.h
        include/ResourcePool.h
        )

set(SOURCE_FILES
        src/main.cpp
        src/RenderGraphDemo.cpp
        src/RenderGraphRoot.cpp
        src/RenderPass.cpp
        src/ResourceId.cpp
        src/ResourcePool.cpp
        )

set(SHADER_FILES_VERTEX
        )

set(SHADER_FILES_PIXEL
        shaders/WhiteShader_PS.hlsl
        shaders/PostProcessing_PS.hlsl
        )

set(SHADER_FILES_COMPUTE
        )

set(TARGET_NAME RenderGraphDemo)
include(${CMAKE_SOURCE_DIR}/CMakeIncludes/ProjectBase.cmake)
include(${CMAKE_SOURCE_DIR}/CMakeIncludes/DemoBase.cmake)

target_link_libraries(${TARGET_NAME}
        PUBLIC Framework
        )
