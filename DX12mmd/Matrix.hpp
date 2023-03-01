#pragma once

#include <DirectXMath.h>

static constexpr uint32_t k_BoneMetricesNum = 256;
struct SceneMatrix
{
    DirectX::XMMATRIX World;        
    DirectX::XMMATRIX View;     // ビュー行列
    DirectX::XMMATRIX Proj;     // プロジェクション行列
    DirectX::XMFLOAT3 Eye;      // 視点座標

    DirectX::XMMATRIX Bones[k_BoneMetricesNum];     // ボーン行列
};