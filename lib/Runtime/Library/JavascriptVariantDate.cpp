//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    bool JavascriptVariantDate::Is(Var aValue)
    {LOGMEIN("JavascriptVariantDate.cpp] 9\n");
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_VariantDate;
    }

    JavascriptVariantDate* JavascriptVariantDate::FromVar(Js::Var aValue)
    {
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptVariantDate'");

        return static_cast<JavascriptVariantDate *>(RecyclableObject::FromVar(aValue));
    }

    Var JavascriptVariantDate::GetTypeOfString(ScriptContext* requestContext)
    {LOGMEIN("JavascriptVariantDate.cpp] 21\n");
        return requestContext->GetLibrary()->GetVariantDateTypeDisplayString();
    }

    JavascriptString* JavascriptVariantDate::GetValueString(ScriptContext* scriptContext)
    {LOGMEIN("JavascriptVariantDate.cpp] 26\n");
        return DateImplementation::ConvertVariantDateToString(this->value, scriptContext);
    }

    BOOL JavascriptVariantDate::GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {LOGMEIN("JavascriptVariantDate.cpp] 31\n");
        BOOL ret;

        ENTER_PINNED_SCOPE(JavascriptString, resultString);
        resultString = DateImplementation::ConvertVariantDateToString(this->value, GetScriptContext());
        if (resultString != nullptr)
        {LOGMEIN("JavascriptVariantDate.cpp] 37\n");
            stringBuilder->Append(resultString->GetString(), resultString->GetLength());
            ret = TRUE;
        }
        else
        {
            ret = FALSE;
        }

        LEAVE_PINNED_SCOPE();

        return ret;
    }

    BOOL JavascriptVariantDate::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {LOGMEIN("JavascriptVariantDate.cpp] 52\n");
        stringBuilder->AppendCppLiteral(_u("Date")); // For whatever reason in IE8 jscript, typeof returns "date"
                                                  // while the debugger displays "Date" for the type
        return TRUE;
    }

    RecyclableObject * JavascriptVariantDate::CloneToScriptContext(ScriptContext* requestContext)
    {LOGMEIN("JavascriptVariantDate.cpp] 59\n");
        return requestContext->GetLibrary()->CreateVariantDate(value);
    }

    RecyclableObject* JavascriptVariantDate::ToObject(ScriptContext* requestContext)
    {LOGMEIN("JavascriptVariantDate.cpp] 64\n");
        // WOOB 1124298: Just return a new object when converting to object.
        return requestContext->GetLibrary()->CreateObject(true);
    }

    BOOL JavascriptVariantDate::GetProperty(Js::Var originalInstance, Js::PropertyId propertyId, Js::Var* value, PropertyValueInfo* info, Js::ScriptContext* requestContext)
    {LOGMEIN("JavascriptVariantDate.cpp] 70\n");
        if (requestContext->GetThreadContext()->RecordImplicitException())
        {LOGMEIN("JavascriptVariantDate.cpp] 72\n");
            JavascriptError::ThrowTypeError(requestContext, JSERR_Property_VarDate, requestContext->GetPropertyName(propertyId)->GetBuffer());
        }
        *value = nullptr;
        return true;
    };

    BOOL JavascriptVariantDate::GetProperty(Js::Var originalInstance, Js::JavascriptString* propertyNameString, Js::Var* value, PropertyValueInfo* info, Js::ScriptContext* requestContext)
    {LOGMEIN("JavascriptVariantDate.cpp] 80\n");
        if (requestContext->GetThreadContext()->RecordImplicitException())
        {LOGMEIN("JavascriptVariantDate.cpp] 82\n");
            JavascriptError::ThrowTypeError(requestContext, JSERR_Property_VarDate, propertyNameString);
        }
        *value = nullptr;
        return true;
    };

    BOOL JavascriptVariantDate::GetPropertyReference(Js::Var originalInstance, Js::PropertyId propertyId, Js::Var* value, PropertyValueInfo* info, Js::ScriptContext* requestContext)
    {LOGMEIN("JavascriptVariantDate.cpp] 90\n");
        if (requestContext->GetThreadContext()->RecordImplicitException())
        {LOGMEIN("JavascriptVariantDate.cpp] 92\n");
            JavascriptError::ThrowTypeError(requestContext, JSERR_Property_VarDate, requestContext->GetPropertyName(propertyId)->GetBuffer());
        }
        *value = nullptr;
        return true;
    };

    BOOL JavascriptVariantDate::SetProperty(Js::PropertyId propertyId, Js::Var value, Js::PropertyOperationFlags flags, PropertyValueInfo* info)
    {LOGMEIN("JavascriptVariantDate.cpp] 100\n");
        ScriptContext* scriptContext = this->GetScriptContext();
        JavascriptError::ThrowTypeError(scriptContext, JSERR_Property_VarDate, scriptContext->GetPropertyName(propertyId)->GetBuffer());
    };

    BOOL JavascriptVariantDate::SetProperty(Js::JavascriptString* propertyNameString, Js::Var value, Js::PropertyOperationFlags flags, PropertyValueInfo* info)
    {LOGMEIN("JavascriptVariantDate.cpp] 106\n");
        ScriptContext* scriptContext = this->GetScriptContext();
        JavascriptError::ThrowTypeError(scriptContext, JSERR_Property_VarDate, propertyNameString->GetSz());
    };

    BOOL JavascriptVariantDate::InitProperty(Js::PropertyId propertyId, Js::Var value, PropertyOperationFlags flags, Js::PropertyValueInfo* info)
    {LOGMEIN("JavascriptVariantDate.cpp] 112\n");
        ScriptContext* scriptContext = this->GetScriptContext();
        JavascriptError::ThrowTypeError(scriptContext, JSERR_Property_VarDate, scriptContext->GetPropertyName(propertyId)->GetBuffer());
    };

    BOOL JavascriptVariantDate::DeleteProperty(Js::PropertyId propertyId, Js::PropertyOperationFlags flags)
    {LOGMEIN("JavascriptVariantDate.cpp] 118\n");
        ScriptContext* scriptContext = this->GetScriptContext();
        JavascriptError::ThrowTypeError(scriptContext, JSERR_Property_VarDate, scriptContext->GetPropertyName(propertyId)->GetBuffer());
    };

    BOOL JavascriptVariantDate::DeleteProperty(JavascriptString *propertyNameString, Js::PropertyOperationFlags flags)
    {LOGMEIN("JavascriptVariantDate.cpp] 124\n");
        ScriptContext* scriptContext = this->GetScriptContext();
        JavascriptError::ThrowTypeError(scriptContext, JSERR_Property_VarDate, propertyNameString->GetString());
    };

    BOOL JavascriptVariantDate::GetItemReference(Js::Var originalInstance, uint32 index, Js::Var* value, Js::ScriptContext * scriptContext)
    {LOGMEIN("JavascriptVariantDate.cpp] 130\n");
        JavascriptError::ThrowTypeError(scriptContext, JSERR_Property_VarDate, JavascriptNumber::ToStringRadix10(index, scriptContext)->GetSz());
    };

    BOOL JavascriptVariantDate::GetItem(Js::Var originalInstance, uint32 index, Js::Var* value, Js::ScriptContext * scriptContext)
    {LOGMEIN("JavascriptVariantDate.cpp] 135\n");
        JavascriptError::ThrowTypeError(scriptContext, JSERR_Property_VarDate, JavascriptNumber::ToStringRadix10(index, scriptContext)->GetSz());
    };

    BOOL JavascriptVariantDate::SetItem(uint32 index, Js::Var value, Js::PropertyOperationFlags flags)
    {LOGMEIN("JavascriptVariantDate.cpp] 140\n");
        ScriptContext* scriptContext = this->GetScriptContext();
        JavascriptError::ThrowTypeError(scriptContext, JSERR_Property_VarDate, JavascriptNumber::ToStringRadix10(index, scriptContext)->GetSz());
    };

    BOOL JavascriptVariantDate::ToPrimitive(JavascriptHint hint, Var* result, ScriptContext * requestContext)
    {LOGMEIN("JavascriptVariantDate.cpp] 146\n");
        if (hint == JavascriptHint::HintString)
        {LOGMEIN("JavascriptVariantDate.cpp] 148\n");
            JavascriptString* resultString = this->GetValueString(requestContext);
            if (resultString != nullptr)
            {LOGMEIN("JavascriptVariantDate.cpp] 151\n");
                (*result) = resultString;
                return TRUE;
            }
            Assert(false);
        }
        else if (hint == JavascriptHint::HintNumber)
        {LOGMEIN("JavascriptVariantDate.cpp] 158\n");
            *result = JavascriptNumber::ToVarNoCheck(DateImplementation::JsUtcTimeFromVarDate(value, requestContext), requestContext);
            return TRUE;
        }
        else
        {
            Assert(hint == JavascriptHint::None);
            *result = this;
            return TRUE;
        }
        return FALSE;
    }

    BOOL JavascriptVariantDate::Equals(Var other, BOOL *value, ScriptContext * requestContext)
    {LOGMEIN("JavascriptVariantDate.cpp] 172\n");
        // Calling .Equals on a VT_DATE variant at least gives the "[property name] is null or not An object error"
        *value = FALSE;
        return TRUE;
    }
}
