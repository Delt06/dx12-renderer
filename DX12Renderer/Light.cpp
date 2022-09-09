#include "Light.h"

#include <cstdlib>

namespace
{
	float Lerp(const float a, const float b, const float t)
	{
		return (1.0f - t) * a + b * t;
	}

	float InverseLerp(const float a, const float b, const float v)
	{
		if (a == b)
			return 0;
		return (v - a) / (b - a);
	}
}

void PointLight::RecalculateAttenuationCoefficients()
{
	constexpr size_t recordsCount = _countof(ATTENUATION_COEFFICIENTS_RECORDS);

	for (size_t i = 1; i < recordsCount; ++i)
	{
		const auto& record1 = ATTENUATION_COEFFICIENTS_RECORDS[i - 1];
		const auto& record2 = ATTENUATION_COEFFICIENTS_RECORDS[i];

		if (m_Range < record1.m_Range)
		{
			ApplyAttenuationCoefficients(ATTENUATION_COEFFICIENTS_RECORDS[i - 1]);
			return;
		}

		if (m_Range > record2.m_Range)
		{
			continue;
		}

		const float t = InverseLerp(record1.m_Range, record2.m_Range, m_Range);
		AttenuationCoefficientsRecord intermediateCoefficients = {
			m_Range,
			Lerp(record1.m_ConstantAttenuation, record2.m_ConstantAttenuation, t),
			Lerp(record1.m_LinearAttenuation, record2.m_LinearAttenuation, t),
			Lerp(record1.m_QuadraticAttenuation, record2.m_QuadraticAttenuation, t),
		};
		ApplyAttenuationCoefficients(intermediateCoefficients);
	}

	ApplyAttenuationCoefficients(ATTENUATION_COEFFICIENTS_RECORDS[recordsCount - 1]);
}

void PointLight::ApplyAttenuationCoefficients(const AttenuationCoefficientsRecord& coefficientsRecord)
{
	m_ConstantAttenuation = coefficientsRecord.m_ConstantAttenuation;
	m_LinearAttenuation = coefficientsRecord.m_LinearAttenuation;
	m_QuadraticAttenuation = coefficientsRecord.m_QuadraticAttenuation;
}
