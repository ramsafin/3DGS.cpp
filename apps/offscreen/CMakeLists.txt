cmake_minimum_required(VERSION 3.26)
project(3dgs_offscreen)

add_executable(3dgs_render main.cpp)

target_link_libraries(3dgs_render PRIVATE
    3dgs_cpp
    nlohmann_json::nlohmann_json)
