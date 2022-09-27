# Copy Shaders
add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_BINARY_DIR}/Shaders
        $<TARGET_FILE_DIR:${TARGET_NAME}>/Shaders)