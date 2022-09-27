# WinPixEventRuntime
add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/../WinPixEventRuntime/bin/WinPixEventRuntime.dll
        $<TARGET_FILE_DIR:${TARGET_NAME}>
        )