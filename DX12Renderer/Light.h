#pragma once
#include <DirectXMath.h>

struct DirectionalLight
{
	DirectionalLight()
		: m_DirectionWs(1.0f, 0.0f, 1.0f, 0.0f)
		  , m_DirectionVs(0.0f, 0.0f, 1.0f, 0.0f)
		  , m_Color(1.0f, 1.0f, 1.0f, 1.0f)
	{
	}

	DirectX::XMFLOAT4 m_DirectionWs; // Light direction in world space.
	//----------------------------------- (16 byte boundary)
	DirectX::XMFLOAT4 m_DirectionVs; // Light direction in view space.
	//----------------------------------- (16 byte boundary)
	DirectX::XMFLOAT4 m_Color;
	//----------------------------------- (16 byte boundary)
	// Total:                              16 * 3 = 48 bytes 
};

struct PointLight
{
	PointLight(): PointLight(DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f))
	{
		RecalculateAttenuationCoefficients();
	}

	explicit PointLight(const DirectX::XMFLOAT4 positionWs, const float range = 20.0f)
		: m_PositionWs(positionWs),
		  m_Range(range)
	{
		RecalculateAttenuationCoefficients();
	}

	DirectX::XMFLOAT4 m_PositionWs; // Light position in world space.
	//----------------------------------- (16 byte boundary)
	DirectX::XMFLOAT4 m_PositionVs = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f); // Light position in view space.
	//----------------------------------- (16 byte boundary)
	DirectX::XMFLOAT4 m_Color = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	//----------------------------------- (16 byte boundary)
	float m_ConstantAttenuation = 1.0f;
	float m_LinearAttenuation = 0.22f;
	float m_QuadraticAttenuation = 0.2f;
	float m_Range = 20.0f;
	//----------------------------------- (16 byte boundary)
	// Total:                              16 * 4 = 64 bytes

	void RecalculateAttenuationCoefficients();

private:
	struct AttenuationCoefficientsRecord
	{
		float m_Range;
		float m_ConstantAttenuation;
		float m_LinearAttenuation;
		float m_QuadraticAttenuation;
	};


	// The first is estimated manually, others are from the source below:
	// https://wiki.ogre3d.org/tiki-index.php?page=-Point+Light+Attenuation
	static constexpr AttenuationCoefficientsRecord ATTENUATION_COEFFICIENTS_RECORDS[] = {
		{1, 1, 45.0f, 20.0f},
		{7, 1, 0.7f, 1.8f},
		{13, 1, 0.35f, 0.44f},
		{20, 1, 0.22f, 0.2f},
		{32, 1, 0.14f, 0.07f},
		{50, 1, 0.09f, 0.032f},
		{65, 1, 0.07f, 0.017f},
		{100, 1, 0.045f, 0.0075f},
		{160, 1, 0.027f, 0.0028f},
		{200, 1, 0.022f, 0.0019f},
		{325, 1, 0.014f, 0.0007f},
		{600, 1, 0.007f, 0.0002f},
	};

	void ApplyAttenuationCoefficients(const AttenuationCoefficientsRecord& coefficientsRecord);
};
