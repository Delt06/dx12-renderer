#include "MatricesCb.h"
#include <DirectXMath.h>

using namespace DirectX;

void MatricesCb::Compute(DirectX::CXMMATRIX model, DirectX::CXMMATRIX view, DirectX::CXMMATRIX viewProjection,
	DirectX::CXMMATRIX projection)
{
	Model = model;
	ModelView = model * view;
	InverseTransposeModelView = XMMatrixTranspose(XMMatrixInverse(nullptr, ModelView));
	ModelViewProjection = model * viewProjection;
	View = view;
	Projection = projection;
	Model = model;
	InverseTransposeModel = XMMatrixTranspose(XMMatrixInverse(nullptr, model));
	CameraPosition = XMVector4Transform(XMVectorSet(0, 0, 0, 1), XMMatrixInverse(nullptr, view));
}
