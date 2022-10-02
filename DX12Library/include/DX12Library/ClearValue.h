#pragma once

#include <d3d12.h>
#include <memory>

class ClearValue
{
public:
	using COLOR = FLOAT[4];

	ClearValue();
	explicit ClearValue(const COLOR color);
	explicit ClearValue(DXGI_FORMAT format, const COLOR color);

	ClearValue(const ClearValue&) = delete;
	ClearValue& operator=(const ClearValue&) = delete;

	ClearValue(ClearValue&&) = delete;
	ClearValue& operator=(ClearValue&&) = delete;

	[[nodiscard]] const D3D12_CLEAR_VALUE* GetD3D12ClearValue() const;
	[[nodiscard]] const COLOR& GetColor() const;

	[[nodiscard]] static ClearValue Empty();

private:
	COLOR m_Color;
	std::unique_ptr<D3D12_CLEAR_VALUE> m_D3D12ClearValue;
};
