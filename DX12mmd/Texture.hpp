#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <d3d12.h>

struct ImageFmt
{
    size_t Height;
    size_t Width;
    size_t RowPitchByte;
    size_t Size;
    size_t Depth;
    uint16_t ArraySize;
    uint16_t MipLevels;
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
    TextureGroup(TextureGroup&) = delete;
    TextureGroup& operator=(TextureGroup&) = delete;

    bool CreateTextures(const wchar_t* filename);

    ID3D12DescriptorHeap* TextureDescriptorHeap();
    D3D12_ROOT_PARAMETER* RootParameter();
    D3D12_STATIC_SAMPLER_DESC* SamplerDescriptor();

private:

    bool CreateDescriptorHeap();
    void CreateRootParameter();
    void CreateSampler();

    std::vector<TexRGBA> m_TextureData;
    std::vector<TexturePtr> m_Textures;
    ID3D12DescriptorHeap* m_TexDescHeap;
    D3D12_ROOT_PARAMETER m_RootParam;
    D3D12_DESCRIPTOR_RANGE m_DescRange;
    D3D12_STATIC_SAMPLER_DESC m_Sampler;
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

    ID3D12Resource* CreateTemporaryUploadResource(const ImageFmt& image);
    ID3D12Resource* CreateTextureBuffer(const ImageFmt& image);
    void CreateShaderResourceView();

    ImageFmt        m_ImageInfo;
    ID3D12Resource* m_TexBuff;
    ID3D12Resource* m_UploadBuff;
    TextureGroup*   m_Resource;
};