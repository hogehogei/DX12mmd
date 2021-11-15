
#include "Shader.hpp"

#include <string>
#include <d3dcompiler.h>

namespace
{
    LPCSTR k_ShaderStr[] = {
        "vs_5_0",
        "ps_5_0"
    };
}

CompileShader::CompileShader()
    :
    m_IsValid(false),
    m_Blob(nullptr)
{}

CompileShader::CompileShader(LPCWSTR filename, LPCSTR entrypoint, Type type)
    :
    m_IsValid(false),
    m_Blob(nullptr)
{
    ID3DBlob* errorblob = nullptr;

    auto result = D3DCompileFromFile(
        filename,
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entrypoint,
        k_ShaderStr[static_cast<int>(type)],
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
        0,
        &m_Blob,
        &errorblob
    );

    m_IsValid = (result == S_OK) ? true : false;

    if (result != S_OK) {
        if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
            ::OutputDebugStringA("Not found file");
        }
        else
        {
            std::string errstr;
            errstr.resize(errorblob->GetBufferSize());

            std::copy_n(
                (char*)errorblob->GetBufferPointer(),
                errorblob->GetBufferSize(),
                errstr.begin()
            );
            errstr += "\n";

            ::OutputDebugStringA(errstr.c_str());
        }
    }
}

CompileShader::~CompileShader()
{}

bool CompileShader::IsValid() const
{
    return m_IsValid;
}

ID3DBlob* CompileShader::GetBlob()
{
    return m_Blob;
}