include(GoogleTest)

set(VKGS_TEST_INCLUDE_DIRS
    "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_SOURCE_DIR}/src/third_party"
    "${PROJECT_SOURCE_DIR}/apps/offscreen"
)

set(VKGS_TEST_COMPILE_DEFINITIONS
    VULKAN_HPP_TYPESAFE_CONVERSION=1
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
)

function(vkgs_add_test name)
    if(NOT DEFINED VKGS_GOLDEN_DIR)
        set(VKGS_GOLDEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/golden" PARENT_SCOPE)
        set(VKGS_GOLDEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/golden")
        file(MAKE_DIRECTORY "${VKGS_GOLDEN_DIR}")
    endif()

    add_executable(${name} ${ARGN})
    target_include_directories(${name} PRIVATE ${VKGS_TEST_INCLUDE_DIRS})
    target_compile_definitions(${name} PRIVATE
        ${VKGS_TEST_COMPILE_DEFINITIONS}
        VKGS_RENDER_MODE_OFFSCREEN
        VKGS_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
        VKGS_GOLDEN_DIR="${VKGS_GOLDEN_DIR}"
    )
    target_link_libraries(${name} PRIVATE
        3dgs::core
        vkgs_generated_shaders
        vkgs_vulkan_headers
        Vulkan::Vulkan
        glm::glm
        spdlog::spdlog_header_only
        GTest::gtest_main
    )
    gtest_discover_tests(${name})
endfunction()
