#include "SceneRenderer.h"
#include <memory>
#include <Helpers.h>
#include <d3dcompiler.h>
#include <Model.h>
#include <Mesh.h>
#include <MatricesCb.h>
#include <Cubemap.h>

using namespace DirectX;
using namespace Microsoft::WRL;

namespace
{
	struct LightPropertiesCb
	{
		uint32_t NumPointLights;
		uint32_t NumSpotLights;
	};

	struct ShadowReceiverParametersCb
	{
		XMMATRIX ViewProjection;
		float PoissonSpreadInv;
		float PointLightPoissonSpreadInv;
		float SpotLightPoissonSpreadInv;
		float Padding;
	};

	namespace RootParameters
	{
		// An enum for root signature parameters.
		// I'm not using scoped enums to avoid the explicit cast that would be required
		// to use these as root indices in the root signature.
		enum RootParameters
		{
			// ConstantBuffer register(b0);
			MatricesCb,
			// ConstantBuffer register(b0, space1);
			MaterialCb,
			// ConstantBuffer register(b1, space1);
			DirLightCb,
			// ConstantBuffer register(b2);
			LightPropertiesCb,
			// ConstantBuffer register(b1);
			ShadowMatricesCb,
			// Texture2D register(t5);
			ShadowMaps,
			// Texture2D register(t6);
			PointLightShadowMaps,
			// StructuredBuffer<XMMATRIX> register(t7);
			PointLightMatrices,
			// Texture2D register(t0-t3);
			Textures,
			// StructuredBuffer PointLights : register(t4);
			PointLights,
			// StructuredBuffer SpotLights : register(t8);
			SpotLights,
			// Texture2DArray : register(t9)
			SpotLightShadowMaps,
			// StructuredBuffer<XMMATRIX> register(t10);
			SpotLightMatrices,
			// Texture2DCube : register(t11)
			EnvironmentReflectionsCubemap,
			NumRootParameters
		};
	}
}

SceneRenderer::SceneRenderer(Microsoft::WRL::ComPtr<ID3D12Device2> device, CommandList& commandList, const GraphicsSettings& graphicsSettings, DXGI_FORMAT backBufferFormat, DXGI_FORMAT depthBufferFormat)
	: m_GraphicsSettings(graphicsSettings)
{
	// Setup RS and PSO
	{
		ComPtr<ID3DBlob> vertexShaderBlob;
		ThrowIfFailed(D3DReadFileToBlob(L"LightingDemo_VertexShader.cso", &vertexShaderBlob));

		ComPtr<ID3DBlob> pixelShaderBlob;
		ThrowIfFailed(D3DReadFileToBlob(L"LightingDemo_PixelShader.cso", &pixelShaderBlob));

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

		CD3DX12_DESCRIPTOR_RANGE1 descriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, ModelMaps::TotalNumber, 0);
		CD3DX12_DESCRIPTOR_RANGE1 descriptorRangeShadowMaps(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, ModelMaps::TotalNumber + 1);
		CD3DX12_DESCRIPTOR_RANGE1 descriptorRangePointLightShadowMaps(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1,
			ModelMaps::TotalNumber + 2);
		CD3DX12_DESCRIPTOR_RANGE1 descriptorRangeSpotLightShadowMaps(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1,
			ModelMaps::TotalNumber + 5);
		CD3DX12_DESCRIPTOR_RANGE1 environmentReflectionsCubemap(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1,
			ModelMaps::TotalNumber + 7);

		CD3DX12_ROOT_PARAMETER1 rootParameters[RootParameters::NumRootParameters];
		rootParameters[RootParameters::MatricesCb].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
			D3D12_SHADER_VISIBILITY_VERTEX);
		rootParameters[RootParameters::MaterialCb].InitAsConstantBufferView(0, 1, D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
			D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[RootParameters::DirLightCb].InitAsConstantBufferView(1, 1, D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
			D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[RootParameters::LightPropertiesCb].InitAsConstants(sizeof(LightPropertiesCb) / sizeof(float), 2, 0,
			D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[RootParameters::ShadowMatricesCb].InitAsConstantBufferView(3, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
			D3D12_SHADER_VISIBILITY_ALL);
		rootParameters[RootParameters::ShadowMaps].InitAsDescriptorTable(1, &descriptorRangeShadowMaps,
			D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[RootParameters::PointLightShadowMaps].InitAsDescriptorTable(1, &descriptorRangePointLightShadowMaps,
			D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[RootParameters::PointLightMatrices].InitAsShaderResourceView(
			ModelMaps::TotalNumber + 3, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
			D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[RootParameters::Textures].InitAsDescriptorTable(1, &descriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[RootParameters::PointLights].InitAsShaderResourceView(
			ModelMaps::TotalNumber, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[RootParameters::SpotLights].InitAsShaderResourceView(
			ModelMaps::TotalNumber + 4, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
			D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[RootParameters::SpotLightShadowMaps].InitAsDescriptorTable(1, &descriptorRangeSpotLightShadowMaps,
			D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[RootParameters::SpotLightMatrices].InitAsShaderResourceView(
			ModelMaps::TotalNumber + 6, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[RootParameters::EnvironmentReflectionsCubemap].InitAsDescriptorTable(
			1, &environmentReflectionsCubemap, D3D12_SHADER_VISIBILITY_PIXEL);

		CD3DX12_STATIC_SAMPLER_DESC samplers[] = {
			// linear repeat
			CD3DX12_STATIC_SAMPLER_DESC(0, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR),
			// shadow map
			CD3DX12_STATIC_SAMPLER_DESC(1, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
										D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 0, 16,
										D3D12_COMPARISON_FUNC_LESS_EQUAL),
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
			CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DsvFormat;
			CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RtvFormats;
			CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER Rasterizer;
		} pipelineStateStream;

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
		pipelineStateStream.Rasterizer = CD3DX12_RASTERIZER_DESC(D3D12_FILL_MODE_SOLID, D3D12_CULL_MODE_BACK, FALSE, 0, 0,
			0, TRUE, FALSE, FALSE, 0,
			D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF);

		const D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
			sizeof(PipelineStateStream), &pipelineStateStream
		};

		ThrowIfFailed(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&m_PipelineState)));
	}

	m_PointLightPso = std::make_unique<PointLightPso>(device, commandList);

	// Setup shadows
	{
		// Directional Light
		const auto& directionalLightShadowsQuality = graphicsSettings.DirectionalLightShadows;
		m_DirectionalLightShadowPassPso = std::make_unique<DirectionalLightShadowPassPso>(
			device, directionalLightShadowsQuality.m_Resolution);
		m_DirectionalLightShadowPassPso->SetBias(directionalLightShadowsQuality.m_DepthBias,
			directionalLightShadowsQuality.m_NormalBias);

		
		// Point Lights
		const auto& pointLightShadowsQuality = graphicsSettings.PointLightShadows;
		m_PointLightShadowPassPso = std::make_unique<PointLightShadowPassPso>(
			device, pointLightShadowsQuality.m_Resolution);
		m_PointLightShadowPassPso->SetBias(pointLightShadowsQuality.m_DepthBias,
			pointLightShadowsQuality.m_NormalBias);

		// Spot Lights
		const auto& spotLightShadowsQuality = graphicsSettings.SpotLightShadows;
		m_SpotLightShadowPassPso = std::make_unique<SpotLightShadowPassPso>(
			device, spotLightShadowsQuality.m_Resolution);
		m_SpotLightShadowPassPso->SetBias(spotLightShadowsQuality.m_DepthBias,
			spotLightShadowsQuality.m_NormalBias);
	}
}

void SceneRenderer::ResetShadowMatrices()
{
	m_DirectionalLightShadowMatrix = XMMatrixIdentity();
	m_PointLightShadowMatrices.clear();
	m_SpotLightShadowMatrices.clear();
}

void SceneRenderer::SetScene(const std::shared_ptr<Scene> scene)
{
	m_Scene = scene;
}

void SceneRenderer::SetEnvironmentReflectionsCubemap(const std::shared_ptr<Cubemap> cubemap)
{
	m_EnvironmentReflectionsCubemap = cubemap;
}

void SceneRenderer::ToggleEnvironmentReflections(bool enabled)
{
	m_AreEnvironmentReflectionsEnabled = enabled;
}

void SceneRenderer::SetMatrices(DirectX::XMMATRIX viewMatrix, DirectX::XMMATRIX projectionMatrix)
{
	m_ViewMatrix = viewMatrix;
	m_ProjectionMatrix = projectionMatrix;
}

void SceneRenderer::SetEnvironmentReflectionsCubemapSrv(CommandList& commandList, Cubemap& cubemap)
{
	cubemap.SetShaderResourceView(commandList, RootParameters::EnvironmentReflectionsCubemap);
}

void SceneRenderer::SetEmptyEnvironmentReflectionsCubemapSrv(CommandList& commandList, Cubemap& cubemap)
{
	cubemap.SetEmptyShaderResourceView(commandList, RootParameters::EnvironmentReflectionsCubemap);
}

void SceneRenderer::ShadowPass(CommandList& commandList)
{
	PIXScope(commandList, "Shadow Pass");
	ResetShadowMatrices();

	{
		PIXScope(commandList, "Directional Light Shadows");

		m_DirectionalLightShadowPassPso->SetContext(commandList);
		m_DirectionalLightShadowPassPso->ClearShadowMap(commandList);
		m_DirectionalLightShadowPassPso->ComputePassParameters(*m_Scene, m_Scene->MainDirectionalLight);
		m_DirectionalLightShadowPassPso->SetRenderTarget(commandList);

		for (auto& gameObject : m_Scene->GameObjects)
		{
			m_DirectionalLightShadowPassPso->DrawToShadowMap(commandList, gameObject);
		}
	}

	{
		PIXScope(commandList, "Point Light Shadows");

		const auto pointLightsCount = static_cast<uint32_t>(m_Scene->PointLights.size());
		m_PointLightShadowPassPso->SetShadowMapsCount(pointLightsCount);
		m_PointLightShadowPassPso->ClearShadowMap(commandList);
		m_PointLightShadowPassPso->SetContext(commandList);

		for (uint32_t lightIndex = 0; lightIndex < pointLightsCount; ++lightIndex)
		{
			const PointLight& pointLight = m_Scene->PointLights[lightIndex];

			for (uint32_t cubeMapSideIndex = 0; cubeMapSideIndex < Cubemap::SIDES_COUNT; ++
				cubeMapSideIndex)
			{
				m_PointLightShadowPassPso->SetCurrentShadowMap(lightIndex, cubeMapSideIndex);
				m_PointLightShadowPassPso->ComputePassParameters(pointLight);
				m_PointLightShadowPassPso->SetRenderTarget(commandList);

				for (auto& gameObject : m_Scene->GameObjects)
				{
					m_PointLightShadowPassPso->DrawToShadowMap(commandList, gameObject);
				}

				m_PointLightShadowMatrices.push_back(m_PointLightShadowPassPso->GetShadowViewProjectionMatrix());
			}
		}
	}
	{
		PIXScope(commandList, "Spot Light Shadows");

		const auto spotLightsCount = static_cast<uint32_t>(m_Scene->SpotLights.size());
		m_SpotLightShadowPassPso->SetShadowMapsCount(spotLightsCount);
		m_SpotLightShadowPassPso->ClearShadowMap(commandList);
		m_SpotLightShadowPassPso->SetContext(commandList);

		for (uint32_t lightIndex = 0; lightIndex < spotLightsCount; ++lightIndex)
		{
			const SpotLight& spotLight = m_Scene->SpotLights[lightIndex];

			m_SpotLightShadowPassPso->SetCurrentShadowMap(lightIndex);
			m_SpotLightShadowPassPso->ComputePassParameters(spotLight);
			m_SpotLightShadowPassPso->SetRenderTarget(commandList);

			for (auto& gameObject : m_Scene->GameObjects)
			{
				m_SpotLightShadowPassPso->DrawToShadowMap(commandList, gameObject);
			}

			m_SpotLightShadowMatrices.push_back(m_SpotLightShadowPassPso->GetShadowViewProjectionMatrix());
		}
	}
}

void SceneRenderer::MainPass(CommandList& commandList)
{
	const XMMATRIX viewMatrix = m_ViewMatrix;
	const XMMATRIX projectionMatrix = m_ProjectionMatrix;
	const XMMATRIX viewProjectionMatrix = viewMatrix * projectionMatrix;

	{
		PIXScope(commandList, "Draw Point Lights");

		m_PointLightPso->Set(commandList);

		for (const auto& pointLight : m_Scene->PointLights)
		{
			m_PointLightPso->Draw(commandList, pointLight, viewMatrix, viewProjectionMatrix, projectionMatrix,
				1.0f);
		}
	}

	{
		PIXScope(commandList, "Opaque Pass");

		commandList.SetPipelineState(m_PipelineState);
		commandList.SetGraphicsRootSignature(m_RootSignature);

		if (m_AreEnvironmentReflectionsEnabled)
			SetEnvironmentReflectionsCubemapSrv(commandList, *m_EnvironmentReflectionsCubemap);
		else
			SetEmptyEnvironmentReflectionsCubemapSrv(commandList, *m_EnvironmentReflectionsCubemap);

		// Update directional light cbuffer
		{
			commandList.SetGraphicsDynamicConstantBuffer(RootParameters::DirLightCb, m_Scene->MainDirectionalLight);
		}

		// Update other light buffers
		{
			LightPropertiesCb lightPropertiesCb;
			lightPropertiesCb.NumPointLights = static_cast<uint32_t>(m_Scene->PointLights.size());
			lightPropertiesCb.NumSpotLights = static_cast<uint32_t>(m_Scene->SpotLights.size());

			commandList.SetGraphics32BitConstants(RootParameters::LightPropertiesCb, lightPropertiesCb);
			commandList.SetGraphicsDynamicStructuredBuffer(RootParameters::PointLights, m_Scene->PointLights);
			commandList.SetGraphicsDynamicStructuredBuffer(RootParameters::SpotLights, m_Scene->SpotLights);
		}

		// Bind shadow parameters
		{
			m_DirectionalLightShadowPassPso->SetShadowMapShaderResourceView(
				commandList, RootParameters::ShadowMaps);

			m_PointLightShadowPassPso->SetShadowMapShaderResourceView(
				commandList, RootParameters::PointLightShadowMaps);

			ShadowReceiverParametersCb shadowReceiverParametersCb;
			shadowReceiverParametersCb.ViewProjection = m_DirectionalLightShadowPassPso->
				GetShadowViewProjectionMatrix();
			shadowReceiverParametersCb.PoissonSpreadInv = 1.0f / m_GraphicsSettings.DirectionalLightShadows.
				m_PoissonSpread;
			shadowReceiverParametersCb.PointLightPoissonSpreadInv = 1.0f / m_GraphicsSettings.PointLightShadows.
				m_PoissonSpread;
			shadowReceiverParametersCb.SpotLightPoissonSpreadInv = 1.0f / m_GraphicsSettings.PointLightShadows.
				m_PoissonSpread;
			commandList.SetGraphicsDynamicConstantBuffer(RootParameters::ShadowMatricesCb,
				shadowReceiverParametersCb);

			commandList.SetGraphicsDynamicStructuredBuffer(RootParameters::PointLightMatrices, m_PointLightShadowMatrices);

			m_SpotLightShadowPassPso->SetShadowMapShaderResourceView(
				commandList, RootParameters::SpotLightShadowMaps);

			commandList.SetGraphicsDynamicStructuredBuffer(RootParameters::SpotLightMatrices, m_SpotLightShadowMatrices);
		}

		for (const auto& go : m_Scene->GameObjects)
		{
			go.Draw([this, &viewMatrix, &viewProjectionMatrix, &projectionMatrix](auto& cmd, auto worldMatrix)
				{
					MatricesCb matrices;
					matrices.Compute(worldMatrix, viewMatrix, viewProjectionMatrix, projectionMatrix);
					cmd.SetGraphicsDynamicConstantBuffer(RootParameters::MatricesCb, matrices);
				},
				commandList, RootParameters::MaterialCb, RootParameters::Textures);
		}
	}

	{
		PIXScope(commandList, "Particle Systems");

		for (auto& ps : m_Scene->ParticleSystems)
		{
			ps.Draw(commandList, viewMatrix, viewProjectionMatrix, projectionMatrix);
		}
	}
}
