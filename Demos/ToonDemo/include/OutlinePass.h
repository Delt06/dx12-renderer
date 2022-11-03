#pragma once

#include <Framework/Material.h>
#include <Framework/Mesh.h>


class OutlinePass
{
public:
    explicit OutlinePass(const std::shared_ptr<CommonRootSignature>& rootSignature, CommandList& commandList);

    void Render(CommandList& commandList, const std::shared_ptr<Texture>& sourceColor, const std::shared_ptr<Texture>& sourceDepth, const std::shared_ptr<Texture>& sourceNormals);
private:
    std::shared_ptr<Material> m_Material;
    std::shared_ptr<Mesh> m_BlitMesh;
};
