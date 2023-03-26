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

    // @brief Z軸を特定の方向に向ける行列を作成する
    // @param lookat 向かせたい方向
    // @param up     上ベクトル
    // @param right  右ベクトル
    // @retval Z軸を特定の方向に向けるための行列
    XMMATRIX CreateLookAtMatrixZ(
        const XMVECTOR& lookat,
        const XMFLOAT3& up,
        const XMFLOAT3& right
    );

    // @brief 任意のベクトルを特定の方向に向ける行列を作成する
    // @param origin 方向を操作したい任意のベクトル
    // @param lookat 向かせたい方向
    // @param up    上ベクトル
    // @param right 右ベクトル
    // @retval  引数origin を特定の方向lookatに向けるための行列
    XMMATRIX CreateLookAtMatrix(
        const XMVECTOR& origin,
        const XMVECTOR& lookat,
        const XMFLOAT3& up,
        const XMFLOAT3& right
    );

    void IKSolve(uint32_t frame_no);
    void SolveLookAt(const PMDIK& ik);
    void SolveCosineIK(const PMDIK& ik);
    void SolveCCDIK(const PMDIK& ik);

    std::filesystem::path m_PMDModelPath;
    std::filesystem::path m_VMDMotionPath;
    std::string       m_ModelName;
    ResourceManager*  m_ResourceManager;
    PMDData           m_PMDData;
    VMDMotionTable    m_VMDData;
    VertexBufferPtr   m_VertBuff;
    IndexBufferPtr    m_IdxBuff;
    ConstantBufferPtr m_MaterialBuff;

    // todo: メモリアライン設定要
    std::vector<DirectX::XMMATRIX> m_BoneMetricesForMotion;  // モーション用ボーン行列

    TextureGroup      m_TextureManager;
    std::vector<TexturePtr> m_Textures;

    DWORD             m_AnimeStartTimeMs;                    // モーション開始時のミリ秒
};