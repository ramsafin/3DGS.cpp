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
    target_include_directories(implot SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
    target_link_libraries(implot PUBLIC imgui::imgui)
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
            ${glm_SOURCE_DIR}
            ${spdlog_SOURCE_DIR}/include
            ${vulkan_headers_SOURCE_DIR}/include
            ${VKGS_SHADER_GENERATED_DIR}
    )
    target_compile_definitions(${target}
        PRIVATE
            VULKAN_HPP_TYPESAFE_CONVERSION=1
            VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
            VKGS_RENDER_MODE_${mode}
            $<$<CONFIG:Debug>:DEBUG>
            $<$<NOT:$<CONFIG:Debug>>:NDEBUG>
    )
    target_link_libraries(${target} PRIVATE Vulkan::Vulkan)
    add_dependencies(${target} vkgs_shaders)
    if(CMAKE_DL_LIBS)
        target_link_libraries(${target} PRIVATE ${CMAKE_DL_LIBS})
    endif()
endfunction()
