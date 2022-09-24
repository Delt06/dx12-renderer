﻿#include "DX12LibPCH.h"

#include "IndexBuffer.h"

#include <cassert>

IndexBuffer::IndexBuffer(const std::wstring& name)
	: Buffer(name)
	  , NumIndices(0)
	  , IndexFormat(DXGI_FORMAT_UNKNOWN)
	  , IndexBufferView({})
{
}

IndexBuffer::~IndexBuffer() = default;

void IndexBuffer::CreateViews(const size_t numElements, const size_t elementSize)
{
	assert(elementSize == 2 || elementSize == 4 && "Indices must be 16, or 32-bit integers.");

	NumIndices = numElements;
	IndexFormat = (elementSize == 2) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;

	IndexBufferView.BufferLocation = m_d3d12Resource->GetGPUVirtualAddress();
	IndexBufferView.SizeInBytes = static_cast<UINT>(numElements * elementSize);
	IndexBufferView.Format = IndexFormat;
}

D3D12_CPU_DESCRIPTOR_HANDLE IndexBuffer::GetShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc) const
{
	throw std::exception("IndexBuffer::GetShaderResourceView should not be called.");
}

D3D12_CPU_DESCRIPTOR_HANDLE IndexBuffer::GetUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc) const
{
	throw std::exception("IndexBuffer::GetUnorderedAccessView should not be called.");
}
