include(cmake/Helpers.cmake)

# CPU-only tests (run anywhere, no Vulkan device required).
vkgs_add_test(test_ply_parser test_ply_parser.cpp)
vkgs_add_test(test_abi_contracts test_abi_contracts.cpp)
vkgs_add_test(test_shader test_shader.cpp)
vkgs_add_test(test_camera_controller test_camera_controller.cpp)
vkgs_add_test(test_query_manager test_query_manager.cpp)
vkgs_add_test(test_pass_sizing test_pass_sizing.cpp)
vkgs_add_test(test_device_requirements test_device_requirements.cpp)

vkgs_add_test(test_output_filename
    test_output_filename.cpp
    "${PROJECT_SOURCE_DIR}/apps/offscreen/OutputFilename.cpp"
)

vkgs_add_test(test_render_job
    test_render_job.cpp
    "${PROJECT_SOURCE_DIR}/apps/offscreen/PpmWriter.cpp"
    "${PROJECT_SOURCE_DIR}/apps/offscreen/RenderJob.cpp"
)
target_link_libraries(test_render_job PRIVATE nlohmann_json::nlohmann_json)

# Device-gated tests (skip gracefully when no Vulkan device is present).
vkgs_add_test(test_gpu_sort test_gpu_sort.cpp)
vkgs_add_test(test_golden_image test_golden_image.cpp)
vkgs_add_test(test_buffer test_buffer.cpp)

configure_file(ExternalConsumerTest.cmake.in ExternalConsumerTest.cmake @ONLY)
add_test(NAME ExternalConsumer.Build
    COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_BINARY_DIR}/ExternalConsumerTest.cmake"
)
