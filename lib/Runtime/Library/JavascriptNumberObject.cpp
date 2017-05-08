//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    JavascriptNumberObject::JavascriptNumberObject(DynamicType * type)
        : DynamicObject(type), value(Js::TaggedInt::ToVarUnchecked(0))
    {TRACE_IT(60354);
        Assert(type->GetTypeId() == TypeIds_NumberObject);
    }

    JavascriptNumberObject::JavascriptNumberObject(Var value, DynamicType * type)
        : DynamicObject(type), value(value)
    {TRACE_IT(60355);
        Assert(type->GetTypeId() == TypeIds_NumberObject);
        Assert(TaggedInt::Is(value) || JavascriptNumber::Is(value));
        Assert(TaggedInt::Is(value) || !ThreadContext::IsOnStack(value));
    }

    bool JavascriptNumberObject::Is(Var aValue)
    {TRACE_IT(60356);
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_NumberObject;
    }

    JavascriptNumberObject* JavascriptNumberObject::FromVar(Var aValue)
    {
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptNumber'");

        return static_cast<JavascriptNumberObject *>(RecyclableObject::FromVar(aValue));
    }

    Var JavascriptNumberObject::Unwrap() const
    {TRACE_IT(60357);
        return value;
    }

    double JavascriptNumberObject::GetValue() const
    {TRACE_IT(60358);
        if (TaggedInt::Is(value))
        {TRACE_IT(60359);
            return TaggedInt::ToDouble(value);
        }
        Assert(JavascriptNumber::Is(value));
        return JavascriptNumber::GetValue(value);
    }

    void JavascriptNumberObject::SetValue(Var value)
    {TRACE_IT(60360);
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
    {TRACE_IT(60361);
        stringBuilder->AppendCppLiteral(_u("Number, (Object)"));
        return TRUE;
    }

#if ENABLE_TTD
    void JavascriptNumberObject::SetValue_TTD(Js::Var val)
    {TRACE_IT(60362);
        TTDAssert(TaggedInt::Is(value) || JavascriptNumber::Is(value), "Only valid values!");

        this->value = val;
    }

    void JavascriptNumberObject::MarkVisitKindSpecificPtrs(TTD::SnapshotExtractor* extractor)
    {TRACE_IT(60363);
        extractor->MarkVisitVar(this->value);
    }

    TTD::NSSnapObjects::SnapObjectType JavascriptNumberObject::GetSnapTag_TTD() const
    {TRACE_IT(60364);
        return TTD::NSSnapObjects::SnapObjectType::SnapBoxedValueObject;
    }

    void JavascriptNumberObject::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {TRACE_IT(60365);
        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD::TTDVar, TTD::NSSnapObjects::SnapObjectType::SnapBoxedValueObject>(objData, this->value);
    }
#endif
}
