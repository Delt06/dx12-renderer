#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <DirectXMath.h>

#include "RootSignature.h"

class GameObject;
class Camera;
class CommandList;

class ShadowPassPsoBase
{
public:
	explicit ShadowPassPsoBase(Microsoft::WRL::ComPtr<ID3D12Device2> device, UINT resolution = 4096);
	virtual ~ShadowPassPsoBase();

	void SetContext(CommandList& commandList) const;
	void SetBias(float depthBias, float normalBias);
	void DrawToShadowMap(CommandList& commandList, const GameObject& gameObject) const;

	virtual void SetRenderTarget(CommandList& commandList) const = 0;
	virtual void ClearShadowMap(CommandList& commandList) const = 0;
	virtual void SetShadowMapShaderResourceView(CommandList& commandList,
	                                            uint32_t rootParameterIndex,
	                                            uint32_t descriptorOffset) const = 0;

protected:
	static constexpr DXGI_FORMAT SHADOW_MAP_FORMAT = DXGI_FORMAT_D32_FLOAT;

	struct ShadowPassParameters
	{
		DirectX::XMMATRIX Model;
		DirectX::XMMATRIX InverseTransposeModel;
		DirectX::XMMATRIX ViewProjection;
		DirectX::XMFLOAT4 LightDirectionWs;
		DirectX::XMFLOAT4 Bias;
	};

	ShadowPassParameters m_ShadowPassParameters;
	const UINT m_Resolution;

private:

	struct RootParameter
	{
		enum RootParameters
		{
			ShadowPassParametersCb,
			NumRootParameters,
		};
	};

	

	RootSignature m_RootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PipelineState;
	D3D12_VIEWPORT m_Viewport;
	D3D12_RECT m_ScissorRect;
};
