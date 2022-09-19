#pragma once

#include "RootSignature.h"
#include <wrl.h>
#include <d3d12.h>
#include <memory>
#include "Texture.h"

class CommandList;
class Mesh;

class Taa
{
public:
	explicit Taa(Microsoft::WRL::ComPtr<ID3D12Device2> device, CommandList& commandList, DXGI_FORMAT backBufferFormat, uint32_t width, uint32_t height);

	void Resolve(CommandList& commandList, const Texture& currentBuffer, const Texture& velocityBuffer);
	void Resize(uint32_t width, uint32_t height);
	void CaptureHistory(CommandList& commandList, const Texture& currentBuffer);

private:
	RootSignature m_ResolveRootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_ResolvePipelineState;

	std::shared_ptr<Mesh> m_BlitMesh;
	Texture m_HistoryBuffer;
};

