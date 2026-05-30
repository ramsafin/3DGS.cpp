add_executable(3dgs_viewer main.cpp)

target_include_directories(3dgs_viewer PRIVATE third_party)

target_link_libraries(3dgs_viewer PRIVATE spdlog::spdlog 3dgs::3dgs)
