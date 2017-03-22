//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "RuntimeLanguagePch.h"
#include "Types/PropertyIndexRanges.h"
#include "Types/SimpleDictionaryPropertyDescriptor.h"
#include "Types/SimpleDictionaryTypeHandler.h"
#include "ModuleNamespace.h"
#include "ModuleNamespaceEnumerator.h"

namespace Js
{
    ModuleNamespaceEnumerator::ModuleNamespaceEnumerator(ModuleNamespace* _nsObject, EnumeratorFlags flags, ScriptContext* scriptContext) :
        JavascriptEnumerator(scriptContext), nsObject(_nsObject), currentLocalMapIndex(Constants::NoBigSlot), currentNonLocalMapIndex(Constants::NoBigSlot), nonLocalMap(nullptr),
        flags(flags)
    {LOGMEIN("ModuleNamespaceEnumerator.cpp] 17\n");
    }

    ModuleNamespaceEnumerator* ModuleNamespaceEnumerator::New(ModuleNamespace* nsObject, EnumeratorFlags flags, ScriptContext* scriptContext, ForInCache * forInCache)
    {LOGMEIN("ModuleNamespaceEnumerator.cpp] 21\n");
        ModuleNamespaceEnumerator* enumerator = RecyclerNew(scriptContext->GetRecycler(), ModuleNamespaceEnumerator, nsObject, flags, scriptContext);
        if (enumerator->Init(forInCache))
        {LOGMEIN("ModuleNamespaceEnumerator.cpp] 24\n");
            return enumerator;
        }
        return nullptr;
    }

    BOOL ModuleNamespaceEnumerator::Init(ForInCache * forInCache)
    {LOGMEIN("ModuleNamespaceEnumerator.cpp] 31\n");
        if (!nsObject->DynamicObject::GetEnumerator(&symbolEnumerator, flags, GetScriptContext(), forInCache))
        {LOGMEIN("ModuleNamespaceEnumerator.cpp] 33\n");
            return FALSE;
        }
        nonLocalMap = nsObject->GetUnambiguousNonLocalExports();
        Reset();
        return TRUE;
    }

    void ModuleNamespaceEnumerator::Reset()
    {LOGMEIN("ModuleNamespaceEnumerator.cpp] 42\n");
        currentLocalMapIndex = Constants::NoBigSlot;
        currentNonLocalMapIndex = Constants::NoBigSlot;
        doneWithLocalExports = false;
        if (!!(flags & EnumeratorFlags::EnumSymbols))
        {LOGMEIN("ModuleNamespaceEnumerator.cpp] 47\n");
            doneWithSymbol = false;
        }
        else
        {
            doneWithSymbol = true;
        }
        symbolEnumerator.Reset();
    }

    // enumeration order: symbol first; local exports next; nonlocal exports last.
    Var ModuleNamespaceEnumerator::MoveAndGetNext(PropertyId& propertyId, PropertyAttributes* attributes)
    {LOGMEIN("ModuleNamespaceEnumerator.cpp] 59\n");
        Var undefined = GetLibrary()->GetUndefined();
        Var result = undefined;
        if (attributes != nullptr)
        {LOGMEIN("ModuleNamespaceEnumerator.cpp] 63\n");
            // all the attribute should have the same setting here in namespace object.
            *attributes = PropertyModuleNamespaceDefault;
        }
        if (!doneWithSymbol)
        {LOGMEIN("ModuleNamespaceEnumerator.cpp] 68\n");
            result = symbolEnumerator.MoveAndGetNext(propertyId, attributes);
            if (result == nullptr)
            {LOGMEIN("ModuleNamespaceEnumerator.cpp] 71\n");
                doneWithSymbol = true;
            }
            else
            {
                return result;
            }
        }
        if (!this->doneWithLocalExports)
        {LOGMEIN("ModuleNamespaceEnumerator.cpp] 80\n");
            currentLocalMapIndex++;
            JavascriptString* propertyString = nullptr;
            if (!nsObject->FindNextProperty(currentLocalMapIndex, &propertyString, &propertyId, attributes, this->GetScriptContext()))
            {LOGMEIN("ModuleNamespaceEnumerator.cpp] 84\n");
                // we are done with the object part; 
                this->doneWithLocalExports = true;
            }
            else
            {
                return propertyString;
            }
        }
        if (this->nonLocalMap != nullptr && (currentNonLocalMapIndex + 1 < nonLocalMap->Count()))
        {LOGMEIN("ModuleNamespaceEnumerator.cpp] 94\n");
            currentNonLocalMapIndex++;
            result = this->GetScriptContext()->GetPropertyString(this->nonLocalMap->GetKeyAt(currentNonLocalMapIndex));
            return result;
        }
        return nullptr;
    }
}
