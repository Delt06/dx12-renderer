#include <LightingDemo/DirectionalLightShadowPassPso.h>

#include <d3d12.h>
#include <d3dcompiler.h>
#include <d3dx12.h>

#include <DX12Library/Helpers.h>
#include <Framework/Mesh.h>
#include <DX12Library/Texture.h>

#include <DirectXMath.h>

#include <Framework/Light.h>
#include <Framework/Model.h>

using namespace Microsoft::WRL;
using namespace DirectX;

DirectionalLightShadowPassPso::DirectionalLightShadowPassPso(const ComPtr<ID3D12Device2> device,
                                                             const UINT resolution) :
	ShadowPassPsoBase(device, resolution)
{
	m_ShadowPassParameters.LightType = ShadowPassParameters::DirectionalLight;

	const auto shadowMapDesc = CD3DX12_RESOURCE_DESC::Tex2D(SHADOW_MAP_FORMAT,
	                                                        m_Resolution, m_Resolution,
	                                                        1, 1,
	                                                        1, 0,
	                                                        D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
	D3D12_CLEAR_VALUE depthClearValue;
	depthClearValue.Format = shadowMapDesc.Format;
	depthClearValue.DepthStencil = {1.0f, 0};

	const auto shadowMap = Texture(shadowMapDesc, &depthClearValue,
	                               TextureUsageType::Depth,
	                               L"Shadow Map");
	m_ShadowMap.AttachTexture(DepthStencil, shadowMap);
}

void DirectionalLightShadowPassPso::ComputePassParameters(
	const Scene& scene, const DirectionalLight& directionalLight)
{
	const XMVECTOR lightDirection = XMLoadFloat4(&directionalLight.m_DirectionWs);
	const auto sceneBounds = scene.ComputeBoundingSphere();
	const float radius = sceneBounds.GetRadius();


	const auto viewMatrix = XMMatrixLookToLH(2.0f * radius * lightDirection, -lightDirection,
	                                         XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
	XMFLOAT3 sceneCenterVs{};
	XMStoreFloat3(&sceneCenterVs, XMVector3TransformCoord(sceneBounds.GetCenter(), viewMatrix));

	const float projLeft = sceneCenterVs.x - radius;
	const float projRight = sceneCenterVs.x + radius;
	const float projBottom = sceneCenterVs.y - radius;
	const float projTop = sceneCenterVs.y + radius;
	const float projNear = sceneCenterVs.z - radius;
	const float projFar = sceneCenterVs.z + radius;

	const auto projectionMatrix = XMMatrixOrthographicOffCenterLH(projLeft, projRight,
	                                                              projBottom, projTop,
	                                                              projNear, projFar
	);
	const auto viewProjection = viewMatrix * projectionMatrix;

	m_ShadowPassParameters.LightDirectionWs = directionalLight.m_DirectionWs;
	m_ShadowPassParameters.ViewProjection = viewProjection;
}

void DirectionalLightShadowPassPso::ClearShadowMap(CommandList& commandList) const
{
	commandList.ClearDepthStencilTexture(GetShadowMapAsTexture(), D3D12_CLEAR_FLAG_DEPTH);
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

void DirectionalLightShadowPassPso::SetRenderTarget(CommandList& commandList) const
{
	commandList.SetRenderTarget(m_ShadowMap);
}

const Texture& DirectionalLightShadowPassPso::GetShadowMapAsTexture() const
{
	return m_ShadowMap.GetTexture(DepthStencil);
}
