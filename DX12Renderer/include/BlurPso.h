#pragma once

#include <DX12Library/RootSignature.h>
#include <d3d12.h>
#include <wrl.h>
#include <d3d12.h>
#include <memory>

class CommandList;
class Texture;
class RenderTarget;
class Mesh;

enum class BlurDirection
{
	Horizontal, Vertical
};

class Blur
{
public:
	explicit Blur(Microsoft::WRL::ComPtr<ID3D12Device2> device, CommandList& commandList, DXGI_FORMAT renderTargetFormat, UINT resolutionScaling = 2u, float spread = 1.0f);

	Texture Execute(CommandList& commandList, Texture& source, BlurDirection direction,
		const D3D12_SHADER_RESOURCE_VIEW_DESC* sourceDesc = nullptr);

private:

	RootSignature m_RootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PipelineState;
	std::shared_ptr<Mesh> m_BlitMesh;

	D3D12_RECT m_ScissorRect;
	DXGI_FORMAT m_RenderTargetFormat;

	UINT m_ResolutionScaling;
	float m_Spread;
};

