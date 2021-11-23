// 頂点シェーダからピクセルシェーダへのやり取りに使用する構造体

struct UVVertex
{
    float4 svpos : SV_POSITION;     // システム用頂点座標
    float2 uv    : TEXCOORD;        // uv値
};

// 定数バッファー
cbuffer cbuff0 : register(b0)
{
    matrix mat;    // 変換行列
};