cmake_minimum_required(VERSION 3.28)
project(3dgs_cpp VERSION 0.1.0 LANGUAGES C CXX)

include(GNUInstallDirs)

include(cmake/Helpers.cmake)
include(cmake/ProjectOptions.cmake)
include(cmake/ToolchainChecks.cmake)
include(cmake/Tooling.cmake)

vkgs_check_msvc_ninja_environment()
vkgs_add_msvc_ninja_environment_target()

# Dependencies
# - GLM
# - spdlog
# - Vulkan Headers
# - GTest (optional)
# - glfw (optional)
# - imgui (optional)
# - nlohmann_json (optional)
include(cmake/Dependencies.cmake)

# Host tools
# - Python Interpreter
# - Vulkan SDK (glslangValidator)
include(cmake/HostTools.cmake)

find_package(Vulkan REQUIRED)

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
vkgs_add_compile_commands_target()

add_subdirectory(src)

if(VKGS_BUILD_VIEWER OR VKGS_BUILD_OFFSCREEN_APP)
    add_subdirectory(apps)
endif()

if(VKGS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

include(cmake/Install.cmake)

vkgs_add_tooling_targets()
vkgs_sync_compile_commands()
