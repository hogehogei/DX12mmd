#pragma once

#include <cstdint>
#include <memory>
#include <limits>
#include <map>
#include <filesystem>
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

class TextureGroup
{
public:

    TextureGroup();
    TextureGroup( ResourceManager* shader_resource );
    TextureGroup(TextureGroup&) = delete;
    TextureGroup& operator=(TextureGroup&) = delete;

    void SetShaderResource(ResourceManager* shader_resource);
    bool CreateTextures(
        const std::filesystem::path& filepath, 
        TexturePtr* handle
    );
    bool CreatePlaneTexture(
        const std::wstring& name, 
        TexturePtr* handle,
        uint8_t color_r = 0xFF, uint8_t color_g = 0xFF, uint8_t color_b = 0xFF
    );
    bool CreateGradationTexture(
        const std::wstring& name,
        TexturePtr* handle
    );
    bool CreateShaderResourceView(
        const TexturePtr& handle,
        ResourceManager* resource_manager, 
        const ResourceDescHandle& res_desc_handle
    );

private:

    ResourceManager* m_ShaderResource;
    std::map<std::wstring, TexturePtr> m_Textures;
};

class Texture
{
public:

    Texture();
    Texture(Texture&) = delete;
    Texture operator=(Texture&) = delete;

private:

    friend class TextureGroup;
    bool Create(const ImageFmt& image, TextureGroup* group);
    void CreateShaderResourceView(ResourceManager* resource_manager, const ResourceDescHandle& res_desc_handle);

    ID3D12Resource* CreateTemporaryUploadResource(const ImageFmt& image);
    ID3D12Resource* CreateTextureBuffer(const ImageFmt& image);

    ImageFmt        m_ImageInfo;
    ID3D12Resource* m_TexBuff;
    ID3D12Resource* m_UploadBuff;
    TextureGroup*   m_Parent;
};