#include <Windows.h>
#include <tchar.h>

#pragma comment( lib, "d3d12.lib" )
#pragma comment( lib, "dxgi.lib" )

#ifdef _DEBUG
#include <iostream>
#endif

#include "AppManager.hpp"

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

        
    // �E�B���h�E�N���X����
    WNDCLASSEX window = {};

    window.cbSize = sizeof(WNDCLASSEX);
    window.lpfnWndProc = (WNDPROC)WindowProcedure;
    window.lpszClassName = _T("DX12mmd");
    window.hInstance = GetModuleHandle(nullptr);

    RegisterClassEx(&window);

    RECT window_rect = { 0, 0, GraphicEngine::k_WindowWidth, GraphicEngine::k_WindowHeight };

    // �E�B���h�E�T�C�Y�␳
    AdjustWindowRect(&window_rect, WS_OVERLAPPEDWINDOW, false);

    HWND hwnd = CreateWindow(
        window.lpszClassName,       // �N���X���w��
        _T("DX12 MMD"),             // �^�C�g������
        WS_OVERLAPPEDWINDOW,        // �^�C�g���o�[�Ƌ��E��������E�B���h�E
        CW_USEDEFAULT,              // �\��X���W��OS�ɂ��C��
        CW_USEDEFAULT,              // �\��Y���W��OS�ɂ��C��
        window_rect.right - window_rect.left,   // ��
        window_rect.bottom - window_rect.top,   // ����
        nullptr,                    // �e�E�B���h�E�n���h��
        nullptr,                    // ���j���[�n���h��
        window.hInstance,           // �Ăяo���A�v���P�[�V�����n���h��
        nullptr                     // �ǉ��p�����[�^
    );

    // �E�B���h�E�\��
    ShowWindow(hwnd, SW_SHOW);

#ifdef _DEBUG
    GraphicEngine::EnableDebugLayer();
#endif
    GraphicEngine::Initialize(hwnd);


    MSG msg = {};

    while (1) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        GraphicEngine::Instance().FlipWindow();

        // �A�v���P�[�V�������I���Ƃ��� message �� WM_QUIT �ɂȂ�
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
