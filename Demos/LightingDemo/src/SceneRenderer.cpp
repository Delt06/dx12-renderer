#include <SceneRenderer.h>
#include <memory>

#include <DX12Library/Helpers.h>
#include <Framework/Model.h>
#include <Framework/Mesh.h>
#include <Framework/MatricesCb.h>
#include <DX12Library/Cubemap.h>
#include <DX12Library/ShaderUtils.h>
#include "CBuffers.h"

using namespace DirectX;
using namespace Microsoft::WRL;

namespace
{
    constexpr UINT POINT_LIGHTS_REGISTER_INDEX = 1;
    constexpr UINT SPOT_LIGHTS_REGISTER_INDEX = 4;

    constexpr UINT POINT_LIGHTS_MATRICES_REGISTER_INDEX = 3;
    constexpr UINT SPOT_LIGHTS_MATRICES_REGISTER_INDEX = 6;

    constexpr UINT DIRECTIONAL_LIGHT_SHADOW_MAP_REGISTER_INDEX = 0;
    constexpr UINT POINT_LIGHT_SHADOW_MAPS_REGISTER_INDEX = 2;
    constexpr UINT SPOT_LIGHT_SHADOW_MAPS_REGISTER_INDEX = 5;

    constexpr UINT ENVIRONMENT_REFLECTIONS_REGISTER_INDEX = 7;
}

SceneRenderer::SceneRenderer(const std::shared_ptr<CommonRootSignature> &rootSignature, CommandList &commandList, const GraphicsSettings &graphicsSettings, DXGI_FORMAT backBufferFormat, DXGI_FORMAT depthBufferFormat)
    : m_GraphicsSettings(graphicsSettings), m_RootSignature(rootSignature)
{
    m_PointLightPass = std::make_unique<PointLightPass>(rootSignature, commandList);

    // Setup shadows
    {
        // Directional Light
        const auto &directionalLightShadowsQuality = graphicsSettings.DirectionalLightShadows;
        m_DirectionalLightShadowPassPso = std::make_unique<DirectionalLightShadowPassPso>(
            rootSignature, directionalLightShadowsQuality.m_Resolution);
        m_DirectionalLightShadowPassPso->SetBias(directionalLightShadowsQuality.m_DepthBias,
            directionalLightShadowsQuality.m_NormalBias);

        // Point Lights
        const auto &pointLightShadowsQuality = graphicsSettings.PointLightShadows;
        m_PointLightShadowPassPso = std::make_unique<PointLightShadowPassPso>(
            rootSignature, pointLightShadowsQuality.m_Resolution);
        m_PointLightShadowPassPso->SetBias(pointLightShadowsQuality.m_DepthBias,
            pointLightShadowsQuality.m_NormalBias);

        // Spot Lights
        const auto &spotLightShadowsQuality = graphicsSettings.SpotLightShadows;
        m_SpotLightShadowPassPso = std::make_unique<SpotLightShadowPassPso>(
            rootSignature, spotLightShadowsQuality.m_Resolution);
        m_SpotLightShadowPassPso->SetBias(spotLightShadowsQuality.m_DepthBias,
            spotLightShadowsQuality.m_NormalBias);
    }

    m_PointLightsStructuredBuffer = std::make_shared<StructuredBuffer>(L"Point Lights");
    m_SpotLightsStructuredBuffer = std::make_shared<StructuredBuffer>(L"Spot Lights");
    m_PointLightsMatricesStructuredBuffer = std::make_shared<StructuredBuffer>(L"Point Lights Matrices");
    m_SpotLightsMatricesStructuredBuffer = std::make_shared<StructuredBuffer>(L"Spot Lights Matrices");
}

void SceneRenderer::ResetShadowMatrices()
{
    m_DirectionalLightShadowMatrix = XMMatrixIdentity();
    m_PointLightShadowMatrices.clear();
    m_SpotLightShadowMatrices.clear();
}

void SceneRenderer::SetScene(const std::shared_ptr<Scene> scene)
{
    m_Scene = scene;
}

void SceneRenderer::SetEnvironmentReflectionsCubemap(const std::shared_ptr<Cubemap> cubemap)
{
    m_EnvironmentReflectionsCubemap = cubemap;
}

void SceneRenderer::ToggleEnvironmentReflections(bool enabled)
{
    m_AreEnvironmentReflectionsEnabled = enabled;
}

void SceneRenderer::SetMatrices(DirectX::XMMATRIX viewMatrix, DirectX::XMMATRIX projectionMatrix)
{
    m_ViewMatrix = viewMatrix;
    m_ProjectionMatrix = projectionMatrix;
}

void SceneRenderer::SetEnvironmentReflectionsCubemapSrv(CommandList &commandList, Cubemap &cubemap)
{
    m_RootSignature->SetPipelineShaderResourceView(commandList, ENVIRONMENT_REFLECTIONS_REGISTER_INDEX, ShaderResourceView(cubemap.GetTexture(), 0, Cubemap::SIDES_COUNT, cubemap.GetSrvDesc()));
}

void SceneRenderer::SetEmptyEnvironmentReflectionsCubemapSrv(CommandList &commandList, Cubemap &cubemap)
{
    m_RootSignature->SetPipelineShaderResourceView(commandList, ENVIRONMENT_REFLECTIONS_REGISTER_INDEX, ShaderResourceView(cubemap.GetFallbackTexture(), 0, Cubemap::SIDES_COUNT, cubemap.GetSrvDesc()));
}

void SceneRenderer::ShadowPass(CommandList &commandList)
{
    PIXScope(commandList, "Shadow Pass");
    ResetShadowMatrices();

    {
        PIXScope(commandList, "Directional Light Shadows");

        m_DirectionalLightShadowPassPso->Begin(commandList);
        m_DirectionalLightShadowPassPso->ClearShadowMap(commandList);
        m_DirectionalLightShadowPassPso->ComputePassParameters(*m_Scene, m_Scene->MainDirectionalLight);
        m_DirectionalLightShadowPassPso->SetRenderTarget(commandList);

        for (auto &gameObject : m_Scene->GameObjects)
        {
            m_DirectionalLightShadowPassPso->DrawToShadowMap(commandList, gameObject);
        }

        m_DirectionalLightShadowPassPso->End(commandList);
    }

    {
        PIXScope(commandList, "Point Light Shadows");

        const auto pointLightsCount = static_cast<uint32_t>(m_Scene->PointLights.size());
        m_PointLightShadowPassPso->SetShadowMapsCount(pointLightsCount);
        m_PointLightShadowPassPso->ClearShadowMap(commandList);
        m_PointLightShadowPassPso->Begin(commandList);

        for (uint32_t lightIndex = 0; lightIndex < pointLightsCount; ++lightIndex)
        {
            const PointLight &pointLight = m_Scene->PointLights[lightIndex];

            for (uint32_t cubeMapSideIndex = 0; cubeMapSideIndex < Cubemap::SIDES_COUNT; ++cubeMapSideIndex)
            {
                m_PointLightShadowPassPso->SetCurrentShadowMap(lightIndex, cubeMapSideIndex);
                m_PointLightShadowPassPso->ComputePassParameters(pointLight);
                m_PointLightShadowPassPso->SetRenderTarget(commandList);

                for (auto &gameObject : m_Scene->GameObjects)
                {
                    m_PointLightShadowPassPso->DrawToShadowMap(commandList, gameObject);
                }

                m_PointLightShadowMatrices.push_back(m_PointLightShadowPassPso->GetShadowViewProjectionMatrix());
            }
        }

        m_PointLightShadowPassPso->End(commandList);
    }
    {
        PIXScope(commandList, "Spot Light Shadows");

        const auto spotLightsCount = static_cast<uint32_t>(m_Scene->SpotLights.size());
        m_SpotLightShadowPassPso->SetShadowMapsCount(spotLightsCount);
        m_SpotLightShadowPassPso->ClearShadowMap(commandList);
        m_SpotLightShadowPassPso->Begin(commandList);

        for (uint32_t lightIndex = 0; lightIndex < spotLightsCount; ++lightIndex)
        {
            const SpotLight &spotLight = m_Scene->SpotLights[lightIndex];

            m_SpotLightShadowPassPso->SetCurrentShadowMap(lightIndex);
            m_SpotLightShadowPassPso->ComputePassParameters(spotLight);
            m_SpotLightShadowPassPso->SetRenderTarget(commandList);

            for (auto &gameObject : m_Scene->GameObjects)
            {
                m_SpotLightShadowPassPso->DrawToShadowMap(commandList, gameObject);
            }

            m_SpotLightShadowMatrices.push_back(m_SpotLightShadowPassPso->GetShadowViewProjectionMatrix());
        }

        m_SpotLightShadowPassPso->End(commandList);
    }
}

void SceneRenderer::MainPass(CommandList &commandList)
{
    const XMMATRIX viewMatrix = m_ViewMatrix;
    const XMMATRIX projectionMatrix = m_ProjectionMatrix;
    const XMMATRIX viewProjectionMatrix = viewMatrix * projectionMatrix;

    Demo::Pipeline::CBuffer pipelineCBuffer{};
    {
        pipelineCBuffer.m_View = viewMatrix;
        pipelineCBuffer.m_Projection = projectionMatrix;
        pipelineCBuffer.m_ViewProjection = viewProjectionMatrix;

        DirectX::XMStoreFloat4(&pipelineCBuffer.m_CameraPosition, m_Scene->MainCamera.GetTranslation());

        pipelineCBuffer.m_InverseView = DirectX::XMMatrixInverse(nullptr, viewMatrix);
        pipelineCBuffer.m_InverseProjection = DirectX::XMMatrixInverse(nullptr, projectionMatrix);

        pipelineCBuffer.m_ScreenResolution = { static_cast<float>(m_Width), static_cast<float>(m_Height) };
        pipelineCBuffer.m_ScreenTexelSize = { 1.0f / static_cast<float>(m_Width), 1.0f / static_cast<float>(m_Height) };

        pipelineCBuffer.m_DirectionalLightViewProjection = m_DirectionalLightShadowPassPso->GetShadowViewProjectionMatrix();
        pipelineCBuffer.m_DirectionalLight = m_Scene->MainDirectionalLight;

        pipelineCBuffer.m_NumPointLights = m_Scene->PointLights.size();
        pipelineCBuffer.m_NumSpotLights = m_Scene->SpotLights.size();

        pipelineCBuffer.m_ShadowReceiverParameters.PoissonSpreadInv = 1.0f / m_GraphicsSettings.DirectionalLightShadows.m_PoissonSpread;
        pipelineCBuffer.m_ShadowReceiverParameters.PointLightPoissonSpreadInv = 1.0f / m_GraphicsSettings.PointLightShadows.m_PoissonSpread;
        pipelineCBuffer.m_ShadowReceiverParameters.SpotLightPoissonSpreadInv = 1.0f / m_GraphicsSettings.PointLightShadows.m_PoissonSpread;
    }
    m_RootSignature->SetPipelineConstantBuffer(commandList, pipelineCBuffer);

    {
        PIXScope(commandList, "Draw Point Lights");

        m_PointLightPass->Begin(commandList);

        for (const auto &pointLight : m_Scene->PointLights)
        {
            m_PointLightPass->Draw(commandList, pointLight, viewProjectionMatrix, 1.0f);
        }

        m_PointLightPass->End(commandList);
    }

    {
        PIXScope(commandList, "Opaque Pass");

        if (m_AreEnvironmentReflectionsEnabled)
        {
            SetEnvironmentReflectionsCubemapSrv(commandList, *m_EnvironmentReflectionsCubemap);
        }
        else
        {
            SetEmptyEnvironmentReflectionsCubemapSrv(commandList, *m_EnvironmentReflectionsCubemap);
        }

        // Update other light buffers
        {
            commandList.CopyStructuredBuffer(*m_PointLightsStructuredBuffer, m_Scene->PointLights);
            commandList.CopyStructuredBuffer(*m_SpotLightsStructuredBuffer, m_Scene->SpotLights);

            m_RootSignature->SetPipelineShaderResourceView(commandList, POINT_LIGHTS_REGISTER_INDEX, ShaderResourceView(m_PointLightsStructuredBuffer));
            m_RootSignature->SetPipelineShaderResourceView(commandList, SPOT_LIGHTS_REGISTER_INDEX, ShaderResourceView(m_SpotLightsStructuredBuffer));
        }

        // Bind shadow maps
        {
            m_RootSignature->SetPipelineShaderResourceView(commandList, DIRECTIONAL_LIGHT_SHADOW_MAP_REGISTER_INDEX,
                m_PointLightShadowPassPso->GetShadowMapShaderResourceView()
            );

            m_RootSignature->SetPipelineShaderResourceView(commandList, POINT_LIGHT_SHADOW_MAPS_REGISTER_INDEX,
                m_PointLightShadowPassPso->GetShadowMapShaderResourceView()
            );

            m_RootSignature->SetPipelineShaderResourceView(commandList, SPOT_LIGHT_SHADOW_MAPS_REGISTER_INDEX,
                m_SpotLightShadowPassPso->GetShadowMapShaderResourceView()
            );

            // bind shadow matrices
            {
                commandList.CopyStructuredBuffer(*m_PointLightsMatricesStructuredBuffer, m_PointLightShadowMatrices);
                commandList.CopyStructuredBuffer(*m_SpotLightsMatricesStructuredBuffer, m_SpotLightShadowMatrices);

                m_RootSignature->SetPipelineShaderResourceView(commandList, POINT_LIGHTS_MATRICES_REGISTER_INDEX, ShaderResourceView(m_PointLightsMatricesStructuredBuffer));
                m_RootSignature->SetPipelineShaderResourceView(commandList, SPOT_LIGHTS_MATRICES_REGISTER_INDEX, ShaderResourceView(m_SpotLightsMatricesStructuredBuffer));
            }
        }

        for (const auto &go : m_Scene->GameObjects)
        {
            Demo::Model::CBuffer modelCBuffer{};
            modelCBuffer.Compute(go.GetWorldMatrix(), viewProjectionMatrix);
            m_RootSignature->SetModelConstantBuffer(commandList, modelCBuffer);

            go.Draw(commandList);
        }
    }

    {
        PIXScope(commandList, "Particle Systems");

        for (auto &ps : m_Scene->ParticleSystems)
        {
            ps.Draw(commandList);
        }
    }
}

void SceneRenderer::SetScreenResolution(int width, int height)
{
    m_Width = width;
    m_Height = height;
}
