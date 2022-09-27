cmake_minimum_required(VERSION 3.8.0)

project("LightingDemo")

set(TARGET_NAME LightingDemo)
include(${CMAKE_SOURCE_DIR}/CMakeIncludes/ProjectBase.cmake)
include(${CMAKE_SOURCE_DIR}/CMakeIncludes/DemoBase.cmake)

target_link_libraries(${TARGET_NAME}
        PUBLIC Framework
        PUBLIC PhongLighting
        )
