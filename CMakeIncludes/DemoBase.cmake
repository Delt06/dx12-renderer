# Add source to this project's executable.
add_executable(${TARGET_NAME} ${HEADER_FILES} ${SOURCE_FILES} ${SHADER_FILES} ${SHADERS_HEADER_FILES})

include(${CMAKE_SOURCE_DIR}/CMakeIncludes/ProjectBasePost.cmake)

# Setup as Windows app
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")

include(${CMAKE_SOURCE_DIR}/CMakeIncludes/CopyDLLs.cmake)
include(${CMAKE_SOURCE_DIR}/CMakeIncludes/CopyAssets.cmake)

# Includes and libraries
target_include_directories(${TARGET_NAME}
        PUBLIC ${EXT_HEADER_FILES}  # headers of external libraries
        PUBLIC "${CMAKE_SOURCE_DIR}/WinPixEventRuntime/Include/WinPixEventRuntime/"
        )