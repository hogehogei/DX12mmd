#pragma once

#include <DirectXMath.h>

struct SceneMatrix
{
    DirectX::XMMATRIX World;        
    DirectX::XMMATRIX View;     // �r���[�s��
    DirectX::XMMATRIX Proj;     // �v���W�F�N�V�����s��
    DirectX::XMFLOAT3 Eye;      // ���_���W
};