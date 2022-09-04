#include "DX12LibPCH.h"

#include "DescriptorAllocation.h"

#include "Application.h"
#include "DescriptorAllocatorPage.h"

DescriptorAllocation::DescriptorAllocation() :
	Descriptor{0},
	NumHandles(0),
	DescriptorSize(0),
	Page(nullptr)
{
}

DescriptorAllocation::DescriptorAllocation(const D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle,
                                           const uint32_t numHandles,
                                           const uint32_t descriptorSize,
                                           const std::shared_ptr<DescriptorAllocatorPage> page) :
	Descriptor(descriptorHandle),
	NumHandles(numHandles),
	DescriptorSize(descriptorSize),
	Page(page)
{
}


DescriptorAllocation::~DescriptorAllocation()
{
	Free();
}

DescriptorAllocation::DescriptorAllocation(DescriptorAllocation&& allocation) :
	Descriptor(allocation.Descriptor),
	NumHandles(allocation.NumHandles),
	DescriptorSize(allocation.DescriptorSize),
	Page(std::move(allocation.Page))
{
	allocation.Descriptor.ptr = 0;
	allocation.NumHandles = 0;
	allocation.DescriptorSize = 0;
}


DescriptorAllocation& DescriptorAllocation::operator=(DescriptorAllocation&& other)
{
	Free();

	Descriptor = other.Descriptor;
	NumHandles = other.NumHandles;
	DescriptorSize = other.DescriptorSize;
	Page = std::move(other.Page);

	other.Descriptor.ptr = 0;
	other.NumHandles = 0;
	other.DescriptorSize = 0;

	return *this;
}

void DescriptorAllocation::Free()
{
	if (!IsNull() && Page)
	{
		Page->Free(std::move(*this), Application::GetFrameCount());

		Descriptor.ptr = 0;
		NumHandles = 0;
		DescriptorSize = 0;
		Page.reset();
	}
}

bool DescriptorAllocation::IsNull() const
{
	return Descriptor.ptr == 0;
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorAllocation::GetDescriptorHandle(const uint32_t offset) const
{
	assert(offset < NumHandles);
	return {Descriptor.ptr + static_cast<SIZE_T>(DescriptorSize) * offset};
}

uint32_t DescriptorAllocation::GetNumHandles() const
{
	return NumHandles;
}

std::shared_ptr<DescriptorAllocatorPage> DescriptorAllocation::GetDescriptorAllocatorPage() const
{
	return Page;
}
