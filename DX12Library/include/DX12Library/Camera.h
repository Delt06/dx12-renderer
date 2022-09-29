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
  *  @file Camera.h
  *  @date October 24, 2018
  *  @author Jeremiah van Oosten
  *
  *  @brief DirectX 12 Camera class.
  */

#include <DirectXMath.h>

enum class Space
{
	Local,
	World,
};

class Camera
{
public:
	Camera();
	virtual ~Camera();

	void XM_CALLCONV SetLookAt(DirectX::FXMVECTOR eye, DirectX::FXMVECTOR target, DirectX::FXMVECTOR up);
	DirectX::XMMATRIX GetViewMatrix() const;
	DirectX::XMMATRIX GetInverseViewMatrix() const;

	/**
	 * Set the camera to a perspective projection matrix.
	 * @param vFov The vertical field of view in degrees.
	 * @param aspectRatio The aspect ratio of the screen.
	 * @param zNear The distance to the near clipping plane.
	 * @param zFar The distance to the far clipping plane.
	 */
	void SetProjection(float vFov, float aspectRatio, float zNear, float zFar);
	DirectX::XMMATRIX GetProjectionMatrix() const;
	DirectX::XMMATRIX GetInverseProjectionMatrix() const;

	/**
	 * \brief Set the FoV.
	 * \param vFov vertical FoV (degrees)
	 */
	void SetFov(float vFov);


	/**
	 * \brief Get the FoV.
	 * \param vFov vertical FoV (degrees)
	 */
	float GetFov() const;

	void XM_CALLCONV SetTranslation(DirectX::FXMVECTOR translation);
	DirectX::XMVECTOR GetTranslation() const;

	void XM_CALLCONV SetRotation(DirectX::FXMVECTOR qRotation);
	DirectX::XMVECTOR GetRotation() const;

	void XM_CALLCONV Translate(DirectX::FXMVECTOR translation, Space space = Space::Local);
	void Rotate(DirectX::FXMVECTOR qRotation);

protected:
	virtual void UpdateViewMatrix() const;
	virtual void UpdateInverseViewMatrix() const;
	virtual void UpdateProjectionMatrix() const;
	virtual void UpdateInverseProjectionMatrix() const;

	static constexpr size_t ALIGNMENT = 16;

	// This data must be aligned otherwise the SSE intrinsics fail
	// and throw exceptions.
	__declspec(align(ALIGNMENT)) struct AlignedData
	{
		// World-space position of the camera.
		DirectX::XMVECTOR Translation;
		// World-space rotation of the camera.
		// THIS IS A QUATERNION!!!!
		DirectX::XMVECTOR QRotation;

		DirectX::XMMATRIX ViewMatrix, InverseViewMatrix;
		DirectX::XMMATRIX ProjectionMatrix, InverseProjectionMatrix;
	};

	AlignedData* PData;

	float VFov;
	float AspectRatio;
	float ZNear;
	float ZFar;

	mutable bool ViewDirty, InverseViewDirty;
	mutable bool ProjectionDirty, InverseProjectionDirty;
};
