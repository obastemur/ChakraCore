//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    JavascriptBooleanObject::JavascriptBooleanObject(JavascriptBoolean* value, DynamicType * type)
        : DynamicObject(type), value(value)
    {TRACE_IT(58216);
        Assert(type->GetTypeId() == TypeIds_BooleanObject);
    }

    bool JavascriptBooleanObject::Is(Var aValue)
    {TRACE_IT(58217);
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_BooleanObject;
    }

    JavascriptBooleanObject* JavascriptBooleanObject::FromVar(Js::Var aValue)
    {
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptBooleanObject'");

        return static_cast<JavascriptBooleanObject *>(RecyclableObject::FromVar(aValue));
    }

    BOOL JavascriptBooleanObject::GetValue() const
    {TRACE_IT(58218);
        if (this->value == nullptr)
        {TRACE_IT(58219);
            return false;
        }
        return this->value->GetValue();
    }

    void JavascriptBooleanObject::Initialize(JavascriptBoolean* value)
    {TRACE_IT(58220);
        Assert(this->value == nullptr);

        this->value = value;
    }

    BOOL JavascriptBooleanObject::GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {TRACE_IT(58221);
        if (this->GetValue())
        {TRACE_IT(58222);
            JavascriptString* trueDisplayString = GetLibrary()->GetTrueDisplayString();
            stringBuilder->Append(trueDisplayString->GetString(), trueDisplayString->GetLength());
        }
        else
        {TRACE_IT(58223);
            JavascriptString* falseDisplayString = GetLibrary()->GetFalseDisplayString();
            stringBuilder->Append(falseDisplayString->GetString(), falseDisplayString->GetLength());
        }
        return TRUE;
    }

    BOOL JavascriptBooleanObject::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {TRACE_IT(58224);
        stringBuilder->AppendCppLiteral(_u("Boolean, (Object)"));
        return TRUE;
    }

#if ENABLE_TTD
    void JavascriptBooleanObject::SetValue_TTD(Js::Var val)
    {TRACE_IT(58225);
        TTDAssert(val == nullptr || Js::JavascriptBoolean::Is(val), "Only allowable values!");

        this->value = static_cast<Js::JavascriptBoolean*>(val);
    }

    void JavascriptBooleanObject::MarkVisitKindSpecificPtrs(TTD::SnapshotExtractor* extractor)
    {TRACE_IT(58226);
        if(this->value != nullptr)
        {TRACE_IT(58227);
            extractor->MarkVisitVar(this->value);
        }
    }

    TTD::NSSnapObjects::SnapObjectType JavascriptBooleanObject::GetSnapTag_TTD() const
    {TRACE_IT(58228);
        return TTD::NSSnapObjects::SnapObjectType::SnapBoxedValueObject;
    }

    void JavascriptBooleanObject::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {TRACE_IT(58229);
        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD::TTDVar, TTD::NSSnapObjects::SnapObjectType::SnapBoxedValueObject>(objData, this->value);
    }
#endif
} // namespace Js
