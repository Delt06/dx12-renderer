#pragma once

#include <DirectXMath.h>

struct Transform
{
    DirectX::XMMATRIX m_WorldMatrix;
    DirectX::XMMATRIX m_InverseTransposeWorldMatrix;

    void Compute(const DirectX::XMMATRIX& matrix)
    {
        m_WorldMatrix = matrix;
        m_InverseTransposeWorldMatrix = XMMatrixTranspose(XMMatrixInverse(nullptr, matrix));
    }
};
