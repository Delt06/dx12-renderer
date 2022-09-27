#include <SSR/Ssr.h>
#include <DX12Library/Helpers.h>

Ssr::Ssr(Shader::Format renderTargetFormat, const D3D12_SHADER_RESOURCE_VIEW_DESC& depthSrv, uint32_t width, uint32_t height)
	: m_Trace(renderTargetFormat)
	, m_DepthSrv(depthSrv)
	, m_BlurPass(renderTargetFormat)
{
	m_Trace.SetDepthSrv(&m_DepthSrv);

	const auto traceRtDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		renderTargetFormat, width, height, 
		1, 1, 1, 0, 
		D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
	);
	auto traceTexture = Texture(traceRtDesc, nullptr, TextureUsageType::RenderTarget, L"SSR Trace Result");
	m_TraceRenderTarget.AttachTexture(Color0, traceTexture);

	auto finalTexture = Texture(traceRtDesc, nullptr, TextureUsageType::RenderTarget, L"SSR Reflections");
	m_FinalReflectionsTexture.AttachTexture(Color0, finalTexture);

	const auto emptyDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		renderTargetFormat, 1, 1,
		1, 1
	);
	m_EmptyReflections = Texture(emptyDesc, nullptr, TextureUsageType::Other, L"SSR Empty Reflections");

	const auto sceneColorDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		renderTargetFormat, width, height,
		1, 1
	);
	m_SceneColor = Texture(sceneColorDesc, nullptr, TextureUsageType::Other, L"SSR Scene Color");
}

void Ssr::Resize(uint32_t width, uint32_t height)
{
	m_TraceRenderTarget.Resize(width, height);
	m_FinalReflectionsTexture.Resize(width, height);
	m_SceneColor.Resize(width, height);
}

void Ssr::CaptureSceneColor(CommandList& commandList, const Texture& source)
{
	PIXScope(commandList, "SSR: Capture Scene Color");

	commandList.CopyResource(m_SceneColor, source);
}

void Ssr::SetMatrices(DirectX::XMMATRIX viewMatrix, DirectX::XMMATRIX projectionMatrix)
{
	m_ViewMatrix = viewMatrix;
	m_ProjectionMatrix = projectionMatrix;
}

void Ssr::SetJitterOffset(DirectX::XMFLOAT2 jitterOffset)
{
	m_Trace.SetJitterOffset(jitterOffset);
}

void Ssr::Execute(CommandList& commandList, const Texture& normals, const Texture& surface, const Texture& depth) const
{
	PIXScope(commandList, "SSR");

	{
		PIXScope(commandList, "SSR Trace");
		m_Trace.Execute(commandList, m_SceneColor, normals, surface, depth, m_TraceRenderTarget, m_ViewMatrix, m_ProjectionMatrix);
	}

	{
		PIXScope(commandList, "SSR Blur Pass");
		const Texture& traceResult = m_TraceRenderTarget.GetTexture(Color0);
		m_BlurPass.Execute(commandList, traceResult, m_FinalReflectionsTexture);
	}
	
}

void Ssr::Init(Microsoft::WRL::ComPtr<IDevice> device, CommandList& commandList)
{
	m_Trace.Init(device, commandList);
	m_BlurPass.Init(device, commandList);
}

const Texture& Ssr::GetReflectionsTexture() const
{
	return m_FinalReflectionsTexture.GetTexture(Color0);
}

const Texture& Ssr::GetEmptyReflectionsTexture() const
{
	return m_EmptyReflections;
}
