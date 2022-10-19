#include <PBR/PreFilterEnvironment.h>
#include <DX12Library/Helpers.h>
#include <DirectXMath.h>
#include <Framework/Mesh.h>
#include <DX12Library/Texture.h>
#include <DX12Library/RenderTarget.h>
#include <DX12Library/Cubemap.h>
#include <DX12Library/ShaderUtils.h>

using namespace Microsoft::WRL;
using namespace DirectX;

namespace
{
	struct Parameters
	{
		XMFLOAT4 Forward;
		XMFLOAT4 Up;

		float Roughness;
		XMFLOAT2 SourceResolution;
		float _Padding;
	};
}

PreFilterEnvironment::PreFilterEnvironment(const std::shared_ptr<CommonRootSignature>& rootSignature, CommandList& commandList)
	: m_BlitMesh(Mesh::CreateBlitTriangle(commandList))
{
	auto shader = std::make_shared<Shader>(
		rootSignature,
		ShaderBlob(L"IBL_PreFilterEnvironment_VS.cso"),
		ShaderBlob(L"IBL_PreFilterEnvironment_PS.cso")
		);
	m_Material = Material::Create(shader);
}

void PreFilterEnvironment::SetSourceCubemap(CommandList& commandList, const std::shared_ptr<Texture>& texture)
{
	const auto sourceDesc = texture->GetD3D12ResourceDesc();
	m_SourceSrvDesc = {};
	m_SourceSrvDesc.Format = sourceDesc.Format;
	m_SourceSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	m_SourceSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	m_SourceSrvDesc.TextureCube.MipLevels = -1;
	m_SourceSrvDesc.TextureCube.MostDetailedMip = 0;
	m_SourceSrvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

	m_Material->SetShaderResourceView("source", ShaderResourceView(texture, 0, UINT_MAX, &m_SourceSrvDesc));

	m_SourceWidth = sourceDesc.Width;
	m_SourceHeight = sourceDesc.Height;
}

void PreFilterEnvironment::SetRenderTarget(CommandList& commandList, RenderTarget& renderTarget, UINT texArrayIndex, UINT mipLevel)
{
	const auto rtColorDesc = renderTarget.GetTexture(Color0)->GetD3D12ResourceDesc();
	const float sizeScale = pow(2, mipLevel);
	const auto width = static_cast<float>(rtColorDesc.Width) / sizeScale;
	const auto height = static_cast<float>(rtColorDesc.Height) / sizeScale;
	auto viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, width, height);

	commandList.SetRenderTarget(renderTarget, texArrayIndex, mipLevel);
	commandList.SetViewport(viewport);
	commandList.SetInfiniteScrissorRect();
}

void PreFilterEnvironment::Draw(CommandList& commandList, float roughness, uint32_t cubemapSideIndex)
{
	Parameters parameters{};

	const auto orientation = Cubemap::SIDE_ORIENTATIONS[cubemapSideIndex];
	XMStoreFloat4(&parameters.Forward, orientation.Forward);
	XMStoreFloat4(&parameters.Up, orientation.Up);

	parameters.Roughness = roughness;
	parameters.SourceResolution = { m_SourceWidth, m_SourceHeight };

	m_Material->SetAllVariables(parameters);

	m_Material->Bind(commandList);
	m_BlitMesh->Draw(commandList);
	m_Material->Unbind(commandList);
}
