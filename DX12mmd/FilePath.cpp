#include "FilePath.hpp"

TexturePath GetTexturePathFromModelAndTexPath(
    const std::filesystem::path& model_path,
    const std::filesystem::path& texture_path
)
{
    if (texture_path.empty()) {
        return TexturePath();
    }

    std::wstring path = texture_path;
    TexturePath result;

    wchar_t asterisc = '*';
    auto begin_pos = 0;
    auto separator_pos = path.find(asterisc);

    std::vector<std::filesystem::path> splited_paths;
    while (separator_pos != std::wstring::npos) {
        auto p = path.substr(begin_pos, separator_pos);
        splited_paths.push_back(p);

        begin_pos = separator_pos + 1;
        separator_pos = path.find(asterisc, begin_pos);
    }
    splited_paths.push_back(path.substr(begin_pos));

    for (const auto& p : splited_paths) {
        if (p.extension() == L".sph" ) {
            result.SphereMapPath = model_path.parent_path();
            result.SphereMapPath /= p;
        }
        else if (p.extension() == L".spa") {
            result.AddSphereMapPath = model_path.parent_path();
            result.AddSphereMapPath /= p;
        }
        else {
            result.TexPath = model_path.parent_path();
            result.TexPath /= p;
        }
    }

    return result;
}