cmake_minimum_required(VERSION 3.28)
project(3dgs_external_consumer LANGUAGES CXX)

find_package(3dgs CONFIG REQUIRED)

add_executable(3dgs_external_consumer main.cpp)
target_compile_features(3dgs_external_consumer PRIVATE cxx_std_20)
target_link_libraries(3dgs_external_consumer PRIVATE 3dgs::core)
