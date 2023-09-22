cmake_minimum_required(VERSION 3.8.0)

project("MeshletsDemo")

set(HEADER_FILES
        include/Meshlet.h
        include/MeshletBuilder.h
        include/MeshletDrawIndirect.h
        include/MeshletsDemo.h
        include/RenderGraph.User.h
        include/Transform.h
        )

set(SOURCE_FILES
        src/main.cpp
        src/MeshletBuilder.cpp
        src/MeshletDrawIndirect.cpp
        src/MeshletsDemo.cpp
        src/RenderGraph.User.cpp
        )

set(SHADER_FILES_VERTEX
        shaders/Debugging/DebugBoundingSphere_VS.hlsl
        shaders/Debugging/DebugBoundingSquare_VS.hlsl
        shaders/Debugging/DebugAABB_VS.hlsl
        shaders/Meshlet_VS.hlsl
        shaders/Occluder_VS.hlsl
        )

set(SHADER_FILES_PIXEL
        shaders/Debugging/SelectedMeshletColor_PS.hlsl
        shaders/HDB_Downsample_PS.hlsl
        shaders/Meshlet_PS.hlsl
        shaders/Occluder_DepthOnly_PS.hlsl
        shaders/Occluder_PS.hlsl
        )

set(SHADER_FILES_COMPUTE
        shaders/MeshletCulling_CS.hlsl
        )

set(TARGET_NAME MeshletsDemo)
include(${CMAKE_SOURCE_DIR}/CMakeIncludes/ProjectBase.cmake)
include(${CMAKE_SOURCE_DIR}/CMakeIncludes/DemoBase.cmake)

target_link_libraries(${TARGET_NAME}
        PUBLIC Framework
        PUBLIC RenderGraph
        )

# meshoptimizer
find_package(meshoptimizer CONFIG REQUIRED)
target_link_libraries(${TARGET_NAME} PUBLIC meshoptimizer::meshoptimizer)