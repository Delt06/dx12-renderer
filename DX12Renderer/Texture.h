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
 *  @file Texture.h
 *  @date October 24, 2018
 *  @author Jeremiah van Oosten
 *
 *  @brief A wrapper for a DX12 Texture object.
 */


#include "DescriptorAllocation.h"
#include "TextureUsageType.h"

#include "d3dx12.h"

#include <mutex>
#include <unordered_map>

#include "ResourceWrapper.h"

class Texture : public ResourceWrapper
{
public:
	explicit Texture(TextureUsageType textureUsage = TextureUsageType::Albedo,
	                 const std::wstring& name = L"");
	explicit Texture(const D3D12_RESOURCE_DESC& resourceDesc,
	                 const D3D12_CLEAR_VALUE* clearValue = nullptr,
	                 TextureUsageType textureUsage = TextureUsageType::Albedo,
	                 const std::wstring& name = L"");
	explicit Texture(Microsoft::WRL::ComPtr<ID3D12Resource> resource,
	                 TextureUsageType textureUsage = TextureUsageType::Albedo,
	                 const std::wstring& name = L"");

	Texture(const Texture& copy);
	Texture(Texture&& copy);

	Texture& operator=(const Texture& other);
	Texture& operator=(Texture&& other);

	~Texture() override;

	TextureUsageType GetTextureUsage() const
	{
		return TextureUsage;
	}

	void SetTextureUsage(TextureUsageType textureUsage)
	{
		TextureUsage = textureUsage;
	}

	/**
	 * Resize the texture.
	 */
	void Resize(uint32_t width, uint32_t height, uint32_t depthOrArraySize = 1);

	/**
	 * Create SRV and UAVs for the resource.
	 */
	virtual void CreateViews();

	/**
	* Get the SRV for a resource.
	*
	* @param dxgiFormat The required format of the resource. When accessing a
	* depth-stencil buffer as a shader resource view, the format will be different.
	*/
	D3D12_CPU_DESCRIPTOR_HANDLE
	GetShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc = nullptr) const override;

	/**
	* Get the UAV for a (sub)resource.
	*/
	D3D12_CPU_DESCRIPTOR_HANDLE
	GetUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc = nullptr) const override;

	/**
	 * Get the RTV for the texture.
	 */
	virtual D3D12_CPU_DESCRIPTOR_HANDLE GetRenderTargetView() const;

	/**
	 * Get the DSV for the texture.
	 */
	virtual D3D12_CPU_DESCRIPTOR_HANDLE GetDepthStencilView() const;

	static bool CheckSRVSupport(D3D12_FORMAT_SUPPORT1 formatSupport)
	{
		return ((formatSupport & D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE) != 0 ||
			(formatSupport & D3D12_FORMAT_SUPPORT1_SHADER_LOAD) != 0);
	}

	static bool CheckRTVSupport(D3D12_FORMAT_SUPPORT1 formatSupport)
	{
		return ((formatSupport & D3D12_FORMAT_SUPPORT1_RENDER_TARGET) != 0);
	}

	static bool CheckUAVSupport(D3D12_FORMAT_SUPPORT1 formatSupport)
	{
		return ((formatSupport & D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW) != 0);
	}

	static bool CheckDSVSupport(D3D12_FORMAT_SUPPORT1 formatSupport)
	{
		return ((formatSupport & D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL) != 0);
	}

	static bool IsUAVCompatibleFormat(DXGI_FORMAT format);
	static bool IsSRGBFormat(DXGI_FORMAT format);
	static bool IsBGRFormat(DXGI_FORMAT format);
	static bool IsDepthFormat(DXGI_FORMAT format);

	// Return a typeless format from the given format.
	static DXGI_FORMAT GetTypelessFormat(DXGI_FORMAT format);

protected:
private:
	DescriptorAllocation CreateShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc) const;
	DescriptorAllocation CreateUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc) const;

	mutable std::unordered_map<size_t, DescriptorAllocation> ShaderResourceViews;
	mutable std::unordered_map<size_t, DescriptorAllocation> UnorderedAccessViews;

	mutable std::mutex ShaderResourceViewsMutex;
	mutable std::mutex UnorderedAccessViewsMutex;

	DescriptorAllocation RenderTargetView;
	DescriptorAllocation DepthStencilView;

	TextureUsageType TextureUsage;
};
