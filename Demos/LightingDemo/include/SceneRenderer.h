#pragma once

#include <memory>

#include <wrl.h>
#include <DirectXMath.h>
#include <d3d12.h>
#include <vector>

#include "Scene.h"
#include "DirectionalLightShadowPassPso.h"
#include "PointLightShadowPassPso.h"
#include "SpotLightShadowPassPso.h"
#include <DX12Library/Cubemap.h>
#include <DX12Library/StructuredBuffer.h>
#include <Framework/GraphicsSettings.h>
#include "PointLightPass.h"
#include <Framework/CommonRootSignature.h>


class SceneRenderer
{
public:
    SceneRenderer(const std::shared_ptr<CommonRootSignature>& rootSignature, CommandList& commandList, const GraphicsSettings& graphicsSettings, DXGI_FORMAT backBufferFormat, DXGI_FORMAT depthBufferFormat);

    void SetScene(const std::shared_ptr<Scene> scene);
    void SetEnvironmentReflectionsCubemap(const std::shared_ptr<Cubemap> cubemap);

    void ToggleEnvironmentReflections(bool enabled);
    void SetMatrices(DirectX::XMMATRIX viewMatrix, DirectX::XMMATRIX projectionMatrix);

    void ShadowPass(CommandList& commandList);
    void MainPass(CommandList& commandList);

    void SetScreenResolution(int width, int height);

private:
    void ResetShadowMatrices();

    void SetEnvironmentReflectionsCubemapSrv(CommandList& commandList, Cubemap& cubemap);
    void SetEmptyEnvironmentReflectionsCubemapSrv(CommandList& commandList, Cubemap& cubemap);

    std::shared_ptr<CommonRootSignature> m_RootSignature;

    GraphicsSettings m_GraphicsSettings;

    std::unique_ptr<PointLightPass> m_PointLightPass;
    std::unique_ptr<DirectionalLightShadowPassPso> m_DirectionalLightShadowPassPso;
    std::unique_ptr<PointLightShadowPassPso> m_PointLightShadowPassPso;
    std::unique_ptr<SpotLightShadowPassPso> m_SpotLightShadowPassPso;

    std::shared_ptr<Scene> m_Scene;
    std::shared_ptr<Cubemap> m_EnvironmentReflectionsCubemap;
    bool m_AreEnvironmentReflectionsEnabled;
    DirectX::XMMATRIX m_ViewMatrix;
    DirectX::XMMATRIX m_ProjectionMatrix;

    int m_Width;
    int m_Height;

    DirectX::XMMATRIX m_DirectionalLightShadowMatrix;
    std::vector<DirectX::XMMATRIX> m_PointLightShadowMatrices;
    std::vector<DirectX::XMMATRIX> m_SpotLightShadowMatrices;

    std::shared_ptr<StructuredBuffer> m_PointLightsStructuredBuffer;
    std::shared_ptr<StructuredBuffer> m_SpotLightsStructuredBuffer;

    std::shared_ptr<StructuredBuffer> m_PointLightsMatricesStructuredBuffer;
    std::shared_ptr<StructuredBuffer> m_SpotLightsMatricesStructuredBuffer;
};

