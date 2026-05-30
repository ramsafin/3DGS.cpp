include(GoogleTest)

# Shared include paths so tests can reach internal renderer headers, the public
# API, the generated SPIR-V header, and third-party single-header libraries.
set(VKGS_TEST_INCLUDE_DIRS
    "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_SOURCE_DIR}/src/third_party"
)

# Vulkan-hpp must use the same dispatch/conversion configuration as the core
# library (whose translation units define the dynamic dispatcher storage).
set(VKGS_TEST_COMPILE_DEFINITIONS
    VULKAN_HPP_TYPESAFE_CONVERSION=1
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
)

set(VKGS_GOLDEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/golden")
file(MAKE_DIRECTORY "${VKGS_GOLDEN_DIR}")

function(vkgs_add_test name)
    add_executable(${name} ${ARGN})
    target_include_directories(${name} PRIVATE ${VKGS_TEST_INCLUDE_DIRS})
    target_compile_definitions(${name} PRIVATE
        ${VKGS_TEST_COMPILE_DEFINITIONS}
        VKGS_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
        VKGS_GOLDEN_DIR="${VKGS_GOLDEN_DIR}"
    )
    target_link_libraries(${name} PRIVATE
        3dgs::3dgs
        vkgs_generated_shaders
        Vulkan::Vulkan
        glm::glm
        spdlog::spdlog
        GTest::gtest_main
    )
    gtest_discover_tests(${name})
endfunction()

# CPU-only tests (run anywhere, no Vulkan device required).
vkgs_add_test(test_ply_parser test_ply_parser.cpp)
vkgs_add_test(test_abi_contracts test_abi_contracts.cpp)

# Device-gated tests (skip gracefully when no Vulkan device is present).
vkgs_add_test(test_gpu_sort test_gpu_sort.cpp)
vkgs_add_test(test_golden_image test_golden_image.cpp)
vkgs_add_test(test_buffer test_buffer.cpp)
