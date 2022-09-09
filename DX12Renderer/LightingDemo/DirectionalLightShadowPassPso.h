#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <DirectXMath.h>

#include "GameObject.h"
#include "RenderTarget.h"
#include "RootSignature.h"
#include "Scene.h"

class Camera;
struct DirectionalLight;
class CommandList;

class DirectionalLightShadowPassPso
{
public:
	DirectionalLightShadowPassPso(Microsoft::WRL::ComPtr<ID3D12Device2> device, CommandList& commandList,
	                              UINT resolution = 4096);
	void SetContext(CommandList& commandList) const;
	void ComputePassParameters(const Camera& camera, const DirectionalLight& directionalLight, const Scene& scene);
	void SetBias(float depthBias, float normalBias);
	void ClearShadowMap(CommandList& commandList) const;
	void DrawToShadowMap(CommandList& commandList, const GameObject& gameObject) const;

	void SetShadowMapShaderResourceView(CommandList& commandList, uint32_t rootParameterIndex,
	                                    uint32_t descriptorOffset = 0) const;

	[[nodiscard]] DirectX::XMMATRIX ComputeShadowModelViewProjectionMatrix(DirectX::XMMATRIX worldMatrix) const;
	[[nodiscard]] DirectX::XMMATRIX GetShadowViewProjectionMatrix() const;


private:
	struct ShadowPassParameters
	{
		DirectX::XMMATRIX Model;
		DirectX::XMMATRIX InverseTransposeModel;
		DirectX::XMMATRIX ViewProjection;
		DirectX::XMFLOAT4 LightDirectionWs;
		DirectX::XMFLOAT4 Bias;
	};

	const Texture& GetShadowMapAsTexture() const;

	UINT m_Resolution;

	RenderTarget m_ShadowMap;
	RootSignature m_RootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PipelineState;
	D3D12_VIEWPORT m_Viewport;
	D3D12_RECT m_ScissorRect;

	ShadowPassParameters m_ShadowPassParameters;;
};
