cmake_minimum_required(VERSION 3.28)
project(3dgs_cpp VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

include(FetchContent)

set(VKGS_RENDER_MODE "ONSCREEN" CACHE STRING "Rendering mode: ONSCREEN or OFFSCREEN")
set_property(CACHE VKGS_RENDER_MODE PROPERTY STRINGS ONSCREEN OFFSCREEN)
if(NOT VKGS_RENDER_MODE STREQUAL "ONSCREEN" AND
   NOT VKGS_RENDER_MODE STREQUAL "OFFSCREEN")
    message(FATAL_ERROR "VKGS_RENDER_MODE must be ONSCREEN or OFFSCREEN")
endif()

option(VKGS_BUILD_APPS "Build repository applications" ON)
option(VKGS_BUILD_TESTS "Build unit and integration tests" OFF)
option(VKGS_VERBOSE_CONFIGURE "Print dependency and toolchain resolution details" ON)

find_package(Vulkan REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

find_program(VKGS_GLSLANG_VALIDATOR
    NAMES glslangValidator glslangValidator.exe
    HINTS
        "$ENV{VULKAN_SDK}/Bin"
        "$ENV{VULKAN_SDK}/bin"
    NO_CMAKE_FIND_ROOT_PATH
    REQUIRED
)

function(vkgs_status label value)
    if(VKGS_VERBOSE_CONFIGURE)
        message(STATUS "[vkgs] ${label}: ${value}")
    endif()
endfunction()

vkgs_status("Generator" "${CMAKE_GENERATOR}")
vkgs_status("Build type" "${CMAKE_BUILD_TYPE}")
vkgs_status("C compiler" "${CMAKE_C_COMPILER}")
vkgs_status("C++ compiler" "${CMAKE_CXX_COMPILER}")
vkgs_status("Render mode" "${VKGS_RENDER_MODE}")
vkgs_status("Build apps" "${VKGS_BUILD_APPS}")
vkgs_status("Vulkan include dirs" "${Vulkan_INCLUDE_DIRS}")
vkgs_status("Vulkan library" "${Vulkan_LIBRARY}")
vkgs_status("Host glslangValidator" "${VKGS_GLSLANG_VALIDATOR}")
vkgs_status("Host Python" "${Python3_EXECUTABLE}")

if (CMAKE_SYSTEM_NAME STREQUAL "OHOS")
    vkgs_status("OHOS SDK native root" "$ENV{OHOS_SDK_NATIVE}")
    vkgs_status("OHOS architecture" "${OHOS_ARCH}")
    vkgs_status("OHOS STL" "${OHOS_STL}")
endif()

include(cmake/Dependencies.cmake)
add_subdirectory(src)

if(VKGS_BUILD_APPS)
    add_subdirectory(apps)
endif()

if(VKGS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

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
