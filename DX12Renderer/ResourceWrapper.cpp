#include "DX12LibPCH.h"

#include "ResourceWrapper.h"

#include "Application.h"

#include "ResourceStateTracker.h"

ResourceWrapper::ResourceWrapper(const std::wstring& name)
	: ResourceName(name)
{
}

ResourceWrapper::ResourceWrapper(const D3D12_RESOURCE_DESC& resourceDesc, const D3D12_CLEAR_VALUE* clearValue,
                                 const std::wstring& name)
{
	const auto device = Application::Get().GetDevice();

	if (clearValue)
	{
		D3d12ClearValue = std::make_unique<D3D12_CLEAR_VALUE>(*clearValue);
	}

	const auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COMMON,
		D3d12ClearValue.get(),
		IID_PPV_ARGS(&D3d12Resource)
	));

	ResourceStateTracker::AddGlobalResourceState(D3d12Resource.Get(), D3D12_RESOURCE_STATE_COMMON);

	SetName(name);
}

ResourceWrapper::ResourceWrapper(const ComPtr<ID3D12Resource> resource, const std::wstring& name)
	: D3d12Resource(resource)
{
	SetName(name);
}

ResourceWrapper::ResourceWrapper(const ResourceWrapper& copy)
	: D3d12Resource(copy.D3d12Resource)
	  , D3d12ClearValue(std::make_unique<D3D12_CLEAR_VALUE>(*copy.D3d12ClearValue))
	  , ResourceName(copy.ResourceName)
{
	int i = 3;
}

ResourceWrapper::ResourceWrapper(ResourceWrapper&& copy)
	: D3d12Resource(std::move(copy.D3d12Resource))
	  , D3d12ClearValue(std::move(copy.D3d12ClearValue))
	  , ResourceName(std::move(copy.ResourceName))
{
}

ResourceWrapper& ResourceWrapper::operator=(const ResourceWrapper& other)
{
	if (this != &other)
	{
		D3d12Resource = other.D3d12Resource;
		ResourceName = other.ResourceName;
		if (other.D3d12ClearValue)
		{
			D3d12ClearValue = std::make_unique<D3D12_CLEAR_VALUE>(*other.D3d12ClearValue);
		}
	}

	return *this;
}

ResourceWrapper& ResourceWrapper::operator=(ResourceWrapper&& other)
{
	if (this != &other)
	{
		D3d12Resource = other.D3d12Resource;
		ResourceName = other.ResourceName;
		D3d12ClearValue = std::move(other.D3d12ClearValue);

		other.D3d12Resource.Reset();
		other.ResourceName.clear();
	}

	return *this;
}


ResourceWrapper::~ResourceWrapper() = default;

void ResourceWrapper::SetD3D12Resource(const ComPtr<ID3D12Resource> d3d12Resource, const D3D12_CLEAR_VALUE* clearValue)
{
	D3d12Resource = d3d12Resource;
	if (D3d12ClearValue)
	{
		D3d12ClearValue = std::make_unique<D3D12_CLEAR_VALUE>(*clearValue);
	}
	else
	{
		D3d12ClearValue.reset();
	}
	SetName(ResourceName);
}

void ResourceWrapper::SetName(const std::wstring& name)
{
	ResourceName = name;
	if (D3d12Resource && !ResourceName.empty())
	{
		D3d12Resource->SetName(ResourceName.c_str());
	}
}

void ResourceWrapper::Reset()
{
	D3d12Resource.Reset();
	D3d12ClearValue.reset();
}
