target_include_directories(${TARGET_NAME}
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# Copy Shaders
add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_BINARY_DIR}/Shaders
        $<TARGET_FILE_DIR:${TARGET_NAME}>/Shaders)