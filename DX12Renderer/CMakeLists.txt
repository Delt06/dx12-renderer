cmake_minimum_required(VERSION 3.8.0)

project("DX12Renderer")

set(TARGET_NAME DX12Renderer)
include(${CMAKE_CURRENT_SOURCE_DIR}/../ProjectBase/CMakeLists.txt)

# External headers and libraries
set(EXT_HEADER_FILES 

)
set(EXT_LIBRARIES
    DX12Library
)

# Add source to this project's executable.
add_executable(${TARGET_NAME} ${HEADER_FILES} ${SOURCE_FILES} ${SHADER_FILES} ${SHADERS_HEADER_FILES})

include(${CMAKE_CURRENT_SOURCE_DIR}/../ProjectBase/Post/CMakeLists.txt)

# Setup as Windows app
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")

# Includes and libraries
target_include_directories(${TARGET_NAME}
        PUBLIC ${EXT_HEADER_FILES}  # headers of external libraries
        PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../WinPixEventRuntime/Include/WinPixEventRuntime/"
)
target_link_libraries(${TARGET_NAME}
        PUBLIC ${EXT_LIBRARIES}
)


# Packages

# assimp
find_package(assimp CONFIG REQUIRED)
target_link_libraries(${TARGET_NAME} PRIVATE assimp::assimp)

# DirectXTex
find_package(directxtex CONFIG REQUIRED)
target_link_libraries(${TARGET_NAME} PRIVATE Microsoft::DirectXTex)

# DirectXMesh
find_package(directxmesh CONFIG REQUIRED)
target_link_libraries(${TARGET_NAME} PRIVATE Microsoft::DirectXMesh)

# WinPixEventRuntime
add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/../WinPixEventRuntime/bin/WinPixEventRuntime.dll
        $<TARGET_FILE_DIR:${TARGET_NAME}>
)

# Copy Assets
add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/../Assets/
        $<TARGET_FILE_DIR:${TARGET_NAME}>/Assets)

# Copy command list presets
add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/${TARGET_NAME}.args.json
        ${CMAKE_BINARY_DIR})