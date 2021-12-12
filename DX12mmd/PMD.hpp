#pragma once

#include <cstdint>
#include <vector>
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

class PMDData
{
public:

    PMDData();

    bool Open(const char* filename);

    uint32_t VertexNum() const;             // ���_��
    uint32_t VertexStrideByte() const;      // 1���_������̃o�C�g��
    uint64_t VertexBuffSize() const;        // ���_�o�b�t�@�̃T�C�Y��
    const uint8_t* VertexData() const;

    uint32_t IndexNum() const;
    uint64_t IndexBuffSize() const;
    const uint8_t* IndexData() const;

private:

    PMDHeader m_PMDHeader;
    uint32_t  m_VertexNum;
    std::vector<uint8_t> m_VerticesBuff;
    uint32_t  m_IndexNum;
    std::vector<uint8_t> m_IndicesBuff;
};