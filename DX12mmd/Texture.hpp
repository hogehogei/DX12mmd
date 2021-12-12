#pragma once

#include <cstdint>
#include <memory>
#include <limits>
#include <vector>
#include <d3d12.h>

#include "Resource.hpp"

struct ImageFmt
{
    size_t Height;
    size_t Width;
    size_t RowPitchByte;
    size_t Size;
    size_t Depth;
    size_t ArraySize;
    size_t MipLevels;
    DXGI_FORMAT Format;
    uint8_t* Pixels;
    D3D12_RESOURCE_DIMENSION Dimension;
};

struct TexRGBA
{
    uint8_t R, G, B, A;
};

class Texture;
using TexturePtr = std::shared_ptr<Texture>;
using TextureHandle = uint32_t;

class TextureGroup
{
public:

    TextureGroup();
    TextureGroup(TextureGroup&) = delete;
    TextureGroup& operator=(TextureGroup&) = delete;

    bool CreateTextures(
        ResourceManager* shader_resource,
        const ResourceDescHandle& shader_resource_desc_handle,
        const wchar_t* filename, 
        TextureHandle* handle
    );
    
    D3D12_CPU_DESCRIPTOR_HANDLE TextureDescriptorHeapCPU(TextureHandle handle);
    D3D12_GPU_DESCRIPTOR_HANDLE TextureDescriptorHeapGPU(TextureHandle handle);

    TextureHandle GetTextureHandle(Texture* texptr);

private:

    ResourceManager* m_ShaderResource;
    std::vector<TexturePtr> m_Textures;
};

class Texture
{
public:

    static constexpr TextureHandle k_InvalidHandle = 0xFFFFFFFF;

    Texture();
    Texture(Texture&) = delete;
    Texture operator=(Texture&) = delete;

private:

    friend class TextureGroup;
    bool Create(const ImageFmt& image, TextureGroup* group, const ResourceDescHandle& res_desc_handle);
    ResourceDescHandle GetResourceDescHandle() const;

    ID3D12Resource* CreateTemporaryUploadResource(const ImageFmt& image);
    ID3D12Resource* CreateTextureBuffer(const ImageFmt& image);
    void CreateShaderResourceView();

    ImageFmt        m_ImageInfo;
    ID3D12Resource* m_TexBuff;
    ID3D12Resource* m_UploadBuff;
    TextureGroup*   m_Parent;
    ResourceDescHandle m_ResourceDescHandle;
};