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
    {LOGMEIN("ArgumentsObjectEnumerator.cpp] 14\n");
        Reset();
    }

    Var ArgumentsObjectPrefixEnumerator::MoveAndGetNext(PropertyId& propertyId, PropertyAttributes* attributes)
    {LOGMEIN("ArgumentsObjectEnumerator.cpp] 19\n");
        if (!doneFormalArgs)
        {LOGMEIN("ArgumentsObjectEnumerator.cpp] 21\n");
            formalArgIndex = argumentsObject->GetNextFormalArgIndex(formalArgIndex, !!(flags & EnumeratorFlags::EnumNonEnumerable), attributes);
            if (formalArgIndex != JavascriptArray::InvalidIndex
                && formalArgIndex < argumentsObject->GetNumberOfArguments())
            {LOGMEIN("ArgumentsObjectEnumerator.cpp] 25\n");
                propertyId = Constants::NoProperty;
                return this->GetScriptContext()->GetIntegerString(formalArgIndex);
            }

            doneFormalArgs = true;
        }
        return nullptr;
    }

    void ArgumentsObjectPrefixEnumerator::Reset()
    {LOGMEIN("ArgumentsObjectEnumerator.cpp] 36\n");
        formalArgIndex = JavascriptArray::InvalidIndex;
        doneFormalArgs = false;
    }

    //---------------------- ES5ArgumentsObjectEnumerator -------------------------------
    ES5ArgumentsObjectEnumerator * ES5ArgumentsObjectEnumerator::New(ArgumentsObject* argumentsObject, EnumeratorFlags flags, ScriptContext* requestContext, ForInCache * forInCache)
    {LOGMEIN("ArgumentsObjectEnumerator.cpp] 43\n");
        ES5ArgumentsObjectEnumerator * enumerator = RecyclerNew(requestContext->GetRecycler(), ES5ArgumentsObjectEnumerator, argumentsObject, flags, requestContext);
        if (!enumerator->Init(forInCache))
        {LOGMEIN("ArgumentsObjectEnumerator.cpp] 46\n");
            return nullptr;
        }
        return enumerator;
    }

    ES5ArgumentsObjectEnumerator::ES5ArgumentsObjectEnumerator(ArgumentsObject* argumentsObject, EnumeratorFlags flags, ScriptContext* requestcontext)
        : ArgumentsObjectPrefixEnumerator(argumentsObject, flags, requestcontext),
        enumeratedFormalsInObjectArrayCount(0)
    {LOGMEIN("ArgumentsObjectEnumerator.cpp] 55\n");
    }

    BOOL ES5ArgumentsObjectEnumerator::Init(ForInCache * forInCache)
    {LOGMEIN("ArgumentsObjectEnumerator.cpp] 59\n");
        __super::Reset();
        this->enumeratedFormalsInObjectArrayCount = 0;
        return argumentsObject->DynamicObject::GetEnumerator(&objectEnumerator, flags, GetScriptContext(), forInCache);
    }

    Var ES5ArgumentsObjectEnumerator::MoveAndGetNext(PropertyId& propertyId, PropertyAttributes* attributes)
    {LOGMEIN("ArgumentsObjectEnumerator.cpp] 66\n");
        // Formals:
        // - deleted => not in objectArray && not connected -- do not enum, do not advance
        // - connected,     in objectArray -- if (enumerable) enum it, advance objectEnumerator
        // - disconnected =>in objectArray -- if (enumerable) enum it, advance objectEnumerator

        if (!doneFormalArgs)
        {LOGMEIN("ArgumentsObjectEnumerator.cpp] 73\n");
            ES5HeapArgumentsObject* es5HAO = static_cast<ES5HeapArgumentsObject*>(
                static_cast<ArgumentsObject*>(argumentsObject));
            formalArgIndex = es5HAO->GetNextFormalArgIndexHelper(formalArgIndex, !!(flags & EnumeratorFlags::EnumNonEnumerable), attributes);
            if (formalArgIndex != JavascriptArray::InvalidIndex
                && formalArgIndex < argumentsObject->GetNumberOfArguments())
            {LOGMEIN("ArgumentsObjectEnumerator.cpp] 79\n");
                if (argumentsObject->HasObjectArrayItem(formalArgIndex))
                {LOGMEIN("ArgumentsObjectEnumerator.cpp] 81\n");
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
    {LOGMEIN("ArgumentsObjectEnumerator.cpp] 98\n");
        Init(nullptr);
    }
}
