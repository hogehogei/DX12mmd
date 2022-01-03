// 頂点シェーダからピクセルシェーダへのやり取りに使用する構造体

struct VertexShaderOutput
{
    float4 svpos  :  SV_POSITION;     // システム用頂点座標
    float4 pos  :    POSITION;        // 頂点座標
    float4 normal :  NORMAL0;         // 法線ベクトル
    float4 vnormal : NORMAL1;         // ビュー変換後の法線ベクトル
    float2 uv     :  TEXCOORD;        // uv値
    float3 ray : VECTOR;              // 視線ベクトル
};

// 定数バッファー
cbuffer cbuff0 : register(b0)
{
    matrix world;       // ワールド行列
    matrix view;        // ビュー行列
    matrix proj;        // プロジェクション行列
    float3 eye;         // 視点座標
};

cbuffer Material : register(b1)
{
    float4 diffuse;     // ディフューズ色
    float4 specular;    // スペキュラ
    float3 ambient;     // アンビエント
};
