cmake_minimum_required(VERSION 3.8.0)

project("RenderGraph")

set(HEADER_FILES
        include/RenderGraph/RenderContext.h
        include/RenderGraph/RenderGraphRoot.h
        include/RenderGraph/RenderMetadata.h
        include/RenderGraph/RenderPass.h
        include/RenderGraph/ResourceDescription.h
        include/RenderGraph/ResourceId.h
        include/RenderGraph/ResourcePool.h
        include/RenderGraph/TransientResourceAllocator.h
        )

set(SOURCE_FILES
        src/RenderGraphRoot.cpp
        src/RenderPass.cpp
        src/ResourceId.cpp
        src/ResourcePool.cpp
        src/TransientResourceAllocator.cpp
        )

set(SHADER_FILES_VERTEX
        )

set(SHADER_FILES_PIXEL
        )

set(SHADER_FILES_COMPUTE
        )

set(TARGET_NAME RenderGraph)
set(SHADERS_OUTPUT_HEADERS TRUE)
include(${CMAKE_SOURCE_DIR}/CMakeIncludes/ProjectBase.cmake)

# Add source to this project's executable.
add_library(${TARGET_NAME} STATIC ${HEADER_FILES} ${SOURCE_FILES} ${SHADER_FILES} ${SHADERS_HEADER_FILES})

include(${CMAKE_SOURCE_DIR}/CMakeIncludes/ProjectBasePost.cmake)

target_include_directories(${TARGET_NAME}
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/RenderGraph
        )

target_link_libraries(${TARGET_NAME}
        PUBLIC DX12Library
        PUBLIC Framework
        )

