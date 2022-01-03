#pragma once

#include "Resource.hpp"
#include <d3d12.h>
#include <functional>
#include <cstdint>
#include <memory>

class ConstantBuffer
{
public:
    using WriterFunc = std::function<void(void* srcdata, uint8_t* dst)>;

public:

    ConstantBuffer();

    ConstantBuffer(const ConstantBuffer&) = delete;
    const ConstantBuffer& operator=(const ConstantBuffer&) = delete;

    bool Create(ResourceManager* resource_manager, 
                uint32_t buffer_size,
                uint32_t buffer_count, 
                const ResourceDescHandle& const_resource_desc_handle);
    bool Write(void* ptr, uint32_t size);
    bool Write(void* srcdata, WriterFunc func);

private:

    ID3D12Resource*  m_ConstBuff;
    ResourceManager* m_Resource;
};

using ConstantBufferPtr = std::shared_ptr<ConstantBuffer>;