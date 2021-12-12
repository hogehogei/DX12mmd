#include <Windows.h>
#include <tchar.h>
#include <DirectXMath.h>
#include <DirectXTex.h>

#pragma comment( lib, "d3d12.lib" )
#pragma comment( lib, "dxgi.lib" )
#pragma comment( lib, "d3dcompiler.lib" )
#pragma comment( lib, "DirectXTex.lib")

#ifdef _DEBUG
#include <iostream>
#endif

#include "AppManager.hpp"
#include "Resource.hpp"
#include "VertexBuffer.hpp"
#include "IndexBuffer.hpp"
#include "PMD.hpp"

using namespace std;

//
// constants
//

// 
// static variables
//


//
// static functions
//
static LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

void DebugOutputFormatString(const char* format, ...)
{
#ifdef _DEBUG
    va_list valist;
    va_start(valist, format);
    printf(format, valist);
    va_end(valist);
#endif
}

#ifdef _DEBUG
int main()
{
#else
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
#endif

        
    // ウィンドウクラス生成
    WNDCLASSEX window = {};

    window.cbSize = sizeof(WNDCLASSEX);
    window.lpfnWndProc = (WNDPROC)WindowProcedure;
    window.lpszClassName = _T("DX12mmd");
    window.hInstance = GetModuleHandle(nullptr);

    RegisterClassEx(&window);

    RECT window_rect = { 0, 0, GraphicEngine::k_WindowWidth, GraphicEngine::k_WindowHeight };

    // ウィンドウサイズ補正
    AdjustWindowRect(&window_rect, WS_OVERLAPPEDWINDOW, false);

    HWND hwnd = CreateWindow(
        window.lpszClassName,       // クラス名指定
        _T("DX12 MMD"),             // タイトル文字
        WS_OVERLAPPEDWINDOW,        // タイトルバーと境界線があるウィンドウ
        CW_USEDEFAULT,              // 表示X座標はOSにお任せ
        CW_USEDEFAULT,              // 表示Y座標はOSにお任せ
        window_rect.right - window_rect.left,   // 幅
        window_rect.bottom - window_rect.top,   // 高さ
        nullptr,                    // 親ウィンドウハンドル
        nullptr,                    // メニューハンドル
        window.hInstance,           // 呼び出しアプリケーションハンドル
        nullptr                     // 追加パラメータ
    );

    // ウィンドウ表示
    ShowWindow(hwnd, SW_SHOW);

#ifdef _DEBUG
    GraphicEngine::EnableDebugLayer();
#endif
    if (!GraphicEngine::Initialize(hwnd)) {
        return 1;
    }
    GraphicEngine::Instance().SetViewPort();


    MSG msg = {};
    uint16_t incides[] =
    {
        0, 1, 2,
        2, 1, 3
    };

    PMDData pmd;
    if (!pmd.Open("Model/初音ミク.pmd")) {
        return 1;
    }

    auto vertbuff = std::make_shared<VertexBufferPMD>();
    if (!vertbuff->CreateVertexBuffer(pmd)) {
        return 1;
    }
    auto idxbuff = std::make_shared<IndexBuffer>();
    if (!idxbuff->CreateIndexBuffer(pmd)) {
        return 1;
    }

    GraphicEngine::Instance().SetVertexBuffer(vertbuff);
    GraphicEngine::Instance().SetIndexBuffer(idxbuff);

    if (!GraphicEngine::Instance().CreateGraphicPipeLine()) {
        return 1;
    }



    while (1) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        GraphicEngine::Instance().FlipWindow();

        // アプリケーションが終わるときに message が WM_QUIT になる
        if (msg.message == WM_QUIT) {
            break;
        }
    }

    UnregisterClass(window.lpszClassName, window.hInstance);

    return 0;
}

static LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wparam, lparam);
}

