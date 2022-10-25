#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <DirectXMath.h>

#include <memory>

#include <Framework/Material.h>

class Mesh;
class CommandList;
class Texture;
class Camera;

using namespace DirectX;

class PostFxPso
{
public:
	explicit PostFxPso(const std::shared_ptr<CommonRootSignature>& rootSignature, CommandList& commandList);

	void SetSourceColorTexture(CommandList& commandList, const std::shared_ptr<Texture>& texture);
	void SetSourceDepthTexture(CommandList& commandList, const std::shared_ptr<Texture>& texture);

	struct PostFxParameters
	{
		XMMATRIX ProjectionInverse;
		XMFLOAT3 FogColor;
		float FogDensity;
	};

	void SetParameters(CommandList& commandList, const PostFxParameters& parameters);

	void Blit(CommandList& commandList);
private:
	std::shared_ptr<const Mesh> m_BlitTriangle;
    std::shared_ptr<Material> m_Material;
};

