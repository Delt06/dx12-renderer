#pragma once

#include <DirectXMath.h>
#include <Framework/MaterialBase.h>

class Material final : public MaterialBase
{
public:
	struct Constants
	{
		DirectX::XMFLOAT4 Emissive = { 0.0f, 0.0f, 0.0f, 1.0f };
		DirectX::XMFLOAT4 Ambient = { 0.1f, 0.1f, 0.1f, 1.0f };
		DirectX::XMFLOAT4 Diffuse = { 1.0f, 1.0f, 1.0f, 1.0f };
		DirectX::XMFLOAT4 Specular = { 1.0f, 1.0f, 1.0f, 1.0f };

		float SpecularPower = 128.0f;
		float Reflectivity = 0;
		uint32_t Padding[2]{};

		uint32_t HasDiffuseMap{};
		uint32_t HasNormalMap{};
		uint32_t HasSpecularMap{};
		uint32_t HasGlossMap{};

		DirectX::XMFLOAT4 TilingOffset{ 1, 1, 0, 0 };
	};
	enum MapType
	{
		Diffuse = 0,
		Normal,
		Specular,
		Gloss,
		TotalNumMaps
	};

	Constants& GetConstants();

	void SetDynamicConstantBuffer(CommandList& commandList, uint32_t rootParameterIndex) override;
	void SetShaderResourceViews(CommandList& commandList, uint32_t rootParameterIndex, uint32_t baseDescriptorOffset = 0) override;
	void SetMapsEmpty(std::shared_ptr<Texture> emptyMap) override;
	void SetMap(MapType mapType, std::shared_ptr<Texture> map);

	std::shared_ptr<Texture> GetMap(MapType mapType) const;

private:

	Constants m_Constants;
	std::shared_ptr<Texture> m_Maps[TotalNumMaps];
};
