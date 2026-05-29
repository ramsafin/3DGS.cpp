cmake_minimum_required(VERSION 3.26)
project(3dgs_cpp)

add_subdirectory(shaders)

file(GLOB SOURCE
        *.cpp
        vulkan/*.cpp
        vulkan/pipelines/*.cpp)

list(REMOVE_ITEM SOURCE
        ${CMAKE_CURRENT_SOURCE_DIR}/GUIManager.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DummyGUIManager.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/ImguiManager.cpp)

set(EXTERNAL_SOURCE)

if (VKGS_RENDER_MODE STREQUAL "ONSCREEN")
    list(APPEND SOURCE
            ${CMAKE_CURRENT_SOURCE_DIR}/GUIManager.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/ImguiManager.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/windowing/GLFWWindow.cpp)

    file(GLOB EXTERNAL_SOURCE
            ${imgui_SOURCE_DIR}/*.cpp
            ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
            ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
            third_party/implot/implot.cpp
            third_party/implot/implot_demo.cpp
            third_party/implot/implot_items.cpp)
else ()
    list(APPEND SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/DummyGUIManager.cpp)
endif ()

add_library(3dgs_cpp STATIC
        ${SOURCE}
        ${EXTERNAL_SOURCE}
)

target_compile_definitions(3dgs_cpp PRIVATE
  VULKAN_HPP_TYPESAFE_CONVERSION=1
  VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
)

if (VKGS_RENDER_MODE STREQUAL "ONSCREEN")
    target_compile_definitions(3dgs_cpp PUBLIC VKGS_RENDER_MODE_ONSCREEN VKGS_ENABLE_GLFW)
else ()
    target_compile_definitions(3dgs_cpp PUBLIC VKGS_RENDER_MODE_OFFSCREEN)
endif ()

target_include_directories(3dgs_cpp
        PUBLIC
        ${CMAKE_SOURCE_DIR}/include/3dgs
        PRIVATE
        third_party
        ${Vulkan_INCLUDE_DIRS}
        ${GLM_INCLUDE_DIRS}
        ${glm_SOURCE_DIR}
        ${spdlog_SOURCE_DIR}/include
        ${CMAKE_BINARY_DIR}/shaders
)

if (VKGS_RENDER_MODE STREQUAL "ONSCREEN")
    target_include_directories(3dgs_cpp PRIVATE
            ${imgui_SOURCE_DIR}
            ${imgui_SOURCE_DIR}/backends)
endif ()

add_dependencies(3dgs_cpp shaders)

target_link_libraries(3dgs_cpp PUBLIC Vulkan::Vulkan spdlog::spdlog)
if (VKGS_RENDER_MODE STREQUAL "ONSCREEN")
    target_link_libraries(3dgs_cpp PUBLIC glfw)
endif ()
if (UNIX)
    target_link_libraries(3dgs_cpp PUBLIC ${CMAKE_DL_LIBS})
endif ()
