include(FetchContent)

vkgs_declare_archive(glm
    "https://github.com/g-truc/glm/archive/refs/tags/1.0.3.tar.gz"
    "6775e47231a446fd086d660ecc18bcd076531cfedd912fbd66e576b118607001"
)
vkgs_declare_archive(spdlog
    "https://github.com/gabime/spdlog/archive/refs/tags/v1.17.0.tar.gz"
    "d8862955c6d74e5846b3f580b1605d2428b11d97a410d86e2fb13e857cd3a744"
)

vkgs_declare_archive(vulkan_headers
    "https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/vulkan-sdk-1.4.309.0.tar.gz"
    "2bc1b4127950badc80212abf1edfa5c3b5032f3425edf37255863ba7592c1969"
)

set(GLM_ENABLE_CXX_20 ON CACHE BOOL "" FORCE)
set(GLM_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_INSTALL OFF CACHE BOOL "" FORCE)

set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glm spdlog vulkan_headers)
vkgs_enable_msvc_utf8(spdlog)
vkgs_require_msvc_ninja_environment(spdlog)

add_library(vkgs_vulkan_headers INTERFACE)
target_include_directories(vkgs_vulkan_headers SYSTEM INTERFACE "${vulkan_headers_SOURCE_DIR}/include")

if(VKGS_BUILD_TESTS)
    vkgs_declare_archive(googletest
        "https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz"
        "8ad598c73ad796e0d8280b082cebd82a630d73e73cd3c70057938a6501bba5d7"
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(BUILD_GMOCK ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    vkgs_require_msvc_ninja_environment(gtest)
    vkgs_require_msvc_ninja_environment(gtest_main)
    vkgs_require_msvc_ninja_environment(gmock)
    vkgs_require_msvc_ninja_environment(gmock_main)
endif()

if(VKGS_BUILD_VIEWER)
    vkgs_declare_archive(glfw
        "https://github.com/glfw/glfw/archive/refs/tags/3.3.9.tar.gz"
        "a7e7faef424fcb5f83d8faecf9d697a338da7f7a906fc1afbc0e1879ef31bd53"
    )
    vkgs_declare_archive(imgui
        "https://github.com/ocornut/imgui/archive/refs/tags/v1.90.3.tar.gz"
        "40b302d01092c9393373b372fe07ea33ac69e9491893ebab3bf952b2c1f5fd23"
    )

    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(glfw imgui)
    vkgs_require_msvc_ninja_environment(glfw)
endif()

if(VKGS_BUILD_OFFSCREEN_APP OR VKGS_BUILD_TESTS)
    vkgs_declare_archive(nlohmann_json
        "https://github.com/nlohmann/json/archive/refs/tags/v3.12.0.tar.gz"
        "4b92eb0c06d10683f7447ce9406cb97cd4b453be18d7279320f7b2f025c10187"
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()
