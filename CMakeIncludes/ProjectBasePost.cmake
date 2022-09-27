target_include_directories(${TARGET_NAME}
        PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
        )

# Copy Shaders
if (NOT ("${SHADER_FILES}" STREQUAL ""))
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_BINARY_DIR}/Shaders
            $<TARGET_FILE_DIR:${TARGET_NAME}>/Shaders)
endif ()

if (CMAKE_VERSION VERSION_GREATER 3.12)
    set_property(TARGET ${TARGET_NAME} PROPERTY CXX_STANDARD 20)
endif ()