#pragma once

#include <d3d12.h>

class CompileShader
{
public:
    enum class Type
    {
        k_VertexShader = 0,
        k_PixelShader,
    };

public:

    CompileShader();
    CompileShader(LPCWSTR filename, LPCSTR entrypoint, Type type);
    ~CompileShader() noexcept;

    bool IsValid() const;
    ID3DBlob* GetBlob();

private:

    bool      m_IsValid;
    ID3DBlob* m_Blob;
};