//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

#include "Library/ArgumentsObjectEnumerator.h"

namespace Js
{
    ArgumentsObjectPrefixEnumerator::ArgumentsObjectPrefixEnumerator(ArgumentsObject* argumentsObject, EnumeratorFlags flags, ScriptContext* requestContext)
        : JavascriptEnumerator(requestContext),
        argumentsObject(argumentsObject),
        flags(flags)
    {TRACE_IT(54228);
        Reset();
    }

    Var ArgumentsObjectPrefixEnumerator::MoveAndGetNext(PropertyId& propertyId, PropertyAttributes* attributes)
    {TRACE_IT(54229);
        if (!doneFormalArgs)
        {TRACE_IT(54230);
            formalArgIndex = argumentsObject->GetNextFormalArgIndex(formalArgIndex, !!(flags & EnumeratorFlags::EnumNonEnumerable), attributes);
            if (formalArgIndex != JavascriptArray::InvalidIndex
                && formalArgIndex < argumentsObject->GetNumberOfArguments())
            {TRACE_IT(54231);
                propertyId = Constants::NoProperty;
                return this->GetScriptContext()->GetIntegerString(formalArgIndex);
            }

            doneFormalArgs = true;
        }
        return nullptr;
    }

    void ArgumentsObjectPrefixEnumerator::Reset()
    {TRACE_IT(54232);
        formalArgIndex = JavascriptArray::InvalidIndex;
        doneFormalArgs = false;
    }

    //---------------------- ES5ArgumentsObjectEnumerator -------------------------------
    ES5ArgumentsObjectEnumerator * ES5ArgumentsObjectEnumerator::New(ArgumentsObject* argumentsObject, EnumeratorFlags flags, ScriptContext* requestContext, ForInCache * forInCache)
    {TRACE_IT(54233);
        ES5ArgumentsObjectEnumerator * enumerator = RecyclerNew(requestContext->GetRecycler(), ES5ArgumentsObjectEnumerator, argumentsObject, flags, requestContext);
        if (!enumerator->Init(forInCache))
        {TRACE_IT(54234);
            return nullptr;
        }
        return enumerator;
    }

    ES5ArgumentsObjectEnumerator::ES5ArgumentsObjectEnumerator(ArgumentsObject* argumentsObject, EnumeratorFlags flags, ScriptContext* requestcontext)
        : ArgumentsObjectPrefixEnumerator(argumentsObject, flags, requestcontext),
        enumeratedFormalsInObjectArrayCount(0)
    {TRACE_IT(54235);
    }

    BOOL ES5ArgumentsObjectEnumerator::Init(ForInCache * forInCache)
    {TRACE_IT(54236);
        __super::Reset();
        this->enumeratedFormalsInObjectArrayCount = 0;
        return argumentsObject->DynamicObject::GetEnumerator(&objectEnumerator, flags, GetScriptContext(), forInCache);
    }

    Var ES5ArgumentsObjectEnumerator::MoveAndGetNext(PropertyId& propertyId, PropertyAttributes* attributes)
    {TRACE_IT(54237);
        // Formals:
        // - deleted => not in objectArray && not connected -- do not enum, do not advance
        // - connected,     in objectArray -- if (enumerable) enum it, advance objectEnumerator
        // - disconnected =>in objectArray -- if (enumerable) enum it, advance objectEnumerator

        if (!doneFormalArgs)
        {TRACE_IT(54238);
            ES5HeapArgumentsObject* es5HAO = static_cast<ES5HeapArgumentsObject*>(
                static_cast<ArgumentsObject*>(argumentsObject));
            formalArgIndex = es5HAO->GetNextFormalArgIndexHelper(formalArgIndex, !!(flags & EnumeratorFlags::EnumNonEnumerable), attributes);
            if (formalArgIndex != JavascriptArray::InvalidIndex
                && formalArgIndex < argumentsObject->GetNumberOfArguments())
            {TRACE_IT(54239);
                if (argumentsObject->HasObjectArrayItem(formalArgIndex))
                {TRACE_IT(54240);
                    PropertyId tempPropertyId;
                    Var tempIndex = objectEnumerator.MoveAndGetNext(tempPropertyId, attributes);
                    AssertMsg(tempIndex, "We advanced objectEnumerator->MoveNext() too many times.");
                }

                propertyId = Constants::NoProperty;
                return this->GetScriptContext()->GetIntegerString(formalArgIndex);
            }

            doneFormalArgs = true;
        }

        return objectEnumerator.MoveAndGetNext(propertyId, attributes);
    }

    void ES5ArgumentsObjectEnumerator::Reset()
    {TRACE_IT(54241);
        Init(nullptr);
    }
}
