#pragma once

#include <string.h>

#include <Framework/Material.h>
#include <Framework/Mesh.h>

class DownsamplePass
{
public:
    explicit DownsamplePass(const std::shared_ptr<CommonRootSignature>& rootSignature, CommandList& commandList, const std::wstring& textureName, DXGI_FORMAT format, uint32_t width, uint32_t height, uint32_t factor = 2);

    void Resize(uint32_t width, uint32_t height);
    void Execute(CommandList& commandList, const ShaderResourceView& sourceSRV);

    const std::shared_ptr<Texture>& GetResultTexture() const;

private:
    RenderTarget m_RenderTarget;
    std::shared_ptr<Material> m_Material;
    std::shared_ptr<Mesh> m_BlitMesh;
    uint32_t m_Factor;
};
