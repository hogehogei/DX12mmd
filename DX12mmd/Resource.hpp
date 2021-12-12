#pragma once

#include <d3d12.h>
#include <cstdint>
#include <string>
#include <vector>
#include <optional>

struct ResourceOrder
{
    enum Type
    {
        k_ShaderResource,
        k_ConstantResource,
        k_UnorderedResource,
    };

    size_t ResourceCount() const
    {
        return Order.size() * RepeatCount;
    }

    std::string Name;
    std::vector<Type> Order;
    size_t RepeatCount;
    uint64_t ShaderNum;
};

using ResourceDescHandle = std::optional<uint32_t>;

class ResourceManager
{
private:

    struct ResourcePack
    {
        ResourceOrder Order;
        std::vector<D3D12_DESCRIPTOR_RANGE> DescRange;
    };

public:

    ResourceManager();

    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;

    bool Initialize(
        const std::vector<ResourceOrder>& order
    );

    ID3D12DescriptorHeap* DescriptorHeap();
    D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeapCPU(const ResourceDescHandle& res_desc_handle);
    D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeapGPU(const ResourceDescHandle& res_desc_handle);
    ResourceDescHandle ResourceHandle(const std::string& name, size_t repeat_offset = 0, size_t pos = 0) const;

    D3D12_ROOT_PARAMETER* RootParameter();
    D3D12_STATIC_SAMPLER_DESC* SamplerDescriptor();

    uint32_t RootParameterSize() const;
    uint32_t RootParameterID(const std::string& name) const;

private:

    bool CreateDescriptorHeap();
    void CreateRootParameter();
    void CreateSampler();

    size_t CalcDescriptorHeapSize();
    D3D12_DESCRIPTOR_RANGE_TYPE RangeType(ResourceOrder::Type type) const;

    std::vector<ResourcePack> m_ResourceOrder;
    std::vector<D3D12_ROOT_PARAMETER> m_RootParam;

    ID3D12DescriptorHeap* m_BasicDescHeap;
    D3D12_STATIC_SAMPLER_DESC m_Sampler;
};