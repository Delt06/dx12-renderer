#pragma once
#include "SsrTrace.h"
#include "SsrBlurPass.h"

#include <Framework/CommonRootSignature.h>
#include <DX12Library/CommandList.h>

#include <memory>

class Ssr
{
public:
	explicit Ssr(const std::shared_ptr<CommonRootSignature>& rootSignature, CommandList& commandList, DXGI_FORMAT renderTargetFormat, uint32_t width, uint32_t height, bool downsample);

	void Resize(uint32_t width, uint32_t height);

	void CaptureSceneColor(CommandList& commandList, const Texture& source);


	void Execute(CommandList& commandList) const;

	[[nodiscard]] const std::shared_ptr<Texture>& GetReflectionsTexture() const;
	[[nodiscard]] const std::shared_ptr<Texture>& GetEmptyReflectionsTexture() const;

private:
	bool m_Downsample;

	std::shared_ptr<Texture> m_SceneColor;

	SsrTrace m_Trace;
	SsrBlurPass m_BlurPass;

	RenderTarget m_TraceRenderTarget{};
	RenderTarget m_FinalReflectionsTexture{};
	std::shared_ptr<Texture> m_EmptyReflections{};
};

