#include "MeshletsDemo.h"

#include <DX12Library/Application.h>
#include <DX12Library/CommandQueue.h>
#include <DX12Library/CommandList.h>
#include <DX12Library/Helpers.h>
#include <DX12Library/Window.h>
#include <Framework/Bone.h>
#include <Framework/Animation.h>
#include <Framework/GameObject.h>
#include <Framework/Light.h>

#include <wrl.h>

#include <Framework/ComputeShader.h>
#include <Framework/GraphicsSettings.h>
#include <Framework/Model.h>
#include <Framework/ModelLoader.h>
#include <Framework/Shader.h>

using namespace Microsoft::WRL;

#include <d3d12.h>
#include <d3dx12.h>
#include <DirectXColors.h>

using namespace DirectX;

#include <algorithm> // For std::min and std::max.
#if defined(min)
#undef min
#endif

#include <DX12Library/ShaderUtils.h>

#if defined(max)
#undef max
#endif

#include "RenderGraph.User.h"

namespace
{

    // Clamp a value between a min and max range.
    template <typename T>
    constexpr const T& Clamp(const T& val, const T& min, const T& max)
    {
        return val < min ? min : val > max ? max : val;
    }

    template<typename T>
    constexpr const T& Remap(const T& low1, const T& high1, const T& low2, const T& high2, const T& value)
    {
        return low2 + (value - low1) * (high2 - low2) / (high1 - low1);
    }

    template<typename T>
    constexpr T Smoothstep(const T& edge0, const T& edge1, const T& x)
    {
        auto t = Clamp<T>((x - edge0) / (edge1 - edge0), 0, 1);
        return t * t * (3 - 2 * t);
    }

    bool allowFullscreenToggle = true;

    // Builds a look-at (world) matrix from a point, up and direction vectors.
    XMMATRIX XM_CALLCONV LookAtMatrix(FXMVECTOR position, FXMVECTOR direction, FXMVECTOR up)
    {
        assert(!XMVector3Equal(direction, XMVectorZero()));
        assert(!XMVector3IsInfinite(direction));
        assert(!XMVector3Equal(up, XMVectorZero()));
        assert(!XMVector3IsInfinite(up));

        XMVECTOR R2 = XMVector3Normalize(direction);

        XMVECTOR R0 = XMVector3Cross(up, R2);
        R0 = XMVector3Normalize(R0);

        XMVECTOR R1 = XMVector3Cross(R2, R0);

        XMMATRIX M(R0, R1, R2, position);

        return M;
    }
}

MeshletsDemo::MeshletsDemo(const std::wstring& name, int width, int height, GraphicsSettings graphicsSettings)
    : Base(name, width, height, graphicsSettings.VSync)
    , m_Viewport(CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)))
    , m_ScissorRect(CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX))
    , m_GraphicsSettings(graphicsSettings)
    , m_CameraController{}
    , m_Width(0)
    , m_Height(0)
{
    const XMVECTOR cameraPos = XMVectorSet(0, 5, -20, 1);
    const XMVECTOR cameraTarget = XMVectorSet(0, 5, 0, 1);
    const XMVECTOR cameraUp = XMVectorSet(0, 1, 0, 0);

    m_Camera.SetLookAt(cameraPos, cameraTarget, cameraUp);

    m_PAlignedCameraData = static_cast<CameraData*>(_aligned_malloc(sizeof(CameraData), 16));

    m_PAlignedCameraData->m_InitialPosition = m_Camera.GetTranslation();
    m_PAlignedCameraData->m_InitialQRotation = m_Camera.GetRotation();
}

MeshletsDemo::~MeshletsDemo()
{
    _aligned_free(m_PAlignedCameraData);
}

bool MeshletsDemo::LoadContent()
{
    const auto device = Application::Get().GetDevice();
    const auto commandQueue = Application::Get().GetCommandQueue();
    const auto commandList = commandQueue->GetCommandList();

    // Generate default texture
    {
        m_WhiteTexture2d = std::make_shared<Texture>();
        commandList->LoadTextureFromFile(*m_WhiteTexture2d, L"Assets/Textures/white.png");
    }

    m_RootSignature = std::make_shared<CommonRootSignature>(m_WhiteTexture2d);

    m_DirectionalLight.m_Color = XMFLOAT4(1, 1, 1, 1);
    m_DirectionalLight.m_DirectionWs = XMFLOAT4(1, 1, 0, 0);

    auto pShader = std::make_shared<Shader>(m_RootSignature, ShaderBlob(L"Meshlet_VS.cso"), ShaderBlob(L"Meshlet_PS.cso"));
    auto pMaterial = std::make_shared<Material>(pShader);

    {
        ModelLoader modelLoader;

        {
            auto model = modelLoader.Load(*commandList, "Assets/Models/teapot/teapot.obj", true);

            XMMATRIX translationMatrix = XMMatrixTranslation(0.0f, 0.0f, 0.0f);
            XMMATRIX rotationMatrix = XMMatrixIdentity();
            XMMATRIX scaleMatrix = XMMatrixScaling(0.1f, 0.1f, 0.1f);
            XMMATRIX worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;
            m_GameObjects.push_back(GameObject(worldMatrix, model, pMaterial));
        }
    }

    m_RenderGraph = RenderGraph::User::Create(*this, m_RootSignature, *commandList);

    auto fenceValue = commandQueue->ExecuteCommandList(commandList);
    commandQueue->WaitForFenceValue(fenceValue);

    return true;
}

void MeshletsDemo::OnResize(ResizeEventArgs& e)
{
    Base::OnResize(e);

    if (m_Width != e.Width || m_Height != e.Height)
    {
        m_Width = std::max(1, e.Width);
        m_Height = std::max(1, e.Height);

        const float aspectRatio = static_cast<float>(m_Width) / static_cast<float>(m_Height);
        m_Camera.SetProjection(45.0f, aspectRatio, 0.1f, 1000.0f);

        m_Viewport = CD3DX12_VIEWPORT(0.0f, 0.0f,
            static_cast<float>(m_Width), static_cast<float>(m_Height));

        if (m_RenderGraph)
            m_RenderGraph->MarkDirty();
    }
}

void MeshletsDemo::UnloadContent()
{}

void MeshletsDemo::OnUpdate(UpdateEventArgs& e)
{
    static uint64_t frameCount = 0;
    static double totalTime = 0.0;

    Base::OnUpdate(e);

    if (e.ElapsedTime > 1.0f) return;

    totalTime += e.ElapsedTime;
    m_Time += e.ElapsedTime;
    frameCount++;

    if (totalTime > 1.0)
    {
        double fps = frameCount / totalTime;

        char buffer[512];
        sprintf_s(buffer, "FPS: %f\n", fps);
        OutputDebugStringA(buffer);

        frameCount = 0;
        totalTime = 0.0;
    }

    // Update the camera.
    float speedMultiplier = (m_CameraController.m_Shift ? 16.0f : 4.0f);

    XMVECTOR cameraTranslate = XMVectorSet(m_CameraController.m_Right - m_CameraController.m_Left, 0.0f,
        m_CameraController.m_Forward - m_CameraController.m_Backward,
        1.0f) * speedMultiplier
        * static_cast<float>(e.ElapsedTime);
    XMVECTOR cameraPan = XMVectorSet(0.0f, m_CameraController.m_Up - m_CameraController.m_Down, 0.0f, 1.0f) *
        speedMultiplier *
        static_cast<float>(e.ElapsedTime);
    m_Camera.Translate(cameraTranslate, Space::Local);
    m_Camera.Translate(cameraPan, Space::Local);

    XMVECTOR cameraRotation = XMQuaternionRotationRollPitchYaw(XMConvertToRadians(m_CameraController.m_Pitch),
        XMConvertToRadians(m_CameraController.m_Yaw), 0.0f);
    m_Camera.SetRotation(cameraRotation);

    auto lightDirectionWS = DirectX::XMLoadFloat4(&m_DirectionalLight.m_DirectionWs);
    lightDirectionWS = DirectX::XMVector3Normalize(lightDirectionWS);
    DirectX::XMStoreFloat4(&m_DirectionalLight.m_DirectionWs, lightDirectionWS);

    auto dt = static_cast<float>(e.ElapsedTime);
}

void MeshletsDemo::OnRender(RenderEventArgs& e)
{
    Base::OnRender(e);

    static uint64_t frameIndex = 0;

    const XMMATRIX viewMatrix = m_Camera.GetViewMatrix();
    const XMMATRIX projectionMatrix = m_Camera.GetProjectionMatrix();
    const XMMATRIX viewProjectionMatrix = viewMatrix * projectionMatrix;

    {
        RenderGraph::RenderMetadata metadata;
        metadata.m_ScreenWidth = m_Width;
        metadata.m_ScreenHeight = m_Height;
        metadata.m_Time = m_Time;
        metadata.m_FrameIndex = frameIndex;
        m_RenderGraph->Execute(metadata);
    }


    m_RenderGraph->Present(PWindow);
    frameIndex++;
}

void MeshletsDemo::OnKeyPressed(KeyEventArgs& e)
{
    Base::OnKeyPressed(e);

    switch (e.Key)
    {
    case KeyCode::Escape:
        Application::Get().Quit(0);
        break;
    case KeyCode::Enter:
        if (e.Alt)
        {
    case KeyCode::F11:
        if (allowFullscreenToggle)
        {
            PWindow->ToggleFullscreen();
            allowFullscreenToggle = false;
        }
        break;
        }
    case KeyCode::V:
        PWindow->ToggleVSync();
        m_GraphicsSettings.VSync = !m_GraphicsSettings.VSync;
        break;
    case KeyCode::R:
        // Reset camera transform
        m_Camera.SetTranslation(m_PAlignedCameraData->m_InitialPosition);
        m_Camera.SetRotation(m_PAlignedCameraData->m_InitialQRotation);
        m_CameraController.m_Pitch = 0.0f;
        m_CameraController.m_Yaw = 0.0f;
        break;
    case KeyCode::Up:
    case KeyCode::W:
        m_CameraController.m_Forward = 1.0f;
        break;
    case KeyCode::Left:
    case KeyCode::A:
        m_CameraController.m_Left = 1.0f;
        break;
    case KeyCode::Down:
    case KeyCode::S:
        m_CameraController.m_Backward = 1.0f;
        break;
    case KeyCode::Right:
    case KeyCode::D:
        m_CameraController.m_Right = 1.0f;
        break;
    case KeyCode::Q:
        m_CameraController.m_Down = 1.0f;
        break;
    case KeyCode::E:
        m_CameraController.m_Up = 1.0f;
        break;
    case KeyCode::ShiftKey:
        m_CameraController.m_Shift = true;
        break;
    }
}

void MeshletsDemo::OnKeyReleased(KeyEventArgs& e)
{
    Base::OnKeyReleased(e);

    switch (e.Key)
    {
    case KeyCode::Enter:
        if (e.Alt)
        {
    case KeyCode::F11:
        allowFullscreenToggle = true;
        }
        break;
    case KeyCode::Up:
    case KeyCode::W:
        m_CameraController.m_Forward = 0.0f;
        break;
    case KeyCode::Left:
    case KeyCode::A:
        m_CameraController.m_Left = 0.0f;
        break;
    case KeyCode::Down:
    case KeyCode::S:
        m_CameraController.m_Backward = 0.0f;
        break;
    case KeyCode::Right:
    case KeyCode::D:
        m_CameraController.m_Right = 0.0f;
        break;
    case KeyCode::Q:
        m_CameraController.m_Down = 0.0f;
        break;
    case KeyCode::E:
        m_CameraController.m_Up = 0.0f;
        break;
    case KeyCode::ShiftKey:
        m_CameraController.m_Shift = false;
        break;
    }
}

void MeshletsDemo::OnMouseMoved(MouseMotionEventArgs& e)
{
    Base::OnMouseMoved(e);

    const float mouseSpeed = 0.1f;

    if (e.LeftButton)
    {
        m_CameraController.m_Pitch -= e.RelY * mouseSpeed;

        m_CameraController.m_Pitch = Clamp(m_CameraController.m_Pitch, -90.0f, 90.0f);

        m_CameraController.m_Yaw -= e.RelX * mouseSpeed;
    }
}


void MeshletsDemo::OnMouseWheel(MouseWheelEventArgs& e)
{
    {
        auto fov = m_Camera.GetFov();

        fov -= e.WheelDelta;
        fov = Clamp(fov, 12.0f, 90.0f);

        m_Camera.SetFov(fov);

        char buffer[256];
        sprintf_s(buffer, "FoV: %f\n", fov);
        OutputDebugStringA(buffer);
    }
}
