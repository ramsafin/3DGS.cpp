cmake_minimum_required(VERSION 3.28)
project(3dgs_cpp VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# spdlog's bundled {fmt} requires UTF-8 source/execution charset on MSVC.
if(MSVC)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/utf-8>)
endif()

option(VKGS_BUILD_VIEWER "Build GLFW/ImGui viewer support and the viewer application" ON)
option(VKGS_BUILD_OFFSCREEN_APP "Build the off-screen rendering application" ON)

option(VKGS_BUILD_TESTS "Build unit and integration tests" OFF)
option(VKGS_VERBOSE_CONFIGURE "Print dependency and toolchain resolution details" ON)

include(cmake/Helpers.cmake)

# Dependencies
# - GLM
# - spdlog
# - Vulkan Headers
# - GTest (optional)
# - glfw (optional)
# - imgui (optional)
# - nlohmann_json (optional)
include(cmake/Dependencies.cmake)

find_package(Vulkan REQUIRED)

# Host tools
# - Python Interpreter
# - Vulkan SDK (glslangValidator)
include(cmake/HostTools.cmake)

message(STATUS "")
vkgs_status("Target system" "${CMAKE_SYSTEM_NAME}")
vkgs_status("Target arch/platform" "${CMAKE_SYSTEM_PROCESSOR}")
vkgs_status("Target compiler" "${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
vkgs_status("C++ compiler" "${CMAKE_CXX_COMPILER}")
vkgs_status("Generator" "${CMAKE_GENERATOR}")
vkgs_status("Build type" "${CMAKE_BUILD_TYPE}")
vkgs_status("Build viewer" "${VKGS_BUILD_VIEWER}")
vkgs_status("Build off-screen app" "${VKGS_BUILD_OFFSCREEN_APP}")
vkgs_status("Vulkan include dirs" "${Vulkan_INCLUDE_DIRS}")
vkgs_status("Vulkan library" "${Vulkan_LIBRARY}")
vkgs_status("Host glslangValidator" "${VKGS_GLSLANG_VALIDATOR}")
vkgs_status("Host Python" "${VKGS_HOST_PYTHON_EXECUTABLE}")
message(STATUS "")

if(CMAKE_SYSTEM_NAME STREQUAL "OHOS")
    vkgs_status("OHOS SDK native root" "$ENV{OHOS_SDK_NATIVE}")
    vkgs_status("OHOS architecture" "${OHOS_ARCH}")
    vkgs_status("OHOS STL" "${OHOS_STL}")
    message(STATUS "")
endif()

include(cmake/TargetHelpers.cmake)

if(CMAKE_EXPORT_COMPILE_COMMANDS)
    add_custom_target(vkgs_refresh_compile_commands
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_SOURCE_DIR}/build"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_BINARY_DIR}/compile_commands.json"
            "${PROJECT_SOURCE_DIR}/build/compile_commands.json"
        COMMENT "Refreshing stable clang tooling compilation database"
        VERBATIM
    )
endif()

add_subdirectory(src)

if(VKGS_BUILD_VIEWER OR VKGS_BUILD_OFFSCREEN_APP)
    add_subdirectory(apps)
endif()

if(VKGS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

include(cmake/Install.cmake)
