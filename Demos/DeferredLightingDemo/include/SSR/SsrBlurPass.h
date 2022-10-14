#pragma once

#include <Framework/CommonRootSignature.h>
#include <Framework/Mesh.h>
#include <Framework/Material.h>
#include <memory>

class SsrBlurPass
{
public:
	explicit SsrBlurPass(const std::shared_ptr<CommonRootSignature>& rootSignature, CommandList& commandList);

	void Execute(CommandList& commandList, const std::shared_ptr<Texture>& traceResult, const RenderTarget& renderTarget) const;

private:
	std::shared_ptr<Mesh> m_BlitMesh;
	std::shared_ptr<Material> m_Material;
};