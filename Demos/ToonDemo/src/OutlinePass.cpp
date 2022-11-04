#include "OutlinePass.h"

#include <DirectXMath.h>

#include <DX12Library/Helpers.h>

#include <Framework/Blit_VS.h>

OutlinePass::OutlinePass(const std::shared_ptr<CommonRootSignature>& rootSignature, CommandList& commandList)
    : m_Material(Material::Create(
        std::make_shared<Shader>(rootSignature,
            ShaderBlob(ShaderBytecode_Blit_VS, sizeof(ShaderBytecode_Blit_VS)),
            ShaderBlob(L"Outline_PS.cso")
            )))
    , m_BlitMesh(Mesh::CreateBlitTriangle(commandList))
{

}

void OutlinePass::Render(CommandList& commandList, const std::shared_ptr<Texture>& sourceColor, const std::shared_ptr<Texture>& sourceDepth, const std::shared_ptr<Texture>& sourceNormals)
{
    PIXScope(commandList, "Outline");

    m_Material->SetShaderResourceView("sourceColor", ShaderResourceView(sourceColor));
    m_Material->SetShaderResourceView("sourceDepth", ShaderResourceView(sourceDepth));
    m_Material->SetShaderResourceView("sourceNormals", ShaderResourceView(sourceNormals));

    m_Material->SetVariable<DirectX::XMFLOAT4>("outlineColor", { 0.0f, 0.0f, 0.0f, 1.0f });
    m_Material->SetVariable("depthThreshold", 0.002f);
    m_Material->SetVariable("normalsThreshold", 1.0f);

    m_Material->Bind(commandList);
    m_BlitMesh->Draw(commandList);
    m_Material->Unbind(commandList);
}
