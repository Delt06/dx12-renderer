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
	PointLight() : PointLight(DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f))
	{
		RecalculateAttenuationCoefficients();
	}

	explicit PointLight(const DirectX::XMFLOAT4 positionWs, const float range = 20.0f)
		: PositionWs(positionWs),
		Range(range)
	{
		RecalculateAttenuationCoefficients();
	}

	DirectX::XMFLOAT4 PositionWs; // Light position in world space.
	//----------------------------------- (16 byte boundary)
	DirectX::XMFLOAT4 PositionVs = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f); // Light position in view space.
	//----------------------------------- (16 byte boundary)
	DirectX::XMFLOAT4 Color = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	//----------------------------------- (16 byte boundary)
	float ConstantAttenuation = 1.0f;
	float LinearAttenuation = 0.22f;
	float QuadraticAttenuation = 0.2f;
	float Range = 20.0f;
	//----------------------------------- (16 byte boundary)
	// Total:                              16 * 4 = 64 bytes

	void RecalculateAttenuationCoefficients();

private:
	struct AttenuationCoefficientsRecord
	{
		float Range;
		float ConstantAttenuation;
		float LinearAttenuation;
		float QuadraticAttenuation;
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

struct SpotLight
{
	SpotLight()
		: PositionWs(0.0f, 0.0f, 0.0f, 1.0f)
		, PositionVs(0.0f, 0.0f, 0.0f, 1.0f)
		, DirectionWs(0.0f, 0.0f, 1.0f, 0.0f)
		, DirectionVs(0.0f, 0.0f, 1.0f, 0.0f)
		, Color(1.0f, 1.0f, 1.0f, 1.0f)
		, Intensity(1.0f)
		, SpotAngle(DirectX::XM_PIDIV2)
		, Attenuation(0.0f)
	{
	}

	DirectX::XMFLOAT4 PositionWs; // Light position in world space.
	//----------------------------------- (16 byte boundary)
	DirectX::XMFLOAT4 PositionVs; // Light position in view space.
	//----------------------------------- (16 byte boundary)
	DirectX::XMFLOAT4 DirectionWs; // Light direction in world space.
	//----------------------------------- (16 byte boundary)
	DirectX::XMFLOAT4 DirectionVs; // Light direction in view space.
	//----------------------------------- (16 byte boundary)
	DirectX::XMFLOAT4 Color;
	//----------------------------------- (16 byte boundary)
	float Intensity;
	float SpotAngle;
	float Attenuation;
	float _Padding{}; // Pad to 16 bytes.
	//----------------------------------- (16 byte boundary)
	// Total:                              16 * 6 = 96 bytes
};
