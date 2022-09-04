﻿#include "DX12LibPCH.h"
#include "ByteAddressBuffer.h"
#include "Application.h"

ByteAddressBuffer::ByteAddressBuffer(const std::wstring& name)
    : Buffer(name)
{
    Srv = Application::Get().AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    Uav = Application::Get().AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

ByteAddressBuffer::ByteAddressBuffer(const D3D12_RESOURCE_DESC& resDesc,
                                     const size_t numElements, const size_t elementSize,
                                     const std::wstring& name)
    : Buffer(resDesc, numElements, elementSize, name)
{
}

void ByteAddressBuffer::CreateViews(size_t numElements, size_t elementSize)
{
    auto device = Application::Get().GetDevice();

    // Make sure buffer size is aligned to 4 bytes.
    BufferSize = Math::AlignUp(numElements * elementSize, 4);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.NumElements = static_cast<UINT>(BufferSize) / 4;
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;

    device->CreateShaderResourceView(D3d12Resource.Get(), &srvDesc, Srv.GetDescriptorHandle());

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    uavDesc.Buffer.NumElements = static_cast<UINT>(BufferSize) / 4;
    uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;

    device->CreateUnorderedAccessView(D3d12Resource.Get(), nullptr, &uavDesc, Uav.GetDescriptorHandle());
}
