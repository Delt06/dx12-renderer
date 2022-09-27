cmake_minimum_required(VERSION 3.8.0)

project("PhongLighting")

set(HEADER_FILES

        )

set(SOURCE_FILES

        )

set(SHADER_FILES_VERTEX
        )

set(SHADER_FILES_PIXEL
        )

set(SHADER_FILES_COMPUTE
        )

set(TARGET_NAME PhongLighting)
include(${CMAKE_CURRENT_SOURCE_DIR}/../CMakeIncludes/ProjectBase.cmake)

# Add source to this project's executable.
add_library(${TARGET_NAME} STATIC ${HEADER_FILES} ${SOURCE_FILES} ${SHADER_FILES} ${SHADERS_HEADER_FILES})

include(${CMAKE_CURRENT_SOURCE_DIR}/../CMakeIncludes/ProjectBasePost.cmake)

target_include_directories(${TARGET_NAME}
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/PhongLighting
        )

target_link_libraries(${TARGET_NAME}
        PUBLIC DX12Library
        PUBLIC Framework
        )