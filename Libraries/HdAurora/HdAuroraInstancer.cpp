// Copyright 2023 Autodesk, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "pch.h"

#include "HdAuroraInstancer.h"

HdAuroraInstancer::HdAuroraInstancer(HdSceneDelegate* delegate, SdfPath const& id) :
    HdInstancer(delegate, id)
{
}

HdAuroraInstancer::~HdAuroraInstancer()
{
    TF_FOR_ALL(it, _primvarMap) { delete it->second; }
    _primvarMap.clear();
}

void HdAuroraInstancer::Sync(
    HdSceneDelegate* delegate, HdRenderParam* /*renderParam*/, HdDirtyBits* dirtyBits)
{
    _UpdateInstancer(delegate, dirtyBits);

    if (HdChangeTracker::IsAnyPrimvarDirty(*dirtyBits, GetId()))
    {
        _SyncPrimvars(delegate, *dirtyBits);
    }
}

void HdAuroraInstancer::_SyncPrimvars(HdSceneDelegate* delegate, HdDirtyBits dirtyBits)
{
    SdfPath const& id = GetId();

    HdPrimvarDescriptorVector primvars =
        delegate->GetPrimvarDescriptors(id, HdInterpolationInstance);

    for (HdPrimvarDescriptor const& pv : primvars)
    {
        if (HdChangeTracker::IsPrimvarDirty(dirtyBits, id, pv.name))
        {
            VtValue value = delegate->Get(id, pv.name);
            if (!value.IsEmpty())
            {
                if (_primvarMap.count(pv.name) > 0)
                {
                    delete _primvarMap[pv.name];
                }
                _primvarMap[pv.name] = new HdVtBufferSource(pv.name, value);
            }
        }
    }
}

bool SampleBuffer(HdVtBufferSource const& buffer, int index, void* value, HdTupleType dataType)
{
    if (buffer.GetNumElements() <= (size_t)index || buffer.GetTupleType() != dataType)
    {
        return false;
    }

    size_t elemSize = HdDataSizeOfTupleType(dataType);
    size_t offset   = elemSize * index;

    std::memcpy(value, static_cast<const uint8_t*>(buffer.GetData()) + offset, elemSize);

    return true;
}

VtMatrix4dArray HdAuroraInstancer::ComputeInstanceTransforms(SdfPath const& prototypeId)
{
    // The transforms for this level of instancer are computed by:
    // foreach(index : indices) {
    //     instancerTransform * translate(index) * rotate(index) *
    //     scale(index) * instanceTransform(index)
    // }
    // If any transform isn't provided, it's assumed to be the identity.

    GfMatrix4d instancerTransform = GetDelegate()->GetInstancerTransform(GetId());
    VtIntArray instanceIndices    = GetDelegate()->GetInstanceIndices(GetId(), prototypeId);

    VtMatrix4dArray transforms(instanceIndices.size());
    for (size_t i = 0; i < instanceIndices.size(); ++i)
    {
        transforms[i] = instancerTransform;
    }

    // "translate" holds a translation vector for each index.
    if (_primvarMap.count(HdInstancerTokens->translate) > 0)
    {
        const auto* pBuffer = _primvarMap[HdInstancerTokens->translate];
        for (size_t i = 0; i < instanceIndices.size(); ++i)
        {
            GfVec3f translate;
            if (SampleBuffer(*pBuffer, instanceIndices[i], &translate, { HdTypeFloatVec3, 1 }))
            {
                GfMatrix4d translateMat(1);
                translateMat.SetTranslate(GfVec3d(translate));
                transforms[i] = translateMat * transforms[i];
            }
        }
    }

    // "rotate" holds a quaternion in <real, i, j, k> format for each index.
    if (_primvarMap.count(HdInstancerTokens->rotate) > 0)
    {
        const auto* pBuffer = _primvarMap[HdInstancerTokens->rotate];
        for (size_t i = 0; i < instanceIndices.size(); ++i)
        {
            GfVec4f quat;
            if (SampleBuffer(*pBuffer, instanceIndices[i], &quat, { HdTypeFloatVec4, 1 }))
            {
                GfMatrix4d rotateMat(1);
                rotateMat.SetRotate(GfQuatd(quat[0], quat[1], quat[2], quat[3]));
                transforms[i] = rotateMat * transforms[i];
            }
        }
    }

    // "scale" holds an axis-aligned scale vector for each index.
    if (_primvarMap.count(HdInstancerTokens->scale) > 0)
    {
        const auto* pBuffer = _primvarMap[HdInstancerTokens->scale];
        for (size_t i = 0; i < instanceIndices.size(); ++i)
        {
            GfVec3f scale;
            if (SampleBuffer(*pBuffer, instanceIndices[i], &scale, { HdTypeFloatVec3, 1 }))
            {
                GfMatrix4d scaleMat(1);
                scaleMat.SetScale(GfVec3d(scale));
                transforms[i] = scaleMat * transforms[i];
            }
        }
    }

    // "instanceTransform" holds a 4x4 transform matrix for each index.
    if (_primvarMap.count(HdInstancerTokens->instanceTransform) > 0)
    {
        const auto* pBuffer = _primvarMap[HdInstancerTokens->instanceTransform];
        for (size_t i = 0; i < instanceIndices.size(); ++i)
        {
            GfMatrix4d instanceTransform;
            if (SampleBuffer(
                    *pBuffer, instanceIndices[i], &instanceTransform, { HdTypeDoubleMat4, 1 }))
            {
                transforms[i] = instanceTransform * transforms[i];
            }
        }
    }

    if (GetParentId().IsEmpty())
    {
        return transforms;
    }

    HdInstancer* parentInstancer = GetDelegate()->GetRenderIndex().GetInstancer(GetParentId());
    if (!TF_VERIFY(parentInstancer))
    {
        return transforms;
    }

    // The transforms taking nesting into account are computed by:
    // parentTransforms = parentInstancer->ComputeInstanceTransforms(GetId())
    // foreach (parentXf : parentTransforms, xf : transforms) {
    //     parentXf * xf
    // }
    VtMatrix4dArray parentTransforms =
        static_cast<HdAuroraInstancer*>(parentInstancer)->ComputeInstanceTransforms(GetId());

    VtMatrix4dArray final(parentTransforms.size() * transforms.size());
    for (size_t i = 0; i < parentTransforms.size(); ++i)
    {
        for (size_t j = 0; j < transforms.size(); ++j)
        {
            final[i * transforms.size() + j] = transforms[j] * parentTransforms[i];
        }
    }
    return final;
}
