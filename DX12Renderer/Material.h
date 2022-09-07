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
 *  @file Material.h
 *  @date October 24, 2018
 *  @author Jeremiah van Oosten
 *
 *  @brief Material structure that uses HLSL constant buffer padding rules.
 */

#include <DirectXMath.h>

class Material
{
public:
	Material(const DirectX::XMFLOAT4 emissive = {0.0f, 0.0f, 0.0f, 1.0f},
	         const DirectX::XMFLOAT4 ambient = {0.1f, 0.1f, 0.1f, 1.0f},
	         const DirectX::XMFLOAT4 diffuse = {1.0f, 1.0f, 1.0f, 1.0f},
	         const DirectX::XMFLOAT4 specular = {1.0f, 1.0f, 1.0f, 1.0f},
	         const float specularPower = 128.0f
	)
		: Emissive(emissive)
		  , Ambient(ambient)
		  , Diffuse(diffuse)
		  , Specular(specular)
		  , SpecularPower(specularPower)
	{
	}

	DirectX::XMFLOAT4 Emissive;
	DirectX::XMFLOAT4 Ambient;
	DirectX::XMFLOAT4 Diffuse;
	DirectX::XMFLOAT4 Specular;

	float SpecularPower;
	uint32_t Padding[3];

	uint32_t HasDiffuseMap{};
	uint32_t HasNormalMap{};
	uint32_t HasSpecularMap{};
	uint32_t HasGlossMap{};
};
