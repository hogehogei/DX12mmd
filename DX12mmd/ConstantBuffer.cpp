
#include <d3dx12.h>
#include "ConstantBuffer.hpp"
#include "AppManager.hpp"

ConstantBuffer::ConstantBuffer()
    :
    m_ConstBuff(nullptr),
    m_Resource(nullptr)
{}

bool ConstantBuffer::Create(ResourceManager* resource_manager, uint32_t buffer_size, const ResourceDescHandle& const_resource_desc_handle)
{
    m_Resource = resource_manager;

    auto device = GraphicEngine::Instance().Device();

    auto heap_prop = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto resource_desc = CD3DX12_RESOURCE_DESC::Buffer((buffer_size + 0xFF) & ~0xFF);
    device->CreateCommittedResource(
        &heap_prop,
        D3D12_HEAP_FLAG_NONE,
        &resource_desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_ConstBuff)
    );

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc{};
    cbv_desc.BufferLocation = m_ConstBuff->GetGPUVirtualAddress();
    cbv_desc.SizeInBytes = m_ConstBuff->GetDesc().Width;

    device->CreateConstantBufferView(
        &cbv_desc,
        m_Resource->DescriptorHeapCPU(const_resource_desc_handle)
    );

    return true;
}

bool ConstantBuffer::Write(void* ptr, uint32_t size)
{
    uint8_t* map = nullptr;
    auto result = m_ConstBuff->Map(0, nullptr, reinterpret_cast<void**>(&map));
    if (result != S_OK) {
        return false;
    }

    uint8_t* src = reinterpret_cast<uint8_t*>(ptr);
    std::copy_n(src, size, map);

    return true;
}