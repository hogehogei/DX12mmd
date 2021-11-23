#pragma once

#include "Resource.hpp"
#include <d3d12.h>
#include <cstdint>

class ConstantBuffer
{
public:

    ConstantBuffer();

    ConstantBuffer(const ConstantBuffer&) = delete;
    const ConstantBuffer& operator=(const ConstantBuffer&) = delete;

    bool Create(ResourceManager* resource_manager, uint32_t buffer_size, uint32_t desc_id);
    bool Write(void* ptr, uint32_t size);

private:

    ID3D12Resource*  m_ConstBuff;
    ResourceManager* m_Resource;
};