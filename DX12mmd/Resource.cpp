
#include "Resource.hpp"
#include "AppManager.hpp"

ResourceManager::ResourceManager()
    :
    m_ResourceOrder(),
    m_BasicDescHeap(nullptr),
    m_Sampler()
{}

bool ResourceManager::Initialize(const std::vector<ResourceOrder>& order)
{
    for (auto& itr : order) {
        ResourcePack pack{};
        pack.Order = itr;

        m_ResourceOrder.push_back(pack);
    }
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

D3D12_CPU_DESCRIPTOR_HANDLE ResourceManager::DescriptorHeapCPU(const ResourceDescHandle& res_desc_handle)
{
    assert(res_desc_handle);

    auto heap_handle = m_BasicDescHeap->GetCPUDescriptorHandleForHeapStart();
    auto device = GraphicEngine::Instance().Device();
    heap_handle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * res_desc_handle.value();

    return heap_handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE ResourceManager::DescriptorHeapGPU(const ResourceDescHandle& res_desc_handle)
{
    assert(res_desc_handle);

    auto heap_handle = m_BasicDescHeap->GetGPUDescriptorHandleForHeapStart();
    auto device = GraphicEngine::Instance().Device();
    heap_handle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * res_desc_handle.value();

    return heap_handle;
}

ResourceDescHandle ResourceManager::ResourceHandle(const std::string& name, size_t repeat_offset, size_t pos) const
{
    auto result = std::find_if(
        m_ResourceOrder.begin(),
        m_ResourceOrder.end(),
        [name](const ResourcePack& t) { return t.Order.Name == name; }
    );

    assert(result != m_ResourceOrder.end());

    uint32_t handle_distance = 0;
    for (auto itr = m_ResourceOrder.begin(); itr != result; ++itr) {
        handle_distance += itr->Order.ResourceCount();
    }

    handle_distance += (result->Order.Order.size() * repeat_offset);
    handle_distance += pos;

    return handle_distance;
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

bool ResourceManager::CreateDescriptorHeap()
{
    D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};

    // �V�F�[�_�[���猩����悤��
    heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    // �}�X�N��0
    heap_desc.NodeMask = 0;
    // �r���[�͍��̂Ƃ���1��
    heap_desc.NumDescriptors = CalcDescriptorHeapSize();
    // �V�F�[�_�[���\�[�X�r���[�p
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

    auto result = GraphicEngine::Instance().Device()->CreateDescriptorHeap(
        &heap_desc,
        IID_PPV_ARGS(&m_BasicDescHeap)
    );

    return result == S_OK;
}

void ResourceManager::CreateRootParameter()
{

    for (auto& itr : m_ResourceOrder) {
        const std::vector<ResourceOrder::Type>& types = itr.Order.Order;
        for (auto type : types) {
            D3D12_DESCRIPTOR_RANGE desc{};
            desc.NumDescriptors = 1;
            desc.RangeType = RangeType(type);
            desc.BaseShaderRegister = itr.Order.ShaderNum;
            desc.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
            itr.DescRange.push_back(desc);
        }
    }

    for (auto& resource : m_ResourceOrder) {

        D3D12_ROOT_PARAMETER rootparam{};

        rootparam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootparam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootparam.DescriptorTable.pDescriptorRanges = &resource.DescRange[0];
        rootparam.DescriptorTable.NumDescriptorRanges = resource.DescRange.size();

        m_RootParam.push_back(rootparam);
    }
}

void ResourceManager::CreateSampler()
{
    // �J��Ԃ�
    m_Sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    m_Sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    m_Sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

    m_Sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;    // �{�[�_�[�͍�
    m_Sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;                     // ���`��Ԃ͍�
    m_Sampler.MaxLOD = D3D12_FLOAT32_MAX;       // �~�b�v�}�b�v�ő�l
    m_Sampler.MinLOD = 0.0f;                    // �~�b�v�}�b�v�ŏ��l
    m_Sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;             // �s�N�Z���V�F�[�_���猩����
    m_Sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;                 // ���T���v�����O���Ȃ�
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