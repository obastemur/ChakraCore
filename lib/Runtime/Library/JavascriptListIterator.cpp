//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    JavascriptListIterator::JavascriptListIterator(DynamicType* type, ListForListIterator* list) :
        DynamicObject(type),
        listForIterator(list),
        index(0)
    {LOGMEIN("JavascriptListIterator.cpp] 12\n");
        Assert(type->GetTypeId() == TypeIds_ListIterator);
        count = list->Count();
    }

    bool JavascriptListIterator::Is(Var aValue)
    {LOGMEIN("JavascriptListIterator.cpp] 18\n");
        TypeId typeId = JavascriptOperators::GetTypeId(aValue);
        return typeId == TypeIds_ListIterator;
    }

    JavascriptListIterator* JavascriptListIterator::FromVar(Var aValue)
    {
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptListIterator'");

        return static_cast<JavascriptListIterator *>(RecyclableObject::FromVar(aValue));
    }

    Var JavascriptListIterator::EntryNext(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        JavascriptLibrary* library = scriptContext->GetLibrary();

        Assert(!(callInfo.Flags & CallFlags_New));

        Var thisObj = args[0];

        if (!JavascriptListIterator::Is(thisObj))
        {LOGMEIN("JavascriptListIterator.cpp] 43\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedListIterator, _u("ListIterator.next"));
        }

        JavascriptListIterator* iterator = JavascriptListIterator::FromVar(thisObj);
        ListForListIterator* list = iterator->listForIterator;

        if (list == nullptr)
        {LOGMEIN("JavascriptListIterator.cpp] 51\n");
            return library->CreateIteratorResultObjectUndefinedTrue();
        }

        if (iterator->index >= iterator->count)
        {LOGMEIN("JavascriptListIterator.cpp] 56\n");
            // Nulling out the listForIterator field is important so that the iterator
            // does not keep the list alive after iteration is completed.
            iterator->listForIterator = nullptr;
            return library->CreateIteratorResultObjectUndefinedTrue();
        }

        Var current = list->Item(iterator->index);

        iterator->index++;

        return library->CreateIteratorResultObjectValueFalse(current);
    }
} // namespace Js
