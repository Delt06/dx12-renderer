cmake_minimum_required(VERSION 3.8.0)

project("DX12Renderer")

set(TARGET_NAME DX12Renderer)
include(${CMAKE_SOURCE_DIR}/CMakeIncludes/ProjectBase.cmake)
include(${CMAKE_SOURCE_DIR}/CMakeIncludes/DemoBase.cmake)

target_link_libraries(${TARGET_NAME}
        PUBLIC Framework
        PUBLIC PhongLighting
        )
