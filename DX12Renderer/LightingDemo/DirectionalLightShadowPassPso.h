﻿#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <DirectXMath.h>

#include "GameObject.h"
#include "RenderTarget.h"
#include "RootSignature.h"

class Camera;
struct DirectionalLight;
class CommandList;

class DirectionalLightShadowPassPso
{
public:
	DirectionalLightShadowPassPso(Microsoft::WRL::ComPtr<ID3D12Device2> device, CommandList& commandList,
	                              UINT resolution = 2048);
	void SetContext(CommandList& commandList) const;
	void ComputePassParameters(const Camera& camera, const DirectionalLight& directionalLight);
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
