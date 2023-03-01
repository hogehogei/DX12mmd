#include <sstream>
#include "MMDModel.hpp"
#include "FilePath.hpp"

namespace
{
    void WriteMaterial(void* srcdata, uint8_t* dst)
    {
        PMDData* data = reinterpret_cast<PMDData*>(srcdata);
        const auto& material_data = data->MaterialData();
        uint32_t material_buff_size = (PMDData::k_ShaderMaterialSize + 0xFF) & ~0xFF;

        for (const auto& itr : material_data) {
            const uint8_t* src = reinterpret_cast<const uint8_t*>(&itr.MaterialForShader);
            std::copy_n(src, PMDData::k_ShaderMaterialSize, dst);
            dst += material_buff_size;
        }
    }
}

MMDModel::MMDModel()
    :
    m_ModelPath(),
    m_ModelName(),
    m_ResourceManager(nullptr),
    m_PMDData(),
    m_VertBuff(std::make_shared<VertexBufferPMD>()),
    m_IdxBuff(),
    m_MaterialBuff(),
    m_BoneMetricesForMotion()
{}

bool MMDModel::Create(ResourceManager* resource_manager, const std::string& model_name, const std::filesystem::path& filepath)
{
    if (!m_PMDData.Open(filepath)) {
        return false;
    }
    m_ResourceManager = resource_manager;
    m_TextureManager.SetShaderResource(m_ResourceManager);
    m_ModelPath = filepath;
    m_ModelName = model_name;

    auto vertbuff = std::make_shared<VertexBufferPMD>();
    if (!vertbuff->CreateVertexBuffer(m_PMDData)) {
        return false;
    }
    m_VertBuff = vertbuff;

    auto idxbuff = std::make_shared<IndexBuffer>();
    if (!idxbuff->CreateIndexBuffer(m_PMDData)) {
        return false;
    }
    m_IdxBuff = idxbuff;

    ResourceOrder material_resource =
    {
        model_name,
        {
            {ResourceOrder::k_ConstantResource, 1, 1},
            {ResourceOrder::k_ShaderResource, 4, 0 }
        },
        m_PMDData.MaterialNum()
    };
    resource_manager->AddResource(material_resource);

    m_MaterialBuff = std::make_shared<ConstantBuffer>();
    m_MaterialBuff->Create(resource_manager, PMDData::k_ShaderMaterialSize, m_PMDData.MaterialNum(), resource_manager->ResourceHandle(model_name));
    ConstantBuffer::WriterFunc func = WriteMaterial;
    m_MaterialBuff->Write((void*)&m_PMDData, func);

    // GPU転送用ボーン行列バッファ初期化
    m_BoneMetricesForMotion.resize(k_BoneMetricesNum);
    std::fill(m_BoneMetricesForMotion.begin(), m_BoneMetricesForMotion.end(), DirectX::XMMatrixIdentity());

    CreateTextures();

    return true;
}

VertexBufferPtr MMDModel::GetVertexBuffer()
{
    return m_VertBuff;
}

IndexBufferPtr MMDModel::GetIndexBuffer()
{
    return m_IdxBuff;
}

ConstantBufferPtr MMDModel::GetMaterialBuffer()
{
    return m_MaterialBuff;
}

const PMDData& MMDModel::GetPMDData() const
{
    return m_PMDData;
}

void MMDModel::RecursiveMatrixMultiply(
    const BoneTree::BoneNode* node, const DirectX::XMMATRIX& mat
)
{
    if (!node) {
        return;
    }

    m_BoneMetricesForMotion[node->BoneIdx] *= mat;
    
    for (const auto child_node : node->GetChildlen())
    {
        RecursiveMatrixMultiply(child_node, m_BoneMetricesForMotion[node->BoneIdx]);
    }
}

std::vector<DirectX::XMMATRIX>& MMDModel::GetBoneMetricesForMotion()
{
    return m_BoneMetricesForMotion;
}

void MMDModel::CreateTextures()
{
    const auto& materials = m_PMDData.MaterialData();
    auto shader_resource_handle = m_ResourceManager->ResourceHandle(m_ModelName);

    for (const auto& m : materials) {
        std::filesystem::path filepath = m.Additional.TexturePath;
        TexturePtr texture_handle;
        TexturePath texture_path;

        texture_path = GetTexturePathFromModelAndTexPath(m_ModelPath, filepath);
        
        shader_resource_handle.Offset += 1;
        if (!texture_path.TexPath.empty()) {
            m_TextureManager.CreateTextures(
                texture_path.TexPath,
                &texture_handle
            );
            m_TextureManager.CreateShaderResourceView(texture_handle, m_ResourceManager, shader_resource_handle);
            m_Textures.push_back(texture_handle);
        }
        else {
            m_TextureManager.CreatePlaneTexture(
                L"white",
                &texture_handle
            );
            m_TextureManager.CreateShaderResourceView(texture_handle, m_ResourceManager, shader_resource_handle);
            m_Textures.push_back(texture_handle);
        }

        shader_resource_handle.Offset += 1;
        if (!texture_path.SphereMapPath.empty()) {
            m_TextureManager.CreateTextures(
                texture_path.SphereMapPath,
                &texture_handle
            );
            m_TextureManager.CreateShaderResourceView(texture_handle, m_ResourceManager, shader_resource_handle);
            m_Textures.push_back(texture_handle);
        }
        else {
            m_TextureManager.CreatePlaneTexture(
                L"white",
                &texture_handle
            );
            m_TextureManager.CreateShaderResourceView(texture_handle, m_ResourceManager, shader_resource_handle);
            m_Textures.push_back(texture_handle);
        }

        shader_resource_handle.Offset += 1;
        if (!texture_path.AddSphereMapPath.empty()) {
            m_TextureManager.CreateTextures(
                texture_path.AddSphereMapPath,
                &texture_handle
            );
            m_TextureManager.CreateShaderResourceView(texture_handle, m_ResourceManager, shader_resource_handle);
            m_Textures.push_back(texture_handle);
        }
        else {
            m_TextureManager.CreatePlaneTexture(
                L"black",
                &texture_handle,
                0x00, 0x00, 0x00
            );
            m_TextureManager.CreateShaderResourceView(texture_handle, m_ResourceManager, shader_resource_handle);
            m_Textures.push_back(texture_handle);
        }

        shader_resource_handle.Offset += 1;
        ReadToonTexture(m, shader_resource_handle);

        shader_resource_handle.Offset += 1;
    }
}

void MMDModel::ReadToonTexture(const Material& material, const ResourceDescHandle& handle)
{
    std::ostringstream os;
    std::filesystem::path filepath;

    // 実際のtoon番号は 記録された番号+1される（uint8_t サイズでWrap）
    uint8_t toonidx = (material.Additional.ToonIdx + 1);
    filepath += L"toon/toon";
    os << std::setfill('0') << std::right << std::setw(2) << static_cast<int>(toonidx);

    // string を wstring に変換
    // ASCII しか使ってないと断言できるのでこのようにしている
    std::string number_str = os.str();
    std::vector<wchar_t> wstr;
    std::copy(number_str.begin(), number_str.end(), std::back_inserter(wstr));
    wstr.push_back(L'\0');

    filepath += wstr.data();
    filepath += L".bmp";

    std::error_code ec;
    bool result = std::filesystem::exists(filepath, ec);
    TexturePtr texture_handle;
    if (!ec && result) {
        m_TextureManager.CreateTextures(
            filepath,
            &texture_handle
        );
        m_TextureManager.CreateShaderResourceView(texture_handle, m_ResourceManager, handle);
        m_Textures.push_back(texture_handle);
    }
    else {
        m_TextureManager.CreateGradationTexture(
            L"gradation",
            &texture_handle
        );
        m_TextureManager.CreateShaderResourceView(texture_handle, m_ResourceManager, handle);
        m_Textures.push_back(texture_handle);
    }
}