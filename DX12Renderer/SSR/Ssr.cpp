#include "Ssr.h"
#include "Helpers.h"

Ssr::Ssr(Shader::Format renderTargetFormat, const D3D12_SHADER_RESOURCE_VIEW_DESC& depthSrv, uint32_t width, uint32_t height)
	: m_Trace(renderTargetFormat)
	, m_DepthSrv(depthSrv)
{
	m_Trace.SetDepthSrv(&m_DepthSrv);

	const auto traceRtDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		renderTargetFormat, width, height, 
		1, 1, 1, 0, 
		D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
	);
	auto traceTexture = Texture(traceRtDesc, nullptr, TextureUsageType::RenderTarget, L"SSR Trace Result");
	m_TraceRenderTarget.AttachTexture(Color0, traceTexture);
}

void Ssr::Resize(uint32_t width, uint32_t height)
{
	m_TraceRenderTarget.Resize(width, height);
}

void Ssr::SetMatrices(DirectX::XMMATRIX viewMatrix, DirectX::XMMATRIX projectionMatrix)
{
	m_ViewMatrix = viewMatrix;
	m_ProjectionMatrix = projectionMatrix;
}

void Ssr::Execute(CommandList& commandList, const Texture& sceneColor, const Texture& normals, const Texture& depth) const
{
	PIXScope(commandList, "SSR");

	m_Trace.Execute(commandList, sceneColor, normals, depth, m_TraceRenderTarget, m_ViewMatrix, m_ProjectionMatrix);
}

void Ssr::Init(Microsoft::WRL::ComPtr<IDevice> device, CommandList& commandList)
{
	m_Trace.Init(device, commandList);
}
