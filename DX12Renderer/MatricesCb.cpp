#include "MatricesCb.h"

void MatricesCb::Compute(DirectX::CXMMATRIX model, DirectX::CXMMATRIX view, DirectX::CXMMATRIX viewProjection)
{
	m_Model = model;
	m_ModelView = model * view;
	m_InverseTransposeModelView = XMMatrixTranspose(XMMatrixInverse(nullptr, m_ModelView));
	m_ModelViewProjection = model * viewProjection;
}
