set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(VKGS_BUILD_VIEWER "Build GLFW/ImGui viewer support and the viewer application" ON)
option(VKGS_BUILD_OFFSCREEN_APP "Build the off-screen rendering application" ON)
option(VKGS_BUILD_TESTS "Build unit and integration tests" OFF)
option(VKGS_VERBOSE_CONFIGURE "Print dependency and toolchain resolution details" ON)
option(VKGS_ENABLE_PROJECT_WARNINGS "Enable warnings for project-owned targets" OFF)

add_library(vkgs_project_options INTERFACE)
target_compile_options(vkgs_project_options INTERFACE
    "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>"
)

add_library(vkgs_project_warnings INTERFACE)
if(VKGS_ENABLE_PROJECT_WARNINGS)
    target_compile_options(vkgs_project_warnings INTERFACE
        "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/W4>"
        "$<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wall>"
        "$<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wextra>"
        "$<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wpedantic>"
    )
endif()

function(vkgs_enable_msvc_utf8 target)
    if(TARGET ${target})
        target_compile_options(${target} PRIVATE "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>")
    endif()
endfunction()
