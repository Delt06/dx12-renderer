#pragma once

#include <d3d12.h>
#include <DirectXMath.h>
#include <memory>

class ClearValue
{
public:
	using COLOR = FLOAT[4];
    using DEPTH_STENCIL_VALUE = D3D12_DEPTH_STENCIL_VALUE;

    ClearValue();
	explicit ClearValue(const COLOR color);
	explicit ClearValue(DXGI_FORMAT format, const COLOR color);
	explicit ClearValue(DXGI_FORMAT format, const DirectX::XMFLOAT4& color);
	explicit ClearValue(DXGI_FORMAT format, const DEPTH_STENCIL_VALUE depthStencilValue);

	ClearValue(const ClearValue&);
	ClearValue& operator=(const ClearValue&) = delete;

	ClearValue(ClearValue&&);
	ClearValue& operator=(ClearValue&&) = delete;

	[[nodiscard]] const D3D12_CLEAR_VALUE* GetD3D12ClearValue() const;
	[[nodiscard]] const COLOR& GetColor() const;

	[[nodiscard]] static ClearValue Empty();

private:
	COLOR m_Color;
    DEPTH_STENCIL_VALUE m_DepthStencil;

	std::unique_ptr<D3D12_CLEAR_VALUE> m_D3D12ClearValue;
};
