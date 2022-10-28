#include <LightingDemo.h>

#include <DX12Library/Application.h>
#include <DX12Library/CommandQueue.h>
#include <DX12Library/CommandList.h>
#include <DX12Library/Helpers.h>
#include <Framework/Light.h>
#include <DX12Library/Window.h>
#include <Framework/GameObject.h>

#include <wrl.h>

#include <Framework/GraphicsSettings.h>
#include <Framework/Model.h>
#include <Framework/ModelLoader.h>
#include <ParticleSystem.h>
#include <PointLightShadowPassPso.h>
using namespace Microsoft::WRL;

#include <d3d12.h>
#include <d3dx12.h>
#include <DirectXColors.h>
#include <Framework/MatricesCb.h>

using namespace DirectX;

#include <algorithm> // For std::min and std::max.
#if defined(min)
#undef min
#endif
#include <SceneRenderer.h>
#include <CBuffers.h>

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

    bool allowFullscreenToggle = true;
    constexpr FLOAT CLEAR_COLOR[] = { 0.4f, 0.6f, 0.9f, 1.0f };

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

LightingDemo::LightingDemo(const std::wstring& name, int width, int height, GraphicsSettings graphicsSettings)
    : Base(name, width, height, graphicsSettings.VSync)
    , m_Viewport(CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)))
    , m_ScissorRect(CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX))
    , m_GraphicsSettings(graphicsSettings)
    , m_CameraController{}
    , m_Width(0)
    , m_Height(0)
    , m_Scene(std::make_shared<Scene>())
{
    const XMVECTOR cameraPos = XMVectorSet(0, 5, -20, 1);
    const XMVECTOR cameraTarget = XMVectorSet(0, 5, 0, 1);
    const XMVECTOR cameraUp = XMVectorSet(0, 1, 0, 0);

    m_Scene->MainCamera.SetLookAt(cameraPos, cameraTarget, cameraUp);

    m_PAlignedCameraData = static_cast<CameraData*>(_aligned_malloc(sizeof(CameraData), 16));

    m_PAlignedCameraData->m_InitialPosition = m_Scene->MainCamera.GetTranslation();
    m_PAlignedCameraData->m_InitialQRotation = m_Scene->MainCamera.GetRotation();
}

LightingDemo::~LightingDemo()
{
    _aligned_free(m_PAlignedCameraData);
}

bool LightingDemo::LoadContent()
{
    const auto device = Application::Get().GetDevice();
    const auto commandQueue = Application::Get().GetCommandQueue();
    const auto commandList = commandQueue->GetCommandList();

    // sRGB formats provide free gamma correction!
    constexpr DXGI_FORMAT backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    constexpr DXGI_FORMAT depthBufferFormat = DXGI_FORMAT_D32_FLOAT;
    D3D12_CLEAR_VALUE colorClearValue;
    colorClearValue.Format = backBufferFormat;
    memcpy(colorClearValue.Color, CLEAR_COLOR, sizeof CLEAR_COLOR);

    // Generate default texture
    {
        m_WhiteTexture2d = std::make_shared<Texture>();
        commandList->LoadTextureFromFile(*m_WhiteTexture2d, L"Assets/Textures/white.png");
    }

    m_RootSignature = std::make_shared<CommonRootSignature>(m_WhiteTexture2d);

    m_SceneRenderer = std::make_shared<SceneRenderer>(m_RootSignature, *commandList, m_GraphicsSettings, backBufferFormat, depthBufferFormat);
    m_SceneRenderer->SetScreenResolution(m_Width, m_Height);
    m_SceneRenderer->SetScene(m_Scene);

    m_ReflectionCubemap = std::make_shared<Cubemap>(m_GraphicsSettings.DynamicReflectionsResolution, XMVectorSet(0, 5, 25, 1), *commandList, backBufferFormat, depthBufferFormat, colorClearValue);
    m_SceneRenderer->SetEnvironmentReflectionsCubemap(m_ReflectionCubemap);

    const auto phongShader = std::make_shared<Shader>(m_RootSignature,
        ShaderBlob(L"LightingDemo_VS.cso"),
        ShaderBlob(L"LightingDemo_PS.cso")
        );



    // Load models
    {
        ModelLoader modelLoader;

        const auto MaterialSetTexture = [&modelLoader, &commandList](Material& material, const std::string& propertyName, const std::wstring& texturePath, TextureUsageType usage = TextureUsageType::Albedo)
        {
            material.SetShaderResourceView(propertyName, ShaderResourceView(modelLoader.LoadTexture(*commandList, texturePath, usage)));
        };

        const std::string property_diffuseMap = "diffuseMap";
        const std::string property_normalMap = "normalMap";
        const std::string property_specularMap = "specularMap";
        const std::string property_glossMap = "glossMap";

        const std::string property_Emissive = "Emissive";
        const std::string property_Ambient = "Ambient";
        const std::string property_Diffuse = "Diffuse";
        const std::string property_Specular = "Specular";

        const std::string property_SpecularPower = "SpecularPower";
        const std::string property_Reflectivity = "Reflectivity";

        const std::string property_TilingOffset = "TilingOffset";

        auto phongMaterialPreset = Material(phongShader);

        phongMaterialPreset.SetVariable<XMFLOAT4>(property_Emissive, { 0, 0, 0, 1 });
        phongMaterialPreset.SetVariable<XMFLOAT4>(property_Emissive, { 0.1f, 0.1f, 0.1f, 1 });
        phongMaterialPreset.SetVariable<XMFLOAT4>(property_Diffuse, { 1.0f, 1.0f, 1.0f, 1 });
        phongMaterialPreset.SetVariable<XMFLOAT4>(property_Specular, { 1.0f, 1.0f, 1.0f, 1 });

        phongMaterialPreset.SetVariable<float>(property_SpecularPower, 128.0f);
        phongMaterialPreset.SetVariable<float>(property_Reflectivity, 0.0f);

        phongMaterialPreset.SetVariable<XMFLOAT4>(property_TilingOffset, { 1, 1, 0, 0 });

        {
            auto model = modelLoader.Load(*commandList, "Assets/Models/teapot/teapot.obj", true);
            auto material = Material::Create(phongMaterialPreset);

            MaterialSetTexture(*material, property_diffuseMap, L"Assets/Textures/PavingStones/PavingStones_1K_Color.jpg");
            MaterialSetTexture(*material, property_normalMap, L"Assets/Textures/PavingStones/PavingStones_1K_Normal.jpg", TextureUsageType::Normalmap);
            MaterialSetTexture(*material, property_glossMap, L"Assets/Textures/PavingStones/PavingStones_1K_Roughness.jpg", TextureUsageType::Other);

            material->SetVariable(property_SpecularPower, 50.0f);

            XMMATRIX translationMatrix = XMMatrixTranslation(0.0f, 0.0f, 0.0f);
            XMMATRIX rotationMatrix = XMMatrixIdentity();
            XMMATRIX scaleMatrix = XMMatrixScaling(0.1f, 0.1f, 0.1f);
            XMMATRIX worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;
            m_Scene->GameObjects.push_back(GameObject(worldMatrix, model, material));
        }

        {
            auto model = modelLoader.Load(*commandList, "Assets/Models/sphere/sphere-cylcoords-1k.obj");
            auto material = Material::Create(phongMaterialPreset);

            MaterialSetTexture(*material, property_diffuseMap, L"Assets/Textures/Metal/Metal_1K_Color.jpg");
            MaterialSetTexture(*material, property_normalMap, L"Assets/Textures/Metal/Metal_1K_Normal.jpg", TextureUsageType::Normalmap);
            MaterialSetTexture(*material, property_specularMap, L"Assets/Textures/Metal/Metal_1K_Specular.jpg", TextureUsageType::Other);
            MaterialSetTexture(*material, property_glossMap, L"Assets/Textures/Metal/Metal_1K_Roughness.jpg", TextureUsageType::Other);

            material->SetVariable(property_SpecularPower, 100.0f);
            material->SetVariable(property_Reflectivity, 1.0f);

            XMMATRIX translationMatrix = XMMatrixTranslation(0.0f, 5.0f, 25.0f);
            XMMATRIX rotationMatrix = XMMatrixIdentity();
            XMMATRIX scaleMatrix = XMMatrixScaling(0.1f, 0.1f, 0.1f);
            XMMATRIX worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;
            m_Scene->GameObjects.push_back(GameObject(worldMatrix, model, material));
        }

        {
            auto model = modelLoader.LoadExisting(Mesh::CreatePlane(*commandList));
            auto material = Material::Create(phongMaterialPreset);

            MaterialSetTexture(*material, property_diffuseMap, L"Assets/Textures/Moss/Moss_1K_Color.jpg");
            MaterialSetTexture(*material, property_normalMap, L"Assets/Textures/Moss/Moss_1K_Normal.jpg", TextureUsageType::Normalmap);
            MaterialSetTexture(*material, property_glossMap, L"Assets/Textures/Moss/Moss_1K_Roughness.jpg", TextureUsageType::Other);

            material->SetVariable(property_SpecularPower, 10.0f);
            material->SetVariable(property_TilingOffset, XMFLOAT4{ 10, 10, 0, 0 });

            XMMATRIX translationMatrix = XMMatrixTranslation(0.0f, 0.0f, 0.0f);
            XMMATRIX rotationMatrix = XMMatrixIdentity();
            XMMATRIX scaleMatrix = XMMatrixScaling(200.0f, 200.0f, 200.0f);
            XMMATRIX worldMatrix = scaleMatrix * translationMatrix * rotationMatrix;
            m_Scene->GameObjects.push_back(GameObject(worldMatrix, model, material));
        }

        {
            auto model = modelLoader.LoadExisting(Mesh::CreateCube(*commandList, 1.0f));
            auto material = Material::Create(phongMaterialPreset);

            MaterialSetTexture(*material, property_diffuseMap, L"Assets/Textures/PavingStones/PavingStones_1K_Color.jpg");
            MaterialSetTexture(*material, property_normalMap, L"Assets/Textures/PavingStones/PavingStones_1K_Normal.jpg", TextureUsageType::Normalmap);
            MaterialSetTexture(*material, property_glossMap, L"Assets/Textures/PavingStones/PavingStones_1K_Roughness.jpg", TextureUsageType::Other);

            XMMATRIX translationMatrix = XMMatrixTranslation(7.0f, 2.5f, 11.0f);
            XMMATRIX rotationMatrix = XMMatrixIdentity();
            XMMATRIX scaleMatrix = XMMatrixScaling(1.0f, 5.0f, 1.0f);
            XMMATRIX worldMatrix = scaleMatrix * translationMatrix * rotationMatrix;
            m_Scene->GameObjects.push_back(GameObject(worldMatrix, model, material));
        }
    }

    // Create lights
    {
        m_Scene->MainDirectionalLight.m_DirectionWs = XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f);
        m_Scene->MainDirectionalLight.m_Color = XMFLOAT4(0.9f, 0.9f, 0.7f, 0.0f);
        m_Scene->MainDirectionalLight.m_Color = XMFLOAT4(0.5f, 0.5f, 0.4f, 0.0f);

        // magenta
        {
            PointLight pointLight(XMFLOAT4(-8, 2, -2, 1));
            pointLight.Color = XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f);
            m_Scene->PointLights.push_back(pointLight);
        }

        // yellow-ish
        {
            PointLight pointLight(XMFLOAT4(0, 2, -6, 1));
            pointLight.Color = XMFLOAT4(3.0f, 2.0f, 0.25f, 1.0f);
            m_Scene->PointLights.push_back(pointLight);
        }

        // cyan-ish
        {
            PointLight pointLight(XMFLOAT4(6, 2, 10, 1), 25.0f);
            pointLight.Color = XMFLOAT4(0.0f, 5.0f, 2.0f, 1.0f);
            m_Scene->PointLights.push_back(pointLight);
        }

        // spotlight
        {
            SpotLight spotLight;
            spotLight.PositionWs = XMFLOAT4(0, 5.0f, 55.0f, 1.0f);
            spotLight.DirectionWs = XMFLOAT4(0, 0.0f, -1.0f, 0.0f);
            spotLight.Color = XMFLOAT4(1, 1, 1, 1);
            spotLight.Intensity = 5.0f;
            spotLight.SpotAngle = XMConvertToRadians(45.0f);
            spotLight.Attenuation = 0.0005f;
            m_Scene->SpotLights.push_back(spotLight);
        }
    }

    // Setup particles
    {
        m_Scene->ParticleSystems.emplace_back(m_RootSignature, *commandList, XMVectorSet(7.25f, 5.6f, 0.0f, 1.0f));
    }

    XMVECTOR lightDir = XMLoadFloat4(&m_Scene->MainDirectionalLight.m_DirectionWs);
    DirectX::XMStoreFloat4(&m_Scene->MainDirectionalLight.m_DirectionWs, DirectX::XMVector4Normalize(lightDir));

    auto colorDesc = CD3DX12_RESOURCE_DESC::Tex2D(backBufferFormat,
        m_Width, m_Height,
        1, 1,
        1, 0,
        D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

    auto colorTexture = std::make_shared<Texture>(colorDesc, &colorClearValue,
        TextureUsageType::RenderTarget,
        L"Color Render Target");

    auto depthDesc = CD3DX12_RESOURCE_DESC::Tex2D(depthBufferFormat,
        m_Width, m_Height,
        1, 1,
        1, 0,
        D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
    D3D12_CLEAR_VALUE depthClearValue;
    depthClearValue.Format = depthDesc.Format;
    depthClearValue.DepthStencil = { 1.0f, 0 };

    auto depthTexture = std::make_shared<Texture>(depthDesc, &depthClearValue,
        TextureUsageType::Depth,
        L"Depth Render Target");

    m_RenderTarget.AttachTexture(Color0, colorTexture);
    m_RenderTarget.AttachTexture(DepthStencil, depthTexture);

    // PostFX
    {
        auto postFxColorTexture = std::make_shared<Texture>(colorDesc, &colorClearValue, TextureUsageType::RenderTarget, L"PostFX Render Target");
        m_PostFxRenderTarget.AttachTexture(Color0, postFxColorTexture);

        m_PostFxPso = std::make_unique<PostFxPso>(m_RootSignature, *commandList);
    }

    // Bloom
    {
        m_Bloom = std::make_unique<Bloom>(m_RootSignature, *commandList, m_Width, m_Height, backBufferFormat);
    }

    auto fenceValue = commandQueue->ExecuteCommandList(commandList);
    commandQueue->WaitForFenceValue(fenceValue);

    return true;
}

void LightingDemo::OnResize(ResizeEventArgs& e)
{
    Base::OnResize(e);

    if (m_Width != e.Width || m_Height != e.Height)
    {
        m_Width = std::max(1, e.Width);
        m_Height = std::max(1, e.Height);

        const float aspectRatio = static_cast<float>(m_Width) / static_cast<float>(m_Height);
        m_Scene->MainCamera.SetProjection(45.0f, aspectRatio, 0.1f, 1000.0f);

        m_Viewport = CD3DX12_VIEWPORT(0.0f, 0.0f,
            static_cast<float>(m_Width), static_cast<float>(m_Height));

        m_RenderTarget.Resize(m_Width, m_Height);
        m_PostFxRenderTarget.Resize(m_Width, m_Height);

        if (m_Bloom != nullptr)
            m_Bloom->Resize(m_Width, m_Height);

        if (m_SceneRenderer != nullptr)
            m_SceneRenderer->SetScreenResolution(m_Width, m_Height);
    }
}

void LightingDemo::UnloadContent()
{}

void LightingDemo::OnUpdate(UpdateEventArgs& e)
{
    static uint64_t frameCount = 0;
    static double totalTime = 0.0;

    Base::OnUpdate(e);

    if (e.ElapsedTime > 1.0f) return;

    totalTime += e.ElapsedTime;
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
    m_Scene->MainCamera.Translate(cameraTranslate, Space::Local);
    m_Scene->MainCamera.Translate(cameraPan, Space::Local);

    XMVECTOR cameraRotation = XMQuaternionRotationRollPitchYaw(XMConvertToRadians(m_CameraController.m_Pitch),
        XMConvertToRadians(m_CameraController.m_Yaw), 0.0f);
    m_Scene->MainCamera.SetRotation(cameraRotation);

    for (auto& ps : m_Scene->ParticleSystems)
    {
        ps.Update(e.ElapsedTime);
    }

    auto dt = static_cast<float>(e.ElapsedTime);

    if (m_AnimateLights)
    {
        XMVECTOR dirLightDirectionWs = XMLoadFloat4(&m_Scene->MainDirectionalLight.m_DirectionWs);
        dirLightDirectionWs = XMVector4Transform(dirLightDirectionWs,
            XMMatrixRotationAxis(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),
                XMConvertToRadians(90.0f * dt)));
        DirectX::XMStoreFloat4(&m_Scene->MainDirectionalLight.m_DirectionWs, dirLightDirectionWs);
    }
}

void LightingDemo::OnRender(RenderEventArgs& e)
{
    Base::OnRender(e);

    const auto commandQueue = Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
    const auto commandList = commandQueue->GetCommandList();

    // Clear the render targets
    {
        commandList->ClearTexture(*m_RenderTarget.GetTexture(Color0), CLEAR_COLOR);
        commandList->ClearDepthStencilTexture(*m_RenderTarget.GetTexture(DepthStencil), D3D12_CLEAR_FLAG_DEPTH);
    }

    m_RootSignature->Bind(*commandList);
    m_SceneRenderer->ShadowPass(*commandList);

    {
        PIXScope(*commandList, "Reflection Cubemap");

        m_SceneRenderer->ToggleEnvironmentReflections(false);
        m_ReflectionCubemap->Clear(*commandList);
        m_ReflectionCubemap->SetViewportScissorRect(*commandList);

        for (uint32_t i = 0; i < Cubemap::SIDES_COUNT; ++i)
        {
            PIXScope(*commandList, "Reflection Cubemap Side");
            XMMATRIX cubemapSideView, cubemapSideProjection;
            m_ReflectionCubemap->ComputeViewProjectionMatrices(*commandList, i, &cubemapSideView, &cubemapSideProjection);
            m_SceneRenderer->SetMatrices(cubemapSideView, cubemapSideProjection);

            m_ReflectionCubemap->SetRenderTarget(*commandList, i);

            m_SceneRenderer->MainPass(*commandList);
        }
    }

    {
        PIXScope(*commandList, "Main Pass");
        commandList->SetRenderTarget(m_RenderTarget);
        commandList->SetViewport(m_Viewport);
        commandList->SetScissorRect(m_ScissorRect);

        const XMMATRIX viewMatrix = m_Scene->MainCamera.GetViewMatrix();
        const XMMATRIX projectionMatrix = m_Scene->MainCamera.GetProjectionMatrix();

        m_SceneRenderer->SetMatrices(viewMatrix, projectionMatrix);
        m_SceneRenderer->ToggleEnvironmentReflections(true);
        m_SceneRenderer->MainPass(*commandList);
    }

    {
        PIXScope(*commandList, "PostFX");

        commandList->SetRenderTarget(m_PostFxRenderTarget);
        commandList->SetViewport(m_Viewport);
        commandList->SetScissorRect(m_ScissorRect);

        m_PostFxPso->SetSourceColorTexture(*commandList, m_RenderTarget.GetTexture(Color0));
        m_PostFxPso->SetSourceDepthTexture(*commandList, m_RenderTarget.GetTexture(DepthStencil));

        PostFxPso::PostFxParameters parameters;
        parameters.ProjectionInverse = m_Scene->MainCamera.GetInverseProjectionMatrix();
        parameters.FogColor = XMFLOAT3(CLEAR_COLOR);
        parameters.FogDensity = 0.017f;
        m_PostFxPso->SetParameters(*commandList, parameters);

        m_PostFxPso->Blit(*commandList);
    }

    {
        BloomParameters parameters{};
        parameters.Threshold = 1.25f;
        parameters.SoftThreshold = 0.3f;
        parameters.Intensity = 30.0f;
        m_Bloom->Draw(*commandList, m_PostFxRenderTarget.GetTexture(Color0), m_PostFxRenderTarget, parameters);
    }

    commandQueue->ExecuteCommandList(commandList);
    PWindow->Present(*m_PostFxRenderTarget.GetTexture(Color0));
}

void LightingDemo::OnKeyPressed(KeyEventArgs& e)
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
        m_Scene->MainCamera.SetTranslation(m_PAlignedCameraData->m_InitialPosition);
        m_Scene->MainCamera.SetRotation(m_PAlignedCameraData->m_InitialQRotation);
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
    case KeyCode::ShiftKey:
        m_CameraController.m_Shift = true;
        break;
    }
}

void LightingDemo::OnKeyReleased(KeyEventArgs& e)
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

void LightingDemo::OnMouseMoved(MouseMotionEventArgs& e)
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

void LightingDemo::OnMouseWheel(MouseWheelEventArgs& e)
{
    {
        auto fov = m_Scene->MainCamera.GetFov();

        fov -= e.WheelDelta;
        fov = Clamp(fov, 12.0f, 90.0f);

        m_Scene->MainCamera.SetFov(fov);

        char buffer[256];
        sprintf_s(buffer, "FoV: %f\n", fov);
        OutputDebugStringA(buffer);
    }
}
