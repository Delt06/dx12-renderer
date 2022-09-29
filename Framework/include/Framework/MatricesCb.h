#pragma once
#include <DirectXMath.h>

struct MatricesCb
{
	DirectX::XMMATRIX Model;
	DirectX::XMMATRIX ModelView;
	DirectX::XMMATRIX InverseTransposeModelView;
	DirectX::XMMATRIX ModelViewProjection;
	DirectX::XMMATRIX View;
	DirectX::XMMATRIX Projection;
	DirectX::XMMATRIX InverseTransposeModel;
	DirectX::XMVECTOR CameraPosition;
	DirectX::XMMATRIX InverseProjection;
	DirectX::XMMATRIX InverseView;

	void XM_CALLCONV Compute(DirectX::CXMMATRIX model, DirectX::CXMMATRIX view,
		DirectX::CXMMATRIX viewProjection, DirectX::CXMMATRIX projection);
};
