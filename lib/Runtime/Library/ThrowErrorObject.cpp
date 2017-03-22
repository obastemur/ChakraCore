//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"
#include "Debug/DiagHelperMethodWrapper.h"
#include "Library/ThrowErrorObject.h"

namespace Js
{
    // In some cases we delay throw from helper methods and return ThrowErrorObject instead which we call and throw later.
    // Then the exception is actually thrown when we call this method.
    Var ThrowErrorObject::DefaultEntryPoint(RecyclableObject* function, CallInfo callInfo, ...)
    {
        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        ThrowErrorObject* throwErrorObject = ThrowErrorObject::FromVar(function);

        bool useExceptionWrapper =
            scriptContext->IsScriptContextInDebugMode() /* Check for script context is intentional as library code also uses exception wrapper */ &&
            (ScriptContext::IsExceptionWrapperForBuiltInsEnabled(scriptContext) || ScriptContext::IsExceptionWrapperForHelpersEnabled(scriptContext)) &&
            !AutoRegisterIgnoreExceptionWrapper::IsRegistered(scriptContext->GetThreadContext());

        if (useExceptionWrapper)
        {LOGMEIN("ThrowErrorObject.cpp] 25\n");
            // Forward the throw via regular try-catch wrapper logic that we use for helper/library calls.
            AutoRegisterIgnoreExceptionWrapper autoWrapper(scriptContext->GetThreadContext());

            Var ret = HelperOrLibraryMethodWrapper<true>(scriptContext, [throwErrorObject, scriptContext]() -> Var {
                JavascriptExceptionOperators::Throw(throwErrorObject->m_error, scriptContext);
            });
            return ret;
        }
        else
        {
            JavascriptExceptionOperators::Throw(throwErrorObject->m_error, scriptContext);
        }
    }

    ThrowErrorObject::ThrowErrorObject(StaticType* type, JavascriptError* error)
        : RecyclableObject(type), m_error(error)
    {LOGMEIN("ThrowErrorObject.cpp] 42\n");
    }

    ThrowErrorObject* ThrowErrorObject::New(StaticType* type, JavascriptError* error, Recycler* recycler)
    {LOGMEIN("ThrowErrorObject.cpp] 46\n");
        return RecyclerNew(recycler, ThrowErrorObject, type, error);
    }

    bool ThrowErrorObject::Is(Var aValue)
    {LOGMEIN("ThrowErrorObject.cpp] 51\n");
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Undefined;
    }

    ThrowErrorObject* ThrowErrorObject::FromVar(Var aValue)
    {LOGMEIN("ThrowErrorObject.cpp] 56\n");
        Assert(Is(aValue));
        return static_cast<ThrowErrorObject*>(RecyclableObject::FromVar(aValue));
    }

    RecyclableObject* ThrowErrorObject::CreateThrowErrorObject(CreateErrorFunc createError, ScriptContext* scriptContext, int32 hCode, PCWSTR varName)
    {LOGMEIN("ThrowErrorObject.cpp] 62\n");
        JavascriptLibrary* library = scriptContext->GetLibrary();
        JavascriptError *pError = (library->*createError)();
        JavascriptError::SetErrorMessage(pError, hCode, varName, scriptContext);
        return library->CreateThrowErrorObject(pError);
    }

    RecyclableObject* ThrowErrorObject::CreateThrowTypeErrorObject(ScriptContext* scriptContext, int32 hCode, PCWSTR varName)
    {LOGMEIN("ThrowErrorObject.cpp] 70\n");
        return CreateThrowErrorObject(&JavascriptLibrary::CreateTypeError, scriptContext, hCode, varName);
    }

    RecyclableObject* ThrowErrorObject::CreateThrowTypeErrorObject(ScriptContext* scriptContext, int32 hCode, JavascriptString* varName)
    {LOGMEIN("ThrowErrorObject.cpp] 75\n");
        return CreateThrowTypeErrorObject(scriptContext, hCode, varName->GetSz());
    }
}
