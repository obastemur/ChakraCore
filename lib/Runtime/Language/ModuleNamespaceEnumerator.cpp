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
    {TRACE_IT(51874);
    }

    ModuleNamespaceEnumerator* ModuleNamespaceEnumerator::New(ModuleNamespace* nsObject, EnumeratorFlags flags, ScriptContext* scriptContext, ForInCache * forInCache)
    {TRACE_IT(51875);
        ModuleNamespaceEnumerator* enumerator = RecyclerNew(scriptContext->GetRecycler(), ModuleNamespaceEnumerator, nsObject, flags, scriptContext);
        if (enumerator->Init(forInCache))
        {TRACE_IT(51876);
            return enumerator;
        }
        return nullptr;
    }

    BOOL ModuleNamespaceEnumerator::Init(ForInCache * forInCache)
    {TRACE_IT(51877);
        if (!nsObject->DynamicObject::GetEnumerator(&symbolEnumerator, flags, GetScriptContext(), forInCache))
        {TRACE_IT(51878);
            return FALSE;
        }
        nonLocalMap = nsObject->GetUnambiguousNonLocalExports();
        Reset();
        return TRUE;
    }

    void ModuleNamespaceEnumerator::Reset()
    {TRACE_IT(51879);
        currentLocalMapIndex = Constants::NoBigSlot;
        currentNonLocalMapIndex = Constants::NoBigSlot;
        doneWithLocalExports = false;
        if (!!(flags & EnumeratorFlags::EnumSymbols))
        {TRACE_IT(51880);
            doneWithSymbol = false;
        }
        else
        {TRACE_IT(51881);
            doneWithSymbol = true;
        }
        symbolEnumerator.Reset();
    }

    // enumeration order: symbol first; local exports next; nonlocal exports last.
    Var ModuleNamespaceEnumerator::MoveAndGetNext(PropertyId& propertyId, PropertyAttributes* attributes)
    {TRACE_IT(51882);
        Var undefined = GetLibrary()->GetUndefined();
        Var result = undefined;
        if (attributes != nullptr)
        {TRACE_IT(51883);
            // all the attribute should have the same setting here in namespace object.
            *attributes = PropertyModuleNamespaceDefault;
        }
        if (!doneWithSymbol)
        {TRACE_IT(51884);
            result = symbolEnumerator.MoveAndGetNext(propertyId, attributes);
            if (result == nullptr)
            {TRACE_IT(51885);
                doneWithSymbol = true;
            }
            else
            {TRACE_IT(51886);
                return result;
            }
        }
        if (!this->doneWithLocalExports)
        {TRACE_IT(51887);
            currentLocalMapIndex++;
            JavascriptString* propertyString = nullptr;
            if (!nsObject->FindNextProperty(currentLocalMapIndex, &propertyString, &propertyId, attributes, this->GetScriptContext()))
            {TRACE_IT(51888);
                // we are done with the object part; 
                this->doneWithLocalExports = true;
            }
            else
            {TRACE_IT(51889);
                return propertyString;
            }
        }
        if (this->nonLocalMap != nullptr && (currentNonLocalMapIndex + 1 < nonLocalMap->Count()))
        {TRACE_IT(51890);
            currentNonLocalMapIndex++;
            result = this->GetScriptContext()->GetPropertyString(this->nonLocalMap->GetKeyAt(currentNonLocalMapIndex));
            return result;
        }
        return nullptr;
    }
}
