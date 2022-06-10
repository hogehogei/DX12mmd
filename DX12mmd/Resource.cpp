
#include "Resource.hpp"
#include "AppManager.hpp"

ResourceManager::ResourceManager()
    :
    m_ResourceOrder(),
    m_DescHeap(),
    m_Sampler()
{}

bool ResourceManager::Initialize(const std::vector<ResourceOrder>& order)
{
    for (auto& itr : order) {
        ResourcePack pack{};
        pack.Order = itr;

        m_ResourceOrder.push_back(pack);
    }
    for( int i = 0; i < m_ResourceOrder.size(); ++i ){
        CreateRootParameter(&m_ResourceOrder[i]);
        if (!CreateDescriptorHeap(m_ResourceOrder[i])) {
            return false;
        }
    }
    CreateSampler();

    return true;
}

bool ResourceManager::AddResource(const ResourceOrder& order)
{
    ResourcePack pack{};
    pack.Order = order;

    m_ResourceOrder.push_back(pack);
    CreateRootParameter(&m_ResourceOrder[m_ResourceOrder.size() - 1]);
    if (!CreateDescriptorHeap(m_ResourceOrder[m_ResourceOrder.size() - 1])) {
        return false;
    }

    return true;
}

ID3D12DescriptorHeap* ResourceManager::DescriptorHeap(const ResourceDescHandle& res_desc_handle)
{
    assert(res_desc_handle.Handle);

    auto handle = m_DescHeap[res_desc_handle.Handle.value()];

    return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE ResourceManager::DescriptorHeapCPU(const ResourceDescHandle& res_desc_handle)
{
    assert(res_desc_handle.Handle);

    auto device = GraphicEngine::Instance().Device();
    auto heap_handle = m_DescHeap[res_desc_handle.Handle.value()]->GetCPUDescriptorHandleForHeapStart();
    heap_handle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * static_cast<size_t>(res_desc_handle.Offset);

    return heap_handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE ResourceManager::DescriptorHeapGPU(const ResourceDescHandle& res_desc_handle)
{
    assert(res_desc_handle.Handle);

    auto device = GraphicEngine::Instance().Device();
    auto heap_handle = m_DescHeap[res_desc_handle.Handle.value()]->GetGPUDescriptorHandleForHeapStart();
    heap_handle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * static_cast<size_t>(res_desc_handle.Offset);

    return heap_handle;
}

ResourceDescHandle ResourceManager::ResourceHandle(const std::string& name) const
{
    auto result = std::find_if(
        m_ResourceOrder.begin(),
        m_ResourceOrder.end(),
        [name](const ResourcePack& t) { return t.Order.Name == name; }
    );
    
    assert(result != m_ResourceOrder.end());
    if (result == m_ResourceOrder.end()) {
        return ResourceDescHandle{ std::nullopt, 0, 0 };
    }

    return ResourceDescHandle{ std::distance(m_ResourceOrder.begin(), result), 0, result->Order.Advance() };
}

D3D12_ROOT_PARAMETER* ResourceManager::RootParameter()
{
    return &m_RootParam[0];
}

D3D12_STATIC_SAMPLER_DESC* ResourceManager::SamplerDescriptor()
{
    return m_Sampler;
}

uint32_t ResourceManager::RootParameterSize() const
{
    return m_RootParam.size();
}

uint32_t ResourceManager::RootParameterID(const std::string& name) const
{
    auto result = std::find_if(
        m_ResourceOrder.begin(),
        m_ResourceOrder.end(),
        [name](const ResourcePack& t) { return t.Order.Name == name; }
    );

    assert(result != m_ResourceOrder.end());

    return std::distance(m_ResourceOrder.begin(), result);
}

uint32_t ResourceManager::GetDescriptorIncrementSize() const
{
    auto device = GraphicEngine::Instance().Device();
    return device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

bool ResourceManager::CreateDescriptorHeap( const ResourcePack& resouce_pack )
{
    D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};

    // シェーダーから見えるように
    heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    // マスクは0
    heap_desc.NodeMask = 0;
    // ビュー数を設定
    heap_desc.NumDescriptors = resouce_pack.Order.ResourceCount();
    // シェーダーリソースビュー用
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    
    ID3D12DescriptorHeap* descheap = nullptr;
    auto result = GraphicEngine::Instance().Device()->CreateDescriptorHeap(
        &heap_desc,
        IID_PPV_ARGS(&descheap)
    );
    
    if (result == S_OK) {
        m_DescHeap.push_back(descheap);
    }

    return result == S_OK;
}

void ResourceManager::CreateRootParameter(ResourceManager::ResourcePack* resource_pack)
{
    const std::vector<ResourceOrder::ResourceType>& types = resource_pack->Order.Types;

    for (auto type : types) {
        D3D12_DESCRIPTOR_RANGE desc{};
        desc.NumDescriptors = type.Count;
        desc.RangeType = RangeType(type.Type);
        desc.BaseShaderRegister = type.ShaderNum;
        desc.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        resource_pack->DescRange.push_back(desc);
    }

    D3D12_ROOT_PARAMETER rootparam{};

    rootparam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootparam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootparam.DescriptorTable.pDescriptorRanges = resource_pack->DescRange.data();
    rootparam.DescriptorTable.NumDescriptorRanges = resource_pack->DescRange.size();

    m_RootParam.push_back(rootparam);
}

void ResourceManager::CreateSampler()
{
    // 繰り返し
    m_Sampler[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    m_Sampler[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    m_Sampler[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

    m_Sampler[0].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;    // ボーダーは黒
    m_Sampler[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;                     // 線形補間は黒
    m_Sampler[0].MaxLOD = D3D12_FLOAT32_MAX;       // ミップマップ最大値
    m_Sampler[0].MinLOD = 0.0f;                    // ミップマップ最小値
    m_Sampler[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;             // ピクセルシェーダから見える
    m_Sampler[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;                 // リサンプリングしない
    m_Sampler[0].ShaderRegister = 0;                // スロット番号0

    m_Sampler[1] = m_Sampler[0];
    // 繰り返し
    m_Sampler[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    m_Sampler[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    m_Sampler[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    m_Sampler[1].ShaderRegister = 1;                // スロット番号1
}

size_t ResourceManager::CalcDescriptorHeapSize()
{
    size_t sum = 0;
    for (const auto& itr : m_ResourceOrder) {
        sum += itr.Order.ResourceCount();
    }

    return sum;
}

D3D12_DESCRIPTOR_RANGE_TYPE ResourceManager::RangeType(ResourceOrder::Type type) const
{
    switch (type)
    {
    case ResourceOrder::k_ShaderResource:
        return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    case ResourceOrder::k_ConstantResource:
        return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    case ResourceOrder::k_UnorderedResource:
        return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    default:
        break;
    }

    assert(0);
}