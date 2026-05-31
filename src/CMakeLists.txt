add_subdirectory(shaders)

set(VKGS_COMMON_SOURCES
    CameraController.cpp
    Renderer.cpp
    core/CheckedArithmetic.cpp
    core/FileIO.cpp
    render/PassSizing.cpp
    scene/GpuScene.cpp
    scene/PlyReader.cpp
    vulkan/BarrierBuilder.cpp
    vulkan/Buffer.cpp
    vulkan/DescriptorSet.cpp
    vulkan/DeviceRequirements.cpp
    vulkan/OffscreenRenderTarget.cpp
    vulkan/QueryManager.cpp
    vulkan/Shader.cpp
    vulkan/StringList.cpp
    vulkan/VMA.cpp
    vulkan/VulkanContext.cpp
    vulkan/pipelines/ComputePipeline.cpp
    vulkan/pipelines/Pipeline.cpp
)

if(VKGS_BUILD_VIEWER)
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
        third_party/implot/implot.cpp
        third_party/implot/implot_demo.cpp
        third_party/implot/implot_items.cpp
    )
    add_library(implot::implot ALIAS implot)
    target_include_directories(implot SYSTEM PUBLIC third_party)
    target_link_libraries(implot PUBLIC imgui::imgui)
endif()

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
    target_link_libraries(${target}
        PRIVATE
            Vulkan::Vulkan
    )
    add_dependencies(${target} vkgs_shaders)
    if(CMAKE_DL_LIBS)
        target_link_libraries(${target} PRIVATE ${CMAKE_DL_LIBS})
    endif()
endfunction()

add_library(3dgs_core STATIC
    ${VKGS_COMMON_SOURCES}
    api/OffscreenRenderer.cpp
    session/OffscreenSession.cpp
    DummyGUIManager.cpp
)
add_library(3dgs::core ALIAS 3dgs_core)
set_target_properties(3dgs_core PROPERTIES EXPORT_NAME core)
vkgs_configure_library(3dgs_core OFFSCREEN)

if(VKGS_BUILD_VIEWER)
    add_library(3dgs_viewer_lib STATIC
        ${VKGS_COMMON_SOURCES}
        api/Viewer.cpp
        session/ViewerSession.cpp
        GUIManager.cpp
        vulkan/ImguiManager.cpp
        vulkan/Swapchain.cpp
        vulkan/windowing/GLFWWindow.cpp
    )
    add_library(3dgs::viewer ALIAS 3dgs_viewer_lib)
    vkgs_configure_library(3dgs_viewer_lib ONSCREEN)
    target_compile_definitions(3dgs_viewer_lib PRIVATE VKGS_ENABLE_GLFW)
    target_link_libraries(3dgs_viewer_lib PRIVATE glfw imgui::imgui implot::implot)
endif()

set(VKGS_HEADER_CHECK_SOURCES
    header_checks/BarrierBuilder.cpp
    header_checks/Buffer.cpp
    header_checks/PublicApi.cpp
    header_checks/CheckedArithmetic.cpp
    header_checks/DescriptorSet.cpp
    header_checks/DeviceRequirements.cpp
    header_checks/FileIO.cpp
    header_checks/GpuTypes.cpp
    header_checks/GaussianSceneData.cpp
    header_checks/GpuScene.cpp
    header_checks/Pipeline.cpp
    header_checks/PassSizing.cpp
    header_checks/PlyReader.cpp
    header_checks/Renderer.cpp
    header_checks/RenderConstants.cpp
    header_checks/RenderImageView.cpp
    header_checks/Shader.cpp
    header_checks/OffscreenSession.cpp
    header_checks/StringList.cpp
    header_checks/ViewerSession.cpp
    header_checks/VulkanContext.cpp
)

add_library(vkgs_header_checks OBJECT ${VKGS_HEADER_CHECK_SOURCES})
target_compile_features(vkgs_header_checks PRIVATE cxx_std_20)
target_include_directories(vkgs_header_checks PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)
target_compile_definitions(vkgs_header_checks PRIVATE
    VULKAN_HPP_TYPESAFE_CONVERSION=1
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
    $<$<CONFIG:Debug>:DEBUG>
    $<$<NOT:$<CONFIG:Debug>>:NDEBUG>
)
target_link_libraries(vkgs_header_checks PRIVATE
    vkgs_vulkan_headers
    Vulkan::Vulkan
    spdlog::spdlog_header_only
    glm::glm
    vkgs_generated_shaders
)
add_dependencies(vkgs_header_checks vkgs_shaders)

target_compile_definitions(vkgs_header_checks PRIVATE VKGS_RENDER_MODE_OFFSCREEN)
