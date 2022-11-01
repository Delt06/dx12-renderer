#pragma once

#include <d3d12.h>

#include "RenderTargetFormats.h"
#include "DXGIExtensions.h"
#include "RenderTarget.h"

class RenderTargetState
{
public:
    explicit RenderTargetState(const RenderTarget& renderTarget)
        : m_Formats(renderTarget)
        , m_SampleDesc{}
    {
        const auto& colorTexture = renderTarget.GetTexture(Color0);
        const auto& depthTexture = renderTarget.GetTexture(DepthStencil);
        if (colorTexture->IsValid())
        {
            m_SampleDesc = colorTexture->GetD3D12ResourceDesc().SampleDesc;
        }
        else if (depthTexture->IsValid())
        {
            m_SampleDesc = depthTexture->GetD3D12ResourceDesc().SampleDesc;
        }
        else
        {
            throw std::exception("Textures of the render target are invalid.");
        }
    }

    RenderTargetState()
        : m_Formats{}
        , m_SampleDesc{}
    {}

    const RenderTargetFormats& GetFormats() const { return m_Formats; };

    const DXGI_SAMPLE_DESC& GetSampleDesc() const { return m_SampleDesc; };

    bool operator==(const RenderTargetState& other) const
    {
        if (m_Formats != other.m_Formats)
        {
            return false;
        }

        if (m_SampleDesc != other.m_SampleDesc)
        {
            return false;
        }

        return true;
    }

    bool operator!=(const RenderTargetState& other) const
    {
        return !(*this == other);
    }

private:
    RenderTargetFormats m_Formats;
    DXGI_SAMPLE_DESC m_SampleDesc;
};

namespace std
{
    template <>
    struct hash<RenderTargetState>
    {
        std::size_t operator()(const RenderTargetState& state) const
        {
            using std::size_t;
            using std::hash;
            using std::string;

            return hash<RenderTargetFormats>()(state.GetFormats())
                ^ hash<DXGI_SAMPLE_DESC>()(state.GetSampleDesc());
        }
    };
}
