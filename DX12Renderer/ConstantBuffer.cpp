﻿#include "DX12LibPCH.h"

#include "ConstantBuffer.h"

#include "Application.h"

#include <d3dx12.h>

ConstantBuffer::ConstantBuffer(const std::wstring& name)
	: Buffer(name)
	  , SizeInBytes(0)
{
	ConstantBufferView = Application::Get().AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

ConstantBuffer::~ConstantBuffer() = default;

void ConstantBuffer::CreateViews(const size_t numElements, const size_t elementSize)
{
	SizeInBytes = numElements * elementSize;

	D3D12_CONSTANT_BUFFER_VIEW_DESC d3d12ConstantBufferViewDesc;
	d3d12ConstantBufferViewDesc.BufferLocation = D3d12Resource->GetGPUVirtualAddress();
	d3d12ConstantBufferViewDesc.SizeInBytes = static_cast<UINT>(Math::AlignUp(SizeInBytes, 16));

	const auto device = Application::Get().GetDevice();

	device->CreateConstantBufferView(&d3d12ConstantBufferViewDesc, ConstantBufferView.GetDescriptorHandle());
}

D3D12_CPU_DESCRIPTOR_HANDLE ConstantBuffer::GetShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc) const
{
	throw std::exception("ConstantBuffer::GetShaderResourceView should not be called.");
}

D3D12_CPU_DESCRIPTOR_HANDLE ConstantBuffer::GetUnorderedAccessView(
	const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc) const
{
	throw std::exception("ConstantBuffer::GetUnorderedAccessView should not be called.");
}
