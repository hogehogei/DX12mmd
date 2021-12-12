
#include "PMD.hpp"
#include <fstream>
#include <iostream>

PMDData::PMDData()
    :
    m_PMDHeader(),
    m_VertexNum(0),
    m_VerticesBuff(),
    m_IndexNum(0),
    m_IndicesBuff()
{}

bool PMDData::Open(const char* filename)
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
    ifs.read(reinterpret_cast<char*>(m_IndicesBuff.data()), m_IndexNum * sizeof(uint16_t));

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