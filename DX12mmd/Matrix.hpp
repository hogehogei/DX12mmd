#pragma once

#include <DirectXMath.h>

struct SceneMatrix
{
    DirectX::XMMATRIX World;        
    DirectX::XMMATRIX View;     // ビュー行列
    DirectX::XMMATRIX Proj;     // プロジェクション行列
    DirectX::XMFLOAT3 Eye;      // 視点座標
};