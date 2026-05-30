set(VKGS_SHADER_SOURCES
    precomp_cov3d.comp
    prefix_sum.comp
    preprocess.comp
    preprocess_sort.comp
    render.comp
    tile_boundary.comp
    sort/hist.comp
    sort/sort.comp
)

set(VKGS_SHADER_INCLUDE_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/common.glsl"
    "${CMAKE_CURRENT_SOURCE_DIR}/shared_constants.glsl"
)

set(VKGS_SHADER_SPV_DIR "${CMAKE_CURRENT_BINARY_DIR}/spv")
set(VKGS_SHADER_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/3dgs")
set(VKGS_SHADER_HEADER "${VKGS_SHADER_GENERATED_DIR}/shaders.h")
set(VKGS_SHADER_EMBED_SCRIPT "${PROJECT_SOURCE_DIR}/tools/embed_shaders.py")

set(VKGS_SHADER_DEFINITIONS
    "$<$<CONFIG:Debug>:-DDEBUG>"
    "$<$<NOT:$<CONFIG:Debug>>:-DNDEBUG>"
)

set(VKGS_SPIRV_FILES)
set(VKGS_EMBED_ARGUMENTS)
set(VKGS_SHADER_SYMBOLS)

foreach(shader IN LISTS VKGS_SHADER_SOURCES)
    get_filename_component(shader_name "${shader}" NAME_WE)
    string(TOUPPER "${shader_name}" shader_name_upper)
    set(shader_symbol "SPV_${shader_name_upper}")
    set(shader_source "${CMAKE_CURRENT_SOURCE_DIR}/${shader}")
    set(shader_spirv "${VKGS_SHADER_SPV_DIR}/${shader_name}.spv")

    list(FIND VKGS_SHADER_SYMBOLS "${shader_symbol}" existing_symbol_index)
    if(NOT existing_symbol_index EQUAL -1)
        message(FATAL_ERROR "Duplicate shader symbol: ${shader_symbol}")
    endif()
    list(APPEND VKGS_SHADER_SYMBOLS "${shader_symbol}")

    add_custom_command(
        OUTPUT "${shader_spirv}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${VKGS_SHADER_SPV_DIR}"
        COMMAND "${VKGS_GLSLANG_VALIDATOR}"
            --target-env vulkan1.2
            -V
            "-I${CMAKE_CURRENT_SOURCE_DIR}"
            ${VKGS_SHADER_DEFINITIONS}
            "${shader_source}"
            -o "${shader_spirv}"
        DEPENDS
            "${shader_source}"
            ${VKGS_SHADER_INCLUDE_FILES}
        COMMENT "Compiling shader ${shader}"
        VERBATIM
        COMMAND_EXPAND_LISTS
    )

    list(APPEND VKGS_SPIRV_FILES "${shader_spirv}")
    list(APPEND VKGS_EMBED_ARGUMENTS --shader "${shader_symbol}=${shader_spirv}")
endforeach()

add_custom_command(
    OUTPUT "${VKGS_SHADER_HEADER}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${VKGS_SHADER_GENERATED_DIR}"
    COMMAND "${Python3_EXECUTABLE}" "${VKGS_SHADER_EMBED_SCRIPT}"
        --output "${VKGS_SHADER_HEADER}"
        ${VKGS_EMBED_ARGUMENTS}
    DEPENDS
        "${VKGS_SHADER_EMBED_SCRIPT}"
        ${VKGS_SPIRV_FILES}
    COMMENT "Embedding SPIR-V shaders"
    VERBATIM
    COMMAND_EXPAND_LISTS
)

add_custom_target(vkgs_shaders DEPENDS "${VKGS_SHADER_HEADER}")

add_library(vkgs_generated_shaders INTERFACE)
add_dependencies(vkgs_generated_shaders vkgs_shaders)
target_include_directories(vkgs_generated_shaders INTERFACE
    "${VKGS_SHADER_GENERATED_DIR}"
)
