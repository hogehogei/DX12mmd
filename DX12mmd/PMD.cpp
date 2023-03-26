
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
    BoneType(0),
    ParentBone(0),
    IkParentBone(0),
    BoneStartPos(),
    m_Childlen()
{}

BoneTree::BoneNode::BoneNode(int idx, const PMDBone& bone)
    : 
    BoneIdx(idx),
    BoneType(bone.BoneType),
    ParentBone(bone.ParentBoneNo),
    IkParentBone(bone.IkBoneNo),
    BoneStartPos(bone.Pos),
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
    : m_RawBonesData(),
      m_BoneNodes()
{}

void BoneTree::Create(const std::vector<PMDBone>& bones)
{
    CreateBoneTree(bones);
}

BoneTree::BoneNodeResult BoneTree::GetBoneNode(const std::string& bonename) const
{
    auto result = std::find_if(
        m_BoneNodes.begin(),
        m_BoneNodes.end(),
        [bonename](const NamedBone& bone) {
            return bone.BoneName == bonename;
        }
    );

    if (result != m_BoneNodes.end()) {
        return result->Node;
    }

    return std::nullopt;

#if 0
    auto result = m_BoneNodeTable.find(bonename);
    if (result == m_BoneNodeTable.end()) {
        return std::nullopt;
    }
    return result->second;
#endif
}

BoneTree::BoneNodeResult BoneTree::GetBoneNode(uint16_t idx) const
{
    if (idx >= m_BoneNodes.size()) {
        return std::nullopt;
    }

    return m_BoneNodes[idx].Node;
}

std::string BoneTree::GetBoneNameFromIdx(uint16_t idx) const
{
    if (idx >= m_BoneNodes.size()) {
        return "";
    }
    return m_BoneNodes[idx].BoneName;
#if 0
    auto itr = std::find_if(
        m_BoneNodeTable.begin(),
        m_BoneNodeTable.end(),
        [idx](const std::pair<std::string, BoneNode>& pair) {
            return pair.second.BoneIdx == idx;
        }
    );

    if (itr != m_BoneNodeTable.end()) {
        return itr->first;
    }

    return "";
#endif
}

void BoneTree::CreateBoneTree(const std::vector<PMDBone>& bones)
{
    m_RawBonesData = bones;

    // ボーンノードマップを作る
    m_BoneNodes.resize(bones.size());
    for (int idx = 0; idx < m_RawBonesData.size(); ++idx) {
        m_BoneNodes[idx] = NamedBone(
            std::string(bones[idx].BoneName),
            BoneNode(idx, bones[idx])
        );
    }

    // 親子関係を構築する
    for (int idx = 0; idx < bones.size(); ++idx) {
        // 親インデックスをチェック（ありえない番号なら飛ばす）
        if (bones[idx].ParentBoneNo >= bones.size()) {
            continue;
        }

        // 親ノードに対して、自分を子ノードとして追加
        auto& parent_bone = m_BoneNodes[bones[idx].ParentBoneNo];
        parent_bone.Node.AddChild(&(m_BoneNodes[idx].Node));
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
    m_KneeIndexes(),
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

    //std::ostringstream os;
    //os << "VertexNum: " << m_VertexNum << std::endl;

    // 頂点バッファ読み込み
    char* ptr = reinterpret_cast<char*>(&m_VerticesBuff[0]);
    for (uint32_t i = 0; i < m_VertexNum; ++i) {
        ifs.read(ptr, PMDVertex::k_PMDVertexSize);

        PMDVertex* vtx = reinterpret_cast<PMDVertex*>(ptr);
        //os << "Vertex (" << i << ") : pos_x[" << vtx->Pos.x << "], pos_y[" << vtx->Pos.y << "], pos_z[" << vtx->Pos.z << "]" << std::endl;

        ptr += sizeof(PMDVertex);
    }
    //::OutputDebugStringA(os.str().c_str());

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
    // IKのボーンだけ記録
    for (int idx = 0; idx < m_BonesBuff.size(); ++idx) {
        const std::string& bone_name = m_BoneTree.GetBoneNameFromIdx(idx);
        if (bone_name.find("ひざ") != std::string::npos) {
            m_KneeIndexes.emplace_back(idx);
        }
    }

    // IK読み込み
    ifs.read(reinterpret_cast<char*>(&m_IkNum), sizeof(m_IkNum));
    ReadIKData(ifs);

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

const uint8_t* PMDData::GetVertexData() const
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

const uint8_t* PMDData::GetIndexData() const
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

const std::vector<Material>& PMDData::GetMaterialData() const
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

BoneTree::BoneNodeResult PMDData::GetBoneFromName(const std::string& bonename) const
{
    return m_BoneTree.GetBoneNode(bonename);
}

BoneTree::BoneNodeResult PMDData::GetBoneFromIndex(uint16_t idx) const
{
    return m_BoneTree.GetBoneNode(idx);
}

const std::vector<uint32_t>& PMDData::GetBoneIndexes() const
{
    return m_KneeIndexes;
}

std::string PMDData::GetBoneName(uint16_t idx) const
{
    return m_BoneTree.GetBoneNameFromIdx(idx);
}

uint32_t PMDData::IKNum() const
{
    return m_IkNum;
}

const std::vector<PMDIK> PMDData::GetPMDIKData() const
{
    return m_PMDIkData;
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

void PMDData::ReadIKData( std::ifstream& ifs )
{
    m_PMDIkData.resize(m_IkNum);

    for (auto& ik : m_PMDIkData) {
        ifs.read(reinterpret_cast<char*>(&ik.BoneIdx), sizeof(ik.BoneIdx));
        ifs.read(reinterpret_cast<char*>(&ik.TargetIdx), sizeof(ik.TargetIdx));

        uint8_t chain_len = 0;      // 間にいくつノードがあるか？
        ifs.read(reinterpret_cast<char*>(&chain_len), sizeof(chain_len));
        ik.NodeIdxes.resize(chain_len);
        ifs.read(reinterpret_cast<char*>(&ik.Iterations), sizeof(ik.Iterations));
        ifs.read(reinterpret_cast<char*>(&ik.Limit), sizeof(ik.Limit));

        if (chain_len == 0) {
            continue;
        }
        ifs.read(reinterpret_cast<char*>(ik.NodeIdxes.data()), sizeof(ik.NodeIdxes[0]) * chain_len);
    }

    DebugPrintIKData();
}

void PMDData::DebugPrintIKData()
{

    for (auto& ik : m_PMDIkData) {
        std::ostringstream oss;
        oss << "IKBoneNum = " << ik.BoneIdx
            << ":" << m_BoneTree.GetBoneNameFromIdx(ik.BoneIdx) << std::endl;
        oss << "IKBoneTarget = " << ik.TargetIdx
            << ":" << m_BoneTree.GetBoneNameFromIdx(ik.TargetIdx) << std::endl;

        auto bone = m_BoneTree.GetBoneNode(ik.BoneIdx);
        if (bone) {
            uint32_t parent_bone_idx = bone.value().IkParentBone;
            oss << "IKBoneParent = " << parent_bone_idx
                << ":" << m_BoneTree.GetBoneNameFromIdx(parent_bone_idx) << std::endl;
        }
        
        for (auto& node : ik.NodeIdxes) {
            oss << "\tNodeBone = " << node
                << ":" << m_BoneTree.GetBoneNameFromIdx(ik.BoneIdx) << std::endl;
        }

        ::OutputDebugStringA(oss.str().c_str());
    }
}