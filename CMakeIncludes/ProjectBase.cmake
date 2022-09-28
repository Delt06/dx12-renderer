# Enable multi-threaded builds
if (MSVC)
    add_compile_options(/MP)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif ()


# Source and header files
if ("${HEADER_FILES}" STREQUAL "")
    FILE(GLOB_RECURSE HEADER_FILES "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")
endif ()

if ("${SOURCE_FILES}" STREQUAL "")
    FILE(GLOB_RECURSE SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
endif ()

# Shaders
if ("${SHADER_FILES_VERTEX}" STREQUAL "")
    FILE(GLOB_RECURSE SHADER_FILES_VERTEX ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*_VS.hlsl)
endif ()

if ("${SHADER_FILES_PIXEL}" STREQUAL "")
    FILE(GLOB_RECURSE SHADER_FILES_PIXEL ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*_PS.hlsl)
endif ()

if ("${SHADER_FILES_COMPUTE}" STREQUAL "")
    FILE(GLOB_RECURSE SHADER_FILES_COMPUTE ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*_CS.hlsl)
endif ()

list(APPEND SHADER_FILES ${SHADER_FILES_VERTEX} ${SHADER_FILES_PIXEL} ${SHADER_FILES_COMPUTE})

source_group("Resources\\Shaders" FILES ${SHADER_FILES})

set_source_files_properties(${SHADER_FILES}
        PROPERTIES
        VS_SHADER_MODEL 5.1
        VS_SHADER_DISABLE_OPTIMIZATIONS $<$<CONFIG:Debug>:ON>
        VS_SHADER_ENABLE_DEBUG $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:ON>
        VS_SHADER_FLAGS "/I ${CMAKE_SOURCE_DIR}/Shaders" # include shader library
        )

if (${SHADERS_OUTPUT_HEADERS}) 
    # use headers
    set_source_files_properties(${SHADER_FILES}
        PROPERTIES
                VS_SHADER_VARIABLE_NAME "ShaderBytecode_%(Filename)"
                VS_SHADER_OUTPUT_HEADER_FILE "${CMAKE_CURRENT_BINARY_DIR}/Shaders/${TARGET_NAME}/%(Filename).h"
        )
else()
    # use object files
    set_source_files_properties(${SHADER_FILES}
        PROPERTIES
                VS_SHADER_OBJECT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/Shaders/%(Filename).cso"
        )
endif()

set_source_files_properties(${SHADER_FILES_VERTEX} PROPERTIES
        VS_SHADER_TYPE Vertex
        )

set_source_files_properties(${SHADER_FILES_PIXEL} PROPERTIES
        VS_SHADER_TYPE Pixel
        )

set_source_files_properties(${SHADER_FILES_COMPUTE} PROPERTIES
        VS_SHADER_TYPE Compute
        )