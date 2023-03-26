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

    // @brief Z�������̕����Ɍ�����s����쐬����
    // @param lookat ��������������
    // @param up     ��x�N�g��
    // @param right  �E�x�N�g��
    // @retval Z�������̕����Ɍ����邽�߂̍s��
    XMMATRIX CreateLookAtMatrixZ(
        const XMVECTOR& lookat,
        const XMFLOAT3& up,
        const XMFLOAT3& right
    );

    // @brief �C�ӂ̃x�N�g�������̕����Ɍ�����s����쐬����
    // @param origin �����𑀍삵�����C�ӂ̃x�N�g��
    // @param lookat ��������������
    // @param up    ��x�N�g��
    // @param right �E�x�N�g��
    // @retval  ����origin �����̕���lookat�Ɍ����邽�߂̍s��
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

    // todo: �������A���C���ݒ�v
    std::vector<DirectX::XMMATRIX> m_BoneMetricesForMotion;  // ���[�V�����p�{�[���s��

    TextureGroup      m_TextureManager;
    std::vector<TexturePtr> m_Textures;

    DWORD             m_AnimeStartTimeMs;                    // ���[�V�����J�n���̃~���b
};