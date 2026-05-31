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
    vkgs_add_viewer_ui_libraries()
endif()

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
