#include <PBR/DiffuseIrradiance.h>

#include <Mesh.h>
#include <DX12Library/CommandList.h>
#include <DX12Library/Helpers.h>
#include <DX12Library/RenderTarget.h>
#include <d3dx12.h>
#include <DX12Library/Texture.h>
#include <DX12Library/Cubemap.h>
#include <DX12Library/ShaderUtils.h>

using namespace Microsoft::WRL;
using namespace DirectX;

namespace
{
	namespace RootParameters
	{
		enum RootParameters
		{
			// Texture2D : register(t0)
			Source,
			// ConstantBuffer : register(b0)
			ParametersCB,
			NumRootParameters
		};
	}

	struct Parameters
	{
		XMFLOAT4 Forward;
		XMFLOAT4 Up;
	};
}

DiffuseIrradiance::DiffuseIrradiance(Microsoft::WRL::ComPtr<ID3D12Device2> device, CommandList& commandList, DXGI_FORMAT renderTargetFormat)
	: m_BlitMesh(Mesh::CreateBlitTriangle(commandList))
	, m_ScissorRect(CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX))
{
	// create root signature and PSO
	{
		ComPtr<ID3DBlob> vertexShaderBlob = ShaderUtils::LoadShaderFromFile(L"IBL_CubeMapSideBlit_VS.cso");
		ComPtr<ID3DBlob> pixelShaderBlob = ShaderUtils::LoadShaderFromFile(L"IBL_DiffuseIrradiance_PS.cso");

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
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

		CD3DX12_DESCRIPTOR_RANGE1 sourceDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		CD3DX12_ROOT_PARAMETER1 rootParameters[RootParameters::NumRootParameters];
		rootParameters[RootParameters::Source].InitAsDescriptorTable(1, &sourceDescriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[RootParameters::ParametersCB].InitAsConstants(sizeof(Parameters) / sizeof(float), 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);

		CD3DX12_STATIC_SAMPLER_DESC samplers[] = {
			// linear clamp
			CD3DX12_STATIC_SAMPLER_DESC(0, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP),
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
		pipelineStateStream.Vs = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
		pipelineStateStream.Ps = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
		pipelineStateStream.RtvFormats = rtvFormats;

		const D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
			sizeof(PipelineStateStream), &pipelineStateStream
		};

		ThrowIfFailed(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&m_PipelineState)));
	}
}

void DiffuseIrradiance::SetContext(CommandList& commandList)
{
	commandList.SetGraphicsRootSignature(m_RootSignature);
	commandList.SetPipelineState(m_PipelineState);
	commandList.SetScissorRect(m_ScissorRect);
}

void DiffuseIrradiance::SetSourceCubemap(CommandList& commandList, Texture& texture)
{
	const auto sourceDesc = texture.GetD3D12ResourceDesc();
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = sourceDesc.Format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MipLevels = 1;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	commandList.SetShaderResourceView(RootParameters::Source, 0, texture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, UINT_MAX, &srvDesc);
}

void DiffuseIrradiance::SetRenderTarget(CommandList& commandList, RenderTarget& renderTarget, UINT texArrayIndex /*= -1*/)
{
	const auto rtColorDesc = renderTarget.GetTexture(Color0).GetD3D12ResourceDesc();
	const auto width = static_cast<float>(rtColorDesc.Width);
	const auto height = static_cast<float>(rtColorDesc.Height);

	auto viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, width, height);
	commandList.SetViewport(viewport);
	commandList.SetRenderTarget(renderTarget, texArrayIndex);
}

void DiffuseIrradiance::Draw(CommandList& commandList, uint32_t cubemapSideIndex)
{
	const auto orientation = Cubemap::SIDE_ORIENTATIONS[cubemapSideIndex];
	Parameters parameters;
	XMStoreFloat4(&parameters.Forward, orientation.Forward);
	XMStoreFloat4(&parameters.Up, orientation.Up);
	commandList.SetGraphics32BitConstants(RootParameters::ParametersCB, parameters);
	m_BlitMesh->Draw(commandList);
}
