#pragma once

#include <DirectXMath.h>

namespace Demo
{
    struct PipelineCBuffer
    {
        DirectX::XMMATRIX m_View;
        DirectX::XMMATRIX m_Projection;
        DirectX::XMMATRIX m_ViewProjection;
    };

    namespace Grass
    {
        struct alignas(256) ModelCBuffer
        {
            DirectX::XMMATRIX m_Model;
        };

        struct alignas(256) MaterialCBuffer
        {
            DirectX::XMFLOAT4 m_Color;
        };
    }
}
