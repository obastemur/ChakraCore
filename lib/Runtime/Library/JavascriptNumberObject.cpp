//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    JavascriptNumberObject::JavascriptNumberObject(DynamicType * type)
        : DynamicObject(type), value(Js::TaggedInt::ToVarUnchecked(0))
    {LOGMEIN("JavascriptNumberObject.cpp] 10\n");
        Assert(type->GetTypeId() == TypeIds_NumberObject);
    }

    JavascriptNumberObject::JavascriptNumberObject(Var value, DynamicType * type)
        : DynamicObject(type), value(value)
    {LOGMEIN("JavascriptNumberObject.cpp] 16\n");
        Assert(type->GetTypeId() == TypeIds_NumberObject);
        Assert(TaggedInt::Is(value) || JavascriptNumber::Is(value));
        Assert(TaggedInt::Is(value) || !ThreadContext::IsOnStack(value));
    }

    bool JavascriptNumberObject::Is(Var aValue)
    {LOGMEIN("JavascriptNumberObject.cpp] 23\n");
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_NumberObject;
    }

    JavascriptNumberObject* JavascriptNumberObject::FromVar(Var aValue)
    {
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptNumber'");

        return static_cast<JavascriptNumberObject *>(RecyclableObject::FromVar(aValue));
    }

    Var JavascriptNumberObject::Unwrap() const
    {LOGMEIN("JavascriptNumberObject.cpp] 35\n");
        return value;
    }

    double JavascriptNumberObject::GetValue() const
    {LOGMEIN("JavascriptNumberObject.cpp] 40\n");
        if (TaggedInt::Is(value))
        {LOGMEIN("JavascriptNumberObject.cpp] 42\n");
            return TaggedInt::ToDouble(value);
        }
        Assert(JavascriptNumber::Is(value));
        return JavascriptNumber::GetValue(value);
    }

    void JavascriptNumberObject::SetValue(Var value)
    {LOGMEIN("JavascriptNumberObject.cpp] 50\n");
        Assert(TaggedInt::Is(value) || JavascriptNumber::Is(value));
        Assert(TaggedInt::Is(value) || !ThreadContext::IsOnStack(value));

        this->value = value;
    }

    BOOL JavascriptNumberObject::GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {
        ENTER_PINNED_SCOPE(JavascriptString, valueStr);
        valueStr = JavascriptNumber::ToStringRadix10(this->GetValue(), GetScriptContext());
        stringBuilder->Append(valueStr->GetString(), valueStr->GetLength());
        LEAVE_PINNED_SCOPE();
        return TRUE;
    }

    BOOL JavascriptNumberObject::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {LOGMEIN("JavascriptNumberObject.cpp] 67\n");
        stringBuilder->AppendCppLiteral(_u("Number, (Object)"));
        return TRUE;
    }

#if ENABLE_TTD
    void JavascriptNumberObject::SetValue_TTD(Js::Var val)
    {LOGMEIN("JavascriptNumberObject.cpp] 74\n");
        TTDAssert(TaggedInt::Is(value) || JavascriptNumber::Is(value), "Only valid values!");

        this->value = val;
    }

    void JavascriptNumberObject::MarkVisitKindSpecificPtrs(TTD::SnapshotExtractor* extractor)
    {LOGMEIN("JavascriptNumberObject.cpp] 81\n");
        extractor->MarkVisitVar(this->value);
    }

    TTD::NSSnapObjects::SnapObjectType JavascriptNumberObject::GetSnapTag_TTD() const
    {LOGMEIN("JavascriptNumberObject.cpp] 86\n");
        return TTD::NSSnapObjects::SnapObjectType::SnapBoxedValueObject;
    }

    void JavascriptNumberObject::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {LOGMEIN("JavascriptNumberObject.cpp] 91\n");
        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD::TTDVar, TTD::NSSnapObjects::SnapObjectType::SnapBoxedValueObject>(objData, this->value);
    }
#endif
}
