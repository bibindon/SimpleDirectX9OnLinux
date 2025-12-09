// x86_64-w64-mingw32-g++ main.cpp  -mwindows -ld3d9
// wine a.out

#include <windows.h>
#include <d3d9.h>
#include <math.h> 

// ウィンドウサイズ
const int g_windowWidth = 800;
const int g_windowHeight = 600;

// グローバルなDirect3Dオブジェクト
LPDIRECT3D9 g_direct3D = nullptr;
LPDIRECT3DDEVICE9 g_device = nullptr;
LPDIRECT3DVERTEXBUFFER9 g_vertexBuffer = nullptr;

// 回転角度
float g_angle = 0.0f;

// カスタム頂点構造体（画面座標 + 色）
struct CustomVertex
{
    float x;
    float y;
    float z;
    float rhw;
    DWORD color;
};

const DWORD CUSTOM_FVF = D3DFVF_XYZRHW | D3DFVF_DIFFUSE;

// 前方宣言
LRESULT CALLBACK WindowProc(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam);
bool InitializeD3D(HWND windowHandle);
void CleanupD3D();
void RenderFrame();
void UpdateTriangleVertices(HWND windowHandle);

int WINAPI WinMain(
    HINSTANCE instanceHandle,
    HINSTANCE previousInstanceHandle,
    LPSTR commandLine,
    int commandShow
)
{
    // ウィンドウクラスの登録
    WNDCLASS windowClass = {};
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instanceHandle;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = TEXT("DX9SimpleWindowClass");

    if (RegisterClass(&windowClass) == 0)
    {
        MessageBox(nullptr, TEXT("RegisterClass に失敗しました。"), TEXT("Error"), MB_OK);
        return 0;
    }

    // ウィンドウ矩形の計算（クライアントサイズを指定）
    RECT windowRect = {};
    windowRect.left = 0;
    windowRect.top = 0;
    windowRect.right = g_windowWidth;
    windowRect.bottom = g_windowHeight;

    AdjustWindowRect(
        &windowRect,
        WS_OVERLAPPEDWINDOW,
        FALSE
    );

    int windowWidth = windowRect.right - windowRect.left;
    int windowHeight = windowRect.bottom - windowRect.top;

    // ウィンドウ作成
    HWND windowHandle = CreateWindow(
        windowClass.lpszClassName,
        TEXT("DirectX9 Rotating Triangle"),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        windowWidth,
        windowHeight,
        nullptr,
        nullptr,
        instanceHandle,
        nullptr
    );

    if (windowHandle == nullptr)
    {
        MessageBox(nullptr, TEXT("CreateWindow に失敗しました。"), TEXT("Error"), MB_OK);
        return 0;
    }

    ShowWindow(windowHandle, commandShow);
    UpdateWindow(windowHandle);

    // Direct3D の初期化
    if (!InitializeD3D(windowHandle))
    {
        MessageBox(nullptr, TEXT("Direct3D の初期化に失敗しました。"), TEXT("Error"), MB_OK);
        CleanupD3D();
        return 0;
    }

    // メインループ
    MSG message = {};
    bool isRunning = true;

    while (isRunning)
    {
        if (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE))
        {
            if (message.message == WM_QUIT)
            {
                isRunning = false;
            }
            else
            {
                TranslateMessage(&message);
                DispatchMessage(&message);
            }
        }
        else
        {
            RenderFrame();
        }
    }

    CleanupD3D();

    return static_cast<int>(message.wParam);
}

LRESULT CALLBACK WindowProc(
    HWND windowHandle,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
)
{
    switch (message)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }

    return DefWindowProc(windowHandle, message, wParam, lParam);
}

bool InitializeD3D(HWND windowHandle)
{
    g_direct3D = Direct3DCreate9(D3D_SDK_VERSION);

    if (g_direct3D == nullptr)
    {
        return false;
    }

    D3DPRESENT_PARAMETERS presentParameters = {};
    presentParameters.Windowed = TRUE;
    presentParameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
    presentParameters.BackBufferFormat = D3DFMT_UNKNOWN;
    presentParameters.EnableAutoDepthStencil = FALSE;

    HRESULT result = g_direct3D->CreateDevice(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        windowHandle,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING,
        &presentParameters,
        &g_device
    );

    if (FAILED(result))
    {
        return false;
    }

    result = g_device->CreateVertexBuffer(
        sizeof(CustomVertex) * 3,
        0,
        CUSTOM_FVF,
        D3DPOOL_MANAGED,
        &g_vertexBuffer,
        nullptr
    );

    if (FAILED(result))
    {
        return false;
    }

    return true;
}

void CleanupD3D()
{
    if (g_vertexBuffer != nullptr)
    {
        g_vertexBuffer->Release();
        g_vertexBuffer = nullptr;
    }

    if (g_device != nullptr)
    {
        g_device->Release();
        g_device = nullptr;
    }

    if (g_direct3D != nullptr)
    {
        g_direct3D->Release();
        g_direct3D = nullptr;
    }
}

void RenderFrame()
{
    if (g_device == nullptr)
    {
        return;
    }

    // 回転角度を更新
    g_angle += 0.02f;

    // 頂点データを更新
    HWND foregroundWindowHandle = GetForegroundWindow();
    UpdateTriangleVertices(foregroundWindowHandle);

    // 画面クリア
    g_device->Clear(
        0,
        nullptr,
        D3DCLEAR_TARGET,
        D3DCOLOR_XRGB(0, 40, 80),
        1.0f,
        0
    );

    if (SUCCEEDED(g_device->BeginScene()))
    {
        g_device->SetFVF(CUSTOM_FVF);
        g_device->SetStreamSource(0, g_vertexBuffer, 0, sizeof(CustomVertex));

        g_device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 1);

        g_device->EndScene();
    }

    g_device->Present(nullptr, nullptr, nullptr, nullptr);
}

void UpdateTriangleVertices(HWND windowHandle)
{
    if (g_vertexBuffer == nullptr)
    {
        return;
    }

    RECT clientRect = {};
    int clientWidth = g_windowWidth;
    int clientHeight = g_windowHeight;

    if (windowHandle != nullptr)
    {
        if (GetClientRect(windowHandle, &clientRect))
        {
            clientWidth = clientRect.right - clientRect.left;
            clientHeight = clientRect.bottom - clientRect.top;
        }
    }

    float centerX = static_cast<float>(clientWidth) * 0.5f;
    float centerY = static_cast<float>(clientHeight) * 0.5f;

    int minSize = clientWidth;

    if (clientHeight < minSize)
    {
        minSize = clientHeight;
    }

    float radius = static_cast<float>(minSize) * 0.35f;

    CustomVertex vertices[3];

    for (int i = 0; i < 3; i++)
    {
        float angleOffset = g_angle + static_cast<float>(i) * (2.0f * 3.14159265f / 3.0f);
        float cosineValue = cosf(angleOffset);
        float sineValue = sinf(angleOffset);

        vertices[i].x = centerX + cosineValue * radius;
        vertices[i].y = centerY + sineValue * radius;
        vertices[i].z = 0.5f;
        vertices[i].rhw = 1.0f;

        if (i == 0)
        {
            vertices[i].color = D3DCOLOR_XRGB(255, 0, 0);
        }
        else if (i == 1)
        {
            vertices[i].color = D3DCOLOR_XRGB(0, 255, 0);
        }
        else
        {
            vertices[i].color = D3DCOLOR_XRGB(0, 0, 255);
        }
    }

    void* lockedMemory = nullptr;

    if (SUCCEEDED(g_vertexBuffer->Lock(0, 0, &lockedMemory, 0)))
    {
        memcpy(lockedMemory, vertices, sizeof(vertices));
        g_vertexBuffer->Unlock();
    }
}

