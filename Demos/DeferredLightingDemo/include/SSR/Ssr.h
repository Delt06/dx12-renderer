#pragma once
#include "SsrTrace.h"
#include "SsrBlurPass.h"
class Ssr final : public EffectBase
{
public: 
	explicit Ssr(Shader::Format renderTargetFormat, const D3D12_SHADER_RESOURCE_VIEW_DESC& depthSrv, uint32_t width, uint32_t height);

	void Resize(uint32_t width, uint32_t height);

	void CaptureSceneColor(CommandList& commandList, const Texture& source);

	void SetMatrices(DirectX::XMMATRIX viewMatrix, DirectX::XMMATRIX projectionMatrix);
	void SetJitterOffset(DirectX::XMFLOAT2 jitterOffset);
	void Execute(CommandList& commandList, const Texture& normals, const Texture& surface, const Texture& depth) const;

	void Init(Microsoft::WRL::ComPtr<IDevice> device, CommandList& commandList) override;

	[[nodiscard]] const Texture& GetReflectionsTexture() const;
	[[nodiscard]] const Texture& GetEmptyReflectionsTexture() const;

private:
	Texture m_SceneColor{};

	SsrTrace m_Trace;
	RenderTarget m_TraceRenderTarget{};
	RenderTarget m_FinalReflectionsTexture{};
	Texture m_EmptyReflections{};
	D3D12_SHADER_RESOURCE_VIEW_DESC m_DepthSrv;

	SsrBlurPass m_BlurPass;

	DirectX::XMMATRIX m_ViewMatrix{};
	DirectX::XMMATRIX m_ProjectionMatrix{};
};
