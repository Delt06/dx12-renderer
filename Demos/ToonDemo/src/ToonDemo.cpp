#include "ToonDemo.h"

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

#include <Framework/GraphicsSettings.h>
#include <Framework/Model.h>
#include <Framework/ModelLoader.h>

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

#include "CBuffers.h"

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
    constexpr FLOAT NORMALS_CLEAR_COLOR[] = { 0.5f, 0.5f, 0.5f, 1.0f };

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
}

namespace Pipeline
{
    constexpr UINT SHADOW_MAP_REGISTER_INDEX = 0;
    constexpr UINT SSAO_MAP_REGISTER_INDEX = 1;
}

ToonDemo::ToonDemo(const std::wstring& name, int width, int height, GraphicsSettings graphicsSettings)
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

ToonDemo::~ToonDemo()
{
    _aligned_free(m_PAlignedCameraData);
}

bool ToonDemo::LoadContent()
{
    const auto device = Application::Get().GetDevice();
    const auto commandQueue = Application::Get().GetCommandQueue();
    const auto commandList = commandQueue->GetCommandList();

    // sRGB formats provide free gamma correction!
    constexpr DXGI_FORMAT backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    constexpr DXGI_FORMAT depthBufferFormat = DXGI_FORMAT_D32_FLOAT;
    constexpr DXGI_FORMAT normalsBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    constexpr DXGI_FORMAT resolvedDepthBufferFormat = DXGI_FORMAT_R32_FLOAT;
    D3D12_CLEAR_VALUE colorClearValue;
    colorClearValue.Format = backBufferFormat;
    memcpy(colorClearValue.Color, CLEAR_COLOR, sizeof CLEAR_COLOR);

    D3D12_CLEAR_VALUE normalsClearValue;
    normalsClearValue.Format = normalsBufferFormat;
    memcpy(normalsClearValue.Color, NORMALS_CLEAR_COLOR, sizeof NORMALS_CLEAR_COLOR);

    // Generate default texture
    {
        m_WhiteTexture2d = std::make_shared<Texture>();
        commandList->LoadTextureFromFile(*m_WhiteTexture2d, L"Assets/Textures/white.png");
    }

    m_RootSignature = std::make_shared<CommonRootSignature>(m_WhiteTexture2d);

    // depth pre-pass
    {
        auto shader = std::make_shared<Shader>(m_RootSignature,
            ShaderBlob(L"DepthNormals_VS.cso"),
            ShaderBlob(L"DepthNormals_PS.cso")
            );
        m_DepthNormalsMaterial = Material::Create(shader);
    }

    m_ShadowsPass = std::make_unique<DirectionalLightShadowsPass>(m_RootSignature, *commandList, 1024);
    m_OutlinePass = std::make_unique<OutlinePass>(m_RootSignature, *commandList);
    m_FXAAPass = std::make_unique<FXAAPass>(m_RootSignature, *commandList);
    m_BloomPass = std::make_unique<Bloom>(m_RootSignature, *commandList, m_Width, m_Height, backBufferFormat, 4);
    m_SSAOPass = std::make_unique<SSAOPass>(m_RootSignature, *commandList, m_Width, m_Height);

    m_DirectionalLight.m_Color = XMFLOAT4(1, 1, 1, 1);
    m_DirectionalLight.m_DirectionWs = XMFLOAT4(1, 1, 0, 0);

    // Load models
    {
        ModelLoader modelLoader;

        auto toonShader = std::make_shared<Shader>(m_RootSignature,
            ShaderBlob(L"Toon_VS.cso"),
            ShaderBlob(L"Toon_PS.cso"),
            [](PipelineStateBuilder& builder)
            {
                auto dsDesc = CD3DX12_DEPTH_STENCIL_DESC(CD3DX12_DEFAULT());
                dsDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
                dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
                builder.WithDepthStencil(dsDesc);
            }
        );
        auto toonMaterialPreset = Material(toonShader);
        toonMaterialPreset.SetVariable("mainColor", XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
        toonMaterialPreset.SetVariable("shadowColorOpacity", XMFLOAT4(0.0f, 0.0f, 0.0f, 0.75f));

        toonMaterialPreset.SetVariable("specularColor", XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f));
        toonMaterialPreset.SetVariable("specularRampThreshold", 0.5f);
        toonMaterialPreset.SetVariable("specularRampSmoothness", 0.05f);
        toonMaterialPreset.SetVariable("specularExponent", 15.0f);

        toonMaterialPreset.SetVariable("fresnelRampThreshold", 0.45f);
        toonMaterialPreset.SetVariable("fresnelRampSmoothness", 0.05f);

        toonMaterialPreset.SetVariable("crossHatchTilingOffset", XMFLOAT4{ 1.0f, 1.0f, 0, 0 });
        toonMaterialPreset.SetVariable("crossHatchThreshold", 0.5f);
        toonMaterialPreset.SetVariable("crossHatchSmoothness", 0.05f);

        toonMaterialPreset.SetVariable("ambientLightingColor", XMFLOAT4{ 0.25f, 0.25f, 0.25f, 1.0f });
        toonMaterialPreset.SetVariable("ambientLightingThreshold", 0.25f);
        toonMaterialPreset.SetVariable("ambientLightingSmoothness", 0.5f);

        const auto toonRamp = modelLoader.LoadTexture(*commandList, L"Assets/Textures/toon_ramp.png", TextureUsageType::Other);
        toonMaterialPreset.SetShaderResourceView("toonRamp", ShaderResourceView(toonRamp));

        const auto crossHatchPatternTexture = modelLoader.LoadTexture(*commandList, L"Assets/Textures/crosshatching.jpg", TextureUsageType::Other);
        toonMaterialPreset.SetShaderResourceView("crossHatchPatternTexture", ShaderResourceView(crossHatchPatternTexture));

        const auto MaterialSetTexture = [&modelLoader, &commandList](Material& material, const std::string& propertyName, const std::wstring& texturePath, TextureUsageType usage = TextureUsageType::Albedo)
        {
            material.SetShaderResourceView(propertyName, ShaderResourceView(modelLoader.LoadTexture(*commandList, texturePath, usage)));
        };

        {
            auto model = std::make_shared<Model>(Mesh::CreateSphere(*commandList));
            auto material = Material::Create(toonMaterialPreset);

            material->SetVariable("mainColor", XMFLOAT4(0.75f, 0.65f, 0.1f, 1.0f));
            material->SetVariable("shadowColorOpacity", XMFLOAT4(0.5f, 0.3f, 0.1f, 1.0f));

            material->SetVariable("specularColor", XMFLOAT4(0.5f, 0.0f, 0.0f, 1.0f));
            material->SetVariable("specularExponent", 50.0f);

            XMMATRIX translationMatrix = DirectX::XMMatrixTranslation(-2.5f, 2.0f, 0.0f);
            XMMATRIX rotationMatrix = DirectX::XMMatrixIdentity();
            XMMATRIX scaleMatrix = DirectX::XMMatrixScaling(2.0f, 2.0f, 2.0f);
            XMMATRIX worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;
            m_GameObjects.push_back(GameObject(worldMatrix, model, material));
        }

        {
            auto model = std::make_shared<Model>(Mesh::CreatePlane(*commandList));
            auto material = Material::Create(toonMaterialPreset);

            material->SetVariable("mainColor", XMFLOAT4(68.0f / 255.0f, 58.0f / 255.0f, 66.0f / 255.0f, 1.0f));
            material->SetVariable("specularColor", XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));

            XMMATRIX translationMatrix = DirectX::XMMatrixTranslation(0.0f, 0.0f, 0.0f);
            XMMATRIX rotationMatrix = DirectX::XMMatrixIdentity();
            XMMATRIX scaleMatrix = DirectX::XMMatrixScaling(25.0f, 1.0f, 25.0f);
            XMMATRIX worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;
            m_GameObjects.push_back(GameObject(worldMatrix, model, material));
        }

        {
            auto model = modelLoader.Load(*commandList, "Assets/Models/Quaternius/George_smooth.fbx");
            auto material = Material::Create(toonMaterialPreset);

            MaterialSetTexture(*material, "mainTexture", L"Assets/Models/Quaternius/George_Texture.png");

            material->SetVariable("specularColor", XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
            material->SetVariable("specularExponent", 35.0f);

            XMMATRIX translationMatrix = DirectX::XMMatrixTranslation(0.0f, 0.0f, 0.0f);
            XMMATRIX rotationMatrix = DirectX::XMMatrixRotationRollPitchYaw(
                XMConvertToRadians(0.0f),
                XMConvertToRadians(0.0f),
                XMConvertToRadians(0.0f)
            );
            XMMATRIX scaleMatrix = DirectX::XMMatrixScaling(0.01f, 0.01f, 0.01f);
            XMMATRIX worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;
            m_GameObjects.push_back(GameObject(worldMatrix, model, material));
        }

        {
            auto model = modelLoader.Load(*commandList, "Assets/Models/Quaternius/Leela_smooth.fbx");
            auto material = Material::Create(toonMaterialPreset);

            MaterialSetTexture(*material, "mainTexture", L"Assets/Models/Quaternius/Leela_4_Texture.png");

            material->SetVariable("specularColor", XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
            material->SetVariable("specularExponent", 35.0f);

            XMMATRIX translationMatrix = DirectX::XMMatrixTranslation(4.5f, 0.0f, 0.0f);
            XMMATRIX rotationMatrix = DirectX::XMMatrixRotationRollPitchYaw(
                XMConvertToRadians(0.0f),
                XMConvertToRadians(0.0f),
                XMConvertToRadians(0.0f)
            );
            XMMATRIX scaleMatrix = DirectX::XMMatrixScaling(0.01f, 0.01f, 0.01f);
            XMMATRIX worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;
            m_GameObjects.push_back(GameObject(worldMatrix, model, material));
        }
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

        m_RenderTarget.AttachTexture(Color0, colorTexture);
        m_RenderTarget.AttachTexture(DepthStencil, depthTexture);

        auto normalsDesc = CD3DX12_RESOURCE_DESC::Tex2D(normalsBufferFormat,
            m_Width, m_Height,
            1, 1,
            msaaSampleCount, msaaColorQualityLevel,
            D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
        );

        auto normalsTexture = std::make_shared<Texture>(normalsDesc, &normalsClearValue,
            TextureUsageType::RenderTarget,
            L"Normals Render Target"
            );
        m_DepthNormalsRenderTarget.AttachTexture(Color0, normalsTexture);
        m_DepthNormalsRenderTarget.AttachTexture(DepthStencil, depthTexture);
    }

    {
        auto resolvedDepthDesc = CD3DX12_RESOURCE_DESC::Tex2D(resolvedDepthBufferFormat,
            m_Width, m_Height,
            1, 1,
            1, 0,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
        );
        m_DepthTexture = std::make_shared<Texture>(resolvedDepthDesc, nullptr, TextureUsageType::Other, L"Depth Texture");
    }

    {
        const auto CreatePostFxTexture = [this](const std::wstring& name)
        {
            auto postFxColorDesc = CD3DX12_RESOURCE_DESC::Tex2D(backBufferFormat,
                m_Width, m_Height,
                1, 1,
                1, 0,
                D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
            );
            auto postFxColorTexture = std::make_shared<Texture>(postFxColorDesc, nullptr, TextureUsageType::RenderTarget, name);
            return postFxColorTexture;
        };

        m_PostFxRenderTarget.AttachTexture(Color0, CreatePostFxTexture(L"PostFX Color"));
        m_PostFxRenderTarget2.AttachTexture(Color0, CreatePostFxTexture(L"PostFX Color 2"));
    }

    auto fenceValue = commandQueue->ExecuteCommandList(commandList);
    commandQueue->WaitForFenceValue(fenceValue);

    return true;
}

void ToonDemo::OnResize(ResizeEventArgs& e)
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

        m_RenderTarget.Resize(m_Width, m_Height);
        m_DepthNormalsRenderTarget.Resize(m_Width, m_Height);

        if (m_DepthTexture != nullptr)
            m_DepthTexture->Resize(m_Width, m_Height);

        m_PostFxRenderTarget.Resize(m_Width, m_Height);
        m_PostFxRenderTarget2.Resize(m_Width, m_Height);

        if (m_BloomPass != nullptr)
            m_BloomPass->Resize(m_Width, m_Height);

        if (m_SSAOPass != nullptr)
            m_SSAOPass->Resize(m_Width, m_Height);
    }
}

void ToonDemo::UnloadContent()
{}

void ToonDemo::OnUpdate(UpdateEventArgs& e)
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

    if (m_AnimateLights)
    {
        XMVECTOR dirLightDirectionWs = XMLoadFloat4(&m_DirectionalLight.m_DirectionWs);
        dirLightDirectionWs = DirectX::XMVector4Transform(dirLightDirectionWs,
            DirectX::XMMatrixRotationAxis(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),
                DirectX::XMConvertToRadians(90.0f * dt)));
        DirectX::XMStoreFloat4(&m_DirectionalLight.m_DirectionWs, dirLightDirectionWs);
    }
}

void ToonDemo::OnRender(RenderEventArgs& e)
{
    Base::OnRender(e);

    const auto commandQueue = Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
    const auto commandList = commandQueue->GetCommandList();

    // Clear the render targets
    {
        commandList->ClearTexture(*m_RenderTarget.GetTexture(Color0), CLEAR_COLOR);
        commandList->ClearTexture(*m_DepthNormalsRenderTarget.GetTexture(Color0), NORMALS_CLEAR_COLOR);
        commandList->ClearDepthStencilTexture(*m_RenderTarget.GetTexture(DepthStencil), D3D12_CLEAR_FLAG_DEPTH);
    }

    const XMMATRIX viewMatrix = m_Camera.GetViewMatrix();
    const XMMATRIX projectionMatrix = m_Camera.GetProjectionMatrix();
    const XMMATRIX viewProjectionMatrix = viewMatrix * projectionMatrix;

    m_RootSignature->Bind(*commandList);

    Demo::Pipeline::CBuffer pipelineCBuffer{};
    {
        pipelineCBuffer.m_View = viewMatrix;
        pipelineCBuffer.m_Projection = projectionMatrix;
        pipelineCBuffer.m_ViewProjection = viewProjectionMatrix;

        DirectX::XMStoreFloat4(&pipelineCBuffer.m_CameraPosition, m_Camera.GetTranslation());

        pipelineCBuffer.m_InverseView = DirectX::XMMatrixInverse(nullptr, viewMatrix);
        pipelineCBuffer.m_InverseProjection = DirectX::XMMatrixInverse(nullptr, projectionMatrix);

        pipelineCBuffer.m_ScreenResolution = { static_cast<float>(m_Width), static_cast<float>(m_Height) };
        pipelineCBuffer.m_ScreenTexelSize = { 1.0f / pipelineCBuffer.m_ScreenResolution.x, 1.0f / pipelineCBuffer.m_ScreenResolution.y };

        {
            const auto sceneBounds = ComputeSphereBounds(m_GameObjects);
            const auto shadowMatrices = m_ShadowsPass->ComputeMatrices(sceneBounds, m_DirectionalLight);
            pipelineCBuffer.m_DirectionalLightView = shadowMatrices.m_ViewMatrix;
            pipelineCBuffer.m_DirectionalLightViewProjection = shadowMatrices.m_ViewProjectionMatrix;
        }

        pipelineCBuffer.m_DirectionalLightShadowsBias = { 0.0005f, 0.f, 0.1f, 0 };

        pipelineCBuffer.m_DirectionalLight = m_DirectionalLight;
    }

    m_RootSignature->SetPipelineConstantBuffer(*commandList, pipelineCBuffer);

    {
        PIXScope(*commandList, "Shadows Pass");

        m_ShadowsPass->BeginShadowCasterPass(*commandList);

        for (const auto& go : m_GameObjects)
        {
            Demo::Model::CBuffer modelCBuffer{};
            modelCBuffer.Compute(go.GetWorldMatrix(), viewProjectionMatrix);
            m_RootSignature->SetModelConstantBuffer(*commandList, modelCBuffer);

            go.GetModel()->Draw(*commandList);
        }

        m_ShadowsPass->EndShadowCasterPass(*commandList);

        m_ShadowsPass->Blur(*commandList);
    }

    commandList->SetViewport(m_Viewport);
    commandList->SetScissorRect(m_ScissorRect);

    {
        PIXScope(*commandList, "Depth Normals Pass");

        commandList->SetRenderTarget(m_DepthNormalsRenderTarget);

        m_DepthNormalsMaterial->BeginBatch(*commandList);

        for (const auto& go : m_GameObjects)
        {
            Demo::Model::CBuffer modelCBuffer{};
            modelCBuffer.Compute(go.GetWorldMatrix(), viewProjectionMatrix);
            m_RootSignature->SetModelConstantBuffer(*commandList, modelCBuffer);

            go.GetModel()->Draw(*commandList);
        }

        m_DepthNormalsMaterial->EndBatch(*commandList);
    }

    {
        PIXScope(*commandList, "Copy Depth and Normals");
        commandList->CopyResource(*m_DepthTexture, *m_DepthNormalsRenderTarget.GetTexture(DepthStencil));
    }

    if (m_EnableSSAO)
    {
        m_SSAOPass->Execute(*commandList, m_DepthTexture, m_DepthNormalsRenderTarget.GetTexture(Color0));
    }

    // only read from the depth buffer, disable write
    commandList->SetRenderTarget(m_RenderTarget, -1, 0, true, true);

    // bind shadow map
    m_RootSignature->SetPipelineShaderResourceView(
        *commandList,
        Pipeline::SHADOW_MAP_REGISTER_INDEX,
        m_ShadowsPass->GetVarianceMapShaderResourceView()
    );

    // bind SSAO texture
    m_RootSignature->SetPipelineShaderResourceView(
        *commandList,
        Pipeline::SSAO_MAP_REGISTER_INDEX,
        ShaderResourceView(m_EnableSSAO ? m_SSAOPass->GetFinalTexture() : m_WhiteTexture2d)
    );

    commandList->SetViewport(m_Viewport);
    commandList->SetScissorRect(m_ScissorRect);

    {
        PIXScope(*commandList, "Main Pass");

        for (const auto& go : m_GameObjects)
        {
            Demo::Model::CBuffer modelCBuffer{};
            modelCBuffer.Compute(go.GetWorldMatrix(), viewProjectionMatrix);
            m_RootSignature->SetModelConstantBuffer(*commandList, modelCBuffer);

            go.Draw(*commandList);
        }
    }

    {
        commandList->SetRenderTarget(m_PostFxRenderTarget);
        commandList->SetAutomaticViewportAndScissorRect(m_PostFxRenderTarget);

        m_OutlinePass->Render(*commandList, m_RenderTarget.GetTexture(Color0), m_DepthTexture, m_DepthNormalsRenderTarget.GetTexture(Color0));
    }

    {
        commandList->SetRenderTarget(m_PostFxRenderTarget2);
        commandList->SetAutomaticViewportAndScissorRect(m_PostFxRenderTarget2);

        m_FXAAPass->Render(*commandList, m_PostFxRenderTarget.GetTexture(Color0));
    }

    {
        BloomParameters parameters{};
        parameters.Threshold = 1.25f;
        parameters.SoftThreshold = 0.3f;
        parameters.Intensity = 50.0f;

        m_BloomPass->Draw(
            *commandList,
            m_PostFxRenderTarget2.GetTexture(Color0),
            m_PostFxRenderTarget2,
            parameters
        );
    }

    commandQueue->ExecuteCommandList(commandList);
    PWindow->Present(*m_PostFxRenderTarget2.GetTexture(Color0));
}

void ToonDemo::OnKeyPressed(KeyEventArgs& e)
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
    case KeyCode::L:
        m_AnimateLights = !m_AnimateLights;
        break;
    case KeyCode::O:
        m_EnableSSAO = !m_EnableSSAO;
        break;
    case KeyCode::ShiftKey:
        m_CameraController.m_Shift = true;
        break;
    }
}

void ToonDemo::OnKeyReleased(KeyEventArgs& e)
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

void ToonDemo::OnMouseMoved(MouseMotionEventArgs& e)
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


void ToonDemo::OnMouseWheel(MouseWheelEventArgs& e)
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
