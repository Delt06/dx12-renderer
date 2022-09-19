#include "Taa.h"
#include "Mesh.h"
#include "Helpers.h"
#include <d3dcompiler.h>
#include "CommandList.h"
#include "Texture.h"

using namespace Microsoft::WRL;

namespace RootParameters
{
	enum RootParameters
	{
		// Texture2D : register(t0-t2)
		Buffers,
		// cbuffer : register(b0)
		ParametersCBuffer,
		NumRootParameters
	};

	struct Parameters
	{
		DirectX::XMFLOAT2 OneOverTexelSize;
		float _Padding[2];
	};
}

Taa::Taa(Microsoft::WRL::ComPtr<ID3D12Device2> device, CommandList& commandList, DXGI_FORMAT backBufferFormat, uint32_t width, uint32_t height)
	: m_BlitMesh(Mesh::CreateBlitTriangle(commandList))
{
	// create root signature and PSO
	{
		ComPtr<ID3DBlob> vertexShaderBlob;
		ThrowIfFailed(D3DReadFileToBlob(L"Blit_VertexShader.cso", &vertexShaderBlob));

		ComPtr<ID3DBlob> pixelShaderBlob;
		ThrowIfFailed(D3DReadFileToBlob(L"TAA_Resolve_PixelShader.cso", &pixelShaderBlob));

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

		CD3DX12_DESCRIPTOR_RANGE1 buffersDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0);

		CD3DX12_ROOT_PARAMETER1 rootParameters[RootParameters::NumRootParameters];
		rootParameters[RootParameters::Buffers].InitAsDescriptorTable(1, &buffersDescriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[RootParameters::ParametersCBuffer].InitAsConstants(sizeof(RootParameters::Parameters) / sizeof(float), 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);

		CD3DX12_STATIC_SAMPLER_DESC samplers[] = {
			// point clamp
			CD3DX12_STATIC_SAMPLER_DESC(0, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP),
			// linear clamp
			CD3DX12_STATIC_SAMPLER_DESC(1, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP),
		};

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
		rootSignatureDescription.Init_1_1(RootParameters::NumRootParameters, rootParameters, _countof(samplers), samplers,
			rootSignatureFlags);

		m_ResolveRootSignature.SetRootSignatureDesc(rootSignatureDescription.Desc_1_1, featureData.HighestVersion);

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
		rtvFormats.RTFormats[0] = backBufferFormat;

		pipelineStateStream.RootSignature = m_ResolveRootSignature.GetRootSignature().Get();

		pipelineStateStream.InputLayout = { VertexAttributes::INPUT_ELEMENTS, VertexAttributes::INPUT_ELEMENT_COUNT };
		pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pipelineStateStream.Vs = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
		pipelineStateStream.Ps = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
		pipelineStateStream.RtvFormats = rtvFormats;

		const D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
			sizeof(PipelineStateStream), &pipelineStateStream
		};

		ThrowIfFailed(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&m_ResolvePipelineState)));
	}

	auto historyBufferDesc = CD3DX12_RESOURCE_DESC::Tex2D(backBufferFormat, width, height, 1, 1);
	m_HistoryBuffer = Texture(historyBufferDesc, nullptr, TextureUsageType::Other, L"TAA History Buffer");
}

void Taa::Resolve(CommandList& commandList, const Texture& currentBuffer, const Texture& velocityBuffer)
{
	PIXScope(commandList, "Resolve TAA");

	commandList.SetPipelineState(m_ResolvePipelineState);
	commandList.SetGraphicsRootSignature(m_ResolveRootSignature);

	commandList.SetShaderResourceView(RootParameters::Buffers, 0, currentBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList.SetShaderResourceView(RootParameters::Buffers, 1, m_HistoryBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList.SetShaderResourceView(RootParameters::Buffers, 2, velocityBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	const auto colorDesc = currentBuffer.GetD3D12ResourceDesc();
	RootParameters::Parameters parameters{};
	parameters.OneOverTexelSize = { 1.0f / static_cast<float>(colorDesc.Width), 1.0f / static_cast<float>(colorDesc.Height) };
	commandList.SetGraphics32BitConstants(RootParameters::ParametersCBuffer, parameters);

	m_BlitMesh->Draw(commandList);
}

void Taa::Resize(uint32_t width, uint32_t height)
{
	m_HistoryBuffer.Resize(width, height);
}

void Taa::CaptureHistory(CommandList& commandList, const Texture& currentBuffer)
{
	PIXScope(commandList, "Capture TAA History");
	commandList.CopyResource(m_HistoryBuffer, currentBuffer);
}
