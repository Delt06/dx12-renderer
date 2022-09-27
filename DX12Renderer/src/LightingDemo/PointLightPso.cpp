#include <LightingDemo/PointLightPso.h>

#include <DX12Library/CommandList.h>
#include <DX12Library/Helpers.h>
#include <Framework/Mesh.h>

#include <d3d12.h>

#include <Framework/Light.h>
#include <Framework/MatricesCb.h>
#include <DX12Library/ShaderUtils.h>

using namespace Microsoft::WRL;
using namespace DirectX;

namespace
{
	namespace RootParameters
	{
		enum RootParameters
		{
			MatricesCb,
			MaterialCb,
			NumRootParameters,
		};
	}

	struct MaterialCb
	{
		XMFLOAT4 m_Color;
	};
}

void PointLightPso::Set(CommandList& commandList) const
{
	commandList.SetPipelineState(m_PipelineState);
	commandList.SetGraphicsRootSignature(m_RootSignature);
}

void PointLightPso::Draw(CommandList& commandList, const PointLight& pointLight, const XMMATRIX viewMatrix,
	const XMMATRIX viewProjectionMatrix, const XMMATRIX projectionMatrix, const float scale) const
{
	XMMATRIX worldMatrix = XMMatrixTranslation(pointLight.PositionWs.x, pointLight.PositionWs.y,
		pointLight.PositionWs.z);
	worldMatrix = XMMatrixMultiply(XMMatrixScaling(scale, scale, scale), worldMatrix);
	MatricesCb matricesCb;
	matricesCb.Compute(worldMatrix, viewMatrix, viewProjectionMatrix, projectionMatrix);

	MaterialCb materialCb;
	materialCb.m_Color = pointLight.Color;

	commandList.SetGraphicsDynamicConstantBuffer(RootParameters::MatricesCb, matricesCb);
	commandList.SetGraphics32BitConstants(RootParameters::MaterialCb, sizeof(MaterialCb) / sizeof(float), &materialCb);

	m_Mesh->Draw(commandList);
}

PointLightPso::PointLightPso(const ComPtr<ID3D12Device2> device, CommandList& commandList)
{
	m_Mesh = Mesh::CreateSphere(commandList);

	ComPtr<ID3DBlob> vertexShaderBlob = ShaderUtils::LoadShaderFromFile(L"LightingDemo_PointLight_VS.cso");
	ComPtr<ID3DBlob> pixelShaderBlob = ShaderUtils::LoadShaderFromFile(L"LightingDemo_PointLight_PS.cso");

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

	CD3DX12_ROOT_PARAMETER1 rootParameters[RootParameters::NumRootParameters];
	rootParameters[RootParameters::MatricesCb].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
		D3D12_SHADER_VISIBILITY_VERTEX);
	rootParameters[RootParameters::MaterialCb].InitAsConstants(sizeof(MaterialCb) / sizeof(float), 0, 1,
		D3D12_SHADER_VISIBILITY_PIXEL);

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
	} pipelineStateStream;

	// sRGB formats provide free gamma correction!
	constexpr DXGI_FORMAT backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	constexpr DXGI_FORMAT depthBufferFormat = DXGI_FORMAT_D32_FLOAT;

	D3D12_RT_FORMAT_ARRAY rtvFormats = {};
	rtvFormats.NumRenderTargets = 1;
	rtvFormats.RTFormats[0] = backBufferFormat;

	pipelineStateStream.RootSignature = m_RootSignature.GetRootSignature().Get();
	pipelineStateStream.InputLayout = { VertexAttributes::INPUT_ELEMENTS, VertexAttributes::INPUT_ELEMENT_COUNT };
	pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipelineStateStream.Vs = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
	pipelineStateStream.Ps = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
	pipelineStateStream.DsvFormat = depthBufferFormat;
	pipelineStateStream.RtvFormats = rtvFormats;

	const D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
		sizeof(PipelineStateStream), &pipelineStateStream
	};

	ThrowIfFailed(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&m_PipelineState)));
}
