#pragma once

#include <DirectXMath.h>

struct MatricesData
{
    DirectX::XMMATRIX World;        
    DirectX::XMMATRIX ViewProj;     // ビュー＆プロジェクション合成行列
};