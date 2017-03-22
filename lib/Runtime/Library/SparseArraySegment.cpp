//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    const uint32 SparseArraySegmentBase::MaxLength = static_cast<uint32>(INT32_MAX);

    SparseArraySegmentBase::SparseArraySegmentBase(uint32 left, uint32 length, uint32 size) : left(left), length(length), size(size), next(nullptr)
    {LOGMEIN("SparseArraySegment.cpp] 11\n");
    }

    // "Reduce" size if it exceeds next.left boundary, after operations that shift the following segments.
    void SparseArraySegmentBase::EnsureSizeInBound()
    {
        EnsureSizeInBound(left, length, size, next);
    }

    // Reduce size if it exceeds next.left boundary or MaxArrayLength
    void SparseArraySegmentBase::EnsureSizeInBound(uint32 left, uint32 length, uint32& size, SparseArraySegmentBase* next)
    {LOGMEIN("SparseArraySegment.cpp] 22\n");
        uint32 nextLeft = next ? next->left : JavascriptArray::MaxArrayLength;
        Assert(nextLeft > left);

        if(size != 0)
        {LOGMEIN("SparseArraySegment.cpp] 27\n");
            // Avoid writing to 'size' for an empty segment. The empty segment is a constant structure and writing to it (even
            // if it's not being changed) may cause an AV.
            size = min(size, nextLeft - left);
        }
        Assert(length <= size);
    }

    // Test if an element value is null/undefined.
    inline static bool IsMissingOrUndefined(Var value, RecyclableObject *undefined, uint32& countUndefined)
    {LOGMEIN("SparseArraySegment.cpp] 37\n");
        if (SparseArraySegment<Var>::IsMissingItem(&value))
        {LOGMEIN("SparseArraySegment.cpp] 39\n");
            return true;
        }
        if (JavascriptOperators::IsUndefinedObject(value, undefined))
        {LOGMEIN("SparseArraySegment.cpp] 43\n");
            ++countUndefined;
            return true;
        }
        return false;
    }

    bool SparseArraySegmentBase::IsLeafSegment(SparseArraySegmentBase *seg, Recycler *recycler)
    {LOGMEIN("SparseArraySegment.cpp] 51\n");
        if (!DoNativeArrayLeafSegment())
        {LOGMEIN("SparseArraySegment.cpp] 53\n");
            return false;
        }

        RecyclerHeapObjectInfo heapObject;
        if (recycler->FindHeapObject(
                seg,
                (Memory::FindHeapObjectFlags)(FindHeapObjectFlags_VerifyFreeBitForAttribute | FindHeapObjectFlags_AllowInterior),
                heapObject))
        {LOGMEIN("SparseArraySegment.cpp] 62\n");
            return heapObject.IsLeaf();
        }

        return false;
    }

    // Remove null/undefined from this segment. May reorder elements and compact this segment in preparing for sort.
    uint32 SparseArraySegmentBase::RemoveUndefined(ScriptContext* scriptContext)
    {LOGMEIN("SparseArraySegment.cpp] 71\n");
        SparseArraySegment<Var> *_this = (SparseArraySegment<Var>*)this;
        // Shortcut length==0, otherwise the code below will AV when left==length==0. (WOOB 1114975)
        if (length == 0)
        {LOGMEIN("SparseArraySegment.cpp] 75\n");
            return 0;
        }

        //remove undefine values
        RecyclableObject *undefined = scriptContext->GetLibrary()->GetUndefined();

        uint32 i = 0;
        uint32 j = length - 1;
        uint32 countUndefined = 0;

        while (i <= j)
        {LOGMEIN("SparseArraySegment.cpp] 87\n");
            //get the first null/undefined slot from left
            while (i < j && !IsMissingOrUndefined(_this->elements[i], undefined, countUndefined))
            {LOGMEIN("SparseArraySegment.cpp] 90\n");
                i++;
            }
            bool iIsMissingOrUndefined = (i < j); // Flag to avoid test again if later j comes down to == i

            //get the first slot which is not null/undefined from the right
            while (i < j && IsMissingOrUndefined(_this->elements[j], undefined, countUndefined))
            {LOGMEIN("SparseArraySegment.cpp] 97\n");
                j--;
            }

            if (i < j)
            {LOGMEIN("SparseArraySegment.cpp] 102\n");
                //move
                _this->elements[i] = _this->elements[j];
                i++;
                j--;
            }
            else
            {
                Assert(i == j);
                if (iIsMissingOrUndefined || IsMissingOrUndefined(_this->elements[j], undefined, countUndefined))
                {LOGMEIN("SparseArraySegment.cpp] 112\n");
                    j--; // ok if j becomes -1. We'll truncate to length (j + 1).
                }
                break; // done
            }
        }

        if (j != length - 1) // Truncate if j has changed
        {LOGMEIN("SparseArraySegment.cpp] 120\n");
            uint32 newLen = j + 1;
            Assert(newLen < length);
            Assert(countUndefined <= length - newLen);

            _this->Truncate(left + newLen); // Truncate to new length (also clears moved elements)
        }
        Assert(length <= size);

        return countUndefined;
    }
}
