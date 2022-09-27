cmake_minimum_required(VERSION 3.8.0)

project("DX12Library")

set(TARGET_NAME DX12Library)
include(${CMAKE_CURRENT_SOURCE_DIR}/../ProjectBase/CMakeLists.txt)

# External headers and libraries
set(EXT_HEADER_FILES 
	d3dcompiler.h
)
set(EXT_LIBRARIES 
	d3d12.lib
    d3dcompiler.lib
    shlwapi.lib
    dxgi.lib
    dxguid.lib
    "${CMAKE_CURRENT_SOURCE_DIR}/../WinPixEventRuntime/bin/WinPixEventRuntime.lib"
)

# Add source to this project's executable.
add_library(${TARGET_NAME} STATIC ${HEADER_FILES} ${SOURCE_FILES} ${SHADER_FILES} ${SHADERS_HEADER_FILES})

include(${CMAKE_CURRENT_SOURCE_DIR}/../ProjectBase/Post/CMakeLists.txt)

target_include_directories(${TARGET_NAME}
		PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/DX12Library
)

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
target_link_libraries(${TARGET_NAME} PUBLIC assimp::assimp)

# DirectXTex
find_package(directxtex CONFIG REQUIRED)
target_link_libraries(${TARGET_NAME} PUBLIC Microsoft::DirectXTex)

# DirectXMesh
find_package(directxmesh CONFIG REQUIRED)
target_link_libraries(${TARGET_NAME} PUBLIC Microsoft::DirectXMesh)