#pragma once

#include <d3d12.h>
#include <cstdint>

class ResourceManager
{
public:

    ResourceManager();

    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;

    bool Initialize(
        uint32_t constant_view_num,
        uint32_t texture_view_num
    );

    ID3D12DescriptorHeap* DescriptorHeap();
    D3D12_CPU_DESCRIPTOR_HANDLE TextureDescriptorHeapCPU(uint32_t id);
    D3D12_CPU_DESCRIPTOR_HANDLE ConstantDescriptorHeapCPU(uint32_t id);
    D3D12_GPU_DESCRIPTOR_HANDLE TextureDescriptorHeapGPU(uint32_t id);
    D3D12_GPU_DESCRIPTOR_HANDLE ConstantDescriptorHeapGPU(uint32_t id);
    D3D12_ROOT_PARAMETER* RootParameter();
    D3D12_STATIC_SAMPLER_DESC* SamplerDescriptor();

    uint32_t RootParameterSize() const;
    uint32_t ConstantRootParameterID() const;
    uint32_t TextureRootParameterID() const;

private:

    bool CreateDescriptorHeap();
    void CreateRootParameter();
    void CreateSampler();

    uint32_t m_ConstantViewNum;
    uint32_t m_TextureViewNum;

    ID3D12DescriptorHeap* m_BasicDescHeap;
    D3D12_ROOT_PARAMETER m_RootParam[2];
    D3D12_DESCRIPTOR_RANGE m_DescRange[2];
    D3D12_STATIC_SAMPLER_DESC m_Sampler;
};