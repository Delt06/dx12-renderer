#pragma once

#include <d3d12.h>
#include <d3dx12.h>
#include <wrl.h>
#include "RootSignature.h"
#include <memory>

class CommandList;
class Mesh;
class Texture;
class RenderTarget;

class ToneMappingPso
{
public:
	explicit ToneMappingPso(Microsoft::WRL::ComPtr<ID3D12Device2> device, CommandList& commandList, DXGI_FORMAT renderTargetFormat);

	void Blit(CommandList& commandList, const Texture& source, RenderTarget& destination, float exposure);

private:
	RootSignature m_RootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PipelineState;
	std::shared_ptr<Mesh> m_BlitMesh;

	D3D12_RECT m_ScissorRect;
};
