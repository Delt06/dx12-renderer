#pragma once
#include "RootSignature.h"
#include "wrl.h"

class Mesh;
class CommandList;
class RenderTarget;
class Texture;

class DiffuseIrradiance
{
public:
	DiffuseIrradiance(Microsoft::WRL::ComPtr<ID3D12Device2> device, CommandList& commandList, DXGI_FORMAT renderTargetFormat);

	void SetContext(CommandList& commandList);
	void SetSourceCubemap(CommandList& commandList, Texture& texture);
	void SetRenderTarget(CommandList& commandList, RenderTarget& renderTarget, UINT texArrayIndex = -1);
	void Draw(CommandList& commandList, uint32_t cubemapSideIndex);

private:
	RootSignature m_RootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PipelineState;
	std::shared_ptr<Mesh> m_BlitMesh;

	D3D12_RECT m_ScissorRect;
};

