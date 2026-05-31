include(FetchContent)

set(FETCHCONTENT_QUIET OFF CACHE BOOL "Show FetchContent progress" FORCE)

function(vkgs_declare_archive name url sha256)
    message(VERBOSE "[vkgs] FetchContent ${name}: ${url}")
    FetchContent_Declare(${name}
        URL "${url}"
        URL_HASH "SHA256=${sha256}"
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    )
endfunction()

vkgs_declare_archive(glm
    "https://github.com/g-truc/glm/archive/refs/tags/1.0.0.tar.gz"
    "e51f6c89ff33b7cfb19daafb215f293d106cd900f8d681b9b1295312ccadbd23"
)
vkgs_declare_archive(spdlog
    "https://github.com/gabime/spdlog/archive/refs/tags/v1.13.0.tar.gz"
    "534f2ee1a4dcbeb22249856edfb2be76a1cf4f708a20b0ac2ed090ee24cfdbc9"
)
FetchContent_Declare(vulkan_headers
    URL
    "https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/vulkan-sdk-1.4.309.0.tar.gz"
    URL_HASH
    "SHA256=2bc1b4127950badc80212abf1edfa5c3b5032f3425edf37255863ba7592c1969"
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    SOURCE_SUBDIR unused
)

set(GLM_ENABLE_CXX_20 ON CACHE BOOL "Enable GLM C++20 features" FORCE)
set(GLM_BUILD_LIBRARY OFF CACHE BOOL "Use GLM as a header-only dependency" FORCE)
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_INSTALL OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glm spdlog vulkan_headers)
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
endif()

if(VKGS_BUILD_OFFSCREEN_APP OR VKGS_BUILD_TESTS)
    vkgs_declare_archive(nlohmann_json
        "https://github.com/nlohmann/json/archive/refs/tags/v3.11.3.tar.gz"
        "0d8ef5af7f9794e3263480193c491549b2ba6cc74bb018906202ada498a79406"
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()
