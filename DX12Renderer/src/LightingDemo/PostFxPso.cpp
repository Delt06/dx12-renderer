#include <DirectXMath.h>
#include <d3dcompiler.h>

#include <LightingDemo/PostFxPso.h>
#include <Mesh.h>
#include <CommandList.h>
#include <Helpers.h>
#include <d3dx12.h>
#include <Texture.h>
#include <Camera.h>
#include <ShaderUtils.h>

using namespace DirectX;
using namespace Microsoft::WRL;

namespace
{
	namespace RootParameters
	{
		enum
		{
			PostFxParameters = 0, // b0
			SourceTexture, // t0-t1
			NumRootParameters
		};
	}
}

PostFxPso::PostFxPso(ComPtr<ID3D12Device2> device, CommandList& commandList, DXGI_FORMAT backBufferFormat)
	: m_BackBufferFormat(backBufferFormat)
{
	m_BlitTriangle = Mesh::CreateBlitTriangle(commandList);

	// Setup RS and PSO
	{
		ComPtr<ID3DBlob> vertexShaderBlob = ShaderUtils::LoadShaderFromFile(L"LightingDemo_Blit_VS.cso");
		ComPtr<ID3DBlob> pixelShaderBlob = ShaderUtils::LoadShaderFromFile(L"LightingDemo_PostFX_PS.cso");

		// Create a root signature.
		D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData;
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
		if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
		{
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
		}

		constexpr D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

		CD3DX12_DESCRIPTOR_RANGE1 sourceRenderTargetDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0);

		CD3DX12_ROOT_PARAMETER1 rootParameters[RootParameters::NumRootParameters];
		rootParameters[RootParameters::PostFxParameters].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[RootParameters::SourceTexture].InitAsDescriptorTable(1, &sourceRenderTargetDescriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);

		CD3DX12_STATIC_SAMPLER_DESC samplers[] = {
			// point clamp
			CD3DX12_STATIC_SAMPLER_DESC(0, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP),
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
			CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER Rasterizer;
			CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL DepthStencil;
		} pipelineStateStream;

		D3D12_RT_FORMAT_ARRAY rtvFormats = {};
		rtvFormats.NumRenderTargets = 1;
		rtvFormats.RTFormats[0] = backBufferFormat;

		pipelineStateStream.RootSignature = m_RootSignature.GetRootSignature().Get();
		pipelineStateStream.InputLayout = { VertexAttributes::INPUT_ELEMENTS, VertexAttributes::INPUT_ELEMENT_COUNT };
		pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pipelineStateStream.Vs = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
		pipelineStateStream.Ps = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
		pipelineStateStream.RtvFormats = rtvFormats;
		pipelineStateStream.Rasterizer = CD3DX12_RASTERIZER_DESC(D3D12_FILL_MODE_SOLID, D3D12_CULL_MODE_BACK, FALSE, 0, 0,
			0, TRUE, FALSE, FALSE, 0,
			D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF);

		auto depthStencilDesc = CD3DX12_DEPTH_STENCIL_DESC(CD3DX12_DEFAULT());
		{
			depthStencilDesc.DepthEnable = false;
		}
		pipelineStateStream.DepthStencil = depthStencilDesc;

		const D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
			sizeof(PipelineStateStream), &pipelineStateStream
		};

		ThrowIfFailed(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&m_PipelineState)));
	}
}

void PostFxPso::SetSourceColorTexture(CommandList& commandList, Texture texture)
{
	constexpr auto stateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.PlaneSlice = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	commandList.SetShaderResourceView(RootParameters::SourceTexture, 0, texture, stateAfter, 0,
		1, &srvDesc);
}

void PostFxPso::SetSourceDepthTexture(CommandList& commandList, Texture texture)
{
	constexpr auto stateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.PlaneSlice = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	commandList.SetShaderResourceView(RootParameters::SourceTexture, 1, texture, stateAfter, 0,
		1, &srvDesc);
}

void PostFxPso::SetParameters(CommandList& commandList, const PostFxParameters& parameters)
{
	commandList.SetGraphicsDynamicConstantBuffer(RootParameters::PostFxParameters, parameters);
}

void PostFxPso::SetContext(CommandList& commandList)
{
	commandList.SetGraphicsRootSignature(m_RootSignature);
	commandList.SetPipelineState(m_PipelineState);
}

void PostFxPso::Blit(CommandList& commandList)
{
	m_BlitTriangle->Draw(commandList);
}
