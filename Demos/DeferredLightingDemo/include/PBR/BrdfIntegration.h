#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <memory>

#include <Framework/Material.h>

class CommandList;
class RenderTarget;
class Mesh;

class BrdfIntegration
{
public:
	BrdfIntegration(const std::shared_ptr<CommonRootSignature>& rootSignature, CommandList& commandList);

	void SetRenderTarget(CommandList& commandList, RenderTarget& renderTarget);
	void Draw(CommandList& commandList);

private:
	std::shared_ptr<Mesh> m_BlitMesh;
	std::shared_ptr<Material> m_Material;
};

