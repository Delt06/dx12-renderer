#include "FXAAPass.h"

#include <DX12Library/Helpers.h>

#include <Framework/Blit_VS.h>

FXAAPass::FXAAPass(const std::shared_ptr<CommonRootSignature>& rootSignature, CommandList& commandList)
    : m_Material(
        Material::Create(std::make_shared<Shader>(rootSignature,
            ShaderBlob(ShaderBytecode_Blit_VS, sizeof ShaderBytecode_Blit_VS),
            ShaderBlob(L"FXAA_PS.cso")
            )
        )
    ),
    m_BlitMesh(Mesh::CreateBlitTriangle(commandList))
{

}

void FXAAPass::Render(CommandList& commandList, const std::shared_ptr<Texture>& sourceTexture)
{
    PIXScope(commandList, "FXAA");

    m_Material->SetShaderResourceView("sourceTexture", ShaderResourceView(sourceTexture));
    m_Material->SetVariable("fixedContrastThreshold", 0.0833f);
    m_Material->SetVariable("relativeContrastThreshold", 0.166f);
    m_Material->SetVariable("subpixelBlendingFactor", 0.75f);

    m_Material->Bind(commandList);
    m_BlitMesh->Draw(commandList);
    m_Material->Unbind(commandList);
}

