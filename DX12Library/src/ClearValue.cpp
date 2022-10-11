#include "ClearValue.h"

ClearValue::ClearValue()
	: m_D3D12ClearValue(nullptr)
	, m_Color{ 0, 0, 0, 0 }
{

}

ClearValue::ClearValue(DXGI_FORMAT format, const COLOR color)
	: m_D3D12ClearValue(new D3D12_CLEAR_VALUE)
	, m_Color()
{
	m_D3D12ClearValue->Format = format;
	memcpy(m_D3D12ClearValue->Color, color, sizeof(COLOR));
	memcpy(m_Color, color, sizeof(COLOR));
}

ClearValue::ClearValue(const COLOR color)
	: m_D3D12ClearValue(nullptr)
	, m_Color()
{
	memcpy(m_Color, color, sizeof(COLOR));
}

const D3D12_CLEAR_VALUE* ClearValue::GetD3D12ClearValue() const
{
	return m_D3D12ClearValue.get();
}
const ClearValue::COLOR& ClearValue::GetColor() const
{
	return m_Color;
}