#pragma once

#include <map>
#include <vector>
#include <string>

#include <DirectXMath.h>

#include "Bone.h"

class Armature
{
public:
    void SetBones(const std::vector<Bone>& bones);
    void SetBoneChildren(size_t boneIndex, const std::vector<size_t>& childrenIndices);
    const std::vector<size_t>& GetBoneChildren(size_t boneIndex) const;

    bool HasBones() const;
    const std::vector<Bone>& GetBones() const;

    [[nodiscard]] const Bone& GetBone(const std::string& name) const;
    Bone& GetBone(const std::string& name);
    [[nodiscard]] const Bone& GetBone(size_t index) const;
    Bone& GetBone(size_t index);

    bool HasBone(const std::string& name) const;
    size_t GetBoneIndex(const std::string& name) const;

    void MarkBonesDirty();
    void UpdateBoneGlobalTransforms();

private:
    void UpdateBoneGlobalTransforms(size_t rootIndex, const DirectX::XMMATRIX& parentTransform);

    std::vector<Bone> m_Bones;
    std::map<std::string, size_t> m_BoneIndicesByNames;
    std::vector<std::vector<size_t>> m_BoneChildrenByIndex;
};
