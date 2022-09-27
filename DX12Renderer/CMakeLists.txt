cmake_minimum_required(VERSION 3.8.0)

project("DX12Renderer")

set(TARGET_NAME DX12Renderer)
include(${CMAKE_CURRENT_SOURCE_DIR}/../CMakeIncludes/ProjectBase.cmake)

# Add source to this project's executable.
add_executable(${TARGET_NAME} ${HEADER_FILES} ${SOURCE_FILES} ${SHADER_FILES} ${SHADERS_HEADER_FILES})

include(${CMAKE_CURRENT_SOURCE_DIR}/../CMakeIncludes/ProjectBasePost.cmake)

# Setup as Windows app
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")

# Includes and libraries
target_include_directories(${TARGET_NAME}
        PUBLIC ${EXT_HEADER_FILES}  # headers of external libraries
        PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../WinPixEventRuntime/Include/WinPixEventRuntime/"
)
target_link_libraries(${TARGET_NAME}
        PUBLIC Framework
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../CMakeIncludes/CopyPixDLL.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../CMakeIncludes/CopyAssets.cmake)
