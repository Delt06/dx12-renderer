#include "PointLightPass.h"

#include <DX12Library/CommandList.h>
#include <DX12Library/Helpers.h>
#include <Framework/Mesh.h>

#include <d3d12.h>

#include <Framework/Light.h>
#include <CBuffers.h>
#include <DX12Library/ShaderUtils.h>

using namespace Microsoft::WRL;
using namespace DirectX;

void PointLightPass::Begin(CommandList &commandList) const
{
    m_Material->BeginBatch(commandList);
}

void PointLightPass::End(CommandList &commandList) const
{
    m_Material->EndBatch(commandList);
}

void PointLightPass::Draw(CommandList &commandList, const PointLight &pointLight, const DirectX::XMMATRIX &viewProjection, float scale) const
{
    XMMATRIX worldMatrix = XMMatrixTranslation(pointLight.PositionWs.x, pointLight.PositionWs.y,
        pointLight.PositionWs.z);
    worldMatrix = XMMatrixMultiply(XMMatrixScaling(scale, scale, scale), worldMatrix);

    Demo::Model::CBuffer modelCBuffer{};
    modelCBuffer.Compute(worldMatrix, viewProjection);
    m_RootSignature->SetModelConstantBuffer(commandList, modelCBuffer);

    m_Material->SetVariable<DirectX::XMFLOAT4>("Color", pointLight.Color);

    m_Material->UploadUniforms(commandList);
    m_Mesh->Draw(commandList);
}

PointLightPass::PointLightPass(const std::shared_ptr<CommonRootSignature> &rootSignature, CommandList &commandList)
    : m_Mesh(Mesh::CreateSphere(commandList)), m_RootSignature(rootSignature)
{
    auto shader = std::make_shared<Shader>(rootSignature,
        ShaderBlob(L"LightingDemo_PointLight_VS.cso"),
        ShaderBlob(L"LightingDemo_PointLight_PS.cso"));
    m_Material = Material::Create(shader);
}
