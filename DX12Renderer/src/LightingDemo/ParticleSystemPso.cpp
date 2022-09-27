#include <LightingDemo/ParticleSystemPso.h>

#include <DX12Library/Helpers.h>
#include <d3d12.h>
#include <d3dx12.h>
#include <Mesh.h>
#include <DX12Library/Texture.h>

#include <MatricesCb.h>
#include <DX12Library/ShaderUtils.h>

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

	constexpr UINT INSTANCE_DATA_INPUT_SLOT = 1;

	constexpr D3D12_INPUT_ELEMENT_DESC INSTANCE_INPUT_ELEMENTS[] = {
		{
			"INSTANCE_PIVOT", 0, DXGI_FORMAT_R32G32B32_FLOAT, INSTANCE_DATA_INPUT_SLOT, D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1
		},
		{
			"INSTANCE_COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, INSTANCE_DATA_INPUT_SLOT, D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1
		},
		{
			"INSTANCE_SCALE", 0, DXGI_FORMAT_R32_FLOAT, INSTANCE_DATA_INPUT_SLOT, D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1
		},
	};

	constexpr size_t INSTANCE_INPUT_ELEMENT_COUNT = _countof(INSTANCE_INPUT_ELEMENTS);
}

ParticleSystemPso::ParticleSystemPso(const ComPtr<ID3D12Device2> device, CommandList& commandList)
{
	m_QuadMesh = Mesh::CreateVerticalQuad(commandList);

	m_Texture = std::make_shared<Texture>();
	commandList.LoadTextureFromFile(*m_Texture, L"Assets/Textures/particle.png", TextureUsageType::Albedo);

	ComPtr<ID3DBlob> vertexShaderBlob = ShaderUtils::LoadShaderFromFile(L"LightingDemo_ParticleSystem_VS.cso");
	ComPtr<ID3DBlob> pixelShaderBlob = ShaderUtils::LoadShaderFromFile(L"LightingDemo_ParticleSystem_PS.cso");

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

	constexpr size_t inputElementsCount = VertexAttributes::INPUT_ELEMENT_COUNT + INSTANCE_INPUT_ELEMENT_COUNT;
	D3D12_INPUT_ELEMENT_DESC inputElements[inputElementsCount];
	std::copy_n(VertexAttributes::INPUT_ELEMENTS, VertexAttributes::INPUT_ELEMENT_COUNT, inputElements);
	std::copy_n(INSTANCE_INPUT_ELEMENTS, INSTANCE_INPUT_ELEMENT_COUNT,
	            inputElements + VertexAttributes::INPUT_ELEMENT_COUNT);

	pipelineStateStream.RootSignature = m_RootSignature.GetRootSignature().Get();
	pipelineStateStream.InputLayout = {inputElements, inputElementsCount};
	pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipelineStateStream.Vs = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
	pipelineStateStream.Ps = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
	pipelineStateStream.DsvFormat = depthBufferFormat;
	pipelineStateStream.RtvFormats = rtvFormats;

	// alpha blending
	auto blendDesc = CD3DX12_BLEND_DESC(CD3DX12_DEFAULT());
	{
		auto& renderTargetBlendDesc = blendDesc.RenderTarget[0];
		renderTargetBlendDesc.BlendEnable = TRUE;
		renderTargetBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
		renderTargetBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
		renderTargetBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		renderTargetBlendDesc.SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
		renderTargetBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		renderTargetBlendDesc.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
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

void ParticleSystemPso::SetContext(CommandList& commandList) const
{
	commandList.SetPipelineState(m_PipelineState);
	commandList.SetGraphicsRootSignature(m_RootSignature);
	commandList.SetShaderResourceView(RootParameters::Textures, 0, *m_Texture,
	                                  D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void ParticleSystemPso::UploadInstanceData(CommandList& commandList,
                                           const ParticleInstanceData* instanceData, const uint32_t instancesCount)
{
	m_InstancesCount = instancesCount;
	commandList.CopyVertexBuffer(m_InstanceDataVertexBuffer, instancesCount, sizeof(ParticleInstanceData),
	                             instanceData);
}

void ParticleSystemPso::Draw(CommandList& commandList,
                             const DirectX::XMMATRIX viewMatrix, const DirectX::XMMATRIX viewProjectionMatrix,
                             const DirectX::XMMATRIX projectionMatrix) const
{
	MatricesCb matricesCb;
	matricesCb.Compute(DirectX::XMMatrixIdentity(), viewMatrix, viewProjectionMatrix, projectionMatrix);

	commandList.SetGraphicsDynamicConstantBuffer(RootParameters::MatricesCb, matricesCb);
	commandList.SetVertexBuffer(INSTANCE_DATA_INPUT_SLOT, m_InstanceDataVertexBuffer);

	m_QuadMesh->Draw(commandList, m_InstancesCount);
}
