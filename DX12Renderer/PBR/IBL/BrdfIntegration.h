#pragma once

#include <wrl.h>
#include <d3d12.h>
#include "RootSignature.h"
#include <memory>

class CommandList;
class RenderTarget;
class Mesh;

class BrdfIntegration
{
public:
	BrdfIntegration(Microsoft::WRL::ComPtr<ID3D12Device2> device, CommandList& commandList, DXGI_FORMAT renderTargetFormat);

	void SetContext(CommandList& commandList);
	void SetRenderTarget(CommandList& commandList, RenderTarget& renderTarget);
	void Draw(CommandList& commandList);

private:
	RootSignature m_RootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PipelineState;
	std::shared_ptr<Mesh> m_BlitMesh;

	D3D12_RECT m_ScissorRect;
};

