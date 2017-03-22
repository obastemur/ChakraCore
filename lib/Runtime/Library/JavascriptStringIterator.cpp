//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    JavascriptStringIterator::JavascriptStringIterator(DynamicType* type, JavascriptString* string):
        DynamicObject(type),
        m_string(string),
        m_nextIndex(0)
    {LOGMEIN("JavascriptStringIterator.cpp] 12\n");
        Assert(type->GetTypeId() == TypeIds_StringIterator);
    }

    bool JavascriptStringIterator::Is(Var aValue)
    {LOGMEIN("JavascriptStringIterator.cpp] 17\n");
        TypeId typeId = JavascriptOperators::GetTypeId(aValue);
        return typeId == TypeIds_StringIterator;
    }

    JavascriptStringIterator* JavascriptStringIterator::FromVar(Var aValue)
    {
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptStringIterator'");

        return static_cast<JavascriptStringIterator *>(RecyclableObject::FromVar(aValue));
    }

    Var JavascriptStringIterator::EntryNext(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        JavascriptLibrary* library = scriptContext->GetLibrary();

        Assert(!(callInfo.Flags & CallFlags_New));

        Var thisObj = args[0];

        if (!JavascriptStringIterator::Is(thisObj))
        {LOGMEIN("JavascriptStringIterator.cpp] 42\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedStringIterator, _u("String Iterator.prototype.next"));
        }

        JavascriptStringIterator* iterator = JavascriptStringIterator::FromVar(thisObj);
        JavascriptString* string = iterator->m_string;

        if (string == nullptr)
        {LOGMEIN("JavascriptStringIterator.cpp] 50\n");
            return library->CreateIteratorResultObjectUndefinedTrue();
        }

        charcount_t length = string->GetLength();
        charcount_t index = iterator->m_nextIndex;

        if (index >= length)
        {LOGMEIN("JavascriptStringIterator.cpp] 58\n");
            // Nulling out the m_string field is important so that the iterator
            // does not keep the string alive after iteration is completed.
            iterator->m_string = nullptr;
            return library->CreateIteratorResultObjectUndefinedTrue();
        }

        char16 chFirst = string->GetItem(index);
        Var result;

        if (index + 1 == string->GetLength() ||
            !NumberUtilities::IsSurrogateLowerPart(chFirst) ||
            !NumberUtilities::IsSurrogateUpperPart(string->GetItem(index + 1)))
        {LOGMEIN("JavascriptStringIterator.cpp] 71\n");
            result = scriptContext->GetLibrary()->GetCharStringCache().GetStringForChar(chFirst);
            iterator->m_nextIndex += 1;
        }
        else
        {
            result = JavascriptString::SubstringCore(string, index, 2, scriptContext);
            iterator->m_nextIndex += 2;
        }

        return library->CreateIteratorResultObjectValueFalse(result);
    }
} // namespace Js
