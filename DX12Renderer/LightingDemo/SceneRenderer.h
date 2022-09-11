#pragma once

#include <memory>

#include <wrl.h>
#include <DirectXMath.h>
#include <d3d12.h>

#include <LightingDemo/Scene.h>
#include <vector>
#include <LightingDemo/DirectionalLightShadowPassPso.h>
#include <LightingDemo/PointLightShadowPassPso.h>
#include <LightingDemo/SpotLightShadowPassPso.h>
#include <GraphicsSettings.h>
#include "PointLightPso.h"

class SceneRenderer
{
public:
	SceneRenderer(Microsoft::WRL::ComPtr<ID3D12Device2> device, CommandList& commandList, const GraphicsSettings& graphicsSettings, DXGI_FORMAT backBufferFormat, DXGI_FORMAT depthBufferFormat);

	void Clear();
	void SetScene(const std::shared_ptr<Scene> scene);
	void SetMatrices(DirectX::XMMATRIX viewMatrix, DirectX::XMMATRIX projectionMatrix);

	void ShadowPass(CommandList& commandList);
	void MainPass(CommandList& commandList);

private:
	GraphicsSettings m_GraphicsSettings;

	RootSignature m_RootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PipelineState;

	std::unique_ptr<PointLightPso> m_PointLightPso;
	std::unique_ptr<DirectionalLightShadowPassPso> m_DirectionalLightShadowPassPso;
	std::unique_ptr<PointLightShadowPassPso> m_PointLightShadowPassPso;
	std::unique_ptr<SpotLightShadowPassPso> m_SpotLightShadowPassPso;

	std::shared_ptr<Scene> m_Scene;
	DirectX::XMMATRIX m_ViewMatrix;
	DirectX::XMMATRIX m_ProjectionMatrix;

	DirectX::XMMATRIX m_DirectionalLightShadowMatrix;
	std::vector<DirectX::XMMATRIX> m_PointLightShadowMatrices;
	std::vector<DirectX::XMMATRIX> m_SpotLightShadowMatrices;
};

