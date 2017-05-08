//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeTypePch.h"
#include "Types/SpreadArgument.h"
namespace Js
{
    bool SpreadArgument::Is(Var aValue)
    {TRACE_IT(67650);
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_SpreadArgument;
    }
    SpreadArgument* SpreadArgument::FromVar(Var aValue)
    {TRACE_IT(67651);
        Assert(SpreadArgument::Is(aValue));
        return static_cast<SpreadArgument*>(aValue);
    }

    SpreadArgument::SpreadArgument(Var iterable, RecyclableObject* iterator, DynamicType * type) : DynamicObject(type), iterable(iterable),
        iterator(iterator), iteratorIndices(nullptr)
    {TRACE_IT(67652);
        Var nextItem;
        ScriptContext * scriptContext = this->GetScriptContext();

        while (JavascriptOperators::IteratorStepAndValue(iterator, scriptContext, &nextItem))
        {TRACE_IT(67653);
            if (iteratorIndices == nullptr)
            {TRACE_IT(67654);
                iteratorIndices = RecyclerNew(scriptContext->GetRecycler(), VarList, scriptContext->GetRecycler());
            }

            iteratorIndices->Add(nextItem);
        }
    }

} // namespace Js
