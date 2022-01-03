#pragma once

#include <filesystem>
#include <vector>

struct TexturePath
{
    std::filesystem::path TexPath;
    std::filesystem::path SphereMapPath;
    std::filesystem::path AddSphereMapPath;
};

TexturePath GetTexturePathFromModelAndTexPath(
    const std::filesystem::path& model_path,
    const std::filesystem::path& texture_path
);
