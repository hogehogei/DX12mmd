#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <filesystem>
#include <DirectXMath.h>

struct PMDHeader
{
    float Version;          // 例： 00 00 80 3F == 1.00
    char ModelName[20];     // モデル名
    char Comment[256];      // モデルコメント
};

struct PMDVertex
{
    DirectX::XMFLOAT3 Pos;
    DirectX::XMFLOAT3 Normal;
    DirectX::XMFLOAT2 UV;
    uint16_t BoneNo[2];
    uint8_t  BoneWeight;
    uint8_t  EdgeFlg;
    uint8_t  Reserved[2];

    static constexpr size_t k_PMDVertexSize = 38;
};

#pragma pack(1)
struct PMDMaterial
{
    static constexpr size_t k_TextureFilePathLen = 20;

    DirectX::XMFLOAT3 Diffuse;           // ディフューズ色
    float Alpha;                         // ディフューズα
    float Specularity;                   // スペキュラの強さ（乗算値）
    DirectX::XMFLOAT3 Specular;          // スペキュラ色
    DirectX::XMFLOAT3 Ambient;           // アンビエント色
    unsigned char ToonIdx;               // トゥーン番号
    unsigned char EdgeFlg;               // マテリアル毎の輪郭線フラグ

    unsigned int IndicesNum;                      // このマテリアルが割り当てられるインデックス数
    char TexFilePath[k_TextureFilePathLen];       // テクスチャファイルパス＋α
};
#pragma pack()

struct MaterialForHlsl
{
    DirectX::XMFLOAT3 Diffuse;           // ディフューズ色
    float Alpha;                         // ディフューズα
    DirectX::XMFLOAT3 Specular;          // スペキュラ色
    float Specularity;                   // スペキュラの強さ（乗算値）
    DirectX::XMFLOAT3 Ambient;           // アンビエント色
};

struct AdditionalMaterial
{
    std::string TexturePath;            // テクスチャパス
    uint8_t ToonIdx;                    // トゥーン番号
    bool EdgeFlg;                       // マテリアル毎の輪郭線フラグ
};

struct Material
{
    unsigned int IndicesNum;
    MaterialForHlsl MaterialForShader;
    AdditionalMaterial Additional;
};

class PMDData
{
public:
    static constexpr uint32_t k_ShaderMaterialSize = sizeof(MaterialForHlsl);

public:

    PMDData();

    bool Open(const std::filesystem::path& filename);

    uint32_t VertexNum() const;             // 頂点数
    uint32_t VertexStrideByte() const;      // 1頂点当たりのバイト数
    uint64_t VertexBuffSize() const;        // 頂点バッファのサイズ数
    const uint8_t* VertexData() const;

    uint32_t IndexNum() const;
    uint64_t IndexBuffSize() const;
    const uint8_t* IndexData() const;

    uint32_t MaterialNum() const;
    uint64_t MaterialBuffSize() const;
    const std::vector<Material>& MaterialData() const;

private:

    void CopyMaterialsData();

    PMDHeader m_PMDHeader;
    uint32_t  m_VertexNum;
    std::vector<uint8_t> m_VerticesBuff;
    uint32_t  m_IndexNum;
    std::vector<uint8_t> m_IndicesBuff;
    uint32_t  m_MaterialNum;
    std::vector<PMDMaterial> m_MaterialBuff;
    std::vector<Material> m_Materials;
};