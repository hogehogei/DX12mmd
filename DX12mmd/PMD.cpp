
#include "PMD.hpp"

#include "windows.h"
#include <fstream>
#include <iostream>
#include <sstream>


//
// Implements class BoneTree
// 

BoneTree::BoneNode::BoneNode()
    :
    BoneIdx(0),
    BoneStartPos(),
    BoneEndPos(),
    m_Childlen()
{}

BoneTree::BoneNode::BoneNode(int idx, const PMDBone& bone)
    : 
    BoneIdx(idx),
    BoneStartPos(bone.Pos),
    BoneEndPos(),
    m_Childlen()
{}

void BoneTree::BoneNode::AddChild(const BoneNode* bonenode)
{
    m_Childlen.emplace_back(bonenode);
}

const std::vector<const BoneTree::BoneNode*>& BoneTree::BoneNode::GetChildlen() const
{
    return m_Childlen;
}

BoneTree::BoneTree()
    : m_BoneNodeTable()
{}

void BoneTree::Create(const std::vector<PMDBone>& bones)
{
    CreateBoneTree(bones);
}

BoneTree::BoneNodeResult BoneTree::GetBoneNode(const std::string& bonename) const
{
    auto result = m_BoneNodeTable.find(bonename);
    if (result == m_BoneNodeTable.end()) {
        return std::nullopt;
    }
    return result->second;
}

void BoneTree::CreateBoneTree(const std::vector<PMDBone>& bones)
{
    std::vector<std::string> bone_names(bones.size());

    // ボーンノードマップを作る
    for (int idx = 0; idx < bones.size(); ++idx) {
        auto& bone = bones[idx];
        bone_names[idx] = bone.BoneName;

        m_BoneNodeTable.emplace(bone.BoneName, BoneNode(idx, bone));
    }

    // 親子関係を構築する
    for (auto& bone : bones) {
        // 親インデックスをチェック（ありえない番号なら飛ばす）
        if (bone.ParentBoneNo >= bones.size()) {
            continue;
        }

        // 親ノードを名前で検索して、自分を子ノードとして追加
        auto parent_name = bone_names[bone.ParentBoneNo];
        m_BoneNodeTable[parent_name].AddChild(&m_BoneNodeTable[bone.BoneName]);
    }
}

PMDData::PMDData()
    :
    m_PMDHeader(),
    m_VertexNum(0),
    m_VerticesBuff(),
    m_IndexNum(0),
    m_IndicesBuff(),
    m_MaterialNum(0),
    m_MaterialBuff(),
    m_BoneNum(0),
    m_BonesBuff(),
    m_BoneTree()
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

    std::wostringstream os;
    os << "VertexNum: " << m_VertexNum << std::endl;

    // 頂点バッファ読み込み
    char* ptr = reinterpret_cast<char*>(&m_VerticesBuff[0]);
    for (uint32_t i = 0; i < m_VertexNum; ++i) {
        ifs.read(ptr, PMDVertex::k_PMDVertexSize);

        PMDVertex* vtx = reinterpret_cast<PMDVertex*>(ptr);
        //os << "Vertex (" << i << ") : pos_x[" << vtx->Pos.x << "], pos_y[" << vtx->Pos.y << "], pos_z[" << vtx->Pos.z << "]" << std::endl;

        ptr += sizeof(PMDVertex);
    }
    //::OutputDebugString(reinterpret_cast<LPCWSTR>(os.str().c_str()));

    // 頂点インデックス読み込み
    ifs.read(reinterpret_cast<char*>(&m_IndexNum), sizeof(m_IndexNum));
    m_IndicesBuff.resize(IndexBuffSize());
    ifs.read(reinterpret_cast<char*>(m_IndicesBuff.data()), m_IndicesBuff.size());

    // マテリアル読み込み
    ifs.read(reinterpret_cast<char*>(&m_MaterialNum), sizeof(m_MaterialNum));
    m_MaterialBuff.resize(MaterialNum());
    ifs.read(reinterpret_cast<char*>(m_MaterialBuff.data()), MaterialBuffSize());

    // ボーン読み込み
    ifs.read(reinterpret_cast<char*>(&m_BoneNum), sizeof(m_BoneNum));
    m_BonesBuff.resize(m_BoneNum);
    ifs.read(reinterpret_cast<char*>(m_BonesBuff.data()), BoneBuffSize());
    m_BoneTree.Create(m_BonesBuff);

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

uint32_t PMDData::BoneNum() const
{
    return m_BoneNum;
}

uint64_t PMDData::BoneBuffSize() const
{
    return static_cast<uint64_t>(BoneNum()) * sizeof(PMDBone);
}

BoneTree::BoneNodeResult PMDData::BoneFromName(const std::string& bonename) const
{
    auto bone = m_BoneTree.GetBoneNode(bonename);
    if (!bone) {
        return std::nullopt;
    }
    return bone.value();
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