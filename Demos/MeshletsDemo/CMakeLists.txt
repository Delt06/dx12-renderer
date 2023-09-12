cmake_minimum_required(VERSION 3.8.0)

project("MeshletsDemo")

set(HEADER_FILES
        include/MeshletsDemo.h
        include/RenderGraph.User.h
        include/ResourceIds.User.h
        )

set(SOURCE_FILES
        src/main.cpp
        src/MeshletsDemo.cpp
        src/RenderGraph.User.cpp
        )

set(SHADER_FILES_VERTEX
        )

set(SHADER_FILES_PIXEL
        shaders/WhiteShader_PS.hlsl
        shaders/PostProcessing_PS.hlsl
        )

set(SHADER_FILES_COMPUTE
        shaders/ColorSplitBuffer_CS.hlsl
        )

set(TARGET_NAME MeshletsDemo)
include(${CMAKE_SOURCE_DIR}/CMakeIncludes/ProjectBase.cmake)
include(${CMAKE_SOURCE_DIR}/CMakeIncludes/DemoBase.cmake)

target_link_libraries(${TARGET_NAME}
        PUBLIC Framework
        PUBLIC RenderGraph
        )
