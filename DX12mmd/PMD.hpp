#pragma once

#include <cstdint>
#include <vector>
#include <map>
#include <string>
#include <optional>
#include <filesystem>
#include <DirectXMath.h>

#include "Matrix.hpp"

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

#pragma pack(1)
struct PMDBone
{
    char BoneName[20];                  // ボーン名
    uint16_t ParentBoneNo;              // 親ボーン番号
    uint16_t NextBoneNo;                // 先端のボーン番号
    uint8_t  BoneType;                  // ボーン種別
    uint16_t IkBoneNo;                  // IKボーン番号
    DirectX::XMFLOAT3 Pos;              // ボーンの基準点座標
};
#pragma pack()

enum class BoneType : uint32_t 
{
    Rotation   = 0,         // 回転
    RotAndMove,             // 回転&移動
    IK,                     // IK
    Undefined,              // 未定義
    IKChild,                // IK影響ボーン
    RotationChild,          // 回転影響ボーン
    IKDestination,          // IK接続先
    Invisible,              // 見えないボーン
};

class BoneTree
{
public:
    class BoneNode
    {
    public:

        BoneNode();
        BoneNode(int boneidx, const PMDBone& bone);

        void AddChild(const BoneNode* bonenode);
        const std::vector<const BoneNode*>& GetChildlen() const;

        uint32_t          BoneIdx;           // ボーンインデックス
        uint32_t          BoneType;          // ボーン種別
        uint32_t          ikParentBone;      // IK親ボーン
        DirectX::XMFLOAT3 BoneStartPos;      // ボーン基準点（回転の中心）
        DirectX::XMFLOAT3 BoneEndPos;        // ボーン先端点（実際のスキニングには影響しない）

    private:

        std::vector<const BoneNode*> m_Childlen;   // 子ノード
    };

    struct NamedBone {
        NamedBone()
            : BoneName(), Node() {}
        NamedBone(const std::string& name, const BoneNode& bone)
            : BoneName(name), Node(bone) {}
        std::string BoneName;
        BoneNode    Node;
    };
    typedef std::optional<BoneTree::BoneNode> BoneNodeResult;

public:

    BoneTree();

    void Create(const std::vector<PMDBone>& bones);
    BoneNodeResult GetBoneNode(const std::string& bonename) const;
    std::string GetBoneNameFromIdx(uint16_t idx) const;

private:

    void CreateBoneTree(const std::vector<PMDBone>& bones);

    std::vector<PMDBone>      m_RawBonesData;
    std::vector<NamedBone>    m_BoneNodes;
    //std::map<std::string, BoneNode> m_BoneNodeTable;
};

struct PMDIK
{
public:

    uint16_t BoneIdx;                    // IK対象のボーンを示す
    uint16_t TargetIdx;                  // ターゲットに近づけるためのボーンのインデックス
    uint16_t Iterations;                 // 試行回数
    float    Limit;                      // 1回あたりの回転制限
    std::vector<uint16_t> NodeIdxes;     // 間のノード番号
};


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
    static constexpr uint32_t k_PMDBoneMetricesNum = k_BoneMetricesNum;

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

    uint32_t BoneNum() const;
    uint64_t BoneBuffSize() const;
    BoneTree::BoneNodeResult BoneFromName(const std::string& bonename) const;

private:

    void CopyMaterialsData();
    void ReadIKData(std::ifstream& ifs);
    void DebugPrintIKData();

    PMDHeader m_PMDHeader;
    uint32_t  m_VertexNum;
    std::vector<uint8_t> m_VerticesBuff;

    uint32_t  m_IndexNum;
    std::vector<uint8_t> m_IndicesBuff;
    
    uint32_t  m_MaterialNum;
    std::vector<PMDMaterial> m_MaterialBuff;
    std::vector<Material> m_Materials;

    // ボーン管理用変数
    uint16_t  m_BoneNum;                                  // ボーン数
    std::vector<PMDBone> m_BonesBuff;                     // ボーン保存用バッファ
    BoneTree  m_BoneTree;                                 // ボーン管理クラス

    // IK管理用変数
    uint16_t  m_IkNum;                                    // IK数
    std::vector<PMDIK>   m_PMDIkData;                     // IK読み出しデータ
};