//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeTypePch.h"
#include "Types/SpreadArgument.h"
namespace Js
{
    bool SpreadArgument::Is(Var aValue)
    {LOGMEIN("SpreadArgument.cpp] 9\n");
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_SpreadArgument;
    }
    SpreadArgument* SpreadArgument::FromVar(Var aValue)
    {LOGMEIN("SpreadArgument.cpp] 13\n");
        Assert(SpreadArgument::Is(aValue));
        return static_cast<SpreadArgument*>(aValue);
    }

    SpreadArgument::SpreadArgument(Var iterable, RecyclableObject* iterator, DynamicType * type) : DynamicObject(type), iterable(iterable),
        iterator(iterator), iteratorIndices(nullptr)
    {LOGMEIN("SpreadArgument.cpp] 20\n");
        Var nextItem;
        ScriptContext * scriptContext = this->GetScriptContext();

        while (JavascriptOperators::IteratorStepAndValue(iterator, scriptContext, &nextItem))
        {LOGMEIN("SpreadArgument.cpp] 25\n");
            if (iteratorIndices == nullptr)
            {LOGMEIN("SpreadArgument.cpp] 27\n");
                iteratorIndices = RecyclerNew(scriptContext->GetRecycler(), VarList, scriptContext->GetRecycler());
            }

            iteratorIndices->Add(nextItem);
        }
    }

} // namespace Js
