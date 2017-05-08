//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"
#include "JSONStack.h"
namespace JSON
{
    bool StrictEqualsObjectComparer::Equals(Js::Var x, Js::Var y)
    {TRACE_IT(55957);
        return JSONStack::Equals(x,y);
    }

    JSONStack::JSONStack(ArenaAllocator *allocator, Js::ScriptContext *context) : jsObjectStack(allocator), domObjectStack(nullptr), alloc(allocator), scriptContext(context)
    {TRACE_IT(55958);
    }

    bool JSONStack::Equals(Js::Var x, Js::Var y)
    {TRACE_IT(55959);
        return Js::JavascriptOperators::StrictEqual(x, y, ((Js::RecyclableObject *)x)->GetScriptContext()) == TRUE;
    }

    bool JSONStack::Has(Js::Var data, bool bJsObject) const
    {TRACE_IT(55960);
        if (bJsObject)
        {TRACE_IT(55961);
            return jsObjectStack.Has(data);
        }
        else if (domObjectStack)
        {TRACE_IT(55962);
            return domObjectStack->Contains(data);
        }
        return false;
    }

    bool JSONStack::Push(Js::Var data, bool bJsObject)
    {TRACE_IT(55963);
        if (bJsObject)
        {TRACE_IT(55964);
            return jsObjectStack.Push(data);
        }
        EnsuresDomObjectStack();
        domObjectStack->Add(data);
        return true;
    }

    void JSONStack::Pop(bool bJsObject)
    {TRACE_IT(55965);
        if (bJsObject)
        {TRACE_IT(55966);
            jsObjectStack.Pop();
            return;
        }
        AssertMsg(domObjectStack != NULL, "Misaligned pop");
        domObjectStack->RemoveAtEnd();
    }

    void JSONStack::EnsuresDomObjectStack(void)
    {TRACE_IT(55967);
        if (!domObjectStack)
        {TRACE_IT(55968);
            domObjectStack = DOMObjectStack::New(alloc);
        }
    }
}// namespace JSON
