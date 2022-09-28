#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <DX12Library/RenderTarget.h>
#include <DX12Library/RootSignature.h>
#include <cstdint>
#include <DirectXMath.h>

class CommandList;
class Mesh;

class Ssao
{
public:
	explicit Ssao(Microsoft::WRL::ComPtr<ID3D12Device2> device, CommandList& commandList, DXGI_FORMAT gBufferFormat, uint32_t width, uint32_t height);

	void Resize(uint32_t width, uint32_t height);

	void SsaoPass(CommandList& commandList, const Texture& gBufferNormals, const Texture& gBufferDepth, DirectX::XMMATRIX viewMatrix, DirectX::XMMATRIX projectionMatrix, const D3D12_SHADER_RESOURCE_VIEW_DESC* gBufferDepthSrvDesc = nullptr, float radius = 0.5f, float power = 1.0f);
	void BlurPass(CommandList& commandList, const RenderTarget& surfaceRenderTarget);

private:
	RenderTarget m_RenderTarget;
	std::shared_ptr<Mesh> m_BlitMesh;
	D3D12_RECT m_ScissorRect;
	Texture m_NoiseTexture;
	std::vector<DirectX::XMFLOAT3> m_Samples{};

	RootSignature m_SsaoPassRootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_SsaoPassPipelineState;

	RootSignature m_BlurPassRootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_BlurPassPipelineState;
};
