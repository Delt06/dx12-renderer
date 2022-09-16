#pragma once

#include <DirectXMath.h>
#include <memory>
#include "MaterialBase.h"

class Texture;

class PbrMaterial final : MaterialBase
{
public:
	struct Constants
	{
		DirectX::XMFLOAT4 Diffuse = { 1, 1, 1, 1 };

		float Metallic = 0;
		float Roughness = 0.5;
		uint32_t _Padding[2]{};

		uint32_t HasDiffuseMap{};
		uint32_t HasNormalMap{};
		uint32_t HasMetallicMap{};
		uint32_t HasRoughnessMap{};

		DirectX::XMFLOAT4 TilingOffset{ 1, 1, 0, 0 };
	};

	enum MapType
	{
		Diffuse,
		Normal,
		Metallic,
		Roughness,
		TotalNumMaps
	};

	Constants& GetConstants();

	void SetDynamicConstantBuffer(CommandList& commandList, uint32_t rootParameterIndex) override;
	void SetShaderResourceViews(CommandList& commandList, uint32_t rootParameterIndex, uint32_t baseDescriptorOffset = 0) override;
	void SetMapsEmpty(std::shared_ptr<Texture> emptyMap) override;
	void SetMap(MapType mapType, std::shared_ptr<Texture> map);

private:
	void UpdateMapFlags();

	Constants m_Constants{};

	std::shared_ptr<Texture> m_Maps[TotalNumMaps];
};

