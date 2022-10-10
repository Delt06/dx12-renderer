#include "Material.h"
#include "CommonRootSignature.h"

static const ShaderUtils::ConstantBufferMetadata* FindMaterialConstantBuffer(const Shader::ShaderMetadata& metadata)
{
	for (const auto& cbufferMetadata : metadata.m_ConstantBuffers)
	{
		if (cbufferMetadata.RegisterIndex != 0) continue;
		if (cbufferMetadata.Space != CommonRootSignature::MATERIAL_REGISTER_SPACE) continue;

		return &cbufferMetadata;
	}

	return nullptr;
}

Material::Material(const std::shared_ptr<Shader>& shader)
	: m_Shader(shader)
{
	const auto vsCbuffer = FindMaterialConstantBuffer(shader->GetVertexShaderMetadata());
	const auto psCbuffer = FindMaterialConstantBuffer(shader->GetPixelShaderMetadata());

	size_t cbufferSize;

	if (vsCbuffer != nullptr && psCbuffer != nullptr)
	{
		if (vsCbuffer->Size != psCbuffer->Size)
		{
			throw std::exception("Vertex and Pixel shader material constant buffer sizes are inconsistent.");
		}

		cbufferSize = vsCbuffer->Size;
		m_Metadata = vsCbuffer;
	}
	else if (vsCbuffer != nullptr)
	{
		cbufferSize = vsCbuffer->Size;
		m_Metadata = vsCbuffer;
	}
	else if (psCbuffer != nullptr)
	{
		cbufferSize = psCbuffer->Size;
		m_Metadata = psCbuffer;
	}
	else
	{
		m_ConstantBuffer = nullptr;
		m_ConstantBufferSize = 0;
		return;
	}

	m_ConstantBuffer.reset(new uint8_t[cbufferSize]);
	m_ConstantBufferSize = cbufferSize;
}

void Material::SetVariable(const std::string& name, size_t size, const void* data)
{
	if (m_ConstantBuffer == nullptr)
	{
		return;
	}

	for (const auto& variable : m_Metadata->Variables)
	{
		if (variable.Name != name) continue;

		if (size != variable.Size)
		{
			throw std::exception("Variable size mismatch.");
		}

		memcpy(m_ConstantBuffer.get() + variable.Offset, data, size);
	}

	throw std::exception("Variable not found.");
}

void Material::SetShaderResourceView(const std::string& name, const ShaderResourceView& shaderResourceView)
{
	m_ShaderResourceViews.insert_or_assign(name, shaderResourceView);
}

void Material::Bind(CommandList& commandList)
{
	m_Shader->Bind(commandList);

	UploadConstantBuffer(commandList);
	UploadShaderResourceViews(commandList);
}

std::shared_ptr<Material> Material::Create(const std::shared_ptr<Shader>& shader)
{
	return std::make_shared<Material>(shader);
}

void Material::UploadConstantBuffer(CommandList& commandList)
{
	if (m_ConstantBuffer == nullptr)
	{
		return;
	}

	m_Shader->SetMaterialConstantBuffer(commandList, m_ConstantBufferSize, m_ConstantBuffer.get());
}

void Material::UploadShaderResourceViews(CommandList& commandList)
{
	for (const auto& srvName : m_ShaderResourceViews)
	{
		const auto& name = srvName.first;
		const auto& shaderResourceView = srvName.second;

		m_Shader->SetShaderResourceView(commandList, name, shaderResourceView);
	}
}
