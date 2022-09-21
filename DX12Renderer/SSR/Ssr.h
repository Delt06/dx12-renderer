#pragma once
#include "SsrTrace.h"
#include "SsrLightPass.h"
class Ssr final : public EffectBase
{
public: 
	explicit Ssr(Shader::Format renderTargetFormat, const D3D12_SHADER_RESOURCE_VIEW_DESC& depthSrv, uint32_t width, uint32_t height);

	void Resize(uint32_t width, uint32_t height);

	void CaptureSceneColor(CommandList& commandList, const Texture& source);

	void SetMatrices(DirectX::XMMATRIX viewMatrix, DirectX::XMMATRIX projectionMatrix);
	void Execute(CommandList& commandList, const Texture& normals, const Texture& depth, const RenderTarget& resultRenderTarget) const;

	void Init(Microsoft::WRL::ComPtr<IDevice> device, CommandList& commandList) override;

private:
	Texture m_SceneColor{};

	SsrTrace m_Trace;
	RenderTarget m_TraceRenderTarget{};
	D3D12_SHADER_RESOURCE_VIEW_DESC m_DepthSrv;

	SsrLightPass m_LightPass;

	DirectX::XMMATRIX m_ViewMatrix{};
	DirectX::XMMATRIX m_ProjectionMatrix{};
};

