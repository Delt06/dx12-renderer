#pragma once

#include <DX12Library/Buffer.h>
#include <DX12Library/DescriptorAllocation.h>

class MultiConstantBuffer : public Buffer
{
public:
	explicit MultiConstantBuffer(size_t numElements, const std::wstring& name = L"");
	~MultiConstantBuffer() override;

	void CreateViews(size_t numElements, size_t elementSize) override;

	size_t GetSizeInBytes() const
	{
		return m_SizeInBytes;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE GetConstantBufferView(UINT index) const
	{
		return m_ConstantBufferViewBase.GetDescriptorHandle(index);
	}

    D3D12_GPU_VIRTUAL_ADDRESS GetItemGPUAddress(UINT index) const
    {
        return m_d3d12Resource->GetGPUVirtualAddress() + m_AlignedItemSize * index;
    }

	/**
	* Get the SRV for a resource.
	*/
	D3D12_CPU_DESCRIPTOR_HANDLE
		GetShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc = nullptr) const override;

	/**
	* Get the UAV for a (sub)resource.
	*/
	D3D12_CPU_DESCRIPTOR_HANDLE
		GetUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc = nullptr) const override;


protected:
private:
	size_t m_SizeInBytes;
    size_t m_AlignedItemSize;
	DescriptorAllocation m_ConstantBufferViewBase;
};
