#include "PipelineStateBuilder.h"
#include <DX12Library/Helpers.h>
#include <Framework/Mesh.h>

static constexpr UINT MAX_RENDER_TARGETS = _countof(D3D12_RT_FORMAT_ARRAY::RTFormats);

PipelineStateBuilder::PipelineStateBuilder(const std::shared_ptr<RootSignature> rootSignature)
	: m_RootSignature(rootSignature)
	, m_InputLayout(VertexAttributes::INPUT_ELEMENT_COUNT)
{
	memcpy(m_InputLayout.data(), VertexAttributes::INPUT_ELEMENTS, sizeof(VertexAttributes::INPUT_ELEMENTS));
}

Microsoft::WRL::ComPtr<ID3D12PipelineState> PipelineStateBuilder::Build(Microsoft::WRL::ComPtr<ID3D12Device2> device) const
{
	Assert(m_VertexShader != nullptr, "Vertex Shader cannot be null.");
	Assert(m_PixelShader != nullptr, "Pixel Shader cannot be null.");

	// Setup the pipeline state.
	struct PipelineStateStream
	{
		CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE RootSignature;
		CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
		CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
		CD3DX12_PIPELINE_STATE_STREAM_VS Vs;
		CD3DX12_PIPELINE_STATE_STREAM_PS Ps;
		CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RtvFormats;
		CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC Blend;
	} pipelineStateStream;

	D3D12_RT_FORMAT_ARRAY rtvFormats = {};
	rtvFormats.NumRenderTargets = static_cast<UINT>(m_RenderTargetFormats.size());
	memcpy(rtvFormats.RTFormats, m_RenderTargetFormats.data(), m_RenderTargetFormats.size() * sizeof(DXGI_FORMAT));

	pipelineStateStream.RootSignature = m_RootSignature->GetRootSignature().Get();

	pipelineStateStream.InputLayout = { m_InputLayout.data(), static_cast<UINT>(m_InputLayout.size()) };
	pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipelineStateStream.Vs = { m_VertexShader->GetBufferPointer(), m_VertexShader->GetBufferSize() };
	pipelineStateStream.Ps = { m_PixelShader->GetBufferPointer(), m_PixelShader->GetBufferSize() };
	pipelineStateStream.RtvFormats = rtvFormats;
	pipelineStateStream.Blend = m_BlendDesc;

	const D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
		sizeof(PipelineStateStream), &pipelineStateStream
	};

	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
	ThrowIfFailed(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&pipelineState)));
	return pipelineState;
}

PipelineStateBuilder& PipelineStateBuilder::WithRenderTargetFormats(const std::vector<DXGI_FORMAT>& renderTargetFormats)
{
	Assert(renderTargetFormats.size() < MAX_RENDER_TARGETS, "Too many render target formats.");

	m_RenderTargetFormats = renderTargetFormats;
	return *this;
}

PipelineStateBuilder& PipelineStateBuilder::WithShaders(const Microsoft::WRL::ComPtr<ID3DBlob>& vertexShader, const Microsoft::WRL::ComPtr<ID3DBlob>& pixelShader)
{
	Assert(vertexShader != nullptr, "Vertex Shader cannot be null.");
	Assert(pixelShader != nullptr, "Pixel Shader cannot be null.");

	m_VertexShader = vertexShader;
	m_PixelShader = pixelShader;
	return *this;
}

PipelineStateBuilder& PipelineStateBuilder::WithBlend(const CD3DX12_BLEND_DESC& blendDesc)
{
	m_BlendDesc = blendDesc;
	return *this;
}

PipelineStateBuilder& PipelineStateBuilder::WithInputLayout(const std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout)
{
	m_InputLayout = inputLayout;
	return *this;
}
