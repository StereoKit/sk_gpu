function(SKSHADERC_COMPILE_ASSETS TARGET COMMAND_STRING OUT_LIST)
    message(STATUS "sk_gpu compiling shader assets with args '${SKSHADERC_EXE_PATH} ${COMMAND_STRING}'")
    set(SKSHADERC_COMPILE_COMMANDS ${COMMAND_STRING})
    separate_arguments(SKSHADERC_COMPILE_COMMANDS)

    set(SHADER_LIST)
    foreach(SHADER IN LISTS ARGN)
        add_custom_command(
            TARGET ${TARGET} PRE_BUILD
            COMMAND ${SKSHADERC_EXE_PATH} ${SKSHADERC_COMPILE_COMMANDS} ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER}
            VERBATIM
        )
        list(APPEND SHADER_LIST ${SHADER})
    endforeach(SHADER)

    set(${OUT_LIST} ${SHADER_LIST} PARENT_SCOPE)
endfunction()

function(SKSHADERC_COMPILE_HEADERS ADD_TARGET OUTPUT_FOLDER COMMAND_STRING)
    set(SKSHADERC_COMPILE_COMMANDS "-h -o ${OUTPUT_FOLDER} ${COMMAND_STRING}")
    message(STATUS "sk_gpu compiling shader headers with args '${SKSHADERC_EXE_PATH} ${SKSHADERC_COMPILE_COMMANDS}'")
    separate_arguments(SKSHADERC_COMPILE_COMMANDS)

    set(SHADER_LIST)
    foreach(SHADER IN LISTS ARGN)
        get_filename_component(SHADER_NAME ${SHADER} NAME)
        add_custom_command(
            OUTPUT ${OUTPUT_FOLDER}/${SHADER_NAME}.h
            COMMAND ${SKSHADERC_EXE_PATH} ${SKSHADERC_COMPILE_COMMANDS} ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER}
            VERBATIM
        )
        list(APPEND SHADER_LIST ${OUTPUT_FOLDER}/${SHADER_NAME}.h)
    endforeach(SHADER)

    target_include_directories(${ADD_TARGET} PRIVATE ${OUTPUT_FOLDER})
    target_sources            (${ADD_TARGET} PRIVATE ${SHADER_LIST})
endfunction()
