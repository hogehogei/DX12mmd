#pragma once

#include <DirectXMath.h>

static constexpr uint32_t k_BoneMetricesNum = 256;
struct SceneMatrix
{
    DirectX::XMMATRIX World;        
    DirectX::XMMATRIX View;     // �r���[�s��
    DirectX::XMMATRIX Proj;     // �v���W�F�N�V�����s��
    DirectX::XMFLOAT3 Eye;      // ���_���W

    DirectX::XMMATRIX Bones[k_BoneMetricesNum];     // �{�[���s��
};