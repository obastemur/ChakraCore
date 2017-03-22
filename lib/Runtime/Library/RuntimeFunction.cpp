//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    RuntimeFunction::RuntimeFunction(DynamicType * type)
        : JavascriptFunction(type), functionNameId(nullptr)
    {LOGMEIN("RuntimeFunction.cpp] 10\n");}

    RuntimeFunction::RuntimeFunction(DynamicType * type, FunctionInfo * functionInfo)
        : JavascriptFunction(type, functionInfo), functionNameId(nullptr)
    {LOGMEIN("RuntimeFunction.cpp] 14\n");}

    RuntimeFunction::RuntimeFunction(DynamicType * type, FunctionInfo * functionInfo, ConstructorCache* cache)
        : JavascriptFunction(type, functionInfo, cache), functionNameId(nullptr)
    {LOGMEIN("RuntimeFunction.cpp] 18\n");}

    Var
    RuntimeFunction::EnsureSourceString()
    {LOGMEIN("RuntimeFunction.cpp] 22\n");
        JavascriptLibrary* library = this->GetLibrary();
        ScriptContext * scriptContext = library->GetScriptContext();
        if (this->functionNameId == nullptr)
        {LOGMEIN("RuntimeFunction.cpp] 26\n");
            this->functionNameId = library->GetFunctionDisplayString();
        }
        else
        {
            if (TaggedInt::Is(this->functionNameId))
            {LOGMEIN("RuntimeFunction.cpp] 32\n");
                if (this->GetScriptContext()->GetConfig()->IsES6FunctionNameEnabled() && this->GetTypeHandler()->IsDeferredTypeHandler())
                {LOGMEIN("RuntimeFunction.cpp] 34\n");
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
    {LOGMEIN("RuntimeFunction.cpp] 49\n");
        Assert(functionNameId == NULL);
        Assert(TaggedInt::Is(nameId) || Js::JavascriptString::Is(nameId));

        // We are only reference the propertyId, it needs to be tracked to stay alive
        Assert(!TaggedInt::Is(nameId) || this->GetScriptContext()->IsTrackedPropertyId(TaggedInt::ToInt32(nameId)));
        this->functionNameId = nameId;
    }

#if ENABLE_TTD
    void RuntimeFunction::MarkVisitKindSpecificPtrs(TTD::SnapshotExtractor* extractor)
    {LOGMEIN("RuntimeFunction.cpp] 60\n");
        if(this->functionNameId != nullptr)
        {LOGMEIN("RuntimeFunction.cpp] 62\n");
            extractor->MarkVisitVar(this->functionNameId);
        }

        Var revokableProxy = nullptr;
        RuntimeFunction* function = const_cast<RuntimeFunction*>(this);
        if(function->GetInternalProperty(function, Js::InternalPropertyIds::RevocableProxy, &revokableProxy, nullptr, this->GetScriptContext()))
        {LOGMEIN("RuntimeFunction.cpp] 69\n");
            extractor->MarkVisitVar(revokableProxy);
        }
    }

    TTD::NSSnapObjects::SnapObjectType RuntimeFunction::GetSnapTag_TTD() const
    {LOGMEIN("RuntimeFunction.cpp] 75\n");
        Var revokableProxy = nullptr;
        RuntimeFunction* function = const_cast<RuntimeFunction*>(this);
        if(function->GetInternalProperty(function, Js::InternalPropertyIds::RevocableProxy, &revokableProxy, nullptr, this->GetScriptContext()))
        {LOGMEIN("RuntimeFunction.cpp] 79\n");
            return TTD::NSSnapObjects::SnapObjectType::SnapRuntimeRevokerFunctionObject;
        }
        else
        {
            return TTD::NSSnapObjects::SnapObjectType::SnapRuntimeFunctionObject;
        }
    }

    void RuntimeFunction::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {LOGMEIN("RuntimeFunction.cpp] 89\n");
        //
        //TODO: need to add more promise support
        //

        Var revokableProxy = nullptr;
        RuntimeFunction* function = const_cast<RuntimeFunction*>(this);
        if(function->GetInternalProperty(function, Js::InternalPropertyIds::RevocableProxy, &revokableProxy, nullptr, this->GetScriptContext()))
        {LOGMEIN("RuntimeFunction.cpp] 97\n");
            TTD_PTR_ID* proxyId = alloc.SlabAllocateStruct<TTD_PTR_ID>();
            *proxyId = (JavascriptOperators::GetTypeId(revokableProxy) != TypeIds_Null) ? TTD_CONVERT_VAR_TO_PTR_ID(revokableProxy) : TTD_INVALID_PTR_ID;

            if(*proxyId == TTD_INVALID_PTR_ID)
            {LOGMEIN("RuntimeFunction.cpp] 102\n");
                TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD_PTR_ID*, TTD::NSSnapObjects::SnapObjectType::SnapRuntimeRevokerFunctionObject>(objData, proxyId);
            }
            else
            {
                TTDAssert(TTD::JsSupport::IsVarComplexKind(revokableProxy), "Huh, it looks like we need to check before adding this as a dep on.");

                uint32 depOnCount = 1;
                TTD_PTR_ID* depOnArray = alloc.SlabAllocateArray<TTD_PTR_ID>(1);
                depOnArray[0] = TTD_CONVERT_VAR_TO_PTR_ID(revokableProxy);

                TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD_PTR_ID*, TTD::NSSnapObjects::SnapObjectType::SnapRuntimeRevokerFunctionObject>(objData, proxyId, alloc, depOnCount, depOnArray);
            }
        }
        else
        {
            TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<void*, TTD::NSSnapObjects::SnapObjectType::SnapRuntimeFunctionObject>(objData, nullptr);
        }
    }
#endif
};
