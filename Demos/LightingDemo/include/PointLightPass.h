#pragma once
#include <DirectXMath.h>
#include <wrl.h>
#include <memory>

#include <Framework/Light.h>
#include <Framework/Material.h>
#include <Framework/CommonRootSignature.h>

class CommandList;
class Mesh;

class PointLightPass final
{
public:
    explicit PointLightPass(const std::shared_ptr<CommonRootSignature>& rootSignature, CommandList& commandList);

    void Begin(CommandList& commandList) const;
    void End(CommandList& commandList) const;
    void Draw(CommandList& commandList, const PointLight& pointLight, const DirectX::XMMATRIX& viewProjection, float scale) const;

private:
    std::shared_ptr<CommonRootSignature> m_RootSignature;
    std::shared_ptr<Mesh> m_Mesh;
    std::shared_ptr<Material> m_Material;
};
