#include "DX12LibPCH.h"

#include "Texture.h"

#include "Application.h"
#include "Helpers.h"
#include "ResourceStateTracker.h"

Texture::Texture(TextureUsageType textureUsage, const std::wstring& name)
	: Resource(name)
	, m_TextureUsage(textureUsage)
{
}

Texture::Texture(const D3D12_RESOURCE_DESC& resourceDesc, const ClearValue& clearValue /*= {}*/, TextureUsageType textureUsage /*= TextureUsageType::Albedo*/, const std::wstring& name /*= L""*/)
	: Texture(resourceDesc, clearValue.GetD3D12ClearValue(), textureUsage, name)
{

}

Texture::Texture(const D3D12_RESOURCE_DESC& resourceDesc,
	const D3D12_CLEAR_VALUE* clearValue,
	TextureUsageType textureUsage,
	const std::wstring& name)
	: Resource(resourceDesc, clearValue, name)
	, m_TextureUsage(textureUsage)
{
	CreateViews();
}

Texture::Texture(ComPtr<ID3D12Resource> resource,
	TextureUsageType textureUsage,
	const std::wstring& name)
	: Resource(resource, name)
	, m_TextureUsage(textureUsage)
{
	CreateViews();
}

Texture::Texture(const Texture& copy)
	: Resource(copy)
{
	CreateViews();
}

Texture::Texture(Texture&& copy)
	: Resource(copy)
{
	CreateViews();
}



Texture& Texture::operator=(const Texture& other)
{
	Resource::operator=(other);

	CreateViews();

	return *this;
}

Texture& Texture::operator=(Texture&& other)
{
	Resource::operator=(other);

	CreateViews();

	return *this;
}

Texture::~Texture()
{
}

void Texture::Resize(uint32_t width, uint32_t height, uint32_t depthOrArraySize)
{
	// Resource can't be resized if it was never created in the first place.
	if (m_d3d12Resource)
	{
		ResourceStateTracker::RemoveGlobalResourceState(m_d3d12Resource.Get());

		CD3DX12_RESOURCE_DESC resDesc(m_d3d12Resource->GetDesc());

		resDesc.Width = std::max(width, 1u);
		resDesc.Height = std::max(height, 1u);
		resDesc.DepthOrArraySize = depthOrArraySize;

		auto device = Application::Get().GetDevice();

		{
			auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
			ThrowIfFailed(device->CreateCommittedResource(
				&heapProperties,
				D3D12_HEAP_FLAG_NONE,
				&resDesc,
				D3D12_RESOURCE_STATE_COMMON,
				m_d3d12ClearValue.get(),
				IID_PPV_ARGS(&m_d3d12Resource)
			));
		}


		// Retain the name of the resource if one was already specified.
		m_d3d12Resource->SetName(m_ResourceName.c_str());

		ResourceStateTracker::AddGlobalResourceState(m_d3d12Resource.Get(), D3D12_RESOURCE_STATE_COMMON);

		CreateViews();
	}
}

// Get a UAV description that matches the resource description.
D3D12_UNORDERED_ACCESS_VIEW_DESC GetUAVDesc(const D3D12_RESOURCE_DESC& resDesc, UINT mipSlice, UINT arraySlice = 0,
	UINT planeSlice = 0)
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = resDesc.Format;

	switch (resDesc.Dimension)
	{
	case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
		if (resDesc.DepthOrArraySize > 1)
		{
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
			uavDesc.Texture1DArray.ArraySize = resDesc.DepthOrArraySize - arraySlice;
			uavDesc.Texture1DArray.FirstArraySlice = arraySlice;
			uavDesc.Texture1DArray.MipSlice = mipSlice;
		}
		else
		{
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
			uavDesc.Texture1D.MipSlice = mipSlice;
		}
		break;
	case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
		if (resDesc.DepthOrArraySize > 1)
		{
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
			uavDesc.Texture2DArray.ArraySize = resDesc.DepthOrArraySize - arraySlice;
			uavDesc.Texture2DArray.FirstArraySlice = arraySlice;
			uavDesc.Texture2DArray.PlaneSlice = planeSlice;
			uavDesc.Texture2DArray.MipSlice = mipSlice;
		}
		else
		{
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			uavDesc.Texture2D.PlaneSlice = planeSlice;
			uavDesc.Texture2D.MipSlice = mipSlice;
		}
		break;
	case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
		uavDesc.Texture3D.WSize = resDesc.DepthOrArraySize - arraySlice;
		uavDesc.Texture3D.FirstWSlice = arraySlice;
		uavDesc.Texture3D.MipSlice = mipSlice;
		break;
	default:
		throw std::exception("Invalid resource dimension.");
	}

	return uavDesc;
}

void Texture::CreateViews()
{
	if (m_d3d12Resource)
	{
		auto& app = Application::Get();
		const auto device = app.GetDevice();

		const CD3DX12_RESOURCE_DESC desc(m_d3d12Resource->GetDesc());
		const UINT16 arraySize = desc.ArraySize();
		const UINT16 mipLevels = desc.MipLevels;
		const UINT16 numDescriptors = arraySize * mipLevels + 1;
		// first descriptor is whole resource, other are individual pieces of the array

		if ((desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) != 0 &&
			CheckRtvSupport())
		{
			m_RenderTargetView = app.AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, numDescriptors);
			device->CreateRenderTargetView(m_d3d12Resource.Get(), nullptr,
				m_RenderTargetView.GetDescriptorHandle(0));

			for (UINT16 arrayIndex = 0; arrayIndex < arraySize; arrayIndex++)
			{
				for (UINT16 mipLevel = 0; mipLevel < mipLevels; ++mipLevel)
				{
					D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
					rtvDesc.Format = desc.Format;
					rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
					rtvDesc.Texture2DArray.ArraySize = 1;
					rtvDesc.Texture2DArray.PlaneSlice = 0;
					rtvDesc.Texture2DArray.MipSlice = mipLevel;
					rtvDesc.Texture2DArray.FirstArraySlice = arrayIndex;

					const UINT16 offset = GetRenderTargetSubresourceIndex(arrayIndex, mipLevel) + 1;
					device->CreateRenderTargetView(m_d3d12Resource.Get(), &rtvDesc,
						m_RenderTargetView.GetDescriptorHandle(offset));
				}

			}
		}
		if ((desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0 &&
			CheckDsvSupport())
		{
			m_DepthStencilView = app.AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, numDescriptors);
			device->CreateDepthStencilView(m_d3d12Resource.Get(), nullptr,
				m_DepthStencilView.GetDescriptorHandle(0));

			for (UINT16 i = 0; i < arraySize; ++i)
			{
				D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
				dsvDesc.Format = desc.Format;
				dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
				dsvDesc.Texture2DArray.ArraySize = 1;
				dsvDesc.Texture2DArray.MipSlice = 0;
				dsvDesc.Texture2DArray.FirstArraySlice = i;

				device->CreateDepthStencilView(m_d3d12Resource.Get(), &dsvDesc,
					m_DepthStencilView.GetDescriptorHandle(i + 1));
			}
		}
	}

	std::lock_guard<std::mutex> lock(m_ShaderResourceViewsMutex);
	std::lock_guard<std::mutex> guard(m_UnorderedAccessViewsMutex);

	// SRVs and UAVs will be created as needed.
	m_ShaderResourceViews.clear();
	m_UnorderedAccessViews.clear();
}

DescriptorAllocation Texture::CreateShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc) const
{
	auto& app = Application::Get();
	const auto device = app.GetDevice();
	auto srv = app.AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	device->CreateShaderResourceView(m_d3d12Resource.Get(), srvDesc, srv.GetDescriptorHandle());

	return srv;
}

DescriptorAllocation Texture::CreateUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc) const
{
	auto& app = Application::Get();
	const auto device = app.GetDevice();
	auto uav = app.AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	device->CreateUnorderedAccessView(m_d3d12Resource.Get(), nullptr, uavDesc, uav.GetDescriptorHandle());

	return uav;
}


D3D12_CPU_DESCRIPTOR_HANDLE Texture::GetShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc) const
{
	std::size_t hash = 0;
	if (srvDesc)
	{
		hash = std::hash<D3D12_SHADER_RESOURCE_VIEW_DESC>{}(*srvDesc);
	}

	std::lock_guard<std::mutex> lock(m_ShaderResourceViewsMutex);

	auto iter = m_ShaderResourceViews.find(hash);
	if (iter == m_ShaderResourceViews.end())
	{
		auto srv = CreateShaderResourceView(srvDesc);
		iter = m_ShaderResourceViews.insert({ hash, std::move(srv) }).first;
	}

	return iter->second.GetDescriptorHandle();
}

D3D12_CPU_DESCRIPTOR_HANDLE Texture::GetUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc) const
{
	std::size_t hash = 0;
	if (uavDesc)
	{
		hash = std::hash<D3D12_UNORDERED_ACCESS_VIEW_DESC>{}(*uavDesc);
	}

	std::lock_guard<std::mutex> guard(m_UnorderedAccessViewsMutex);

	auto iter = m_UnorderedAccessViews.find(hash);
	if (iter == m_UnorderedAccessViews.end())
	{
		auto uav = CreateUnorderedAccessView(uavDesc);
		iter = m_UnorderedAccessViews.insert({ hash, std::move(uav) }).first;
	}

	return iter->second.GetDescriptorHandle();
}

D3D12_CPU_DESCRIPTOR_HANDLE Texture::GetRenderTargetView() const
{
	return m_RenderTargetView.GetDescriptorHandle();
}

D3D12_CPU_DESCRIPTOR_HANDLE Texture::GetRenderTargetViewArray(const uint32_t index, uint32_t mipLevel) const
{
	uint32_t offset = GetRenderTargetSubresourceIndex(index, mipLevel) + 1;
	return m_RenderTargetView.GetDescriptorHandle(offset);
}

D3D12_CPU_DESCRIPTOR_HANDLE Texture::GetDepthStencilView() const
{
	return m_DepthStencilView.GetDescriptorHandle();
}

D3D12_CPU_DESCRIPTOR_HANDLE Texture::GetDepthStencilViewArray(const uint32_t index) const
{
	return m_DepthStencilView.GetDescriptorHandle(index + 1);
}


bool Texture::IsUavCompatibleFormat(DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_R32G32B32A32_FLOAT:
	case DXGI_FORMAT_R32G32B32A32_UINT:
	case DXGI_FORMAT_R32G32B32A32_SINT:
	case DXGI_FORMAT_R16G16B16A16_FLOAT:
	case DXGI_FORMAT_R16G16B16A16_UINT:
	case DXGI_FORMAT_R16G16B16A16_SINT:
	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_R8G8B8A8_UINT:
	case DXGI_FORMAT_R8G8B8A8_SINT:
	case DXGI_FORMAT_R32_FLOAT:
	case DXGI_FORMAT_R32_UINT:
	case DXGI_FORMAT_R32_SINT:
	case DXGI_FORMAT_R16_FLOAT:
	case DXGI_FORMAT_R16_UINT:
	case DXGI_FORMAT_R16_SINT:
	case DXGI_FORMAT_R8_UNORM:
	case DXGI_FORMAT_R8_UINT:
	case DXGI_FORMAT_R8_SINT:
		return true;
	default:
		return false;
	}
}

bool Texture::IsSRgbFormat(DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
	case DXGI_FORMAT_BC1_UNORM_SRGB:
	case DXGI_FORMAT_BC2_UNORM_SRGB:
	case DXGI_FORMAT_BC3_UNORM_SRGB:
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
	case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
	case DXGI_FORMAT_BC7_UNORM_SRGB:
		return true;
	default:
		return false;
	}
}

bool Texture::IsBgrFormat(DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_B8G8R8A8_UNORM:
	case DXGI_FORMAT_B8G8R8X8_UNORM:
	case DXGI_FORMAT_B8G8R8A8_TYPELESS:
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
	case DXGI_FORMAT_B8G8R8X8_TYPELESS:
	case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
		return true;
	default:
		return false;
	}
}

bool Texture::IsDepthFormat(DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
	case DXGI_FORMAT_D32_FLOAT:
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
	case DXGI_FORMAT_D16_UNORM:
		return true;
	default:
		return false;
	}
}

DXGI_FORMAT Texture::GetTypelessFormat(DXGI_FORMAT format)
{
	DXGI_FORMAT typelessFormat = format;

	switch (format)
	{
	case DXGI_FORMAT_R32G32B32A32_FLOAT:
	case DXGI_FORMAT_R32G32B32A32_UINT:
	case DXGI_FORMAT_R32G32B32A32_SINT:
		typelessFormat = DXGI_FORMAT_R32G32B32A32_TYPELESS;
		break;
	case DXGI_FORMAT_R32G32B32_FLOAT:
	case DXGI_FORMAT_R32G32B32_UINT:
	case DXGI_FORMAT_R32G32B32_SINT:
		typelessFormat = DXGI_FORMAT_R32G32B32_TYPELESS;
		break;
	case DXGI_FORMAT_R16G16B16A16_FLOAT:
	case DXGI_FORMAT_R16G16B16A16_UNORM:
	case DXGI_FORMAT_R16G16B16A16_UINT:
	case DXGI_FORMAT_R16G16B16A16_SNORM:
	case DXGI_FORMAT_R16G16B16A16_SINT:
		typelessFormat = DXGI_FORMAT_R16G16B16A16_TYPELESS;
		break;
	case DXGI_FORMAT_R32G32_FLOAT:
	case DXGI_FORMAT_R32G32_UINT:
	case DXGI_FORMAT_R32G32_SINT:
		typelessFormat = DXGI_FORMAT_R32G32_TYPELESS;
		break;
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		typelessFormat = DXGI_FORMAT_R32G8X24_TYPELESS;
		break;
	case DXGI_FORMAT_R10G10B10A2_UNORM:
	case DXGI_FORMAT_R10G10B10A2_UINT:
		typelessFormat = DXGI_FORMAT_R10G10B10A2_TYPELESS;
		break;
	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
	case DXGI_FORMAT_R8G8B8A8_UINT:
	case DXGI_FORMAT_R8G8B8A8_SNORM:
	case DXGI_FORMAT_R8G8B8A8_SINT:
		typelessFormat = DXGI_FORMAT_R8G8B8A8_TYPELESS;
		break;
	case DXGI_FORMAT_R16G16_FLOAT:
	case DXGI_FORMAT_R16G16_UNORM:
	case DXGI_FORMAT_R16G16_UINT:
	case DXGI_FORMAT_R16G16_SNORM:
	case DXGI_FORMAT_R16G16_SINT:
		typelessFormat = DXGI_FORMAT_R16G16_TYPELESS;
		break;
	case DXGI_FORMAT_D32_FLOAT:
	case DXGI_FORMAT_R32_FLOAT:
	case DXGI_FORMAT_R32_UINT:
	case DXGI_FORMAT_R32_SINT:
		typelessFormat = DXGI_FORMAT_R32_TYPELESS;
		break;
	case DXGI_FORMAT_R8G8_UNORM:
	case DXGI_FORMAT_R8G8_UINT:
	case DXGI_FORMAT_R8G8_SNORM:
	case DXGI_FORMAT_R8G8_SINT:
		typelessFormat = DXGI_FORMAT_R8G8_TYPELESS;
		break;
	case DXGI_FORMAT_R16_FLOAT:
	case DXGI_FORMAT_D16_UNORM:
	case DXGI_FORMAT_R16_UNORM:
	case DXGI_FORMAT_R16_UINT:
	case DXGI_FORMAT_R16_SNORM:
	case DXGI_FORMAT_R16_SINT:
		typelessFormat = DXGI_FORMAT_R16_TYPELESS;
	case DXGI_FORMAT_R8_UNORM:
	case DXGI_FORMAT_R8_UINT:
	case DXGI_FORMAT_R8_SNORM:
	case DXGI_FORMAT_R8_SINT:
		typelessFormat = DXGI_FORMAT_R8_TYPELESS;
		break;
	case DXGI_FORMAT_BC1_UNORM:
	case DXGI_FORMAT_BC1_UNORM_SRGB:
		typelessFormat = DXGI_FORMAT_BC1_TYPELESS;
		break;
	case DXGI_FORMAT_BC2_UNORM:
	case DXGI_FORMAT_BC2_UNORM_SRGB:
		typelessFormat = DXGI_FORMAT_BC2_TYPELESS;
		break;
	case DXGI_FORMAT_BC3_UNORM:
	case DXGI_FORMAT_BC3_UNORM_SRGB:
		typelessFormat = DXGI_FORMAT_BC3_TYPELESS;
		break;
	case DXGI_FORMAT_BC4_UNORM:
	case DXGI_FORMAT_BC4_SNORM:
		typelessFormat = DXGI_FORMAT_BC4_TYPELESS;
		break;
	case DXGI_FORMAT_BC5_UNORM:
	case DXGI_FORMAT_BC5_SNORM:
		typelessFormat = DXGI_FORMAT_BC5_TYPELESS;
		break;
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		typelessFormat = DXGI_FORMAT_B8G8R8A8_TYPELESS;
		break;
	case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
		typelessFormat = DXGI_FORMAT_B8G8R8X8_TYPELESS;
		break;
	case DXGI_FORMAT_BC6H_UF16:
	case DXGI_FORMAT_BC6H_SF16:
		typelessFormat = DXGI_FORMAT_BC6H_TYPELESS;
		break;
	case DXGI_FORMAT_BC7_UNORM:
	case DXGI_FORMAT_BC7_UNORM_SRGB:
		typelessFormat = DXGI_FORMAT_BC7_TYPELESS;
		break;
	}

	return typelessFormat;
}

DXGI_FORMAT Texture::GetSRgbFormat(const DXGI_FORMAT format)
{
	DXGI_FORMAT sRgbFormat = format;
	switch (format)
	{
	case DXGI_FORMAT_R8G8B8A8_UNORM:
		sRgbFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		break;
	case DXGI_FORMAT_BC1_UNORM:
		sRgbFormat = DXGI_FORMAT_BC1_UNORM_SRGB;
		break;
	case DXGI_FORMAT_BC2_UNORM:
		sRgbFormat = DXGI_FORMAT_BC2_UNORM_SRGB;
		break;
	case DXGI_FORMAT_BC3_UNORM:
		sRgbFormat = DXGI_FORMAT_BC3_UNORM_SRGB;
		break;
	case DXGI_FORMAT_B8G8R8A8_UNORM:
		sRgbFormat = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
		break;
	case DXGI_FORMAT_B8G8R8X8_UNORM:
		sRgbFormat = DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;
		break;
	case DXGI_FORMAT_BC7_UNORM:
		sRgbFormat = DXGI_FORMAT_BC7_UNORM_SRGB;
		break;
	}

	return sRgbFormat;
}

DXGI_FORMAT Texture::GetUavCompatibleFormat(const DXGI_FORMAT format)
{
	DXGI_FORMAT uavFormat = format;

	switch (format)
	{
	case DXGI_FORMAT_R8G8B8A8_TYPELESS:
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
	case DXGI_FORMAT_B8G8R8A8_UNORM:
	case DXGI_FORMAT_B8G8R8X8_UNORM:
	case DXGI_FORMAT_B8G8R8A8_TYPELESS:
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
	case DXGI_FORMAT_B8G8R8X8_TYPELESS:
	case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
		uavFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		break;
	case DXGI_FORMAT_R32_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT:
		uavFormat = DXGI_FORMAT_R32_FLOAT;
		break;
	}

	return uavFormat;
}

uint32_t Texture::GetRenderTargetSubresourceIndex(UINT16 arrayIndex, UINT16 mipLevel) const
{
	CD3DX12_RESOURCE_DESC desc(GetD3D12ResourceDesc());
	return mipLevel + arrayIndex * desc.MipLevels;
}

uint32_t Texture::GetDepthStencilSubresourceIndex(UINT16 arrayIndex) const
{
	return arrayIndex;
}
