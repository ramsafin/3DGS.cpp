#pragma once

#include "GaussianSceneData.hpp"

#include <filesystem>
#include <fstream>

namespace vkgs::scene {

class PlyReader {
  public:
    explicit PlyReader(std::filesystem::path filename);

    [[nodiscard]] GaussianSceneData read() const;
    [[nodiscard]] PlyHeader parseHeaderOnly() const;
    [[nodiscard]] PlyHeader validateHeaderOnly() const;

  private:
    std::filesystem::path filename;

    [[nodiscard]] PlyHeader loadHeader(std::ifstream& plyFile) const;
    static void validateLayout(const PlyHeader& header);
};

} // namespace vkgs::scene
