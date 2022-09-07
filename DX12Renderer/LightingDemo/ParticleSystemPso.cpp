#include "ParticleSystemPso.h"

#include "Helpers.h"
#include <d3d12.h>
#include <d3dcompiler.h>
#include <d3dx12.h>
#include <Mesh.h>
#include <Texture.h>

#include "MatricesCb.h"

using namespace Microsoft::WRL;

namespace
{
	namespace RootParameters
	{
		enum RootParameters
		{
			MatricesCb = 0,
			Textures,
			NumRootParameters,
		};
	}
}

ParticleSystemPso::ParticleSystemPso(const ComPtr<ID3D12Device2> device, CommandList& commandList)
{
	m_QuadMesh = Mesh::CreateVerticalQuad(commandList);

	m_Texture = std::make_shared<Texture>();
	commandList.LoadTextureFromFile(*m_Texture, L"Assets/Textures/particle.png", TextureUsageType::Albedo);

	ComPtr<ID3DBlob> vertexShaderBlob;
	ThrowIfFailed(D3DReadFileToBlob(L"LightingDemo_ParticleSystem_VertexShader.cso", &vertexShaderBlob));

	ComPtr<ID3DBlob> pixelShaderBlob;
	ThrowIfFailed(D3DReadFileToBlob(L"LightingDemo_ParticleSystem_PixelShader.cso", &pixelShaderBlob));

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

	const CD3DX12_DESCRIPTOR_RANGE1 descriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_ROOT_PARAMETER1 rootParameters[RootParameters::NumRootParameters];
	rootParameters[RootParameters::MatricesCb].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
	                                                                    D3D12_SHADER_VISIBILITY_VERTEX);
	rootParameters[RootParameters::Textures].InitAsDescriptorTable(1, &descriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);

	CD3DX12_STATIC_SAMPLER_DESC linearRepeatSampler(0, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR);

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
	rootSignatureDescription.Init_1_1(RootParameters::NumRootParameters, rootParameters, 1, &linearRepeatSampler,
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
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DsvFormat;
		CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RtvFormats;
		CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC Blend;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL DepthStencil;
	} pipelineStateStream;

	// sRGB formats provide free gamma correction!
	constexpr DXGI_FORMAT backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	constexpr DXGI_FORMAT depthBufferFormat = DXGI_FORMAT_D32_FLOAT;

	D3D12_RT_FORMAT_ARRAY rtvFormats = {};
	rtvFormats.NumRenderTargets = 1;
	rtvFormats.RTFormats[0] = backBufferFormat;

	pipelineStateStream.RootSignature = m_RootSignature.GetRootSignature().Get();
	pipelineStateStream.InputLayout = {VertexAttributes::INPUT_ELEMENTS, VertexAttributes::INPUT_ELEMENT_COUNT};
	pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipelineStateStream.Vs = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
	pipelineStateStream.Ps = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
	pipelineStateStream.DsvFormat = depthBufferFormat;
	pipelineStateStream.RtvFormats = rtvFormats;

	// additive blending
	auto blendDesc = CD3DX12_BLEND_DESC(CD3DX12_DEFAULT());
	{
		auto& renderTargetBlendDesc = blendDesc.RenderTarget[0];
		renderTargetBlendDesc.BlendEnable = TRUE;
		renderTargetBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
		renderTargetBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
		renderTargetBlendDesc.SrcBlend = D3D12_BLEND_ONE;
		renderTargetBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
		renderTargetBlendDesc.DestBlend = D3D12_BLEND_ONE;
		renderTargetBlendDesc.DestBlendAlpha = D3D12_BLEND_ONE;
	}
	pipelineStateStream.Blend = blendDesc;

	// disable depth write, keep depth check
	auto depthStencilDesc = CD3DX12_DEPTH_STENCIL_DESC(CD3DX12_DEFAULT());
	{
		depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	}
	pipelineStateStream.DepthStencil = depthStencilDesc;

	const D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
		sizeof(PipelineStateStream), &pipelineStateStream
	};

	ThrowIfFailed(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&m_PipelineState)));
}

void ParticleSystemPso::Set(CommandList& commandList) const
{
	commandList.SetPipelineState(m_PipelineState);
	commandList.SetGraphicsRootSignature(m_RootSignature);
	commandList.SetShaderResourceView(RootParameters::Textures, 0, *m_Texture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void ParticleSystemPso::Draw(CommandList& commandList, const DirectX::XMMATRIX worldMatrix,
                             const DirectX::XMMATRIX viewMatrix, const DirectX::XMMATRIX viewProjectionMatrix,
                             const DirectX::XMMATRIX projectionMatrix) const
{
	MatricesCb matricesCb;
	matricesCb.Compute(worldMatrix, viewMatrix, viewProjectionMatrix, projectionMatrix);

	commandList.SetGraphicsDynamicConstantBuffer(RootParameters::MatricesCb, matricesCb);

	m_QuadMesh->Draw(commandList);
}
