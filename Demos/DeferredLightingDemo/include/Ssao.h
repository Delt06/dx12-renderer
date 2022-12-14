#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <DX12Library/RenderTarget.h>
#include <DX12Library/RootSignature.h>
#include <cstdint>
#include <DirectXMath.h>
#include <Framework/Material.h>
#include <Framework/Shader.h>
#include <Framework/CommonRootSignature.h>

#include <memory>

class CommandList;
class Mesh;

class Ssao
{
public:
	explicit Ssao(const std::shared_ptr<CommonRootSignature>& rootSignature, CommandList& commandList, uint32_t width, uint32_t height, bool downsample);

	void Resize(uint32_t width, uint32_t height);

	void SsaoPass(CommandList& commandList, const Texture& gBufferNormals, const Texture& gBufferDepth, const D3D12_SHADER_RESOURCE_VIEW_DESC* gBufferDepthSrvDesc = nullptr, float radius = 0.5f, float power = 1.0f);
	void BlurPass(CommandList& commandList, const RenderTarget& surfaceRenderTarget);

private:
	bool m_Downsample;

	RenderTarget m_RenderTarget;
	std::shared_ptr<Mesh> m_BlitMesh;
	std::shared_ptr<Texture> m_NoiseTexture;
	std::vector<DirectX::XMFLOAT4> m_Samples{};

	std::shared_ptr<Material> m_SsaoPassMaterial;
	std::shared_ptr<Material> m_BlurPassMaterial;
};

