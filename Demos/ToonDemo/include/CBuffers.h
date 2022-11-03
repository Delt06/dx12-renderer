#pragma once

#include <DirectXMath.h>
#include <Framework/Light.h>

namespace Demo::Pipeline
{
    struct CBuffer
    {
        DirectX::XMMATRIX m_View;
        DirectX::XMMATRIX m_Projection;
        DirectX::XMMATRIX m_ViewProjection;

        DirectX::XMFLOAT4 m_CameraPosition;

        DirectX::XMMATRIX m_InverseView;
        DirectX::XMMATRIX m_InverseProjection;

        DirectX::XMFLOAT2 m_ScreenResolution;
        DirectX::XMFLOAT2 m_ScreenTexelSize;

        DirectX::XMMATRIX m_DirectionalLightViewProjection;

        DirectionalLight m_DirectionalLight;
    };
}

namespace Demo::Model
{
    struct CBuffer
    {
        DirectX::XMMATRIX m_Model;
        DirectX::XMMATRIX m_ModelViewProjection;
        DirectX::XMMATRIX m_InverseTransposeModel;

        void Compute(const DirectX::XMMATRIX& model, const DirectX::XMMATRIX& viewProjection)
        {
            m_Model = model;
            m_ModelViewProjection = model * viewProjection;
            m_InverseTransposeModel = XMMatrixTranspose(XMMatrixInverse(nullptr, model));
        }
    };
}
