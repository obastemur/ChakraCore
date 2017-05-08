//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    Var JavascriptBoolean::OP_LdTrue(ScriptContext*scriptContext)
    {TRACE_IT(58186);
        return scriptContext->GetLibrary()->GetTrue();
    }

    Var JavascriptBoolean::OP_LdFalse(ScriptContext* scriptContext)
    {TRACE_IT(58187);
        return scriptContext->GetLibrary()->GetFalse();
    }

    Js::Var JavascriptBoolean::ToVar(BOOL fValue, ScriptContext* scriptContext)
    {TRACE_IT(58188);
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
        {TRACE_IT(58189);
            value = JavascriptConversion::ToBoolean(args[1], scriptContext) ? true : false;
        }
        else
        {TRACE_IT(58190);
            value = false;
        }

        if (callInfo.Flags & CallFlags_New)
        {TRACE_IT(58191);
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
        {TRACE_IT(58192);
            return args[0];
        }
        else if (JavascriptBooleanObject::Is(args[0]))
        {TRACE_IT(58193);
            JavascriptBooleanObject* booleanObject = JavascriptBooleanObject::FromVar(args[0]);
            return scriptContext->GetLibrary()->CreateBoolean(booleanObject->GetValue());
        }
        else
        {TRACE_IT(58194);
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
        {TRACE_IT(58195);
            bval = JavascriptBoolean::FromVar(aValue)->GetValue();
        }
        else if (JavascriptBooleanObject::Is(aValue))
        {TRACE_IT(58196);
            JavascriptBooleanObject* booleanObject = JavascriptBooleanObject::FromVar(aValue);
            bval = booleanObject->GetValue();
        }
        else
        {TRACE_IT(58197);
            return TryInvokeRemotelyOrThrow(EntryToString, scriptContext, args, JSERR_This_NeedBoolean, _u("Boolean.prototype.toString"));
        }

        return bval ? scriptContext->GetLibrary()->GetTrueDisplayString() : scriptContext->GetLibrary()->GetFalseDisplayString();
    }

    RecyclableObject * JavascriptBoolean::CloneToScriptContext(ScriptContext* requestContext)
    {TRACE_IT(58198);
        if (this->GetValue())
        {TRACE_IT(58199);
            return requestContext->GetLibrary()->GetTrue();
        }
        return requestContext->GetLibrary()->GetFalse();
    }

    Var JavascriptBoolean::TryInvokeRemotelyOrThrow(JavascriptMethod entryPoint, ScriptContext * scriptContext, Arguments & args, int32 errorCode, PCWSTR varName)
    {TRACE_IT(58200);
        if (JavascriptOperators::GetTypeId(args[0]) == TypeIds_HostDispatch)
        {TRACE_IT(58201);
            Var result;
            if (RecyclableObject::FromVar(args[0])->InvokeBuiltInOperationRemotely(entryPoint, args, &result))
            {TRACE_IT(58202);
                return result;
            }
        }
        // Don't error if we disabled implicit calls
        if(scriptContext->GetThreadContext()->RecordImplicitException())
        {TRACE_IT(58203);
            JavascriptError::ThrowTypeError(scriptContext, errorCode, varName);
        }
        else
        {TRACE_IT(58204);
            return scriptContext->GetLibrary()->GetUndefined();
        }
    }

    BOOL JavascriptBoolean::Equals(Var other, BOOL* value, ScriptContext * requestContext)
    {TRACE_IT(58205);
        return JavascriptBoolean::Equals(this, other, value, requestContext);
    }

    BOOL JavascriptBoolean::Equals(JavascriptBoolean* left, Var right, BOOL* value, ScriptContext * requestContext)
    {TRACE_IT(58206);
        switch (JavascriptOperators::GetTypeId(right))
        {
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
    {TRACE_IT(58207);
        if (this->GetValue())
        {TRACE_IT(58208);
            JavascriptString* trueDisplayString = GetLibrary()->GetTrueDisplayString();
            stringBuilder->Append(trueDisplayString->GetString(), trueDisplayString->GetLength());
        }
        else
        {TRACE_IT(58209);
            JavascriptString* falseDisplayString = GetLibrary()->GetFalseDisplayString();
            stringBuilder->Append(falseDisplayString->GetString(), falseDisplayString->GetLength());
        }
        return TRUE;
    }

    BOOL JavascriptBoolean::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {TRACE_IT(58210);
        stringBuilder->AppendCppLiteral(_u("Boolean"));
        return TRUE;
    }

    RecyclableObject* JavascriptBoolean::ToObject(ScriptContext * requestContext)
    {TRACE_IT(58211);
        return requestContext->GetLibrary()->CreateBooleanObject(this->GetValue() ? true : false);
    }

    Var JavascriptBoolean::GetTypeOfString(ScriptContext * requestContext)
    {TRACE_IT(58212);
        return requestContext->GetLibrary()->GetBooleanTypeDisplayString();
    }
} // namespace Js
