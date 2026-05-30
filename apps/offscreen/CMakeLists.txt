add_executable(3dgs_render main.cpp)

target_link_libraries(3dgs_render PRIVATE nlohmann_json::nlohmann_json spdlog::spdlog 3dgs::3dgs)
