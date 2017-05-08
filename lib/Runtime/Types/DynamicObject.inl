//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#pragma once

namespace Js
{
#ifdef RECYCLER_STRESS
    // Only enable RecyclerTrackStress on DynamicObject
    template <class T> bool IsRecyclerTrackStressType() {TRACE_IT(65974); return false; }
    template <> inline bool IsRecyclerTrackStressType<DynamicObject>() {TRACE_IT(65975); return true; }
#endif

    template <class T>
    inline T * DynamicObject::NewObject(Recycler * recycler, DynamicType * type)
    {TRACE_IT(65976);
        size_t inlineSlotsSize = type->GetTypeHandler()->GetInlineSlotsSize();
        if (inlineSlotsSize)
        {TRACE_IT(65977);
#ifdef RECYCLER_STRESS
            if (Js::Configuration::Global.flags.RecyclerTrackStress && IsRecyclerTrackStressType<T>())
            {TRACE_IT(65978);
                return RecyclerNewTrackedLeafPlusZ(recycler, inlineSlotsSize, T, type);
            }
#endif
            return RecyclerNewPlusZ(recycler, inlineSlotsSize, T, type);
        }
        else
        {TRACE_IT(65979);
#ifdef RECYCLER_STRESS
            if (Js::Configuration::Global.flags.RecyclerTrackStress && IsRecyclerTrackStressType<T>())
            {TRACE_IT(65980);
                return RecyclerNewTrackedLeaf(recycler, T, type);
            }
#endif
            return RecyclerNew(recycler, T, type);
        }
    }
}
