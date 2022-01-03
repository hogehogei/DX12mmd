#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <filesystem>
#include <DirectXMath.h>

struct PMDHeader
{
    float Version;          // ��F 00 00 80 3F == 1.00
    char ModelName[20];     // ���f����
    char Comment[256];      // ���f���R�����g
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

    DirectX::XMFLOAT3 Diffuse;           // �f�B�t���[�Y�F
    float Alpha;                         // �f�B�t���[�Y��
    float Specularity;                   // �X�y�L�����̋����i��Z�l�j
    DirectX::XMFLOAT3 Specular;          // �X�y�L�����F
    DirectX::XMFLOAT3 Ambient;           // �A���r�G���g�F
    unsigned char ToonIdx;               // �g�D�[���ԍ�
    unsigned char EdgeFlg;               // �}�e���A�����̗֊s���t���O

    unsigned int IndicesNum;                      // ���̃}�e���A�������蓖�Ă���C���f�b�N�X��
    char TexFilePath[k_TextureFilePathLen];       // �e�N�X�`���t�@�C���p�X�{��
};
#pragma pack()

struct MaterialForHlsl
{
    DirectX::XMFLOAT3 Diffuse;           // �f�B�t���[�Y�F
    float Alpha;                         // �f�B�t���[�Y��
    DirectX::XMFLOAT3 Specular;          // �X�y�L�����F
    float Specularity;                   // �X�y�L�����̋����i��Z�l�j
    DirectX::XMFLOAT3 Ambient;           // �A���r�G���g�F
};

struct AdditionalMaterial
{
    std::string TexturePath;            // �e�N�X�`���p�X
    uint8_t ToonIdx;                    // �g�D�[���ԍ�
    bool EdgeFlg;                       // �}�e���A�����̗֊s���t���O
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

    uint32_t VertexNum() const;             // ���_��
    uint32_t VertexStrideByte() const;      // 1���_������̃o�C�g��
    uint64_t VertexBuffSize() const;        // ���_�o�b�t�@�̃T�C�Y��
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