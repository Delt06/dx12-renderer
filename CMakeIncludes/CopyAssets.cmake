# Copy Assets
add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/Assets/
        $<TARGET_FILE_DIR:${TARGET_NAME}>/Assets
        )