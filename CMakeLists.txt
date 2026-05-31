cmake_minimum_required(VERSION 3.28)
project(3dgs_cpp VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

include(FetchContent)
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

option(VKGS_BUILD_VIEWER "Build GLFW/ImGui viewer support and the viewer application" ON)
option(VKGS_BUILD_OFFSCREEN_APP "Build the off-screen rendering application" ON)
option(VKGS_BUILD_TESTS "Build unit and integration tests" OFF)
option(VKGS_VERBOSE_CONFIGURE "Print dependency and toolchain resolution details" ON)

find_package(Vulkan REQUIRED)

set(VKGS_HOST_TOOL_HINTS)
if(WIN32)
    file(GLOB VKGS_HOST_PYTHON_HINTS LIST_DIRECTORIES true
        "$ENV{LOCALAPPDATA}/Programs/Python/Python*"
    )
    file(GLOB VKGS_HOST_VULKAN_SDK_HINTS LIST_DIRECTORIES true
        "C:/VulkanSDK/*/Bin"
        "C:/VulkanSDK/*/bin"
    )
    list(APPEND VKGS_HOST_TOOL_HINTS ${VKGS_HOST_VULKAN_SDK_HINTS})
endif()

find_program(VKGS_HOST_PYTHON_EXECUTABLE
    NAMES python3 python python.exe
    HINTS
        ${VKGS_HOST_PYTHON_HINTS}
    NO_CMAKE_FIND_ROOT_PATH
)
if(NOT VKGS_HOST_PYTHON_EXECUTABLE)
    message(FATAL_ERROR
        "Could not find a host Python interpreter. "
        "Set VKGS_HOST_PYTHON_EXECUTABLE to a Python executable that runs on the build machine."
    )
endif()

find_program(VKGS_GLSLANG_VALIDATOR
    NAMES glslangValidator glslangValidator.exe
    HINTS
        "$ENV{VULKAN_SDK}/Bin"
        "$ENV{VULKAN_SDK}/bin"
        ${VKGS_HOST_TOOL_HINTS}
    NO_CMAKE_FIND_ROOT_PATH
)
if(NOT VKGS_GLSLANG_VALIDATOR)
    message(FATAL_ERROR
        "Could not find host glslangValidator. "
        "Set VKGS_GLSLANG_VALIDATOR or install the Vulkan SDK on the build machine."
    )
endif()

function(vkgs_status label value)
    if(VKGS_VERBOSE_CONFIGURE)
        message(STATUS "[vkgs] ${label}: ${value}")
    endif()
endfunction()

vkgs_status("Generator" "${CMAKE_GENERATOR}")
vkgs_status("Build type" "${CMAKE_BUILD_TYPE}")
vkgs_status("C compiler" "${CMAKE_C_COMPILER}")
vkgs_status("C++ compiler" "${CMAKE_CXX_COMPILER}")
vkgs_status("Build viewer" "${VKGS_BUILD_VIEWER}")
vkgs_status("Build off-screen app" "${VKGS_BUILD_OFFSCREEN_APP}")
vkgs_status("Vulkan include dirs" "${Vulkan_INCLUDE_DIRS}")
vkgs_status("Vulkan library" "${Vulkan_LIBRARY}")
vkgs_status("Host glslangValidator" "${VKGS_GLSLANG_VALIDATOR}")
vkgs_status("Host Python" "${VKGS_HOST_PYTHON_EXECUTABLE}")

if (CMAKE_SYSTEM_NAME STREQUAL "OHOS")
    vkgs_status("OHOS SDK native root" "$ENV{OHOS_SDK_NATIVE}")
    vkgs_status("OHOS architecture" "${OHOS_ARCH}")
    vkgs_status("OHOS STL" "${OHOS_STL}")
endif()

include(cmake/Dependencies.cmake)
add_subdirectory(src)

if(VKGS_BUILD_VIEWER OR VKGS_BUILD_OFFSCREEN_APP)
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

install(TARGETS 3dgs_core
    EXPORT 3dgsTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)
install(DIRECTORY include/3dgs DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(EXPORT 3dgsTargets
    FILE 3dgsTargets.cmake
    NAMESPACE 3dgs::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/3dgs"
)
configure_package_config_file(
    cmake/3dgsConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/3dgsConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/3dgs"
)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/3dgsConfigVersion.cmake"
    COMPATIBILITY SameMajorVersion
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/3dgsConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/3dgsConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/3dgs"
)
