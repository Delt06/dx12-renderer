#include "DX12LibPCH.h"

#include "UploadBuffer.h"

#include <memory>

#include "Application.h"
#include "Helpers.h"

#include "d3dx12.h"

#include <new>

UploadBuffer::UploadBuffer(const size_t pageSize) :
	m_PageSize(pageSize)
{
}

UploadBuffer::Allocation UploadBuffer::Allocate(const size_t sizeInBytes, const size_t alignment)
{
	if (sizeInBytes > m_PageSize)
	{
		throw std::bad_alloc();
	}

	if (!m_CurrentPage || !m_CurrentPage->HasSpace(sizeInBytes, alignment))
	{
		m_CurrentPage = RequestPage();
	}

	return m_CurrentPage->Allocate(sizeInBytes, alignment);
}

std::shared_ptr<UploadBuffer::Page> UploadBuffer::RequestPage()
{
	std::shared_ptr<Page> page;

	if (!m_AvailablePages.empty())
	{
		page = m_AvailablePages.front();
		m_AvailablePages.pop_front();
	}
	else
	{
		page = std::make_shared<Page>(m_PageSize);
		m_PagePool.push_back(page);
	}

	return page;
}


void UploadBuffer::Reset()
{
	m_CurrentPage = nullptr;
	// Reset all available pages
	m_AvailablePages = m_PagePool;

	for (const auto page : m_AvailablePages)
	{
		// Reset the page for new allocations
		page->Reset();
	}
}

UploadBuffer::Page::Page(const size_t sizeInBytes) :
	m_CpuPtr(nullptr),
	m_GpuPtr(static_cast<D3D12_GPU_VIRTUAL_ADDRESS>(0)),
	m_SizeInBytes(sizeInBytes),
	m_OffsetInBytes(0)
{
	const auto device = Application::Get().GetDevice();
	const auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	const auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(m_SizeInBytes);

	ThrowIfFailed(device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_Resource)
	));

	m_GpuPtr = m_Resource->GetGPUVirtualAddress();
	m_Resource->Map(0, nullptr, &m_CpuPtr);
}

UploadBuffer::Page::~Page()
{
	m_Resource->Unmap(0, nullptr);
	m_CpuPtr = nullptr;
	m_GpuPtr = static_cast<D3D12_GPU_VIRTUAL_ADDRESS>(0);
}

bool UploadBuffer::Page::HasSpace(const size_t sizeInBytes, const size_t alignment) const
{
	const size_t alignedSize = Math::AlignUp(sizeInBytes, alignment);
	const size_t alignedOffset = Math::AlignUp(m_OffsetInBytes, alignment);

	return alignedOffset + alignedSize <= m_SizeInBytes;
}

UploadBuffer::Allocation UploadBuffer::Page::Allocate(const size_t sizeInBytes, const size_t alignment)
{
	if (!HasSpace(sizeInBytes, alignment))
	{
		throw std::bad_alloc();
	}

	const size_t alignedSize = Math::AlignUp(sizeInBytes, alignment);
	m_OffsetInBytes = Math::AlignUp(m_OffsetInBytes, alignment);

	Allocation allocation;
	allocation.Cpu = static_cast<uint8_t*>(m_CpuPtr) + m_OffsetInBytes;
	allocation.Gpu = m_GpuPtr + m_OffsetInBytes;

	m_OffsetInBytes += alignedSize;

	return allocation;
}

void UploadBuffer::Page::Reset()
{
	m_OffsetInBytes = 0;
}
