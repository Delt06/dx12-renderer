#pragma once

#include <DirectXMath.h>
#include <memory>
#include <Framework/MaterialBase.h>

class Texture;

class PbrMaterial final : public MaterialBase
{
public:
	struct Constants
	{
		DirectX::XMFLOAT4 Diffuse = { 1, 1, 1, 1 };

		float Metallic = 1.0;
		float Roughness = 1.0;

		uint32_t HasDiffuseMap{};
		uint32_t HasNormalMap{};

		uint32_t HasMetallicMap{};
		uint32_t HasRoughnessMap{};
		uint32_t HasAmbientOcclusionMap{};
		float Emission = 0;

		DirectX::XMFLOAT4 TilingOffset{ 1, 1, 0, 0 };
	};

	enum MapType
	{
		Diffuse,
		Normal,
		Metallic,
		Roughness,
		AmbientOcclusion,
		TotalNumMaps
	};

	Constants& GetConstants();

	void SetDynamicConstantBuffer(CommandList& commandList, uint32_t rootParameterIndex) override;
	void SetShaderResourceViews(CommandList& commandList, uint32_t rootParameterIndex, uint32_t baseDescriptorOffset = 0) override;
	void SetMapsEmpty(std::shared_ptr<Texture> emptyMap) override;
	void SetMap(MapType mapType, std::shared_ptr<Texture> map);

private:

	Constants m_Constants;
	std::shared_ptr<Texture> m_Maps[TotalNumMaps];
};

