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
        struct ModelCBuffer
        {
            DirectX::XMMATRIX m_Model;
        };

        struct MaterialCBuffer
        {
            DirectX::XMFLOAT4 m_Color;
        };
    }
}
