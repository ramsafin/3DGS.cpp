add_executable(3dgs_render main.cpp OutputFilename.cpp PpmWriter.cpp RenderJob.cpp)

target_link_libraries(3dgs_render PRIVATE nlohmann_json::nlohmann_json spdlog::spdlog_header_only 3dgs::core)
