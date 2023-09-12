#pragma once

/*
 *  Copyright(c) 2018 Jeremiah van Oosten
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files(the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions :
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 *  IN THE SOFTWARE.
 */

 /**
  *  @file StructuredBuffer.h
  *  @date October 24, 2018
  *  @author Jeremiah van Oosten
  *
  *  @brief Structured buffer resource.
  */

#include "Buffer.h"

#include <memory>
#include <string>

#include "DescriptorAllocation.h"
#include "ByteAddressBuffer.h"

class StructuredBuffer final : public Buffer
{
public:
    const static inline D3D12_RESOURCE_DESC COUNTER_DESC = CD3DX12_RESOURCE_DESC::Buffer(4, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	explicit StructuredBuffer(const std::wstring& name = L"");
	explicit StructuredBuffer(const D3D12_RESOURCE_DESC resourceDesc,
		size_t numElements, size_t elementSize,
		const std::wstring& name = L"");

    explicit StructuredBuffer(const D3D12_RESOURCE_DESC& resourceDesc,
        const Microsoft::WRL::ComPtr<ID3D12Heap>& pHeap,
        UINT64 heapOffset,
        size_t numElements, size_t elementSize,
        const std::wstring& name = L"");

	size_t GetNumElements() const;

	size_t GetElementSize() const;

	void CreateViews(size_t numElements, size_t elementSize) override;

	D3D12_CPU_DESCRIPTOR_HANDLE GetShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc = nullptr) const override;

	D3D12_CPU_DESCRIPTOR_HANDLE GetUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc = nullptr) const override;

	ByteAddressBuffer& GetCounterBuffer();
	const std::shared_ptr<ByteAddressBuffer>& GetCounterBufferPtr() const;

    virtual void ForEachResourceRecursive(const std::function<void(const Resource&)>& action) const override
    {
        action(*this);
        m_CounterBuffer->ForEachResourceRecursive(action);
    }

private:
	size_t m_NumElements;
	size_t m_ElementSize;

	DescriptorAllocation m_Srv;
	DescriptorAllocation m_Uav;

	std::shared_ptr<ByteAddressBuffer> m_CounterBuffer;
};

