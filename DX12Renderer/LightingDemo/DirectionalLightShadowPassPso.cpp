#include "DirectionalLightShadowPassPso.h"

#include <d3d12.h>
#include <d3dcompiler.h>
#include <d3dx12.h>

#include "Helpers.h"
#include "MatricesCb.h"
#include "Mesh.h"
#include "Texture.h"

#include <DirectXMath.h>

#include "Camera.h"
#include "GameObject.h"
#include "Light.h"
#include "Model.h"

using namespace Microsoft::WRL;
using namespace DirectX;

namespace
{
	namespace RootParameters
	{
		enum RootParameters
		{
			ShadowPassParametersCb,
			NumRootParameters,
		};
	}
}

DirectionalLightShadowPassPso::DirectionalLightShadowPassPso(ComPtr<ID3D12Device2> device,
                                                             CommandList& commandList, UINT resolution)

	: m_Resolution(resolution)
	  , m_Viewport(CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(m_Resolution), static_cast<float>(m_Resolution)))
	  , m_ScissorRect(CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX))
	  , m_ShadowPassParameters{}
{
	ComPtr<ID3DBlob> vertexShaderBlob;
	ThrowIfFailed(D3DReadFileToBlob(L"LightingDemo_ShadowCaster_VertexShader.cso", &vertexShaderBlob));

	ComPtr<ID3DBlob> pixelShaderBlob;
	ThrowIfFailed(D3DReadFileToBlob(L"LightingDemo_ShadowCaster_PixelShader.cso", &pixelShaderBlob));

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
		D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

	CD3DX12_ROOT_PARAMETER1 rootParameters[RootParameters::NumRootParameters];
	rootParameters[RootParameters::ShadowPassParametersCb].InitAsConstantBufferView(
		0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
		D3D12_SHADER_VISIBILITY_VERTEX);

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
	rootSignatureDescription.Init_1_1(RootParameters::NumRootParameters, rootParameters, 0, nullptr,
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
		CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER Rasterizer;
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

	// ColorMask 0
	auto blendDesc = CD3DX12_BLEND_DESC(CD3DX12_DEFAULT());
	{
		auto& renderTargetBlendDesc = blendDesc.RenderTarget[0];
		renderTargetBlendDesc.RenderTargetWriteMask = 0;
	}
	pipelineStateStream.Blend = blendDesc;

	auto depthStencilDesc = CD3DX12_DEPTH_STENCIL_DESC(CD3DX12_DEFAULT());
	pipelineStateStream.DepthStencil = depthStencilDesc;

	auto rasterizerDesc = CD3DX12_RASTERIZER_DESC(CD3DX12_DEFAULT());
	//rasterizerDesc.SlopeScaledDepthBias = 1.0f; // TODO: check the effect
	pipelineStateStream.Rasterizer = rasterizerDesc;

	const D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
		sizeof(PipelineStateStream), &pipelineStateStream
	};

	ThrowIfFailed(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&m_PipelineState)));

	auto depthDesc = CD3DX12_RESOURCE_DESC::Tex2D(depthBufferFormat,
	                                              m_Resolution, m_Resolution,
	                                              1, 1,
	                                              1, 0,
	                                              D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
	D3D12_CLEAR_VALUE depthClearValue;
	depthClearValue.Format = depthDesc.Format;
	depthClearValue.DepthStencil = {1.0f, 0};

	auto depthTexture = Texture(depthDesc, &depthClearValue,
	                            TextureUsageType::Depth,
	                            L"Shadow Map");
	m_ShadowMap.AttachTexture(DepthStencil, depthTexture);
}

void DirectionalLightShadowPassPso::SetContext(CommandList& commandList) const
{
	commandList.SetViewport(m_Viewport);
	commandList.SetScissorRect(m_ScissorRect);

	commandList.SetRenderTarget(m_ShadowMap);

	commandList.SetPipelineState(m_PipelineState);
	commandList.SetGraphicsRootSignature(m_RootSignature);
}

void DirectionalLightShadowPassPso::ComputePassParameters(const Camera& camera,
                                                          const DirectionalLight& directionalLight)
{
	const XMVECTOR lightDirection = XMLoadFloat4(&directionalLight.m_DirectionWs);
	const auto viewMatrix = XMMatrixLookToLH(lightDirection * 20.0f, -lightDirection,
	                                         XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
	const auto projectionMatrix = XMMatrixOrthographicOffCenterLH(-100.0f, 100.0f, -100.0f, 100.0f, -10.0f, 50.0f);
	const auto viewProjection = viewMatrix * projectionMatrix;


	m_ShadowPassParameters.LightDirectionWs = directionalLight.m_DirectionWs;
	m_ShadowPassParameters.ViewProjection = viewProjection;
}

void DirectionalLightShadowPassPso::SetBias(const float depthBias, const float normalBias)
{
	m_ShadowPassParameters.Bias.x = -depthBias;
	m_ShadowPassParameters.Bias.y = -normalBias;
}

void DirectionalLightShadowPassPso::ClearShadowMap(CommandList& commandList) const
{
	commandList.ClearDepthStencilTexture(GetShadowMapAsTexture(), D3D12_CLEAR_FLAG_DEPTH);
}

void DirectionalLightShadowPassPso::DrawToShadowMap(CommandList& commandList, const GameObject& gameObject) const
{
	ShadowPassParameters parameters = m_ShadowPassParameters;
	const auto worldMatrix = gameObject.GetWorldMatrix();
	parameters.InverseTransposeModel = XMMatrixTranspose(XMMatrixInverse(nullptr, worldMatrix));
	parameters.Model = worldMatrix;

	commandList.SetGraphicsDynamicConstantBuffer(RootParameters::ShadowPassParametersCb, parameters);

	for (auto& mesh : gameObject.GetModel()->GetMeshes())
	{
		mesh->Draw(commandList);
	}
}

void DirectionalLightShadowPassPso::SetShadowMapShaderResourceView(CommandList& commandList,
                                                                   const uint32_t rootParameterIndex,
                                                                   const uint32_t descriptorOffset) const
{
	constexpr auto stateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.PlaneSlice = 0;
	commandList.SetShaderResourceView(rootParameterIndex, descriptorOffset, GetShadowMapAsTexture(), stateAfter, 0, 1,
	                                  &srvDesc);
}

XMMATRIX DirectionalLightShadowPassPso::ComputeShadowModelViewProjectionMatrix(
	const XMMATRIX worldMatrix) const
{
	return worldMatrix * m_ShadowPassParameters.ViewProjection;
}

XMMATRIX DirectionalLightShadowPassPso::GetShadowViewProjectionMatrix() const
{
	return m_ShadowPassParameters.ViewProjection;
}

const Texture& DirectionalLightShadowPassPso::GetShadowMapAsTexture() const
{
	return m_ShadowMap.GetTexture(DepthStencil);
}
