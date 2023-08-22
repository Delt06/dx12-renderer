#include "ClearValue.h"

ClearValue::ClearValue()
    : m_D3D12ClearValue(nullptr)
    , m_Color{ 0, 0, 0, 0 }
    , m_DepthStencil{}
{

}

ClearValue::ClearValue(DXGI_FORMAT format, const COLOR color)
    : m_D3D12ClearValue(new D3D12_CLEAR_VALUE)
    , m_Color()
    , m_DepthStencil{}
{
    m_D3D12ClearValue->Format = format;
    memcpy(m_D3D12ClearValue->Color, color, sizeof(COLOR));
    memcpy(m_Color, color, sizeof(COLOR));
}

ClearValue::ClearValue(DXGI_FORMAT format, const DirectX::XMFLOAT4& color)
    : m_D3D12ClearValue(new D3D12_CLEAR_VALUE)
    , m_Color()
    , m_DepthStencil{}
{
    m_D3D12ClearValue->Format = format;
    memcpy(m_D3D12ClearValue->Color, &color, sizeof(COLOR));
    memcpy(m_Color, &color, sizeof(COLOR));
}

ClearValue::ClearValue(DXGI_FORMAT format, const DEPTH_STENCIL_VALUE depthStencilValue)
    : m_D3D12ClearValue(new D3D12_CLEAR_VALUE)
    , m_Color()
    , m_DepthStencil(depthStencilValue)
{
    m_D3D12ClearValue->Format = format;
    m_D3D12ClearValue->DepthStencil = depthStencilValue;
}

ClearValue::ClearValue(const COLOR color)
    : m_D3D12ClearValue(nullptr)
    , m_Color()
{
    memcpy(m_Color, color, sizeof(COLOR));
}



ClearValue::ClearValue(const ClearValue& other)
{
    memcpy(m_Color, other.m_Color, sizeof(COLOR));
    m_DepthStencil = other.m_DepthStencil;

    if (other.m_D3D12ClearValue != nullptr)
    {
        m_D3D12ClearValue = std::unique_ptr<D3D12_CLEAR_VALUE>(new D3D12_CLEAR_VALUE);
        m_D3D12ClearValue->Format = other.m_D3D12ClearValue->Format;
        m_D3D12ClearValue->DepthStencil = other.m_D3D12ClearValue->DepthStencil;

        memcpy(m_D3D12ClearValue->Color, other.m_D3D12ClearValue->Color, sizeof(COLOR));
    }
    else
    {
        m_D3D12ClearValue = nullptr;
    }
}


ClearValue::ClearValue(ClearValue&& other)
{
    memcpy(m_Color, other.m_Color, sizeof(COLOR));
    memset(other.m_Color, 0, sizeof(COLOR));

    m_DepthStencil = other.m_DepthStencil;
    other.m_DepthStencil = {};

    if (other.m_D3D12ClearValue != nullptr)
    {
        m_D3D12ClearValue = std::move(other.m_D3D12ClearValue);
    }
    else
    {
        m_D3D12ClearValue = nullptr;
    }
}

const D3D12_CLEAR_VALUE* ClearValue::GetD3D12ClearValue() const
{
    return m_D3D12ClearValue.get();
}
const ClearValue::COLOR& ClearValue::GetColor() const
{
    return m_Color;
}
