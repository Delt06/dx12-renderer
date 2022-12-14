#include "GrassDemo.h"

#include <DX12Library/Application.h>
#include <DX12Library/CommandQueue.h>
#include <DX12Library/CommandList.h>
#include <DX12Library/Helpers.h>
#include <DX12Library/Window.h>
#include <Framework/Bone.h>
#include <Framework/Animation.h>
#include <Framework/GameObject.h>
#include <Framework/Light.h>
#include <Framework/BoundingSphere.h>

#include <wrl.h>

#include <Framework/GraphicsSettings.h>
#include <Framework/Model.h>
#include <Framework/ModelLoader.h>

using namespace Microsoft::WRL;

#include <d3d12.h>
#include <d3dx12.h>
#include <DirectXColors.h>
#include "CBuffers.h"
#include "GrassIndirectCommand.h"
#include <random>

using namespace DirectX;

#include <algorithm> // For std::min and std::max.
#if defined(min)
#undef min
#endif

#include <DX12Library/ShaderUtils.h>

#if defined(max)
#undef max
#endif
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
    constexpr FLOAT CLEAR_COLOR[] = { 138.0f / 255.0f, 82.0f / 255.0f, 52.0f / 255.0f, 1.0f };
    constexpr FLOAT ZERO_CLEAR_COLOR[] = { 0.0f, 0.0f, 0.0f, 0.0f };

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

    UINT GetMsaaQualityLevels(ComPtr<ID3D12Device2> device, DXGI_FORMAT format, UINT sampleCount)
    {
        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msLevels;
        msLevels.Format = format;
        msLevels.SampleCount = sampleCount;
        msLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;

        ThrowIfFailed(device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msLevels, sizeof(msLevels)));
        return msLevels.NumQualityLevels;
    }

    BoundingSphere ComputeSphereBounds(const std::vector<GameObject> gameObjects)
    {
        Aabb sceneAabb{};

        for (auto& gameObject : gameObjects)
        {
            sceneAabb.Encapsulate(gameObject.GetAabb());
        }

        const XMVECTOR center = sceneAabb.GetCenter();
        float radius = 0.0f;

        XMVECTOR sceneAabbPoints[Aabb::POINTS_COUNT];
        sceneAabb.GetAllPoints(sceneAabbPoints);

        for (uint32_t i = 0; i < Aabb::POINTS_COUNT; ++i)
        {
            const XMVECTOR point = sceneAabbPoints[i];
            float distance = XMVectorGetX(XMVector3Length(point - center));
            radius = std::max(distance, radius);
        }

        return { radius, center };
    }

    constexpr uint32_t GRASS_SIDE_COUNT = 500;
    constexpr uint32_t GRASS_CHUNKS_SIDE_COUNT = 4;
    constexpr float GRASS_SPACING = 2.0f;
}

namespace Pipeline
{
    constexpr UINT SHADOW_MAP_REGISTER_INDEX = 0;
    constexpr UINT SSAO_MAP_REGISTER_INDEX = 1;
}

GrassDemo::GrassDemo(const std::wstring& name, int width, int height, GraphicsSettings graphicsSettings)
    : Base(name, width, height, graphicsSettings.VSync)
    , m_Viewport(CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)))
    , m_ScissorRect(CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX))
    , m_GraphicsSettings(graphicsSettings)
    , m_CameraController{}
    , m_Width(0)
    , m_Height(0)
{
    const XMVECTOR cameraPos = XMVectorSet(0, 40, -20, 1);
    const XMVECTOR cameraTarget = XMVectorSet(0, 0, 0, 1);
    const XMVECTOR cameraUp = XMVectorSet(0, 1, 0, 0);

    m_Camera.SetLookAt(cameraPos, cameraTarget, cameraUp);

    m_PAlignedCameraData = static_cast<CameraData*>(_aligned_malloc(sizeof(CameraData), 16));

    m_PAlignedCameraData->m_InitialPosition = m_Camera.GetTranslation();
    m_PAlignedCameraData->m_InitialQRotation = m_Camera.GetRotation();
}

GrassDemo::~GrassDemo()
{
    _aligned_free(m_PAlignedCameraData);
}

bool GrassDemo::LoadContent()
{
    const auto device = Application::Get().GetDevice();
    const auto commandQueue = Application::Get().GetCommandQueue();
    const auto commandList = commandQueue->GetCommandList();

    // sRGB formats provide free gamma correction!
    constexpr DXGI_FORMAT backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    constexpr DXGI_FORMAT depthBufferFormat = DXGI_FORMAT_D32_FLOAT;
    constexpr DXGI_FORMAT velocityBufferFormat = DXGI_FORMAT_R8G8B8A8_SNORM;
    constexpr DXGI_FORMAT resolvedDepthBufferFormat = DXGI_FORMAT_R32_FLOAT;
    D3D12_CLEAR_VALUE colorClearValue;
    colorClearValue.Format = backBufferFormat;
    memcpy(colorClearValue.Color, CLEAR_COLOR, sizeof CLEAR_COLOR);

    D3D12_CLEAR_VALUE velocityClearValue;
    velocityClearValue.Format = velocityBufferFormat;
    memcpy(velocityClearValue.Color, ZERO_CLEAR_COLOR, sizeof ZERO_CLEAR_COLOR);

    // Generate default texture
    {
        m_WhiteTexture2d = std::make_shared<Texture>();
        commandList->LoadTextureFromFile(*m_WhiteTexture2d, L"Assets/Textures/white.png");
    }

    m_RootSignature = std::make_shared<CommonRootSignature>(m_WhiteTexture2d);

    // Load models
    {
        ModelLoader modelLoader;

        m_GrassMesh = modelLoader.Load(*commandList, "Assets/Models/grass_cone.fbx")->GetMeshes()[0];
        m_GrassShader = std::make_shared<Shader>(
            m_RootSignature,
            ShaderBlob(L"Grass_VS.cso"),
            ShaderBlob(L"Grass_PS.cso")
            );

        m_WindNoise = modelLoader.LoadTexture(*commandList, L"Assets/Textures/wind_noise.png", TextureUsageType::Other);
    }

    {
        m_Taa = std::make_unique<TAA>(m_RootSignature, *commandList, backBufferFormat, m_Width, m_Height);
    }

    {
        Microsoft::WRL::ComPtr<ID3D12CommandSignature> commandSignature;
        {
            D3D12_INDIRECT_ARGUMENT_DESC argumentDescs[2] = {};
            argumentDescs[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
            argumentDescs[0].Constant.DestOffsetIn32BitValues = 0;
            argumentDescs[0].Constant.Num32BitValuesToSet = 1;
            argumentDescs[0].Constant.RootParameterIndex = CommonRootSignature::RootParameters::Constants;
            argumentDescs[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

            D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc = {};
            commandSignatureDesc.pArgumentDescs = argumentDescs;
            commandSignatureDesc.NumArgumentDescs = _countof(argumentDescs);
            commandSignatureDesc.ByteStride = sizeof(GrassIndirectCommand);

            ThrowIfFailed(device->CreateCommandSignature(&commandSignatureDesc, m_RootSignature->GetRootSignature().Get(), IID_PPV_ARGS(&commandSignature)));
            commandSignature->SetName(L"Grass Command Signature");
        }

        for (int32_t i = 0; i < GRASS_CHUNKS_SIDE_COUNT; ++i)
        {
            for (int32_t j = 0; j < GRASS_CHUNKS_SIDE_COUNT; ++j)
            {
                const float chunkSize = GRASS_SPACING * GRASS_SIDE_COUNT;
                const int32_t offset = GRASS_CHUNKS_SIDE_COUNT / 2;
                const XMFLOAT3 origin = { chunkSize * (i - offset), 0, chunkSize * (j - offset) };
                m_GrassChunks.emplace_back(m_RootSignature,
                    commandSignature,
                    m_GrassMesh,
                    *commandList,
                    GRASS_SIDE_COUNT,
                    GRASS_SPACING,
                    origin
                );
            }
        }
    }

    {
        m_CullGrassComputeShader = std::make_shared<ComputeShader>(m_RootSignature,
            ShaderBlob(L"CullGrass_CS.cso")
            );
    }

    {
        const UINT msaaSampleCount = 1;
        const UINT msaaColorQualityLevel = 0;
        const UINT msaaDepthQualityLevel = 0;

        auto colorDesc = CD3DX12_RESOURCE_DESC::Tex2D(backBufferFormat,
            m_Width, m_Height,
            1, 1,
            msaaSampleCount, msaaColorQualityLevel,
            D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
        );

        auto colorTexture = std::make_shared<Texture>(colorDesc, &colorClearValue,
            TextureUsageType::RenderTarget,
            L"Color Render Target");

        auto depthDesc = CD3DX12_RESOURCE_DESC::Tex2D(depthBufferFormat,
            m_Width, m_Height,
            1, 1,
            msaaSampleCount, msaaDepthQualityLevel,
            D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
        );
        D3D12_CLEAR_VALUE depthClearValue;
        depthClearValue.Format = depthDesc.Format;
        depthClearValue.DepthStencil = { 1.0f, 0 };

        auto depthTexture = std::make_shared<Texture>(depthDesc, &depthClearValue,
            TextureUsageType::Depth,
            L"Depth Render Target");

        auto velocityDesc = CD3DX12_RESOURCE_DESC::Tex2D(velocityBufferFormat,
            m_Width, m_Height,
            1, 1,
            msaaSampleCount, msaaColorQualityLevel,
            D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
        );

        auto velocityTexture = std::make_shared<Texture>(velocityDesc, &velocityClearValue,
            TextureUsageType::RenderTarget,
            L"Velocity Render Target");

        m_RenderTarget.AttachTexture(Color0, colorTexture);
        m_RenderTarget.AttachTexture(Color1, velocityTexture);
        m_RenderTarget.AttachTexture(DepthStencil, depthTexture);
    }

    auto fenceValue = commandQueue->ExecuteCommandList(commandList);
    commandQueue->WaitForFenceValue(fenceValue);

    return true;
}

void GrassDemo::OnResize(ResizeEventArgs& e)
{
    Base::OnResize(e);

    if (m_Width != e.Width || m_Height != e.Height)
    {
        m_Width = std::max(1, e.Width);
        m_Height = std::max(1, e.Height);

        const float aspectRatio = static_cast<float>(m_Width) / static_cast<float>(m_Height);
        m_Camera.SetProjection(45.0f, aspectRatio, 0.1f, 500.0f);

        m_Viewport = CD3DX12_VIEWPORT(0.0f, 0.0f,
            static_cast<float>(m_Width), static_cast<float>(m_Height));

        m_RenderTarget.Resize(m_Width, m_Height);

        if (m_Taa != nullptr)
            m_Taa->Resize(m_Width, m_Height);
    }
}

void GrassDemo::UnloadContent()
{}

void GrassDemo::OnUpdate(UpdateEventArgs& e)
{
    static uint64_t frameCount = 0;
    static double totalTime = 0.0;

    Base::OnUpdate(e);

    if (e.ElapsedTime > 1.0f) return;

    totalTime += e.ElapsedTime;
    m_Time += e.ElapsedTime;
    m_DeltaTime = e.ElapsedTime;
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
    float speedMultiplier = (m_CameraController.m_Shift ? 64.0f : 16.0f);

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
}

void GrassDemo::OnRender(RenderEventArgs& e)
{
    Base::OnRender(e);

    const auto commandQueue = Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
    const auto commandList = commandQueue->GetCommandList();

    // Clear the render targets
    {
        commandList->ClearTexture(*m_RenderTarget.GetTexture(Color0), CLEAR_COLOR);
        commandList->ClearTexture(*m_RenderTarget.GetTexture(Color1), ZERO_CLEAR_COLOR);
        commandList->ClearDepthStencilTexture(*m_RenderTarget.GetTexture(DepthStencil), D3D12_CLEAR_FLAG_DEPTH);
    }

    const XMMATRIX viewMatrix = m_Camera.GetViewMatrix();
    const XMMATRIX projectionMatrix = m_Camera.GetProjectionMatrix();
    const XMMATRIX viewProjectionMatrix = viewMatrix * projectionMatrix;

    m_RootSignature->Bind(*commandList);

    Demo::PipelineCBuffer pipelineCBuffer{};
    {
        pipelineCBuffer.m_View = viewMatrix;
        pipelineCBuffer.m_Projection = projectionMatrix;
        pipelineCBuffer.m_ViewProjection = viewProjectionMatrix;

        pipelineCBuffer.m_Time = static_cast<float>(m_Time);
        pipelineCBuffer.m_DeltaTime = static_cast<float>(m_DeltaTime);

        pipelineCBuffer.m_TAA_PreviousViewProjection = m_Taa->GetPreviousViewProjectionMatrix();
        pipelineCBuffer.m_TAA_JitterOffset = m_Taa->ComputeJitterOffset();
    }
    m_RootSignature->SetPipelineConstantBuffer(*commandList, pipelineCBuffer);

    commandList->SetRenderTarget(m_RenderTarget);
    commandList->SetViewport(m_Viewport);
    commandList->SetScissorRect(m_ScissorRect);

    {
        const auto frustum = m_Camera.GetFrustum();
        m_VisibleGrassChunks.clear();

        for (size_t i = 0; i < m_GrassChunks.size(); ++i)
        {
            auto& chunk = m_GrassChunks[i];
            chunk.SetFrustum(frustum);
            if (chunk.IsVisible())
            {
                m_VisibleGrassChunks.push_back(i);
            }
        }
    }

    if (m_CullingEnabled)
    {
        PIXScope(*commandList, "Cull Grass Batch");

        m_CullGrassComputeShader->Bind(*commandList);

        for (const auto index : m_VisibleGrassChunks)
        {
            m_GrassChunks[index].DispatchCulling(*commandList);
        }
    }

    {
        PIXScope(*commandList, "Draw Grass Batch");

        m_GrassShader->Bind(*commandList);
        m_GrassMesh->Bind(*commandList);
        m_RootSignature->SetPipelineShaderResourceView(*commandList, 2, ShaderResourceView(m_WindNoise));

        const bool ignoreCulling = !m_CullingEnabled;

        for (const auto index : m_VisibleGrassChunks)
        {
            m_GrassChunks[index].Draw(*commandList, ignoreCulling);
        }

        m_GrassShader->Unbind(*commandList);
    }

    if (m_TaaEnabled)
    {
        const auto& colorBuffer = m_RenderTarget.GetTexture(Color0);
        const auto& velocityBuffer = m_RenderTarget.GetTexture(Color1);
        const float modulationFactor = 0.8f;
        m_Taa->Resolve(*commandList, colorBuffer, velocityBuffer, modulationFactor);

        m_Taa->OnRenderedFrame(viewProjectionMatrix);
    }

    commandQueue->ExecuteCommandList(commandList);
    PWindow->Present(m_TaaEnabled ? *m_Taa->GetResolvedTexture() : *m_RenderTarget.GetTexture(Color0));
}

void GrassDemo::OnKeyPressed(KeyEventArgs& e)
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
    case KeyCode::T:
        m_TaaEnabled = !m_TaaEnabled;
        break;
    case KeyCode::C:
        m_CullingEnabled = !m_CullingEnabled;
        break;
    case KeyCode::ShiftKey:
        m_CameraController.m_Shift = true;
        break;
    }
}

void GrassDemo::OnKeyReleased(KeyEventArgs& e)
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

void GrassDemo::OnMouseMoved(MouseMotionEventArgs& e)
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


void GrassDemo::OnMouseWheel(MouseWheelEventArgs& e)
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
