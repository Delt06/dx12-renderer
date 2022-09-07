#pragma once
#include <DirectXMath.h>

struct MatricesCb
{
	DirectX::XMMATRIX m_Model;
	DirectX::XMMATRIX m_ModelView;
	DirectX::XMMATRIX m_InverseTransposeModelView;
	DirectX::XMMATRIX m_ModelViewProjection;
	DirectX::XMMATRIX m_View;
	DirectX::XMMATRIX m_Projection;

	void XM_CALLCONV Compute(DirectX::CXMMATRIX model, DirectX::CXMMATRIX view,
	                         DirectX::CXMMATRIX viewProjection, DirectX::CXMMATRIX projection);
};
