function(vkgs_add_viewer_ui_libraries)
    add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
    )
    add_library(imgui::imgui ALIAS imgui)
    vkgs_require_msvc_ninja_environment(imgui)
    target_include_directories(imgui SYSTEM PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    )
    target_link_libraries(imgui PRIVATE Vulkan::Vulkan glfw)

    add_library(implot STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/implot/implot.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/implot/implot_demo.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/implot/implot_items.cpp
    )
    add_library(implot::implot ALIAS implot)
    vkgs_require_msvc_ninja_environment(implot)
    target_include_directories(implot SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
    target_link_libraries(implot PUBLIC imgui::imgui)
endfunction()

function(vkgs_configure_executable target)
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_link_libraries(${target} PRIVATE vkgs_project_options vkgs_project_warnings)
    vkgs_require_msvc_ninja_environment(${target})
endfunction()

function(vkgs_configure_library target mode)
    target_compile_features(${target} PUBLIC cxx_std_20)
    target_include_directories(${target}
        PUBLIC
            $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    )
    target_compile_definitions(${target}
        PRIVATE
            VULKAN_HPP_TYPESAFE_CONVERSION=1
            VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
            VKGS_RENDER_MODE_${mode}
            $<$<CONFIG:Debug>:DEBUG>
            $<$<NOT:$<CONFIG:Debug>>:NDEBUG>
    )
    target_link_libraries(${target}
        PRIVATE
            "$<BUILD_INTERFACE:vkgs_project_options>"
            "$<BUILD_INTERFACE:vkgs_project_warnings>"
            "$<BUILD_INTERFACE:vkgs_vulkan_headers>"
            Vulkan::Vulkan
            "$<BUILD_INTERFACE:glm::glm>"
            "$<BUILD_INTERFACE:spdlog::spdlog_header_only>"
            "$<BUILD_INTERFACE:vkgs_generated_shaders>"
    )
    add_dependencies(${target} vkgs_shaders)
    vkgs_require_msvc_ninja_environment(${target})
    if(CMAKE_DL_LIBS)
        target_link_libraries(${target} PRIVATE ${CMAKE_DL_LIBS})
    endif()
endfunction()

function(vkgs_add_header_check_target target mode)
    set(options)
    set(one_value_args)
    set(multi_value_args ROOTS)
    cmake_parse_arguments(VKGS_HEADER_CHECK "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

    if(NOT VKGS_HEADER_CHECK_ROOTS)
        message(FATAL_ERROR "vkgs_add_header_check_target requires at least one ROOTS entry")
    endif()

    set(headers)
    foreach(root IN LISTS VKGS_HEADER_CHECK_ROOTS)
        file(GLOB_RECURSE root_headers CONFIGURE_DEPENDS "${root}/*.hpp")
        list(APPEND headers ${root_headers})
    endforeach()
    list(REMOVE_DUPLICATES headers)
    list(SORT headers)

    set(check_dir "${CMAKE_CURRENT_BINARY_DIR}/${target}")
    file(MAKE_DIRECTORY "${check_dir}")

    set(sources)
    foreach(header IN LISTS headers)
        file(RELATIVE_PATH header_id "${PROJECT_SOURCE_DIR}" "${header}")
        string(MAKE_C_IDENTIFIER "${header_id}" source_name)
        set(source "${check_dir}/${source_name}.cpp")

        if(header MATCHES "^${PROJECT_SOURCE_DIR}/include/")
            file(RELATIVE_PATH include_name "${PROJECT_SOURCE_DIR}/include" "${header}")
            file(WRITE "${source}" "#include <${include_name}>\n")
        else()
            file(RELATIVE_PATH include_name "${CMAKE_CURRENT_SOURCE_DIR}" "${header}")
            file(WRITE "${source}" "#include \"${include_name}\"\n")
        endif()

        list(APPEND sources "${source}")
    endforeach()

    add_library(${target} OBJECT ${sources})
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_include_directories(${target} PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    )
    target_compile_definitions(${target} PRIVATE
        VULKAN_HPP_TYPESAFE_CONVERSION=1
        VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
        VKGS_RENDER_MODE_${mode}
        $<$<CONFIG:Debug>:DEBUG>
        $<$<NOT:$<CONFIG:Debug>>:NDEBUG>
    )
    target_link_libraries(${target} PRIVATE
        vkgs_project_options
        vkgs_project_warnings
        vkgs_vulkan_headers
        Vulkan::Vulkan
        glm::glm
        spdlog::spdlog_header_only
        vkgs_generated_shaders
    )
    add_dependencies(${target} vkgs_shaders)
    vkgs_require_msvc_ninja_environment(${target})
endfunction()
