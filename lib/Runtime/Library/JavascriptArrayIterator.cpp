//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    JavascriptArrayIterator::JavascriptArrayIterator(DynamicType* type, Var iterable, JavascriptArrayIteratorKind kind):
        DynamicObject(type),
        m_iterableObject(iterable),
        m_nextIndex(0),
        m_kind(kind)
    {LOGMEIN("JavascriptArrayIterator.cpp] 13\n");
        Assert(type->GetTypeId() == TypeIds_ArrayIterator);
        if (m_iterableObject == this->GetLibrary()->GetUndefined())
        {LOGMEIN("JavascriptArrayIterator.cpp] 16\n");
            m_iterableObject = nullptr;
        }
    }

    bool JavascriptArrayIterator::Is(Var aValue)
    {LOGMEIN("JavascriptArrayIterator.cpp] 22\n");
        TypeId typeId = JavascriptOperators::GetTypeId(aValue);
        return typeId == TypeIds_ArrayIterator;
    }

    JavascriptArrayIterator* JavascriptArrayIterator::FromVar(Var aValue)
    {
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptArrayIterator'");

        return static_cast<JavascriptArrayIterator *>(RecyclableObject::FromVar(aValue));
    }

    Var JavascriptArrayIterator::EntryNext(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        JavascriptLibrary* library = scriptContext->GetLibrary();

        Assert(!(callInfo.Flags & CallFlags_New));

        Var thisObj = args[0];

        if (!JavascriptArrayIterator::Is(thisObj))
        {LOGMEIN("JavascriptArrayIterator.cpp] 47\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedArrayIterator, _u("Array Iterator.prototype.next"));
        }

        JavascriptArrayIterator* iterator = JavascriptArrayIterator::FromVar(thisObj);
        Var iterable = iterator->m_iterableObject;

        if (iterable == nullptr)
        {LOGMEIN("JavascriptArrayIterator.cpp] 55\n");
            return library->CreateIteratorResultObjectUndefinedTrue();
        }

        int64 length;
        JavascriptArray* pArr = nullptr;
        TypedArrayBase *typedArrayBase = nullptr;
        if (JavascriptArray::Is(iterable) && !JavascriptArray::FromVar(iterable)->IsCrossSiteObject())
        {LOGMEIN("JavascriptArrayIterator.cpp] 63\n");
#if ENABLE_COPYONACCESS_ARRAY
            Assert(!JavascriptCopyOnAccessNativeIntArray::Is(iterable));
#endif
            pArr = JavascriptArray::FromAnyArray(iterable);
            length = pArr->GetLength();
        }
        else if (TypedArrayBase::Is(iterable))
        {LOGMEIN("JavascriptArrayIterator.cpp] 71\n");
            typedArrayBase = TypedArrayBase::FromVar(iterable);
            if (typedArrayBase->IsDetachedBuffer())
            {LOGMEIN("JavascriptArrayIterator.cpp] 74\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray);
            }

            length = typedArrayBase->GetLength();
        }
        else
        {
            length = JavascriptConversion::ToLength(JavascriptOperators::OP_GetLength(iterable, scriptContext), scriptContext);
        }

        int64 index = iterator->m_nextIndex;

        if (index >= length)
        {LOGMEIN("JavascriptArrayIterator.cpp] 88\n");
            // Nulling out the m_iterableObject field is important so that the iterator
            // does not keep the iterable object alive after iteration is completed.
            iterator->m_iterableObject = nullptr;
            return library->CreateIteratorResultObjectUndefinedTrue();
        }

        iterator->m_nextIndex += 1;

        if (iterator->m_kind == JavascriptArrayIteratorKind::Key)
        {LOGMEIN("JavascriptArrayIterator.cpp] 98\n");
            return library->CreateIteratorResultObjectValueFalse(JavascriptNumber::ToVar(index, scriptContext));
        }

        Var value;
        if (pArr != nullptr)
        {LOGMEIN("JavascriptArrayIterator.cpp] 104\n");
            Assert(index <= UINT_MAX);
            value = pArr->DirectGetItem((uint32)index);
        }
        else if (typedArrayBase != nullptr)
        {LOGMEIN("JavascriptArrayIterator.cpp] 109\n");
            Assert(index <= UINT_MAX);
            value = typedArrayBase->DirectGetItem((uint32)index);
        }
        else
        {
            value = JavascriptOperators::OP_GetElementI(iterable, JavascriptNumber::ToVar(index, scriptContext), scriptContext);
        }

        if (iterator->m_kind == JavascriptArrayIteratorKind::Value)
        {LOGMEIN("JavascriptArrayIterator.cpp] 119\n");
            return library->CreateIteratorResultObjectValueFalse(value);
        }

        Assert(iterator->m_kind == JavascriptArrayIteratorKind::KeyAndValue);

        JavascriptArray* keyValueTuple = library->CreateArray(2);

        keyValueTuple->SetItem(0, JavascriptNumber::ToVar(index, scriptContext), PropertyOperation_None);
        keyValueTuple->SetItem(1, value, PropertyOperation_None);

        return library->CreateIteratorResultObjectValueFalse(keyValueTuple);
    }
} //namespace Js
