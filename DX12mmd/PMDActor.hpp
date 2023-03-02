#pragma once

#include <filesystem>
#include "PMD.hpp"
#include "VMD.hpp"
#include "VertexBuffer.hpp"
#include "IndexBuffer.hpp"
#include "ConstantBuffer.hpp"
#include "Texture.hpp"
#include "Resource.hpp"

class PMDActor
{
public:

    PMDActor();

    bool Create(
        ResourceManager* resource_manager, 
        const std::string& model_name, 
        const std::filesystem::path& pmd_filepath,
        const std::filesystem::path& vmd_filepath
    );

    VertexBufferPtr GetVertexBuffer();
    IndexBufferPtr GetIndexBuffer();
    ConstantBufferPtr GetMaterialBuffer();
    const PMDData& GetPMDData() const;
    const VMDMotionTable& GetVMDMotionTable() const;

    void RecursiveMatrixMultiply(
        const BoneTree::BoneNode* node, const DirectX::XMMATRIX& mat
    );
    std::vector<DirectX::XMMATRIX>& GetBoneMetricesForMotion();

    void PlayAnimation();
    void MotionUpdate();

private:

    void CreateTextures();
    void ReadToonTexture(const Material& material, const ResourceDescHandle& handle );

    std::filesystem::path m_PMDModelPath;
    std::filesystem::path m_VMDMotionPath;
    std::string       m_ModelName;
    ResourceManager*  m_ResourceManager;
    PMDData           m_PMDData;
    VMDMotionTable    m_VMDData;
    VertexBufferPtr   m_VertBuff;
    IndexBufferPtr    m_IdxBuff;
    ConstantBufferPtr m_MaterialBuff;

    std::vector<DirectX::XMMATRIX> m_BoneMetricesForMotion;  // モーション用ボーン行列

    TextureGroup      m_TextureManager;
    std::vector<TexturePtr> m_Textures;

    DWORD             m_AnimeStartTimeMs;                    // モーション開始時のミリ秒
};