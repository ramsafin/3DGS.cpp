add_executable(vkgs_caps main.cpp)
vkgs_configure_executable(vkgs_caps)

target_compile_definitions(vkgs_caps PRIVATE
    VULKAN_HPP_TYPESAFE_CONVERSION=1
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
)

target_link_libraries(vkgs_caps PRIVATE
    vkgs_vulkan_headers
    Vulkan::Vulkan
    spdlog::spdlog_header_only
    nlohmann_json::nlohmann_json
)

if(CMAKE_DL_LIBS)
    target_link_libraries(vkgs_caps PRIVATE ${CMAKE_DL_LIBS})
endif()
