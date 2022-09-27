#include <LightingDemo/PointLightShadowPassPso.h>
#include <LightingDemo/ShadowPassPsoBase.h>

#include <CommandList.h>
#include <Light.h>
#include <Cubemap.h>

using namespace Microsoft::WRL;
using namespace DirectX;

PointLightShadowPassPso::PointLightShadowPassPso(const ComPtr<ID3D12Device2>& device,
	const UINT resolution)
	: ShadowPassPsoBase(device, resolution)
	, m_CubeShadowMapsCapacity(0)
	, m_CubeShadowMapsCount(0)
	, m_CurrentLightIndex(0)
	, m_CurrentCubeMapSideIndex(0)
{
	m_ShadowPassParameters.LightType = ShadowPassParameters::PointLight;
	SetShadowMapsCount(DEFAULT_CUBE_SHADOW_MAPS_COUNT);
}

void PointLightShadowPassPso::SetRenderTarget(CommandList& commandList) const
{
	commandList.SetRenderTarget(m_ShadowMapArray, GetCurrentSubresource());
}

void PointLightShadowPassPso::ClearShadowMap(CommandList& commandList) const
{
	commandList.ClearDepthStencilTexture(GetShadowMapsAsTexture(), D3D12_CLEAR_FLAG_DEPTH);
}

void PointLightShadowPassPso::SetShadowMapShaderResourceView(CommandList& commandList, uint32_t rootParameterIndex,
	uint32_t descriptorOffset) const
{
	constexpr auto stateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	const uint32_t numSubresources = m_CubeShadowMapsCount * Cubemap::SIDES_COUNT;
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
	srvDesc.Texture2DArray.ArraySize = numSubresources;
	srvDesc.Texture2DArray.MipLevels = 1;
	srvDesc.Texture2DArray.FirstArraySlice = 0;
	srvDesc.Texture2DArray.MostDetailedMip = 0;
	srvDesc.Texture2DArray.PlaneSlice = 0;
	srvDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;


	commandList.SetShaderResourceView(rootParameterIndex, descriptorOffset, GetShadowMapsAsTexture(), stateAfter, 0,
		numSubresources, &srvDesc);
}

void PointLightShadowPassPso::ComputePassParameters(const PointLight& pointLight)
{
	const XMVECTOR eyePosition = XMLoadFloat4(&pointLight.PositionWs);
	const auto& cubeSideOrientation = Cubemap::SIDE_ORIENTATIONS[m_CurrentCubeMapSideIndex];
	const auto viewMatrix = XMMatrixLookToLH(eyePosition, cubeSideOrientation.Forward, cubeSideOrientation.Up);

	float rangeMultiplier = 1.0f;
	rangeMultiplier = max(rangeMultiplier, pointLight.Color.x);
	rangeMultiplier = max(rangeMultiplier, pointLight.Color.y);
	rangeMultiplier = max(rangeMultiplier, pointLight.Color.z);
	const float range = pointLight.Range * rangeMultiplier * 2.0f;
	const auto projectionMatrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(90.0f), 1, 0.1f, range);
	const auto viewProjection = viewMatrix * projectionMatrix;

	m_ShadowPassParameters.LightDirectionWs = pointLight.PositionWs;
	m_ShadowPassParameters.ViewProjection = viewProjection;
}

void PointLightShadowPassPso::SetCurrentShadowMap(const uint32_t lightIndex, const uint32_t cubeMapSideIndex)
{
	if (lightIndex >= m_CubeShadowMapsCount)
		throw std::exception("Light index out of range");

	m_CurrentLightIndex = lightIndex;
	m_CurrentCubeMapSideIndex = cubeMapSideIndex;
}

void PointLightShadowPassPso::SetShadowMapsCount(const uint32_t count)
{
	if (count >= m_CubeShadowMapsCapacity && count > 0)
	{
		const uint32_t arraySize = count * Cubemap::SIDES_COUNT;
		const auto shadowMapDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			SHADOW_MAP_FORMAT,
			m_Resolution, m_Resolution,
			arraySize, 1,
			1, 0,
			D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
		D3D12_CLEAR_VALUE depthClearValue;
		depthClearValue.Format = shadowMapDesc.Format;
		depthClearValue.DepthStencil = { 1.0f, 0 };

		const auto shadowMap = Texture(shadowMapDesc, &depthClearValue,
			TextureUsageType::Depth,
			L"Point Light Shadow Map Array");
		m_ShadowMapArray.AttachTexture(DepthStencil, shadowMap);
		m_CubeShadowMapsCapacity = count;
	}

	m_CubeShadowMapsCount = count;
}

uint32_t PointLightShadowPassPso::GetCurrentSubresource() const
{
	return m_CurrentLightIndex * Cubemap::SIDES_COUNT + m_CurrentCubeMapSideIndex;
}

const Texture& PointLightShadowPassPso::GetShadowMapsAsTexture() const
{
	return m_ShadowMapArray.GetTexture(DepthStencil);
}
