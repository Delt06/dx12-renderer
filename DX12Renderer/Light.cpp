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

		if (Range < record1.Range)
		{
			ApplyAttenuationCoefficients(ATTENUATION_COEFFICIENTS_RECORDS[i - 1]);
			return;
		}

		if (Range > record2.Range)
		{
			continue;
		}

		const float t = InverseLerp(record1.Range, record2.Range, Range);
		AttenuationCoefficientsRecord intermediateCoefficients = {
			Range,
			Lerp(record1.ConstantAttenuation, record2.ConstantAttenuation, t),
			Lerp(record1.LinearAttenuation, record2.LinearAttenuation, t),
			Lerp(record1.QuadraticAttenuation, record2.QuadraticAttenuation, t),
		};
		ApplyAttenuationCoefficients(intermediateCoefficients);
	}

	ApplyAttenuationCoefficients(ATTENUATION_COEFFICIENTS_RECORDS[recordsCount - 1]);
}

void PointLight::ApplyAttenuationCoefficients(const AttenuationCoefficientsRecord& coefficientsRecord)
{
	ConstantAttenuation = coefficientsRecord.ConstantAttenuation;
	LinearAttenuation = coefficientsRecord.LinearAttenuation;
	QuadraticAttenuation = coefficientsRecord.QuadraticAttenuation;
}
