//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    Var JavascriptBoolean::OP_LdTrue(ScriptContext*scriptContext)
    {LOGMEIN("JavascriptBoolean.cpp] 9\n");
        return scriptContext->GetLibrary()->GetTrue();
    }

    Var JavascriptBoolean::OP_LdFalse(ScriptContext* scriptContext)
    {LOGMEIN("JavascriptBoolean.cpp] 14\n");
        return scriptContext->GetLibrary()->GetFalse();
    }

    Js::Var JavascriptBoolean::ToVar(BOOL fValue, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptBoolean.cpp] 19\n");
        return
            fValue ?
            scriptContext->GetLibrary()->GetTrue() :
            scriptContext->GetLibrary()->GetFalse();
    }

    Var JavascriptBoolean::NewInstance(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");

        // SkipDefaultNewObject function flag should have prevented the default object from
        // being created, except when call true a host dispatch.
        Var newTarget = callInfo.Flags & CallFlags_NewTarget ? args.Values[args.Info.Count] : args[0];
        bool isCtorSuperCall = (callInfo.Flags & CallFlags_New) && newTarget != nullptr && !JavascriptOperators::IsUndefined(newTarget);
        Assert(isCtorSuperCall || !(callInfo.Flags & CallFlags_New) || args[0] == nullptr
            || JavascriptOperators::GetTypeId(args[0]) == TypeIds_HostDispatch);

        BOOL value;

        if (args.Info.Count > 1)
        {LOGMEIN("JavascriptBoolean.cpp] 45\n");
            value = JavascriptConversion::ToBoolean(args[1], scriptContext) ? true : false;
        }
        else
        {
            value = false;
        }

        if (callInfo.Flags & CallFlags_New)
        {LOGMEIN("JavascriptBoolean.cpp] 54\n");
            RecyclableObject* pNew = scriptContext->GetLibrary()->CreateBooleanObject(value);
            return isCtorSuperCall ?
                JavascriptOperators::OrdinaryCreateFromConstructor(RecyclableObject::FromVar(newTarget), pNew, nullptr, scriptContext) :
                pNew;
        }

        return scriptContext->GetLibrary()->CreateBoolean(value);
    }

    // Boolean.prototype.valueOf as described in ES6 spec (draft 24) 19.3.3.3
    Var JavascriptBoolean::EntryValueOf(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if(JavascriptBoolean::Is(args[0]))
        {LOGMEIN("JavascriptBoolean.cpp] 75\n");
            return args[0];
        }
        else if (JavascriptBooleanObject::Is(args[0]))
        {LOGMEIN("JavascriptBoolean.cpp] 79\n");
            JavascriptBooleanObject* booleanObject = JavascriptBooleanObject::FromVar(args[0]);
            return scriptContext->GetLibrary()->CreateBoolean(booleanObject->GetValue());
        }
        else
        {
            return TryInvokeRemotelyOrThrow(EntryValueOf, scriptContext, args, JSERR_This_NeedBoolean, _u("Boolean.prototype.valueOf"));
        }
    }

    // Boolean.prototype.toString as described in ES6 spec (draft 24) 19.3.3.2
    Var JavascriptBoolean::EntryToString(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count, "Should always have implicit 'this'.");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        BOOL bval;
        Var aValue = args[0];
        if(JavascriptBoolean::Is(aValue))
        {LOGMEIN("JavascriptBoolean.cpp] 103\n");
            bval = JavascriptBoolean::FromVar(aValue)->GetValue();
        }
        else if (JavascriptBooleanObject::Is(aValue))
        {LOGMEIN("JavascriptBoolean.cpp] 107\n");
            JavascriptBooleanObject* booleanObject = JavascriptBooleanObject::FromVar(aValue);
            bval = booleanObject->GetValue();
        }
        else
        {
            return TryInvokeRemotelyOrThrow(EntryToString, scriptContext, args, JSERR_This_NeedBoolean, _u("Boolean.prototype.toString"));
        }

        return bval ? scriptContext->GetLibrary()->GetTrueDisplayString() : scriptContext->GetLibrary()->GetFalseDisplayString();
    }

    RecyclableObject * JavascriptBoolean::CloneToScriptContext(ScriptContext* requestContext)
    {LOGMEIN("JavascriptBoolean.cpp] 120\n");
        if (this->GetValue())
        {LOGMEIN("JavascriptBoolean.cpp] 122\n");
            return requestContext->GetLibrary()->GetTrue();
        }
        return requestContext->GetLibrary()->GetFalse();
    }

    Var JavascriptBoolean::TryInvokeRemotelyOrThrow(JavascriptMethod entryPoint, ScriptContext * scriptContext, Arguments & args, int32 errorCode, PCWSTR varName)
    {LOGMEIN("JavascriptBoolean.cpp] 129\n");
        if (JavascriptOperators::GetTypeId(args[0]) == TypeIds_HostDispatch)
        {LOGMEIN("JavascriptBoolean.cpp] 131\n");
            Var result;
            if (RecyclableObject::FromVar(args[0])->InvokeBuiltInOperationRemotely(entryPoint, args, &result))
            {LOGMEIN("JavascriptBoolean.cpp] 134\n");
                return result;
            }
        }
        // Don't error if we disabled implicit calls
        if(scriptContext->GetThreadContext()->RecordImplicitException())
        {LOGMEIN("JavascriptBoolean.cpp] 140\n");
            JavascriptError::ThrowTypeError(scriptContext, errorCode, varName);
        }
        else
        {
            return scriptContext->GetLibrary()->GetUndefined();
        }
    }

    BOOL JavascriptBoolean::Equals(Var other, BOOL* value, ScriptContext * requestContext)
    {LOGMEIN("JavascriptBoolean.cpp] 150\n");
        return JavascriptBoolean::Equals(this, other, value, requestContext);
    }

    BOOL JavascriptBoolean::Equals(JavascriptBoolean* left, Var right, BOOL* value, ScriptContext * requestContext)
    {LOGMEIN("JavascriptBoolean.cpp] 155\n");
        switch (JavascriptOperators::GetTypeId(right))
        {LOGMEIN("JavascriptBoolean.cpp] 157\n");
        case TypeIds_Integer:
            *value = left->GetValue() ? TaggedInt::ToInt32(right) == 1 : TaggedInt::ToInt32(right) == 0;
            break;
        case TypeIds_Number:
            *value = left->GetValue() ? JavascriptNumber::GetValue(right) == 1.0 : JavascriptNumber::GetValue(right) == 0.0;
            break;
        case TypeIds_Int64Number:
            *value = left->GetValue() ? JavascriptInt64Number::FromVar(right)->GetValue() == 1 : JavascriptInt64Number::FromVar(right)->GetValue() == 0;
            break;
        case TypeIds_UInt64Number:
            *value = left->GetValue() ? JavascriptUInt64Number::FromVar(right)->GetValue() == 1 : JavascriptUInt64Number::FromVar(right)->GetValue() == 0;
            break;
        case TypeIds_Boolean:
            *value = left->GetValue() == JavascriptBoolean::FromVar(right)->GetValue();
            break;
        case TypeIds_String:
            *value = left->GetValue() ? JavascriptConversion::ToNumber(right, requestContext) == 1.0 : JavascriptConversion::ToNumber(right, requestContext) == 0.0;
            break;
        case TypeIds_Symbol:
            *value = FALSE;
            break;
        case TypeIds_VariantDate:
            // == on a variant always returns false. Putting this in a
            // switch in each .Equals to prevent a perf hit by adding an
            // if branch to JavascriptOperators::Equal_Full
            *value = FALSE;
            break;
        case TypeIds_Undefined:
        case TypeIds_Null:
        default:
            *value = JavascriptOperators::Equal_Full(left->GetValue() ? TaggedInt::ToVarUnchecked(1) : TaggedInt::ToVarUnchecked(0), right, requestContext);
            break;
        }
        return true;
    }

    BOOL JavascriptBoolean::GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {LOGMEIN("JavascriptBoolean.cpp] 195\n");
        if (this->GetValue())
        {LOGMEIN("JavascriptBoolean.cpp] 197\n");
            JavascriptString* trueDisplayString = GetLibrary()->GetTrueDisplayString();
            stringBuilder->Append(trueDisplayString->GetString(), trueDisplayString->GetLength());
        }
        else
        {
            JavascriptString* falseDisplayString = GetLibrary()->GetFalseDisplayString();
            stringBuilder->Append(falseDisplayString->GetString(), falseDisplayString->GetLength());
        }
        return TRUE;
    }

    BOOL JavascriptBoolean::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {LOGMEIN("JavascriptBoolean.cpp] 210\n");
        stringBuilder->AppendCppLiteral(_u("Boolean"));
        return TRUE;
    }

    RecyclableObject* JavascriptBoolean::ToObject(ScriptContext * requestContext)
    {LOGMEIN("JavascriptBoolean.cpp] 216\n");
        return requestContext->GetLibrary()->CreateBooleanObject(this->GetValue() ? true : false);
    }

    Var JavascriptBoolean::GetTypeOfString(ScriptContext * requestContext)
    {LOGMEIN("JavascriptBoolean.cpp] 221\n");
        return requestContext->GetLibrary()->GetBooleanTypeDisplayString();
    }
} // namespace Js
