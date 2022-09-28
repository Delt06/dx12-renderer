#include <HDR/ToneMapping.h>
#include <DX12Library/ShaderUtils.h>
#include <DX12Library/Helpers.h>
#include <DX12Library/RenderTarget.h>
#include <Framework/Mesh.h>
#include <Framework/Blit_VS.h>

using namespace Microsoft::WRL;

namespace
{
	namespace RootParameters
	{
		enum RootParameters
		{
			Source,
			Parameters,
			NumRootParameters
		};
	}

	struct Parameters
	{
		float WhitePoint;
		float _Padding[3];
	};
}

ToneMapping::ToneMapping(Microsoft::WRL::ComPtr<ID3D12Device2> device, CommandList& commandList, DXGI_FORMAT renderTargetFormat)
	: m_BlitMesh(Mesh::CreateBlitTriangle(commandList))
	, m_ScissorRect(CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX))
{
	ComPtr<ID3DBlob> pixelShaderBlob = ShaderUtils::LoadShaderFromFile(L"HDR_ToneMapping_PS.cso");

	// Create a root signature.
	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData;
	featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
	{
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}

	constexpr D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS;

	CD3DX12_DESCRIPTOR_RANGE1 sourceDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0);

	CD3DX12_ROOT_PARAMETER1 rootParameters[RootParameters::NumRootParameters];
	rootParameters[RootParameters::Source].InitAsDescriptorTable(1, &sourceDescriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParameters[RootParameters::Parameters].InitAsConstants(sizeof Parameters / sizeof(float), 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);

	CD3DX12_STATIC_SAMPLER_DESC samplers[] = {
		// point clamp
		CD3DX12_STATIC_SAMPLER_DESC(0, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP),
	};

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
	rootSignatureDescription.Init_1_1(RootParameters::NumRootParameters, rootParameters, _countof(samplers), samplers,
		rootSignatureFlags);

	m_RootSignature.SetRootSignatureDesc(rootSignatureDescription.Desc_1_1, featureData.HighestVersion);

	// Setup the pipeline state.
	struct PipelineStateStream
	{
		CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE RootSignature;
		CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
		CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
		CD3DX12_PIPELINE_STATE_STREAM_VS Vs;
		CD3DX12_PIPELINE_STATE_STREAM_PS Ps;
		CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RtvFormats;
	} pipelineStateStream;

	D3D12_RT_FORMAT_ARRAY rtvFormats = {};
	rtvFormats.NumRenderTargets = 1;
	rtvFormats.RTFormats[0] = renderTargetFormat;

	pipelineStateStream.RootSignature = m_RootSignature.GetRootSignature().Get();

	pipelineStateStream.InputLayout = { VertexAttributes::INPUT_ELEMENTS, VertexAttributes::INPUT_ELEMENT_COUNT };
	pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipelineStateStream.Vs = CD3DX12_SHADER_BYTECODE(ShaderBytecode_Blit_VS, sizeof ShaderBytecode_Blit_VS);
	pipelineStateStream.Ps = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
	pipelineStateStream.RtvFormats = rtvFormats;

	const D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
		sizeof(PipelineStateStream), &pipelineStateStream
	};

	ThrowIfFailed(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&m_PipelineState)));
}

void ToneMapping::Blit(CommandList& commandList, const Texture& source, const Texture& luminanceOutput, RenderTarget& destination, float whitePoint)
{
	PIXScope(commandList, "Tone Mapping");

	commandList.SetGraphicsRootSignature(m_RootSignature);
	commandList.SetPipelineState(m_PipelineState);
	commandList.SetRenderTarget(destination);
	commandList.SetScissorRect(m_ScissorRect);

	auto destinationDesc = destination.GetTexture(Color0).GetD3D12ResourceDesc();
	CD3DX12_VIEWPORT viewport(0.0f, 0.0f, static_cast<float>(destinationDesc.Width), static_cast<float>(destinationDesc.Height));
	commandList.SetViewport(viewport);

	commandList.SetShaderResourceView(RootParameters::Source, 0, source, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList.SetShaderResourceView(RootParameters::Source, 1, luminanceOutput, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	Parameters parameters;
	parameters.WhitePoint = whitePoint;
	commandList.SetGraphics32BitConstants(RootParameters::Parameters, parameters);

	m_BlitMesh->Draw(commandList);
}