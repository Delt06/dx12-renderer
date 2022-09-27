# Enable multi-threaded builds
if (MSVC)
    add_compile_options(/MP)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()


# Source and header files
FILE(GLOB_RECURSE HEADER_FILES "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")
FILE(GLOB_RECURSE SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

# Shaders
FILE(GLOB_RECURSE SHADER_FILES_VERTEX ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*_VS.hlsl)
FILE(GLOB_RECURSE SHADER_FILES_PIXEL ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*_PS.hlsl)
FILE(GLOB_RECURSE SHADER_FILES_COMPUTE ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*_CS.hlsl)

list (APPEND SHADER_FILES ${SHADER_FILES_VERTEX} ${SHADER_FILES_PIXEL} ${SHADER_FILES_COMPUTE})

source_group("Resources\\Shaders" FILES ${SHADER_FILES})

set_source_files_properties(${SHADER_FILES} 
    PROPERTIES
        VS_SHADER_OBJECT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/Shaders/%(Filename).cso"
        VS_SHADER_MODEL 5.1
        VS_SHADER_DISABLE_OPTIMIZATIONS $<$<CONFIG:Debug>:ON>
        VS_SHADER_ENABLE_DEBUG $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:ON>
)

set_source_files_properties(${SHADER_FILES_VERTEX} PROPERTIES 
    VS_SHADER_TYPE Vertex
)

set_source_files_properties(${SHADER_FILES_PIXEL} PROPERTIES 
    VS_SHADER_TYPE Pixel
)

set_source_files_properties(${SHADER_FILES_COMPUTE} PROPERTIES 
    VS_SHADER_TYPE Compute
)