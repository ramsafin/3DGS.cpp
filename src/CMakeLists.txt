include(GNUInstallDirs)

add_subdirectory(shaders)

set(VKGS_CORE_SOURCES
    3dgs.cpp
    GSScene.cpp
    Renderer.cpp
    vulkan/Buffer.cpp
    vulkan/DescriptorSet.cpp
    vulkan/OffscreenRenderTarget.cpp
    vulkan/QueryManager.cpp
    vulkan/Shader.cpp
    vulkan/Swapchain.cpp
    vulkan/Utils.cpp
    vulkan/VMA.cpp
    vulkan/VulkanContext.cpp
    vulkan/pipelines/ComputePipeline.cpp
    vulkan/pipelines/Pipeline.cpp
)

if(VKGS_RENDER_MODE STREQUAL "ONSCREEN")
    list(APPEND VKGS_CORE_SOURCES
        GUIManager.cpp
        vulkan/ImguiManager.cpp
        vulkan/windowing/GLFWWindow.cpp
    )
else()
    list(APPEND VKGS_CORE_SOURCES DummyGUIManager.cpp)
endif()

if(VKGS_RENDER_MODE STREQUAL "ONSCREEN")
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

add_library(3dgs_cpp STATIC ${VKGS_CORE_SOURCES})
add_library(3dgs::3dgs ALIAS 3dgs_cpp)

target_compile_features(3dgs_cpp PUBLIC cxx_std_20)

target_include_directories(3dgs_cpp
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

target_compile_definitions(3dgs_cpp
    PRIVATE
        VULKAN_HPP_TYPESAFE_CONVERSION=1
        VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
        $<$<CONFIG:Debug>:DEBUG>
        $<$<NOT:$<CONFIG:Debug>>:NDEBUG>
)

if(VKGS_RENDER_MODE STREQUAL "ONSCREEN")
    target_compile_definitions(3dgs_cpp PUBLIC VKGS_RENDER_MODE_ONSCREEN VKGS_ENABLE_GLFW)
else()
    target_compile_definitions(3dgs_cpp PUBLIC VKGS_RENDER_MODE_OFFSCREEN)
endif()

target_link_libraries(3dgs_cpp
    PRIVATE
        Vulkan::Vulkan
        spdlog::spdlog
        glm::glm
        vkgs_generated_shaders
)
add_dependencies(3dgs_cpp vkgs_shaders)

if(VKGS_RENDER_MODE STREQUAL "ONSCREEN")
    target_link_libraries(3dgs_cpp PRIVATE glfw imgui::imgui implot::implot)
endif()

if(CMAKE_DL_LIBS)
    target_link_libraries(3dgs_cpp PRIVATE ${CMAKE_DL_LIBS})
endif()
