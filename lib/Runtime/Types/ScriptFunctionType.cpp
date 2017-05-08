//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeTypePch.h"

namespace Js
{
    ScriptFunctionType::ScriptFunctionType(ScriptFunctionType * type)
        : DynamicType(type), entryPointInfo(type->GetEntryPointInfo())
    {TRACE_IT(67005);}

    ScriptFunctionType::ScriptFunctionType(ScriptContext* scriptContext, RecyclableObject* prototype,
        JavascriptMethod entryPoint, ProxyEntryPointInfo * entryPointInfo, DynamicTypeHandler * typeHandler,
        bool isLocked, bool isShared)
        : DynamicType(scriptContext, TypeIds_Function, prototype, entryPoint, typeHandler, isLocked, isShared),
        entryPointInfo(entryPointInfo)
    {TRACE_IT(67006);

    }

    ScriptFunctionType * ScriptFunctionType::New(FunctionProxy * proxy, bool isShared)
    {TRACE_IT(67007);
        Assert(proxy->GetFunctionInfo()->GetFunctionProxy() == proxy);
        ScriptContext * scriptContext = proxy->GetScriptContext();
        JavascriptLibrary * library = scriptContext->GetLibrary();
        DynamicObject * functionPrototype = proxy->IsAsync() ? library->GetAsyncFunctionPrototype() : library->GetFunctionPrototype();
        JavascriptMethod address = proxy->GetDefaultEntryPointInfo()->jsMethod;

        return RecyclerNew(scriptContext->GetRecycler(), ScriptFunctionType,
            scriptContext, functionPrototype,
            address,
            proxy->GetDefaultEntryPointInfo(),
            library->ScriptFunctionTypeHandler(!proxy->IsConstructor(), proxy->GetIsAnonymousFunction()),
            isShared, isShared);
    }
};
