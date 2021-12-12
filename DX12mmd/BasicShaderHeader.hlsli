// 頂点シェーダからピクセルシェーダへのやり取りに使用する構造体

struct UVVertex
{
    float4 svpos : SV_POSITION;     // システム用頂点座標
    float4 normal: NORMAL;          // 法線ベクトル
    float2 uv    : TEXCOORD;        // uv値
};

// 定数バッファー
cbuffer cbuff0 : register(b0)
{
    matrix world;       // ワールド行列
    matrix viewproj;    // ビュープロジェクション行列
};