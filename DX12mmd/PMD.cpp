
#include "PMD.hpp"
#include <fstream>
#include <iostream>

PMDData::PMDData()
    :
    m_PMDHeader(),
    m_VertexNum(0),
    m_VerticesBuff(),
    m_IndexNum(0),
    m_IndicesBuff(),
    m_MaterialNum(0),
    m_MaterialBuff()
{}

bool PMDData::Open(const std::filesystem::path& filename)
{
    char signature[3]{};

    std::ifstream ifs(filename, std::ios::in | std::ios::binary);
    if (!ifs) {
        return false;
    }

    ifs.read(reinterpret_cast<char*>(&signature), sizeof(signature));
    ifs.read(reinterpret_cast<char*>(&m_PMDHeader), sizeof(m_PMDHeader));

    ifs.read(reinterpret_cast<char*>(&m_VertexNum), sizeof(m_VertexNum));
    m_VerticesBuff.resize(VertexBuffSize());

    char* ptr = reinterpret_cast<char*>(&m_VerticesBuff[0]);
    for (uint32_t i = 0; i < m_VertexNum; ++i) {
        ifs.read(ptr, PMDVertex::k_PMDVertexSize);
        ptr += sizeof(PMDVertex);
    }

    ifs.read(reinterpret_cast<char*>(&m_IndexNum), sizeof(m_IndexNum));
    m_IndicesBuff.resize(IndexBuffSize());
    ifs.read(reinterpret_cast<char*>(m_IndicesBuff.data()), m_IndicesBuff.size());

    ifs.read(reinterpret_cast<char*>(&m_MaterialNum), sizeof(m_MaterialNum));
    m_MaterialBuff.resize(MaterialNum());
    ifs.read(reinterpret_cast<char*>(m_MaterialBuff.data()), MaterialBuffSize());

    CopyMaterialsData();

    return true;
}

uint32_t PMDData::VertexNum() const
{
    return m_VertexNum;
}

uint32_t PMDData::VertexStrideByte() const
{
    return sizeof(PMDVertex);
}

uint64_t PMDData::VertexBuffSize() const
{
    return static_cast<uint64_t>(VertexNum()) * VertexStrideByte();
}

const uint8_t* PMDData::VertexData() const
{
    return m_VerticesBuff.data();
}

uint32_t PMDData::IndexNum() const
{
    return m_IndexNum;
}

uint64_t PMDData::IndexBuffSize() const
{
    return static_cast<uint64_t>(IndexNum()) * sizeof(uint16_t);
}

const uint8_t* PMDData::IndexData() const
{
    return m_IndicesBuff.data();
}
uint32_t PMDData::MaterialNum() const
{
    return m_MaterialNum;
}

uint64_t PMDData::MaterialBuffSize() const
{
    return static_cast<uint64_t>(MaterialNum()) * sizeof(PMDMaterial);
}

const std::vector<Material>& PMDData::MaterialData() const
{
    return m_Materials;
}

void PMDData::CopyMaterialsData()
{
    m_Materials.resize(m_MaterialNum);

    for (int i = 0; i < m_MaterialNum; ++i) {
        m_Materials[i].IndicesNum = m_MaterialBuff[i].IndicesNum;
        m_Materials[i].MaterialForShader.Diffuse = m_MaterialBuff[i].Diffuse;
        m_Materials[i].MaterialForShader.Alpha = m_MaterialBuff[i].Alpha;
        m_Materials[i].MaterialForShader.Specular = m_MaterialBuff[i].Specular;
        m_Materials[i].MaterialForShader.Specularity = m_MaterialBuff[i].Specularity;
        m_Materials[i].MaterialForShader.Ambient = m_MaterialBuff[i].Ambient;

        m_Materials[i].Additional.TexturePath += m_MaterialBuff[i].TexFilePath;
        m_Materials[i].Additional.ToonIdx = m_MaterialBuff[i].ToonIdx;
        //std::copy_n(m_MaterialBuff[i].TexFilePath, PMDMaterial::k_TextureFilePathLen, m_Materials[i].Additional.TexturePath.begin());
    }
}