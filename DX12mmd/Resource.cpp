
#include "Resource.hpp"
#include "AppManager.hpp"

ResourceManager::ResourceManager()
    :
    m_ConstantViewNum(0),
    m_TextureViewNum(0),
    m_BasicDescHeap(nullptr),
    m_RootParam(),
    m_DescRange(),
    m_Sampler()
{}

bool ResourceManager::Initialize(uint32_t constant_view_num, uint32_t texture_view_num )
{
    m_ConstantViewNum = constant_view_num;
    m_TextureViewNum = texture_view_num;

    CreateRootParameter();
    CreateSampler();
    
    if (!CreateDescriptorHeap()){
        return false;
    }

    return true;
}

ID3D12DescriptorHeap* ResourceManager::DescriptorHeap()
{
    return m_BasicDescHeap;
}

D3D12_CPU_DESCRIPTOR_HANDLE ResourceManager::TextureDescriptorHeapCPU(uint32_t id)
{
    auto heap_handle = m_BasicDescHeap->GetCPUDescriptorHandleForHeapStart();
    auto device = GraphicEngine::Instance().Device();

    // 定数リソースビューは飛ばす
    heap_handle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * m_ConstantViewNum;
    heap_handle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * id;

    return heap_handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE ResourceManager::ConstantDescriptorHeapCPU(uint32_t id)
{
    auto heap_handle = m_BasicDescHeap->GetCPUDescriptorHandleForHeapStart();
    auto device = GraphicEngine::Instance().Device();

    heap_handle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * id;

    return heap_handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE ResourceManager::TextureDescriptorHeapGPU(uint32_t id)
{
    auto heap_handle = m_BasicDescHeap->GetGPUDescriptorHandleForHeapStart();
    auto device = GraphicEngine::Instance().Device();

    // 定数リソースビューは飛ばす
    heap_handle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * m_ConstantViewNum;
    heap_handle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * id;

    return heap_handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE ResourceManager::ConstantDescriptorHeapGPU(uint32_t id)
{
    auto heap_handle = m_BasicDescHeap->GetGPUDescriptorHandleForHeapStart();
    auto device = GraphicEngine::Instance().Device();

    heap_handle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * id;

    return heap_handle;
}

D3D12_ROOT_PARAMETER* ResourceManager::RootParameter()
{
    return &m_RootParam[0];
}

D3D12_STATIC_SAMPLER_DESC* ResourceManager::SamplerDescriptor()
{
    return &m_Sampler;
}

uint32_t ResourceManager::RootParameterSize() const
{
    return 2;
}

uint32_t ResourceManager::ConstantRootParameterID() const
{
    return 0;
}

uint32_t ResourceManager::TextureRootParameterID() const
{
    return 1;
}

bool ResourceManager::CreateDescriptorHeap()
{
    D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};

    // シェーダーから見えるように
    heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    // マスクは0
    heap_desc.NodeMask = 0;
    // ビューは今のところ1つ
    heap_desc.NumDescriptors = m_ConstantViewNum + m_TextureViewNum;
    // シェーダーリソースビュー用
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

    auto result = GraphicEngine::Instance().Device()->CreateDescriptorHeap(
        &heap_desc,
        IID_PPV_ARGS(&m_BasicDescHeap)
    );

    return result == S_OK;
}

void ResourceManager::CreateRootParameter()
{
    m_DescRange[0].NumDescriptors = m_ConstantViewNum;         // 定数Viewの数を指定
    m_DescRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    m_DescRange[0].BaseShaderRegister = 0;                     // 0番スロットから
    m_DescRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    m_DescRange[1].NumDescriptors = 1;         // テクスチャ数
    m_DescRange[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    m_DescRange[1].BaseShaderRegister = 0;     // 0番スロットから
    m_DescRange[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    m_RootParam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    // 頂点シェーダから見える
    m_RootParam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    m_RootParam[0].DescriptorTable.pDescriptorRanges = &m_DescRange[0];
    m_RootParam[0].DescriptorTable.NumDescriptorRanges = 1;

    m_RootParam[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    // ピクセルシェーダから見える
    m_RootParam[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    m_RootParam[1].DescriptorTable.pDescriptorRanges = &m_DescRange[1];
    m_RootParam[1].DescriptorTable.NumDescriptorRanges = 1;
}

void ResourceManager::CreateSampler()
{
    // 繰り返し
    m_Sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    m_Sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    m_Sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

    m_Sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;    // ボーダーは黒
    m_Sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;                     // 線形補間は黒
    m_Sampler.MaxLOD = D3D12_FLOAT32_MAX;       // ミップマップ最大値
    m_Sampler.MinLOD = 0.0f;                    // ミップマップ最小値
    m_Sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;             // ピクセルシェーダから見える
    m_Sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;                 // リサンプリングしない
}