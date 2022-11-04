#pragma once

#include <Framework/Material.h>
#include <Framework/Mesh.h>

class FXAAPass
{
public:
    explicit FXAAPass(const std::shared_ptr<CommonRootSignature>& rootSignature, CommandList& commandList);

    void Render(CommandList& commandList, const std::shared_ptr<Texture>& sourceTexture);

private:
    std::shared_ptr<Material> m_Material;
    std::shared_ptr<Mesh> m_BlitMesh;
};
