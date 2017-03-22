//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLanguagePch.h"

namespace Js {

    uint64 StackTraceArguments::ObjectToTypeCode(Js::Var object)
    {LOGMEIN("StackTraceArguments.cpp] 9\n");
        switch(JavascriptOperators::GetTypeId(object))
        {LOGMEIN("StackTraceArguments.cpp] 11\n");
            case TypeIds_Null:
                return nullValue;
            case TypeIds_Undefined:
                return undefinedValue;
            case TypeIds_Boolean:
                return booleanValue;
            case TypeIds_String:
                return stringValue;
            case TypeIds_Symbol:
                return symbolValue;
            case TypeIds_Number:
                if (Js::JavascriptNumber::IsNan(JavascriptNumber::GetValue(object)))
                {LOGMEIN("StackTraceArguments.cpp] 24\n");
                    return nanValue;
                }
                else
                {
                    return numberValue;
                }
            case TypeIds_Integer:
            case TypeIds_Int64Number:
            case TypeIds_UInt64Number:
                return numberValue;
        }
        return objectValue;
    }

    JavascriptString *StackTraceArguments::TypeCodeToTypeName(unsigned typeCode, ScriptContext *scriptContext)
    {LOGMEIN("StackTraceArguments.cpp] 40\n");
        switch(typeCode)
        {LOGMEIN("StackTraceArguments.cpp] 42\n");
            case nullValue:
                return scriptContext->GetLibrary()->GetNullDisplayString();
            case undefinedValue:
                return scriptContext->GetLibrary()->GetUndefinedDisplayString();
            case booleanValue:
                return scriptContext->GetLibrary()->GetBooleanTypeDisplayString();
            case stringValue:
                return scriptContext->GetLibrary()->GetStringTypeDisplayString();
            case nanValue:
                return scriptContext->GetLibrary()->GetNaNDisplayString();
            case numberValue:
                return scriptContext->GetLibrary()->GetNumberTypeDisplayString();
            case symbolValue:
                return scriptContext->GetLibrary()->GetSymbolTypeDisplayString();
            case objectValue:
                return scriptContext->GetLibrary()->GetObjectTypeDisplayString();
            default:
              AssertMsg(0, "Unknown type code");
              return scriptContext->GetLibrary()->GetEmptyString();
        }
    }

    void StackTraceArguments::Init(const JavascriptStackWalker &walker)
    {LOGMEIN("StackTraceArguments.cpp] 66\n");
        types = 0;
        if (!walker.IsCallerGlobalFunction())
        {LOGMEIN("StackTraceArguments.cpp] 69\n");
            int64 numberOfArguments = walker.GetCallInfo()->Count;
            if (numberOfArguments > 0) numberOfArguments --; // Don't consider 'this'
            if (walker.GetCallInfo()->Flags & Js::CallFlags_ExtraArg)
            {LOGMEIN("StackTraceArguments.cpp] 73\n");
                Assert(numberOfArguments > 0 );
                // skip the last FrameDisplay argument.
                numberOfArguments--;
            }
            for (int64 j = 0; j < numberOfArguments && j < MaxNumberOfDisplayedArgumentsInStack; j ++)
            {LOGMEIN("StackTraceArguments.cpp] 79\n");
                types |= ObjectToTypeCode(walker.GetJavascriptArgs()[j]) << 3*j; // maximal code is 7, so we can use 3 bits to store it
            }
            if (numberOfArguments > MaxNumberOfDisplayedArgumentsInStack)
            {LOGMEIN("StackTraceArguments.cpp] 83\n");
                types |= fTooManyArgs; // two upper bits are flags
            }
        }
        else
        {
            types |= fCallerIsGlobal; // two upper bits are flags
        }
    }

    HRESULT StackTraceArguments::ToString(LPCWSTR functionName, Js::ScriptContext *scriptContext, _In_ LPCWSTR *outResult) const
    {LOGMEIN("StackTraceArguments.cpp] 94\n");
        HRESULT hr = S_OK;
        uint64 argumentsTypes = types;
        BEGIN_TRANSLATE_EXCEPTION_AND_ERROROBJECT_TO_HRESULT_NESTED
        {
            CompoundString *const stringBuilder = CompoundString::NewWithCharCapacity(40, scriptContext->GetLibrary());
            stringBuilder->AppendCharsSz(functionName);
            bool calleIsGlobalFunction = (argumentsTypes & fCallerIsGlobal) != 0;
            bool toManyArgs = (argumentsTypes & fTooManyArgs) != 0;
            argumentsTypes &= ~fCallerIsGlobal; // erase flags to prevent them from being treated as values
            argumentsTypes &= ~fTooManyArgs;
            if (!calleIsGlobalFunction)
            {LOGMEIN("StackTraceArguments.cpp] 106\n");
                stringBuilder->AppendChars(_u('('));
            }
            for (uint64 i = 0; i < MaxNumberOfDisplayedArgumentsInStack && argumentsTypes != 0; i ++)
            {LOGMEIN("StackTraceArguments.cpp] 110\n");
                if (i > 0)
                {LOGMEIN("StackTraceArguments.cpp] 112\n");
                    stringBuilder->AppendChars(_u(", "));
                }
                stringBuilder->AppendChars(TypeCodeToTypeName(argumentsTypes & 7, scriptContext)); // we use 3 bits to store one code
                argumentsTypes >>= 3;
            }
            if (toManyArgs)
            {LOGMEIN("StackTraceArguments.cpp] 119\n");
                stringBuilder->AppendChars(_u(", ..."));
            }
            if (!calleIsGlobalFunction)
            {LOGMEIN("StackTraceArguments.cpp] 123\n");
                stringBuilder->AppendChars(_u(')'));
            }
            *outResult = stringBuilder->GetString();
        }
        END_TRANSLATE_EXCEPTION_AND_ERROROBJECT_TO_HRESULT(hr);
        return hr;
    }
}
