function(vkgs_collect_owned_cpp_sources out_var)
    set(patterns
        "${PROJECT_SOURCE_DIR}/include/*.hpp"
        "${PROJECT_SOURCE_DIR}/src/*.cpp"
        "${PROJECT_SOURCE_DIR}/src/*.hpp"
        "${PROJECT_SOURCE_DIR}/apps/*.cpp"
        "${PROJECT_SOURCE_DIR}/apps/*.hpp"
        "${PROJECT_SOURCE_DIR}/tests/*.cpp"
        "${PROJECT_SOURCE_DIR}/tests/*.hpp"
    )

    file(GLOB_RECURSE sources CONFIGURE_DEPENDS ${patterns})

    set(filtered_sources)
    foreach(source IN LISTS sources)
        file(TO_CMAKE_PATH "${source}" normalized_source)
        if(NOT normalized_source MATCHES "/third_party/")
            list(APPEND filtered_sources "${source}")
        endif()
    endforeach()

    list(SORT filtered_sources)
    set(${out_var} ${filtered_sources} PARENT_SCOPE)
endfunction()

function(vkgs_add_compile_commands_target)
    if(NOT CMAKE_EXPORT_COMPILE_COMMANDS)
        return()
    endif()

    set(src "${CMAKE_BINARY_DIR}/compile_commands.json")
    set(dst "${PROJECT_SOURCE_DIR}/build/compile_commands.json")
    add_custom_target(vkgs_refresh_compile_commands ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_SOURCE_DIR}/build"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${src}" "${dst}"
        COMMENT "Refreshing stable clang tooling compilation database"
        VERBATIM
    )
endfunction()

function(vkgs_sync_compile_commands)
    if(NOT CMAKE_EXPORT_COMPILE_COMMANDS)
        return()
    endif()

    set(src "${CMAKE_BINARY_DIR}/compile_commands.json")
    set(dst "${PROJECT_SOURCE_DIR}/build/compile_commands.json")
    if(NOT EXISTS "${src}")
        return()
    endif()

    file(MAKE_DIRECTORY "${PROJECT_SOURCE_DIR}/build")
    execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different "${src}" "${dst}")
endfunction()

function(vkgs_add_missing_tool_target target tool_name)
    add_custom_target(${target}
        COMMAND ${CMAKE_COMMAND} -E echo "${tool_name} was not found on PATH"
        COMMAND ${CMAKE_COMMAND} -E false
        VERBATIM
    )
endfunction()

function(vkgs_add_tooling_targets)
    vkgs_collect_owned_cpp_sources(VKGS_OWNED_CPP_SOURCES)

    find_program(VKGS_CLANG_FORMAT_EXE NAMES clang-format)
    if(VKGS_CLANG_FORMAT_EXE)
        add_custom_target(vkgs_format_check
            COMMAND "${VKGS_CLANG_FORMAT_EXE}" --dry-run --Werror ${VKGS_OWNED_CPP_SOURCES}
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            COMMENT "Checking C++ formatting"
            VERBATIM
            COMMAND_EXPAND_LISTS
        )
        add_custom_target(vkgs_format
            COMMAND "${VKGS_CLANG_FORMAT_EXE}" -i ${VKGS_OWNED_CPP_SOURCES}
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            COMMENT "Formatting C++ sources"
            VERBATIM
            COMMAND_EXPAND_LISTS
        )
    else()
        vkgs_add_missing_tool_target(vkgs_format_check clang-format)
        vkgs_add_missing_tool_target(vkgs_format clang-format)
    endif()

    find_program(VKGS_CLANG_TIDY_EXE NAMES clang-tidy)
    if(VKGS_CLANG_TIDY_EXE)
        add_custom_target(vkgs_tidy
            COMMAND "${VKGS_CLANG_TIDY_EXE}" --verify-config
            COMMAND "${VKGS_CLANG_TIDY_EXE}" -p "${CMAKE_BINARY_DIR}" --quiet ${VKGS_OWNED_CPP_SOURCES}
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            COMMENT "Running clang-tidy"
            VERBATIM
            COMMAND_EXPAND_LISTS
        )
        if(TARGET vkgs_shaders)
            add_dependencies(vkgs_tidy vkgs_shaders)
        endif()
    else()
        vkgs_add_missing_tool_target(vkgs_tidy clang-tidy)
    endif()
endfunction()
