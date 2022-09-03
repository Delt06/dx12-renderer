#include "DX12LibPCH.h"

#include "UploadBuffer.h"

#include <memory>

#include "Application.h"
#include "Helpers.h"

#include "d3dx12.h"

#include <new>

UploadBuffer::UploadBuffer(const size_t pageSize) :
	PageSize(pageSize)
{
}

UploadBuffer::Allocation UploadBuffer::Allocate(const size_t sizeInBytes, const size_t alignment)
{
	if (sizeInBytes > PageSize)
	{
		throw std::bad_alloc();
	}

	if (!CurrentPage || !CurrentPage->HasSpace(sizeInBytes, alignment))
	{
		CurrentPage = RequestPage();
	}

	return CurrentPage->Allocate(sizeInBytes, alignment);
}

std::shared_ptr<UploadBuffer::Page> UploadBuffer::RequestPage()
{
	std::shared_ptr<Page> page;

	if (!AvailablePages.empty())
	{
		page = AvailablePages.front();
		AvailablePages.pop_front();
	}
	else
	{
		page = std::make_shared<Page>(PageSize);
		PagePool.push_back(page);
	}

	return page;
}


void UploadBuffer::Reset()
{
	CurrentPage = nullptr;
	// Reset all available pages
	AvailablePages = PagePool;

	for (const auto page : AvailablePages)
	{
		// Reset the page for new allocations
		page->Reset();
	}
}

UploadBuffer::Page::Page(const size_t sizeInBytes) :
	CpuPtr(nullptr),
	GpuPtr(static_cast<D3D12_GPU_VIRTUAL_ADDRESS>(0)),
	SizeInBytes(sizeInBytes),
	OffsetInBytes(0)
{
	const auto device = Application::Get().GetDevice();
	const auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	const auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(SizeInBytes);

	ThrowIfFailed(device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&Resource)
	));

	GpuPtr = Resource->GetGPUVirtualAddress();
	Resource->Map(0, nullptr, &CpuPtr);
}

UploadBuffer::Page::~Page()
{
	Resource->Unmap(0, nullptr);
	CpuPtr = nullptr;
	GpuPtr = static_cast<D3D12_GPU_VIRTUAL_ADDRESS>(0);
}

bool UploadBuffer::Page::HasSpace(const size_t sizeInBytes, const size_t alignment) const
{
	const size_t alignedSize = Math::AlignUp(sizeInBytes, alignment);
	const size_t alignedOffset = Math::AlignUp(OffsetInBytes, alignment);

	return alignedOffset + alignedSize <= SizeInBytes;
}

UploadBuffer::Allocation UploadBuffer::Page::Allocate(const size_t sizeInBytes, const size_t alignment)
{
	if (!HasSpace(sizeInBytes, alignment))
	{
		throw std::bad_alloc();
	}

	const size_t alignedSize = Math::AlignUp(sizeInBytes, alignment);
	OffsetInBytes = Math::AlignUp(OffsetInBytes, alignment);

	Allocation allocation;
	allocation.Cpu = static_cast<uint8_t*>(CpuPtr) + OffsetInBytes;
	allocation.Gpu = GpuPtr + OffsetInBytes;

	OffsetInBytes += alignedSize;

	return allocation;
}

void UploadBuffer::Page::Reset()
{
	OffsetInBytes = 0;
}
