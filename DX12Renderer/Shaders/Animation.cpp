#include "Animation.h"
#include <Mesh.h>
#include <Bone.h>
#include <DirectXMath.h>

using namespace DirectX;

Animation::Animation(float duration, float ticksPerSecond, const std::vector<Channel>& channels)
	: m_Duration(duration)
	, m_TicksPerSecond(ticksPerSecond)
	, m_Channels(channels)
{

}

void Animation::Play(Mesh& mesh, double time)
{
	if (!mesh.HasBones())
		throw std::exception("Can't play an animation on a mesh without bones.");

	for (const auto& channel : m_Channels)
	{
		auto& bone = mesh.GetBone(channel.NodeName);

		auto translation = XMMatrixTranslationFromVector(channel.PositionKeyFrames[0].Value);
		auto rotation = XMMatrixRotationQuaternion(channel.RotationKeyFrames[0].Value);
		auto scaling = XMMatrixScalingFromVector(channel.ScalingKeyFrames[0].Value);
		bone.LocalTransform = translation * rotation * scaling;
	}

	mesh.MarkBonesDirty();
}