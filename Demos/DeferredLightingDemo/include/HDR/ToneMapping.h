#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <memory>

#include <d3dx12.h>
#include <DX12Library/RootSignature.h>
#include <Framework/Material.h>

class CommandList;
class Mesh;
class Texture;
class RenderTarget;

class ToneMapping
{
public:
	explicit ToneMapping(const std::shared_ptr<CommonRootSignature>& rootSignature, CommandList& commandList);

	void Blit(CommandList& commandList, const std::shared_ptr<Texture>& source, const std::shared_ptr<Texture>& luminanceOutput, RenderTarget& destination, float whitePoint = 4.0f);

private:
	std::shared_ptr<Mesh> m_BlitMesh;
	std::shared_ptr<Material> m_Material;
};

