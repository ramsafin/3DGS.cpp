add_executable(3dgs_viewer main.cpp)
vkgs_configure_executable(3dgs_viewer)

target_include_directories(3dgs_viewer PRIVATE third_party)

target_link_libraries(3dgs_viewer PRIVATE spdlog::spdlog_header_only 3dgs::viewer)
