#pragma once

#include <vector>

#include <DirectXMath.h>

#include <Framework/Material.h>
#include <Framework/Mesh.h>

class SSAOPass
{
public:
    explicit SSAOPass(const std::shared_ptr<CommonRootSignature>& rootSignature, CommandList& commandList, uint32_t width, uint32_t height);

    void Resize(uint32_t width, uint32_t height);

    void Execute(CommandList& commandList, const std::shared_ptr<Texture>& sceneDepth, const std::shared_ptr<Texture>& sceneNormals);

    const std::shared_ptr<Texture>& GetFinalTexture() const;

private:
    std::vector<DirectX::XMFLOAT4> m_Samples;
    std::shared_ptr<Texture> m_NoiseTexture;

    RenderTarget m_SSAORawRenderTarget;
    RenderTarget m_SSAOFinalRenderTarget;
    std::shared_ptr<Material> m_SSAOMaterial;
    std::shared_ptr<Material> m_SSAOBlurMaterial;
    std::shared_ptr<Mesh> m_BlitMesh;
};
