#include "VertexBuffer.h"

#include "DX12LibPCH.h"


VertexBuffer::VertexBuffer(const std::wstring& name)
	: Buffer(name)
	  , NumVertices(0)
	  , VertexStride(0)
	  , VertexBufferView({})
{
}

VertexBuffer::~VertexBuffer()
{
}

void VertexBuffer::CreateViews(const size_t numElements, const size_t elementSize)
{
	NumVertices = numElements;
	VertexStride = elementSize;

	VertexBufferView.BufferLocation = D3d12Resource->GetGPUVirtualAddress();
	VertexBufferView.SizeInBytes = static_cast<UINT>(NumVertices * VertexStride);
	VertexBufferView.StrideInBytes = static_cast<UINT>(VertexStride);
}

D3D12_CPU_DESCRIPTOR_HANDLE VertexBuffer::GetShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc) const
{
	throw std::exception("VertexBuffer::GetShaderResourceView should not be called.");
}

D3D12_CPU_DESCRIPTOR_HANDLE VertexBuffer::GetUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc) const
{
	throw std::exception("VertexBuffer::GetUnorderedAccessView should not be called.");
}
