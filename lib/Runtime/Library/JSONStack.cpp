//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"
#include "JSONStack.h"
namespace JSON
{
    bool StrictEqualsObjectComparer::Equals(Js::Var x, Js::Var y)
    {LOGMEIN("JSONStack.cpp] 9\n");
        return JSONStack::Equals(x,y);
    }

    JSONStack::JSONStack(ArenaAllocator *allocator, Js::ScriptContext *context) : jsObjectStack(allocator), domObjectStack(nullptr), alloc(allocator), scriptContext(context)
    {LOGMEIN("JSONStack.cpp] 14\n");
    }

    bool JSONStack::Equals(Js::Var x, Js::Var y)
    {LOGMEIN("JSONStack.cpp] 18\n");
        return Js::JavascriptOperators::StrictEqual(x, y, ((Js::RecyclableObject *)x)->GetScriptContext()) == TRUE;
    }

    bool JSONStack::Has(Js::Var data, bool bJsObject) const
    {LOGMEIN("JSONStack.cpp] 23\n");
        if (bJsObject)
        {LOGMEIN("JSONStack.cpp] 25\n");
            return jsObjectStack.Has(data);
        }
        else if (domObjectStack)
        {LOGMEIN("JSONStack.cpp] 29\n");
            return domObjectStack->Contains(data);
        }
        return false;
    }

    bool JSONStack::Push(Js::Var data, bool bJsObject)
    {LOGMEIN("JSONStack.cpp] 36\n");
        if (bJsObject)
        {LOGMEIN("JSONStack.cpp] 38\n");
            return jsObjectStack.Push(data);
        }
        EnsuresDomObjectStack();
        domObjectStack->Add(data);
        return true;
    }

    void JSONStack::Pop(bool bJsObject)
    {LOGMEIN("JSONStack.cpp] 47\n");
        if (bJsObject)
        {LOGMEIN("JSONStack.cpp] 49\n");
            jsObjectStack.Pop();
            return;
        }
        AssertMsg(domObjectStack != NULL, "Misaligned pop");
        domObjectStack->RemoveAtEnd();
    }

    void JSONStack::EnsuresDomObjectStack(void)
    {LOGMEIN("JSONStack.cpp] 58\n");
        if (!domObjectStack)
        {LOGMEIN("JSONStack.cpp] 60\n");
            domObjectStack = DOMObjectStack::New(alloc);
        }
    }
}// namespace JSON
