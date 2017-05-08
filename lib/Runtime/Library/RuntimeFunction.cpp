//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    RuntimeFunction::RuntimeFunction(DynamicType * type)
        : JavascriptFunction(type), functionNameId(nullptr)
    {TRACE_IT(62829);}

    RuntimeFunction::RuntimeFunction(DynamicType * type, FunctionInfo * functionInfo)
        : JavascriptFunction(type, functionInfo), functionNameId(nullptr)
    {TRACE_IT(62830);}

    RuntimeFunction::RuntimeFunction(DynamicType * type, FunctionInfo * functionInfo, ConstructorCache* cache)
        : JavascriptFunction(type, functionInfo, cache), functionNameId(nullptr)
    {TRACE_IT(62831);}

    Var
    RuntimeFunction::EnsureSourceString()
    {TRACE_IT(62832);
        JavascriptLibrary* library = this->GetLibrary();
        ScriptContext * scriptContext = library->GetScriptContext();
        if (this->functionNameId == nullptr)
        {TRACE_IT(62833);
            this->functionNameId = library->GetFunctionDisplayString();
        }
        else
        {TRACE_IT(62834);
            if (TaggedInt::Is(this->functionNameId))
            {TRACE_IT(62835);
                if (this->GetScriptContext()->GetConfig()->IsES6FunctionNameEnabled() && this->GetTypeHandler()->IsDeferredTypeHandler())
                {TRACE_IT(62836);
                    JavascriptString* functionName = nullptr;
                    DebugOnly(bool status = ) this->GetFunctionName(&functionName);
                    Assert(status);
                    this->SetPropertyWithAttributes(PropertyIds::name, functionName, PropertyConfigurable, nullptr);
                }
                this->functionNameId = GetNativeFunctionDisplayString(scriptContext, scriptContext->GetPropertyString(TaggedInt::ToInt32(this->functionNameId)));
            }
        }
        Assert(JavascriptString::Is(this->functionNameId));
        return this->functionNameId;
    }

    void
    RuntimeFunction::SetFunctionNameId(Var nameId)
    {TRACE_IT(62837);
        Assert(functionNameId == NULL);
        Assert(TaggedInt::Is(nameId) || Js::JavascriptString::Is(nameId));

        // We are only reference the propertyId, it needs to be tracked to stay alive
        Assert(!TaggedInt::Is(nameId) || this->GetScriptContext()->IsTrackedPropertyId(TaggedInt::ToInt32(nameId)));
        this->functionNameId = nameId;
    }

#if ENABLE_TTD
    void RuntimeFunction::MarkVisitKindSpecificPtrs(TTD::SnapshotExtractor* extractor)
    {TRACE_IT(62838);
        if(this->functionNameId != nullptr)
        {TRACE_IT(62839);
            extractor->MarkVisitVar(this->functionNameId);
        }

        Var revokableProxy = nullptr;
        RuntimeFunction* function = const_cast<RuntimeFunction*>(this);
        if(function->GetInternalProperty(function, Js::InternalPropertyIds::RevocableProxy, &revokableProxy, nullptr, this->GetScriptContext()))
        {TRACE_IT(62840);
            extractor->MarkVisitVar(revokableProxy);
        }
    }

    TTD::NSSnapObjects::SnapObjectType RuntimeFunction::GetSnapTag_TTD() const
    {TRACE_IT(62841);
        Var revokableProxy = nullptr;
        RuntimeFunction* function = const_cast<RuntimeFunction*>(this);
        if(function->GetInternalProperty(function, Js::InternalPropertyIds::RevocableProxy, &revokableProxy, nullptr, this->GetScriptContext()))
        {TRACE_IT(62842);
            return TTD::NSSnapObjects::SnapObjectType::SnapRuntimeRevokerFunctionObject;
        }
        else
        {TRACE_IT(62843);
            return TTD::NSSnapObjects::SnapObjectType::SnapRuntimeFunctionObject;
        }
    }

    void RuntimeFunction::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {TRACE_IT(62844);
        //
        //TODO: need to add more promise support
        //

        Var revokableProxy = nullptr;
        RuntimeFunction* function = const_cast<RuntimeFunction*>(this);
        if(function->GetInternalProperty(function, Js::InternalPropertyIds::RevocableProxy, &revokableProxy, nullptr, this->GetScriptContext()))
        {TRACE_IT(62845);
            TTD_PTR_ID* proxyId = alloc.SlabAllocateStruct<TTD_PTR_ID>();
            *proxyId = (JavascriptOperators::GetTypeId(revokableProxy) != TypeIds_Null) ? TTD_CONVERT_VAR_TO_PTR_ID(revokableProxy) : TTD_INVALID_PTR_ID;

            if(*proxyId == TTD_INVALID_PTR_ID)
            {TRACE_IT(62846);
                TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD_PTR_ID*, TTD::NSSnapObjects::SnapObjectType::SnapRuntimeRevokerFunctionObject>(objData, proxyId);
            }
            else
            {TRACE_IT(62847);
                TTDAssert(TTD::JsSupport::IsVarComplexKind(revokableProxy), "Huh, it looks like we need to check before adding this as a dep on.");

                uint32 depOnCount = 1;
                TTD_PTR_ID* depOnArray = alloc.SlabAllocateArray<TTD_PTR_ID>(1);
                depOnArray[0] = TTD_CONVERT_VAR_TO_PTR_ID(revokableProxy);

                TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD_PTR_ID*, TTD::NSSnapObjects::SnapObjectType::SnapRuntimeRevokerFunctionObject>(objData, proxyId, alloc, depOnCount, depOnArray);
            }
        }
        else
        {TRACE_IT(62848);
            TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<void*, TTD::NSSnapObjects::SnapObjectType::SnapRuntimeFunctionObject>(objData, nullptr);
        }
    }
#endif
};
