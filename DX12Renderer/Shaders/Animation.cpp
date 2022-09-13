#include "Animation.h"
#include <Mesh.h>
#include <Bone.h>
#include <DirectXMath.h>

using namespace DirectX;

namespace
{
	std::pair<Animation::KeyFrame, Animation::KeyFrame> GetInterpolatedKeyFrame(const std::vector<Animation::KeyFrame>& keyFrames, float normalizedTime)
	{
		if (keyFrames.size() == 1)
		{
			return std::make_pair(keyFrames[0], keyFrames[0]);
		}

		for (size_t i = 0; i < keyFrames.size() - 1; i++)
		{
			if (normalizedTime < (float)keyFrames[i + 1].NormalizedTime)
			{
				return std::make_pair(keyFrames[i], keyFrames[i + 1]);
			}
		}

		return std::make_pair(keyFrames[keyFrames.size() + 1], keyFrames[0]);
	}

	float InverseLerp(float a, float b, float v)
	{
		if (a == b)
		{
			return 0.0f;
		}

		return (v - a) / (b - a);
	}

	XMVECTOR Interpolate(Animation::KeyFrame first, Animation::KeyFrame second, float normalizedTime)
	{
		float t = InverseLerp(first.NormalizedTime, second.NormalizedTime, normalizedTime);
		return XMVectorLerp(first.Value, second.Value, t);
	}

	XMVECTOR InterpolateRotation(Animation::KeyFrame first, Animation::KeyFrame second, float normalizedTime)
	{
		float t = InverseLerp(first.NormalizedTime, second.NormalizedTime, normalizedTime);
		return XMQuaternionSlerp(first.Value, second.Value, t);
	}
}

Animation::Animation(float duration, float ticksPerSecond, const std::vector<Channel>& channels)
	: m_Duration(duration)
	, m_TicksPerSecond(ticksPerSecond != 0 ? ticksPerSecond : 25.0f)
	, m_Channels(channels)
{

}

void Animation::Play(Mesh& mesh, double time)
{
	if (!mesh.HasBones())
		throw std::exception("Can't play an animation on a mesh without bones.");

	double timeInTicks = time * m_TicksPerSecond;
	float normalizedTime = static_cast<float>(fmod(timeInTicks, m_Duration));

	for (const auto& channel : m_Channels)
	{
		auto& bone = mesh.GetBone(channel.NodeName);

		const auto positionKfs = GetInterpolatedKeyFrame(channel.PositionKeyFrames, normalizedTime);
		const auto position = Interpolate(positionKfs.first, positionKfs.second, normalizedTime);

		const auto rotationKfs = GetInterpolatedKeyFrame(channel.RotationKeyFrames, normalizedTime);
		const auto rotation = InterpolateRotation(rotationKfs.first, rotationKfs.second, normalizedTime);

		const auto scalingKfs = GetInterpolatedKeyFrame(channel.ScalingKeyFrames, normalizedTime);
		const auto scaling = Interpolate(scalingKfs.first, scalingKfs.second, normalizedTime);

		auto translationMatrix = XMMatrixTranslationFromVector(position);
		auto rotationMatrix = XMMatrixRotationQuaternion(rotation);
		auto scalingMatrix = XMMatrixScalingFromVector(scaling);
		bone.LocalTransform = translationMatrix * rotationMatrix * scalingMatrix;
	}

	mesh.MarkBonesDirty();
}