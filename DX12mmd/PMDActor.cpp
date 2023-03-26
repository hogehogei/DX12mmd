#include <sstream>
#include <array>
#include <algorithm>

#include "PMDActor.hpp"
#include "FilePath.hpp"

#pragma comment(lib, "winmm.lib")

namespace
{
    void WriteMaterial(void* srcdata, uint8_t* dst)
    {
        PMDData* data = reinterpret_cast<PMDData*>(srcdata);
        const auto& material_data = data->GetMaterialData();
        uint32_t material_buff_size = (PMDData::k_ShaderMaterialSize + 0xFF) & ~0xFF;

        for (const auto& itr : material_data) {
            const uint8_t* src = reinterpret_cast<const uint8_t*>(&itr.MaterialForShader);
            std::copy_n(src, PMDData::k_ShaderMaterialSize, dst);
            dst += material_buff_size;
        }
    }
}

PMDActor::PMDActor()
    :
    m_PMDModelPath(),
    m_VMDMotionPath(),
    m_ModelName(),
    m_ResourceManager(nullptr),
    m_PMDData(),
    m_VertBuff(std::make_shared<VertexBufferPMD>()),
    m_IdxBuff(),
    m_MaterialBuff(),
    m_BoneMetricesForMotion()
{}

bool PMDActor::Create(
    ResourceManager* resource_manager, 
    const std::string& model_name, 
    const std::filesystem::path& pmd_filepath,
    const std::filesystem::path& vmd_filepath
)
{
    if (!m_PMDData.Open(pmd_filepath)) {
        return false;
    }
    if (!m_VMDData.Open(vmd_filepath)) {
        return false;
    }

    m_ResourceManager = resource_manager;
    m_TextureManager.SetShaderResource(m_ResourceManager);
    m_PMDModelPath = pmd_filepath;
    m_VMDMotionPath = vmd_filepath;
    m_ModelName = model_name;

    auto vertbuff = std::make_shared<VertexBufferPMD>();
    if (!vertbuff->CreateVertexBuffer(m_PMDData)) {
        return false;
    }
    m_VertBuff = vertbuff;

    auto idxbuff = std::make_shared<IndexBuffer>();
    if (!idxbuff->CreateIndexBuffer(m_PMDData)) {
        return false;
    }
    m_IdxBuff = idxbuff;

    ResourceOrder material_resource =
    {
        model_name,
        {
            {ResourceOrder::k_ConstantResource, 1, 1},
            {ResourceOrder::k_ShaderResource, 4, 0 }
        },
        m_PMDData.MaterialNum()
    };
    resource_manager->AddResource(material_resource);

    m_MaterialBuff = std::make_shared<ConstantBuffer>();
    m_MaterialBuff->Create(resource_manager, PMDData::k_ShaderMaterialSize, m_PMDData.MaterialNum(), resource_manager->ResourceHandle(model_name));
    ConstantBuffer::WriterFunc func = WriteMaterial;
    m_MaterialBuff->Write((void*)&m_PMDData, func);

    // GPU�]���p�{�[���s��o�b�t�@������
    m_BoneMetricesForMotion.resize(k_BoneMetricesNum);
    std::fill(m_BoneMetricesForMotion.begin(), m_BoneMetricesForMotion.end(), DirectX::XMMatrixIdentity());

    CreateTextures();

    return true;
}

VertexBufferPtr PMDActor::GetVertexBuffer()
{
    return m_VertBuff;
}

IndexBufferPtr PMDActor::GetIndexBuffer()
{
    return m_IdxBuff;
}

ConstantBufferPtr PMDActor::GetMaterialBuffer()
{
    return m_MaterialBuff;
}

const PMDData& PMDActor::GetPMDData() const
{
    return m_PMDData;
}

const VMDMotionTable& PMDActor::GetVMDMotionTable() const
{
    return m_VMDData;
}

void PMDActor::RecursiveMatrixMultiply(
    const BoneTree::BoneNode* node, const DirectX::XMMATRIX& mat
)
{
    if (!node) {
        return;
    }

    m_BoneMetricesForMotion[node->BoneIdx] *= mat;
    
    for (const auto child_node : node->GetChildlen())
    {
        RecursiveMatrixMultiply(child_node, m_BoneMetricesForMotion[node->BoneIdx]);
    }
}

std::vector<DirectX::XMMATRIX>& PMDActor::GetBoneMetricesForMotion()
{
    return m_BoneMetricesForMotion;
}

void PMDActor::PlayAnimation()
{
    m_AnimeStartTimeMs = timeGetTime();
}

void PMDActor::MotionUpdate()
{
    DWORD elapsed_time = timeGetTime() - m_AnimeStartTimeMs;
    DWORD frame_no = 30 * (elapsed_time / 1000.0f);

    if (frame_no > m_VMDData.MaxKeyFrameNo()) {
        PlayAnimation();
        elapsed_time = 0;
    }

    std::fill(m_BoneMetricesForMotion.begin(), m_BoneMetricesForMotion.end(), DirectX::XMMatrixIdentity());
    VMDMotionTable::NowMotionListPtr motions = m_VMDData.GetNowMotionList(frame_no);

    for (const auto& motion : *motions) {
        auto node = m_PMDData.GetBoneFromName(motion.Name);
        if (node) {
            auto idx = node.value().BoneIdx;
            auto& pos = node.value().BoneStartPos;

            DirectX::XMMATRIX rotation;
            DirectX::XMVECTOR offset;
            motion.Slerp(frame_no, &rotation, &offset);
            auto mat =
                DirectX::XMMatrixTranslation(-pos.x, -pos.y, -pos.z) *
                rotation *
                DirectX::XMMatrixTranslation(pos.x, pos.y, pos.z);

            m_BoneMetricesForMotion[idx] = mat * DirectX::XMMatrixTranslationFromVector(offset);
        }
    }

    auto root_bone = m_PMDData.GetBoneFromName("�Z���^�[");
    if (root_bone) {
        RecursiveMatrixMultiply(&(root_bone.value()), DirectX::XMMatrixIdentity());
    }

    IKSolve(frame_no);
}

void PMDActor::CreateTextures()
{
    const auto& materials = m_PMDData.GetMaterialData();
    auto shader_resource_handle = m_ResourceManager->ResourceHandle(m_ModelName);

    for (const auto& m : materials) {
        std::filesystem::path filepath = m.Additional.TexturePath;
        TexturePtr texture_handle;
        TexturePath texture_path;

        texture_path = GetTexturePathFromModelAndTexPath(m_PMDModelPath, filepath);
        
        shader_resource_handle.Offset += 1;
        if (!texture_path.TexPath.empty()) {
            m_TextureManager.CreateTextures(
                texture_path.TexPath,
                &texture_handle
            );
            m_TextureManager.CreateShaderResourceView(texture_handle, m_ResourceManager, shader_resource_handle);
            m_Textures.push_back(texture_handle);
        }
        else {
            m_TextureManager.CreatePlaneTexture(
                L"white",
                &texture_handle
            );
            m_TextureManager.CreateShaderResourceView(texture_handle, m_ResourceManager, shader_resource_handle);
            m_Textures.push_back(texture_handle);
        }

        shader_resource_handle.Offset += 1;
        if (!texture_path.SphereMapPath.empty()) {
            m_TextureManager.CreateTextures(
                texture_path.SphereMapPath,
                &texture_handle
            );
            m_TextureManager.CreateShaderResourceView(texture_handle, m_ResourceManager, shader_resource_handle);
            m_Textures.push_back(texture_handle);
        }
        else {
            m_TextureManager.CreatePlaneTexture(
                L"white",
                &texture_handle
            );
            m_TextureManager.CreateShaderResourceView(texture_handle, m_ResourceManager, shader_resource_handle);
            m_Textures.push_back(texture_handle);
        }

        shader_resource_handle.Offset += 1;
        if (!texture_path.AddSphereMapPath.empty()) {
            m_TextureManager.CreateTextures(
                texture_path.AddSphereMapPath,
                &texture_handle
            );
            m_TextureManager.CreateShaderResourceView(texture_handle, m_ResourceManager, shader_resource_handle);
            m_Textures.push_back(texture_handle);
        }
        else {
            m_TextureManager.CreatePlaneTexture(
                L"black",
                &texture_handle,
                0x00, 0x00, 0x00
            );
            m_TextureManager.CreateShaderResourceView(texture_handle, m_ResourceManager, shader_resource_handle);
            m_Textures.push_back(texture_handle);
        }

        shader_resource_handle.Offset += 1;
        ReadToonTexture(m, shader_resource_handle);

        shader_resource_handle.Offset += 1;
    }
}

void PMDActor::ReadToonTexture(const Material& material, const ResourceDescHandle& handle)
{
    std::ostringstream os;
    std::filesystem::path filepath;

    // ���ۂ�toon�ԍ��� �L�^���ꂽ�ԍ�+1�����iuint8_t �T�C�Y��Wrap�j
    uint8_t toonidx = (material.Additional.ToonIdx + 1);
    filepath += L"toon/toon";
    os << std::setfill('0') << std::right << std::setw(2) << static_cast<int>(toonidx);

    // string �� wstring �ɕϊ�
    // ASCII �����g���ĂȂ��ƒf���ł���̂ł��̂悤�ɂ��Ă���
    std::string number_str = os.str();
    std::vector<wchar_t> wstr;
    std::copy(number_str.begin(), number_str.end(), std::back_inserter(wstr));
    wstr.push_back(L'\0');

    filepath += wstr.data();
    filepath += L".bmp";

    std::error_code ec;
    bool result = std::filesystem::exists(filepath, ec);
    TexturePtr texture_handle;
    if (!ec && result) {
        m_TextureManager.CreateTextures(
            filepath,
            &texture_handle
        );
        m_TextureManager.CreateShaderResourceView(texture_handle, m_ResourceManager, handle);
        m_Textures.push_back(texture_handle);
    }
    else {
        m_TextureManager.CreateGradationTexture(
            L"gradation",
            &texture_handle
        );
        m_TextureManager.CreateShaderResourceView(texture_handle, m_ResourceManager, handle);
        m_Textures.push_back(texture_handle);
    }
}

// @brief Z�������̕����Ɍ�����s����쐬����
// @param up    ��x�N�g��
// @param right �E�x�N�g��
XMMATRIX PMDActor::CreateLookAtMatrixZ(
    const XMVECTOR& lookat,
    const XMFLOAT3& up,
    const XMFLOAT3& right
)
{
    // ��������������(Z��)
    XMVECTOR vz = lookat;

    // �������������������������Ƃ��́A����Y�x�N�g��
    XMVECTOR vy_tmp = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&up));

    // �O��1��ځB����Y�����g���āA�������������������������Ƃ��̖{����X, Y�x�N�g�����v�Z������
    // 1. Z���Ɖ���Y�����O�ς��āA�܂���X�����v�Z
    XMVECTOR vx = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(vy_tmp, vz));
    // �O��2���
    // 2. Z����X�����O�ς��āA�{����Y�����v�Z
    XMVECTOR vy = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(vz, vx));

    // Lookat��up�����������������Ă�����right����ɂ��č�蒼��
    if ((std::abs(DirectX::XMVector3Dot(vy, vz).m128_f32[0]) - 1.0f) < std::numeric_limits<float>::epsilon()) {
        // �{����X�����v�Z�������B�܂��� right=����X���Ƃ���
        XMVECTOR vx_tmp = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&right));

        // �O��1��� Z���Ɖ���X�����O�ς��āAY�����v�Z
        vy = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(vz, vx_tmp));
        // �O��2��� Z����Y�����O�ς��āA�{����X�����v�Z
        vx = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(vy, vz));
    }

    XMMATRIX ret = DirectX::XMMatrixIdentity();
    ret.r[0] = vx;
    ret.r[1] = vy;
    ret.r[2] = vz;

    return ret;
}

XMMATRIX PMDActor::CreateLookAtMatrix(
    const XMVECTOR& origin,
    const XMVECTOR& lookat,
    const XMFLOAT3& up,
    const XMFLOAT3& right
)
{
    // 1. Z����C�ӂ̃x�N�g���Ɍ�����s����v�Z����
    // 2. 1.�ō쐬�����s��̋t�s����쐬�iZtrans)
    // 3. Z�������̕����Ɍ�����s����쐬 (Zlookat)
    // 4. 1(Ztrans)��4(Zlookat)�ō쐬�����s�����Z�B����ƁA�C�ӂ̃x�N�g��(origin)�� Ztrans -> Zlookat �ɉ�]������s�񂪂ł���B
    //    Ztrans -> Zlookat �ɐ��񂷂�x�N�g�����쐬���Ă���C���[�W
    return DirectX::XMMatrixTranspose(CreateLookAtMatrixZ(origin, up, right)) * CreateLookAtMatrixZ(lookat, up, right);
}

void PMDActor::IKSolve(uint32_t frame_no)
{
    const auto& iks = m_PMDData.GetPMDIKData();
    for (const auto& ik : iks) {
        // IK ON/OFF�f�[�^�m�F
        auto ik_enable_list = m_VMDData.GetIKEnable(frame_no);
        if (ik_enable_list) {
            auto itr = ik_enable_list->IkEnableTable.find(m_PMDData.GetBoneName(ik.BoneIdx));
            if (itr != ik_enable_list->IkEnableTable.end()) {
                // ����OFF�Ȃ�A���̃{�[����IK�������Ȃ�
                if (!itr->second) {
                    continue;
                }
            }
        }

        auto children_nodes_count = ik.NodeIdxes.size();

        switch (children_nodes_count) {
        case 0:     // �Ԃ̃{�[������0(���肦�Ȃ�)
            assert(0);
            break;
        case 1:     // �Ԃ̃{�[������1�̎���LookAt
            SolveLookAt(ik);
            break;
        case 2:     // �Ԃ̃{�[������2�̎��͗]���藝IK
            SolveCosineIK(ik);
            break;
        default:
            SolveCCDIK(ik);
            break;
        }
    }
}

void PMDActor::SolveLookAt(const PMDIK& ik)
{
    // ���̊֐��ɗ������_�Ńm�[�h�͂ЂƂ����Ȃ��A�`�F�[���ɓ����Ă���m�[�h�ԍ���
    // IK�̃��[�g�m�[�h�̂��̂Ȃ̂ŁA���̃��[�g�m�[�h����^�[�Q�b�g�Ɍ������x�N�g�����l����΂悢
    auto root_node = m_PMDData.GetBoneFromIndex(ik.NodeIdxes[0]);
    auto target_node = m_PMDData.GetBoneFromIndex(ik.TargetIdx);    // ���[�{�[��
    assert(root_node && target_node);

    // IK�𓮂����O�̃��[�g->���[�̃x�N�g�����擾
    auto rootpos1 = DirectX::XMLoadFloat3(&(root_node->BoneStartPos));
    auto targetpos1 = DirectX::XMLoadFloat3(&(target_node->BoneStartPos));

    // IK�𓮂�������̃��[�g->���[�i�ڕW�{�[���j�̃x�N�g�����擾�i�ڕW�ʒu�ւ̈ړ��j
    auto rootpos2 = DirectX::XMVector3TransformCoord(rootpos1, m_BoneMetricesForMotion[ik.NodeIdxes[0]]);   // ���W�ϊ���̃��[�g�m�[�h�̍��W�擾
    auto targetpos2 = DirectX::XMVector3TransformCoord(targetpos1, m_BoneMetricesForMotion[ik.BoneIdx]);    // ���W�ϊ���̖ڕW�{�[���̍��W�擾�B
                                                                                                            // ���W�ϊ��s���IK�{�[���̍s���p���邱�ƂŁA���[��ڕW�ֈړ��B

    // IK�ňړ�����x�N�g�����v�Z
    auto origin_vec = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(targetpos1, rootpos1));
    auto target_vec = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(targetpos2, rootpos2));

    // IK�ňړ������]�s����쐬
    m_BoneMetricesForMotion[ik.NodeIdxes[0]] =
        DirectX::XMMatrixTranslationFromVector(-rootpos2) *
        CreateLookAtMatrix(origin_vec, target_vec, DirectX::XMFLOAT3(0.f, 1.f, 0.f), DirectX::XMFLOAT3(1.f, 0.f, 0.f)) *
        DirectX::XMMatrixTranslationFromVector(rootpos2);
}

void PMDActor::SolveCosineIK(const PMDIK& ik)
{
    // IK�\���_��ۑ�
    std::vector<XMVECTOR> positions;
    // IK�̂��ꂼ��̃{�[���Ԃ̋�����ۑ�
    std::array<float, 2> edge_lens;

    // ���W�ϊ����IK�{�[���̍��W���擾
    // �^�[�Q�b�g�i���[�{�[���ł͂Ȃ��A���[�{�[�����߂Â��ڕW�{�[���̍��W���擾�j
    auto target_node = m_PMDData.GetBoneFromIndex(ik.BoneIdx);
    assert(target_node);
    auto target_pos = DirectX::XMVector3Transform(
        DirectX::XMLoadFloat3(&(target_node->BoneStartPos)),
        m_BoneMetricesForMotion[ik.BoneIdx]
    );

    // IK�`�F�[���͖��[����t�@�C���ɋL�^����Ă���̂ŁA�t�ɂ��Ă���
    // ���[�{�[��
    auto end_node = m_PMDData.GetBoneFromIndex(ik.TargetIdx);
    assert(end_node);
    positions.emplace_back(DirectX::XMLoadFloat3(&(end_node->BoneStartPos)));
    // ���ԋy�у��[�g�{�[��
    for (auto& chain_bone_idx : ik.NodeIdxes) {
        auto bone_node = m_PMDData.GetBoneFromIndex(chain_bone_idx);
        assert(bone_node);
        positions.emplace_back(DirectX::XMLoadFloat3(&(bone_node->BoneStartPos)));
    }
    // ���[�g���疖�[�̏���
    std::reverse(positions.begin(), positions.end());

    // �x�N�g���̌��X�̒����𑪂��Ă���
    edge_lens[0] = DirectX::XMVector3Length(DirectX::XMVectorSubtract(positions[1], positions[0])).m128_f32[0];
    edge_lens[1] = DirectX::XMVector3Length(DirectX::XMVectorSubtract(positions[2], positions[1])).m128_f32[0];

    // ���[�g�{�[�����W�ϊ��i�t���ɂȂ��Ă���̂ŁA�g�p����C���f�b�N�X�ɒ��Ӂj
    positions[0] = DirectX::XMVector3Transform(positions[0], m_BoneMetricesForMotion[ik.NodeIdxes[1]]);
    // �^�񒆂͎����v�Z�����̂Ōv�Z���Ȃ�
    // positions[1];
    // ���[�{�[�����W�ϊ�
    positions[2] = DirectX::XMVector3Transform(positions[2], m_BoneMetricesForMotion[ik.BoneIdx]);  // IK�{�[���̍��W�ϊ��s����w��i���[��IK�ɒǏ]����̂Łj

    // ���[�g�����[�ւ̃x�N�g��������Ă���
    auto linear_vec = DirectX::XMVectorSubtract(positions[2], positions[0]);

    double A = DirectX::XMVector3Length(linear_vec).m128_f32[0];
    double B = edge_lens[0];
    double C = edge_lens[1];
    double Apow2 = A * A;
    double Bpow2 = B * B;
    double Cpow2 = C * C;

    if( (A < std::numeric_limits<double>::epsilon()) ||
        (B < std::numeric_limits<double>::epsilon()) ||
        (C < std::numeric_limits<double>::epsilon()) ) {
        // �ǂꂩ��ӂ�0�ɋ߂����̂Ńf�[�^�����������B�������Ȃ��B
        return;
    }

    linear_vec = DirectX::XMVector3Normalize(linear_vec);

    // ���[�g����^�񒆂ւ̊p�x�v�Z
    double theta1 = std::acos((Apow2 + Bpow2 - Cpow2) / (2 * A * B));
    // �^�񒆂���^�[�Q�b�g�ւ̊p�x�v�Z
    double theta2 = std::acos((Bpow2 + Cpow2 - Apow2) / (2 * B * C));

    // �������߂�
    // �����^�񒆂��u�Ђ��v�ł������ꍇ�A�����I��X������]���Ƃ���
    XMVECTOR axis;
    const auto& knee_indexes = m_PMDData.GetBoneIndexes();
    if (std::find(knee_indexes.begin(), knee_indexes.end(), ik.NodeIdxes[0]) == knee_indexes.end()) {
        // �u�Ђ��v�łȂ��ꍇ
        // ���[�g -> ���[ �x�N�g���ƁA���[�g -> �ڕW(IK�ړ���) �x�N�g���̂Ȃ����ʂ��l���A���̊O�σx�N�g�������Ƃ���
        auto vm = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(positions[2], positions[0]));
        auto vt = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(target_pos, positions[0]));
        axis = DirectX::XMVector3Cross(vm, vt);
    }
    else {
        // �u�Ђ��v�̏ꍇ
        auto right = DirectX::XMFLOAT3(1, 0, 0);
        axis = DirectX::XMLoadFloat3(&right);
    }

    // TODO: �p�x���������ĂȂ��̂ŁA�Ђ��Ƃ����������ȕ����ɋȂ���\���L��
    // ���ӓ_�FIK�`�F�[���̓��[�g�Ɍ������Đ�������̂ŁA1�����[�g�ɋ߂�
    // ���[�g -> ���ԓ_ ���񂷍s��
    auto mat1 = DirectX::XMMatrixTranslationFromVector(-positions[0]);
    mat1 *= DirectX::XMMatrixRotationAxis(axis, theta1);
    mat1 *= DirectX::XMMatrixTranslationFromVector(positions[0]);
    // ���ԓ_ -> ���[ ���񂷍s��
    auto mat2 = DirectX::XMMatrixTranslationFromVector(-positions[1]);
    mat2 *= DirectX::XMMatrixRotationAxis(axis, theta2-DirectX::XM_PI);     // (180 - theta2)�� �t��]����
    mat2 *= DirectX::XMMatrixTranslationFromVector(positions[1]);

    // ���[�g����
    m_BoneMetricesForMotion[ik.NodeIdxes[1]] *= mat1;
    // ���ԓ_����(��ɒ��ԓ_���񂵂Ă���A���[�g���񂳂Ȃ��ƁA���ԓ_�̉�]���̒��S�������̂Œ���)
    m_BoneMetricesForMotion[ik.NodeIdxes[0]] = mat2 * m_BoneMetricesForMotion[ik.NodeIdxes[1]];
    // ���[����
    m_BoneMetricesForMotion[ik.TargetIdx] = m_BoneMetricesForMotion[ik.NodeIdxes[0]];
}

void PMDActor::SolveCCDIK(const PMDIK& ik)
{
    // �^�[�Q�b�g�i���[�{�[���ł͂Ȃ��A���[�{�[�����߂Â��ڕW�{�[���̍��W���擾�j
    auto target_bone_node = m_PMDData.GetBoneFromIndex(ik.BoneIdx);
    assert(target_bone_node);
    auto target_origin_pos = DirectX::XMLoadFloat3(&(target_bone_node->BoneStartPos));
    // �eIK�m�[�h�̍��W�ϊ����v�Z�̎ז��Ȃ̂ŁA�t�s��ł������񖳌���
    // (PMD�f�[�^������ƁALookAt, �]���藝IK�̏ꍇ�͐eIK�m�[�h�����݂��Ȃ��̂ŁA�eIK�m�[�h�̍��W�ϊ��͍l������K�v���Ȃ���
    // CCDIK�̏ꍇ�͐eIK�m�[�h�����Ȃ炸���݂���̂ŁA�e�̕ϊ��s����������񖳌������Ă����K�v������j
    auto parent_bone_idx = m_PMDData.GetBoneFromIndex(ik.BoneIdx)->IkParentBone;
    assert(parent_bone_idx < m_BoneMetricesForMotion.size());
    auto parent_mat = m_BoneMetricesForMotion[parent_bone_idx];
    DirectX::XMVECTOR det;
    auto inv_parent_mat = DirectX::XMMatrixInverse(&det, parent_mat);
    auto target_next_pos = DirectX::XMVector3Transform(
        target_origin_pos, m_BoneMetricesForMotion[ik.BoneIdx] * inv_parent_mat
    );

    // �e�{�[���̌��ݍ��W��ۑ�
    std::vector<XMVECTOR> bone_positions;
    // ���[�m�[�h
    auto end_node = m_PMDData.GetBoneFromIndex(ik.TargetIdx);
    assert(end_node);
    auto end_pos = DirectX::XMLoadFloat3(&(end_node->BoneStartPos));
    // ���ԃm�[�h
    for (auto cidx : ik.NodeIdxes) {
        const auto& n = m_PMDData.GetBoneFromIndex(cidx);
        assert(n);
        bone_positions.emplace_back(
            DirectX::XMLoadFloat3(&(n->BoneStartPos))
        );
    }

    // ��]�s����L�����Ă���
    std::vector<XMMATRIX> mats(bone_positions.size(), DirectX::XMMatrixIdentity());
    const auto ik_limit = ik.Limit * DirectX::XM_PI;    // �x���@��180�Ŋ������l���L�^���Ă�H ���W�A���Ƃ��Ďg�p����̂ŁAXM_PI��������

    constexpr double epsilon = 0.0005;
    // IK�ɐݒ肳��Ă��鎎�s�񐔂����J��Ԃ�
    for (int c = 0; c < ik.Iterations; ++c) {
        // �^�[�Q�b�g�Ƃقڈ�v�����甲����
        if (DirectX::XMVector3Length(DirectX::XMVectorSubtract(end_pos, target_next_pos)).m128_f32[0] <= epsilon) {
            break;
        }

        // ���ꂼ��̃{�[���������̂ڂ�Ȃ���
        // �p�x�����Ɉ���������Ȃ��悤�ɋȂ��Ă���
        for (int bidx = 0; bidx < bone_positions.size(); ++bidx) {
            // ���܋Ȃ��悤�Ƃ��Ă���{�[���ʒu
            const auto& pos = bone_positions[bidx];

            // �Ώۃm�[�h���疖�[�m�[�h�܂łƁA
            // �Ώۃm�[�h����^�[�Q�b�g�܂ł̃x�N�g���쐬
            auto vec_to_end = DirectX::XMVectorSubtract(end_pos, pos);              // ���[��
            auto vec_to_target = DirectX::XMVectorSubtract(target_next_pos, pos);   // �^�[�Q�b�g��
            // �������K��
            vec_to_end = DirectX::XMVector3Normalize(vec_to_end);
            vec_to_target = DirectX::XMVector3Normalize(vec_to_target);

            // �قړ����x�N�g���ɂȂ��Ă��܂����ꍇ
            // �O�ςł��Ȃ��̂ŁA���̃{�[���Ɉ����n��
            auto cross = DirectX::XMVectorSubtract(vec_to_end, vec_to_target);
            if (DirectX::XMVector3Length(cross).m128_f32[0] <= epsilon) {
                continue;
            }

            // �O�όv�Z�y�ъp�x�v�Z
            auto cross_normalized = DirectX::XMVector3Normalize(cross);
            float angle = DirectX::XMVector3AngleBetweenNormals(vec_to_end, vec_to_target).m128_f32[0];

            // ��]���E�𒴂��Ă��܂����Ƃ��͌��E�l�ɕ␳
            angle = std::min<float>(angle, ik_limit);
            DirectX::XMMATRIX rot = DirectX::XMMatrixRotationAxis(cross_normalized, angle);

            // ���_���S�ł͂Ȃ��Apos���S�ɉ�]
            auto mat = DirectX::XMMatrixTranslationFromVector(-pos) *
                rot *
                DirectX::XMMatrixTranslationFromVector(pos);

            // ��]�s���ێ����Ă����i��Z�ŉ�]�d�ˊ|��������Ă����j
            mats[bidx] *= mat;

            // �ΏۂƂȂ�_�����ׂĉ�]������i���݂̓_���猩�Ė��[������]�j
            for (auto idx = bidx - 1; idx >= 0; --idx) {
                bone_positions[idx] = DirectX::XMVector3Transform(bone_positions[idx], mat);
            }
            end_pos = DirectX::XMVector3Transform(end_pos, mat);

            // ���������ɋ߂������烋�[�v�𔲂���
            if (DirectX::XMVector3Length(DirectX::XMVectorSubtract(end_pos, target_next_pos)).m128_f32[0] <= epsilon) {
                break;
            }
        }
    }

    // �{�[���s��ɔ��f
    int i = 0;
    for (auto cidx : ik.NodeIdxes) {
        m_BoneMetricesForMotion[cidx] = mats[i];
        ++i;
    }
    auto root_node = m_PMDData.GetBoneFromIndex(ik.NodeIdxes.back());
    RecursiveMatrixMultiply(&(root_node.value()), parent_mat);
}