#pragma once

#include <filesystem>
#include "PMD.hpp"
#include "VertexBuffer.hpp"
#include "IndexBuffer.hpp"
#include "ConstantBuffer.hpp"
#include "Texture.hpp"
#include "Resource.hpp"

class MMDModel
{
public:

    MMDModel();

    bool Create(ResourceManager* resource_manager, const std::string& model_name, const std::filesystem::path& filepath);

    VertexBufferPtr GetVertexBuffer();
    IndexBufferPtr GetIndexBuffer();
    ConstantBufferPtr GetMaterialBuffer();
    const PMDData& GetPMDData() const;

private:

    void CreateTextures();
    void ReadToonTexture(const Material& material, const ResourceDescHandle& handle );

    std::filesystem::path m_ModelPath;
    std::string       m_ModelName;
    ResourceManager*  m_ResourceManager;
    PMDData           m_PMDData;
    VertexBufferPtr   m_VertBuff;
    IndexBufferPtr    m_IdxBuff;
    ConstantBufferPtr m_MaterialBuff;

    TextureGroup      m_TextureManager;
    std::vector<TexturePtr> m_Textures;
};