#include <SSR/Ssr.h>
#include <DX12Library/Helpers.h>

Ssr::Ssr(const std::shared_ptr<CommonRootSignature>& rootSignature, CommandList& commandList, DXGI_FORMAT renderTargetFormat, uint32_t width, uint32_t height, bool downsample)
	: m_Trace(rootSignature, commandList)
	, m_BlurPass(rootSignature, commandList)
	, m_Downsample(downsample)
{
	const auto sceneColorDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		renderTargetFormat, width, height,
		1, 1
	);
	m_SceneColor = std::make_shared<Texture>(sceneColorDesc, nullptr, TextureUsageType::Other, L"SSR Scene Color");

	if (m_Downsample)
	{
		width >>= 1;
		height >>= 1;
	}

	const auto traceRtDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		renderTargetFormat, width, height,
		1, 1, 1, 0,
		D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
	);
	auto traceTexture = std::make_shared<Texture>(traceRtDesc, nullptr, TextureUsageType::RenderTarget, L"SSR Trace Result");
	m_TraceRenderTarget.AttachTexture(Color0, traceTexture);

	auto finalTexture = std::make_shared<Texture>(traceRtDesc, nullptr, TextureUsageType::RenderTarget, L"SSR Reflections");
	m_FinalReflectionsTexture.AttachTexture(Color0, finalTexture);

	const auto emptyDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		renderTargetFormat, 1, 1,
		1, 1
	);
	m_EmptyReflections = std::make_shared<Texture>(emptyDesc, nullptr, TextureUsageType::Other, L"SSR Empty Reflections");
}

void Ssr::Resize(uint32_t width, uint32_t height)
{
	m_SceneColor->Resize(width, height);

	if (m_Downsample)
	{
		width >>= 1;
		height >>= 1;
	}

	m_TraceRenderTarget.Resize(width, height);
	m_FinalReflectionsTexture.Resize(width, height);
}

void Ssr::CaptureSceneColor(CommandList& commandList, const Texture& source)
{
	PIXScope(commandList, "SSR: Capture Scene Color");

	commandList.CopyResource(*m_SceneColor, source);
}

void Ssr::Execute(CommandList& commandList) const
{
	PIXScope(commandList, "SSR");

	{
		PIXScope(commandList, "SSR Trace");
		m_Trace.Execute(commandList, m_SceneColor, m_TraceRenderTarget);
	}

	{
		PIXScope(commandList, "SSR Blur Pass");
		const auto& traceResult = m_TraceRenderTarget.GetTexture(Color0);
		m_BlurPass.Execute(commandList, traceResult, m_FinalReflectionsTexture);
	}

}

const std::shared_ptr<Texture>& Ssr::GetReflectionsTexture() const
{
	return m_FinalReflectionsTexture.GetTexture(Color0);
}

const std::shared_ptr<Texture>& Ssr::GetEmptyReflectionsTexture() const
{
	return m_EmptyReflections;
}
