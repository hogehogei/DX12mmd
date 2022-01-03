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
    struct ResourceType
    {
        Type Type;
        size_t Count;
        uint64_t ShaderNum;
    };

    size_t Advance() const
    {
        size_t sum = 0;
        for (const auto& itr : Types) {
            sum += itr.Count;
        }
        
        return sum;
    }
    size_t ResourceCount() const
    {
        return Advance() * RepeatCount;
    }

    std::string Name;
    std::vector<ResourceOrder::ResourceType> Types;
    size_t RepeatCount;
};

class ResourceDescHandle
{
public:

    ResourceDescHandle()
        :
        Handle(std::nullopt),
        Offset(0),
        m_Advance(0)
    {}
    ResourceDescHandle(std::optional<uint32_t> handle, uint32_t offset, size_t advance)
        :
        Handle(handle),
        Offset(offset),
        m_Advance(advance)
    {}

    std::optional<uint32_t> Handle;
    uint32_t Offset;

    size_t AdvanceResourceSize() const
    {
        return m_Advance;
    }
    void Advance()
    {
        Offset += m_Advance;
    }

private:

    size_t m_Advance;
};

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
    bool AddResource(const ResourceOrder& order);

    ID3D12DescriptorHeap* DescriptorHeap(const ResourceDescHandle& res_desc_handle);
    D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeapCPU(const ResourceDescHandle& res_desc_handle);
    D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeapGPU(const ResourceDescHandle& res_desc_handle);
    ResourceDescHandle ResourceHandle(const std::string& name) const;

    D3D12_ROOT_PARAMETER* RootParameter();
    D3D12_STATIC_SAMPLER_DESC* SamplerDescriptor();

    uint32_t RootParameterSize() const;
    uint32_t RootParameterID(const std::string& name) const;

    uint32_t GetDescriptorIncrementSize() const;

private:

    bool CreateDescriptorHeap(const ResourcePack& resouce_pack);
    void CreateRootParameter(ResourcePack* resource);
    void CreateSampler();

    size_t CalcDescriptorHeapSize();
    D3D12_DESCRIPTOR_RANGE_TYPE RangeType(ResourceOrder::Type type) const;

    std::vector<ResourcePack> m_ResourceOrder;
    std::vector<D3D12_ROOT_PARAMETER> m_RootParam;
    std::vector<ID3D12DescriptorHeap*> m_DescHeap;
    D3D12_STATIC_SAMPLER_DESC m_Sampler[2];
};