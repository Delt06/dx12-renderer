cmake_minimum_required(VERSION 3.8.0)

project("DX12Renderer")

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
add_executable(main ${HEADER_FILES} ${SOURCE_FILES} ${SHADER_FILES} ${SHADERS_HEADER_FILES})

# Setup as Windows app
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")

# Includes and libraries
target_include_directories(main
        PRIVATE ${EXT_HEADER_FILES}  # headers of external libraries
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../WinPixEventRuntime/Include/WinPixEventRuntime/"
)
target_link_libraries(main
        PRIVATE ${EXT_LIBRARIES}
)


# Packages

# assimp
find_package(assimp CONFIG REQUIRED)
target_link_libraries(main PRIVATE assimp::assimp)

# DirectXTex
find_package(directxtex CONFIG REQUIRED)
target_link_libraries(main PRIVATE Microsoft::DirectXTex)

# DirectXMesh
find_package(directxmesh CONFIG REQUIRED)
target_link_libraries(main PRIVATE Microsoft::DirectXMesh)

# WinPixEventRuntime
add_custom_command(TARGET main POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/../WinPixEventRuntime/bin/WinPixEventRuntime.dll
        $<TARGET_FILE_DIR:main>
)

# Copy Assets
add_custom_command(TARGET main POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/../Assets/
        $<TARGET_FILE_DIR:main>/Assets)


# Copy Shaders
add_custom_command(TARGET main POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_BINARY_DIR}/Shaders
        $<TARGET_FILE_DIR:main>/Shaders)

# Copy command list presets
add_custom_command(TARGET main POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/main.args.json
        ${CMAKE_BINARY_DIR})

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET main PROPERTY CXX_STANDARD 20)
endif()