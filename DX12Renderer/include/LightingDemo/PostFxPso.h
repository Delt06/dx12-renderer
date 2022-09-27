#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <DirectXMath.h>

#include <memory>

#include "../RootSignature.h"

class Mesh;
class CommandList;
class Texture;
class Camera;

using namespace DirectX;

class PostFxPso
{
public:
	explicit PostFxPso(Microsoft::WRL::ComPtr<ID3D12Device2> device, CommandList& commandList, DXGI_FORMAT backBufferFormat);

	void SetSourceColorTexture(CommandList& commandList, Texture texture);
	void SetSourceDepthTexture(CommandList& commandList, Texture texture);

	struct PostFxParameters
	{
		XMMATRIX ProjectionInverse;
		XMFLOAT3 FogColor;
		float FogDensity;
	};

	void SetParameters(CommandList& commandList, const PostFxParameters& parameters);

	void SetContext(CommandList& commandList);
	void Blit(CommandList& commandList);
private:
	std::shared_ptr<const Mesh> m_BlitTriangle;

	RootSignature m_RootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PipelineState;
	DXGI_FORMAT m_BackBufferFormat;
};

