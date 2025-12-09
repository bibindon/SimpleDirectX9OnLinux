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

// 3Dベクトル
struct Vector3
{
    float x;
    float y;
    float z;
};

// カスタム頂点構造体（3D座標 + 色）
struct CustomVertex
{
    float x;
    float y;
    float z;
    DWORD color;
};

const DWORD CUSTOM_FVF = D3DFVF_XYZ | D3DFVF_DIFFUSE;

// 前方宣言
LRESULT CALLBACK WindowProc(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam);
bool InitializeD3D(HWND windowHandle);
void CleanupD3D();
void RenderFrame();

// ベクトル／行列関連関数
Vector3 Vector3Create(float xValue, float yValue, float zValue);
Vector3 Vector3Subtract(const Vector3 &left, const Vector3 &right);
Vector3 Vector3Cross(const Vector3 &left, const Vector3 &right);
float Vector3Dot(const Vector3 &left, const Vector3 &right);
Vector3 Vector3Normalize(const Vector3 &value);
D3DMATRIX CreateRotationYMatrix(float angle);
D3DMATRIX CreateLookAtLHMatrix(const Vector3 &eyePosition, const Vector3 &targetPosition, const Vector3 &upDirection);
D3DMATRIX CreatePerspectiveFovLHMatrix(float fieldOfViewY, float aspectRatio, float nearZ, float farZ);

int WINAPI WinMain(
    HINSTANCE instanceHandle,
    HINSTANCE previousInstanceHandle,
    LPSTR commandLine,
    int commandShow
)
{
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

    HWND windowHandle = CreateWindow(
        windowClass.lpszClassName,
        TEXT("DirectX9 Rotating Triangle (Y-Axis)"),
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

    if (!InitializeD3D(windowHandle))
    {
        MessageBox(nullptr, TEXT("Direct3D の初期化に失敗しました。"), TEXT("Error"), MB_OK);
        CleanupD3D();
        return 0;
    }

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

    // 頂点カラーをそのまま使いたいのでライティングを無効化
    g_device->SetRenderState(D3DRS_LIGHTING, FALSE);


    g_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);

    // ビューマトリックス（カメラ位置 (0, 0, -5) から原点を見る）
    Vector3 eyePosition = Vector3Create(0.0f, 0.0f, -5.0f);
    Vector3 targetPosition = Vector3Create(0.0f, 0.0f, 0.0f);
    Vector3 upDirection = Vector3Create(0.0f, 1.0f, 0.0f);

    D3DMATRIX viewMatrix = CreateLookAtLHMatrix(
        eyePosition,
        targetPosition,
        upDirection
    );

    g_device->SetTransform(D3DTS_VIEW, &viewMatrix);

    // プロジェクションマトリックス
    float aspectRatio =
        static_cast<float>(g_windowWidth) /
        static_cast<float>(g_windowHeight);

    const float fieldOfViewY = 3.14159265f / 4.0f;

    D3DMATRIX projectionMatrix = CreatePerspectiveFovLHMatrix(
        fieldOfViewY,
        aspectRatio,
        1.0f,
        100.0f
    );

    g_device->SetTransform(D3DTS_PROJECTION, &projectionMatrix);

    // 頂点バッファ作成
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

    CustomVertex *vertexData = nullptr;

    result = g_vertexBuffer->Lock(
        0,
        0,
        reinterpret_cast<void **>(&vertexData),
        0
    );

    if (FAILED(result))
    {
        return false;
    }

    // 原点付近に1枚の三角形を配置（Z = 0）
    vertexData[0].x = 0.0f;
    vertexData[0].y = 1.0f;
    vertexData[0].z = 0.0f;
    vertexData[0].color = D3DCOLOR_XRGB(255, 0, 0);

    vertexData[1].x = 1.0f;
    vertexData[1].y = -1.0f;
    vertexData[1].z = 0.0f;
    vertexData[1].color = D3DCOLOR_XRGB(0, 255, 0);

    vertexData[2].x = -1.0f;
    vertexData[2].y = -1.0f;
    vertexData[2].z = 0.0f;
    vertexData[2].color = D3DCOLOR_XRGB(0, 0, 255);

    g_vertexBuffer->Unlock();

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

    g_angle += 0.02f;

    const float fullRotation = 6.28318531f;

    if (g_angle > fullRotation)
    {
        g_angle -= fullRotation;
    }

    // Y軸回転のワールドマトリックスを設定
    D3DMATRIX worldMatrix = CreateRotationYMatrix(g_angle);
    g_device->SetTransform(D3DTS_WORLD, &worldMatrix);

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

//====================
// ベクトル／行列関数
//====================

Vector3 Vector3Create(float xValue, float yValue, float zValue)
{
    Vector3 result;
    result.x = xValue;
    result.y = yValue;
    result.z = zValue;
    return result;
}

Vector3 Vector3Subtract(const Vector3 &left, const Vector3 &right)
{
    Vector3 result;
    result.x = left.x - right.x;
    result.y = left.y - right.y;
    result.z = left.z - right.z;
    return result;
}

Vector3 Vector3Cross(const Vector3 &left, const Vector3 &right)
{
    Vector3 result;
    result.x = left.y * right.z - left.z * right.y;
    result.y = left.z * right.x - left.x * right.z;
    result.z = left.x * right.y - left.y * right.x;
    return result;
}

float Vector3Dot(const Vector3 &left, const Vector3 &right)
{
    float result =
        left.x * right.x +
        left.y * right.y +
        left.z * right.z;

    return result;
}

Vector3 Vector3Normalize(const Vector3 &value)
{
    float lengthSquared = Vector3Dot(value, value);

    if (lengthSquared <= 0.000001f)
    {
        Vector3 zeroVector;
        zeroVector.x = 0.0f;
        zeroVector.y = 0.0f;
        zeroVector.z = 0.0f;
        return zeroVector;
    }

    float inverseLength = 1.0f / sqrtf(lengthSquared);

    Vector3 result;
    result.x = value.x * inverseLength;
    result.y = value.y * inverseLength;
    result.z = value.z * inverseLength;

    return result;
}

D3DMATRIX CreateRotationYMatrix(float angle)
{
    float cosineValue = cosf(angle);
    float sineValue = sinf(angle);

    D3DMATRIX matrix;

    matrix._11 = cosineValue;
    matrix._12 = 0.0f;
    matrix._13 = sineValue;
    matrix._14 = 0.0f;

    matrix._21 = 0.0f;
    matrix._22 = 1.0f;
    matrix._23 = 0.0f;
    matrix._24 = 0.0f;

    matrix._31 = -sineValue;
    matrix._32 = 0.0f;
    matrix._33 = cosineValue;
    matrix._34 = 0.0f;

    matrix._41 = 0.0f;
    matrix._42 = 0.0f;
    matrix._43 = 0.0f;
    matrix._44 = 1.0f;

    return matrix;
}

D3DMATRIX CreateLookAtLHMatrix(
    const Vector3 &eyePosition,
    const Vector3 &targetPosition,
    const Vector3 &upDirection
)
{
    Vector3 forwardDirection = Vector3Subtract(targetPosition, eyePosition);
    forwardDirection = Vector3Normalize(forwardDirection);

    Vector3 rightDirection = Vector3Cross(upDirection, forwardDirection);
    rightDirection = Vector3Normalize(rightDirection);

    Vector3 upVector = Vector3Cross(forwardDirection, rightDirection);

    float negativeDotRight = -Vector3Dot(rightDirection, eyePosition);
    float negativeDotUp = -Vector3Dot(upVector, eyePosition);
    float negativeDotForward = -Vector3Dot(forwardDirection, eyePosition);

    D3DMATRIX matrix;

    matrix._11 = rightDirection.x;
    matrix._12 = upVector.x;
    matrix._13 = forwardDirection.x;
    matrix._14 = 0.0f;

    matrix._21 = rightDirection.y;
    matrix._22 = upVector.y;
    matrix._23 = forwardDirection.y;
    matrix._24 = 0.0f;

    matrix._31 = rightDirection.z;
    matrix._32 = upVector.z;
    matrix._33 = forwardDirection.z;
    matrix._34 = 0.0f;

    matrix._41 = negativeDotRight;
    matrix._42 = negativeDotUp;
    matrix._43 = negativeDotForward;
    matrix._44 = 1.0f;

    return matrix;
}

D3DMATRIX CreatePerspectiveFovLHMatrix(
    float fieldOfViewY,
    float aspectRatio,
    float nearZ,
    float farZ
)
{
    float halfFovY = fieldOfViewY * 0.5f;
    float yScale = 1.0f / tanf(halfFovY);
    float xScale = yScale / aspectRatio;
    float zRange = farZ - nearZ;

    D3DMATRIX matrix;

    matrix._11 = xScale;
    matrix._12 = 0.0f;
    matrix._13 = 0.0f;
    matrix._14 = 0.0f;

    matrix._21 = 0.0f;
    matrix._22 = yScale;
    matrix._23 = 0.0f;
    matrix._24 = 0.0f;

    matrix._31 = 0.0f;
    matrix._32 = 0.0f;
    matrix._33 = farZ / zRange;
    matrix._34 = 1.0f;

    matrix._41 = 0.0f;
    matrix._42 = 0.0f;
    matrix._43 = (-nearZ * farZ) / zRange;
    matrix._44 = 0.0f;

    return matrix;
}

