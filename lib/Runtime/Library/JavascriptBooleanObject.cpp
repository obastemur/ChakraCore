//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    JavascriptBooleanObject::JavascriptBooleanObject(JavascriptBoolean* value, DynamicType * type)
        : DynamicObject(type), value(value)
    {LOGMEIN("JavascriptBooleanObject.cpp] 10\n");
        Assert(type->GetTypeId() == TypeIds_BooleanObject);
    }

    bool JavascriptBooleanObject::Is(Var aValue)
    {LOGMEIN("JavascriptBooleanObject.cpp] 15\n");
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_BooleanObject;
    }

    JavascriptBooleanObject* JavascriptBooleanObject::FromVar(Js::Var aValue)
    {
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptBooleanObject'");

        return static_cast<JavascriptBooleanObject *>(RecyclableObject::FromVar(aValue));
    }

    BOOL JavascriptBooleanObject::GetValue() const
    {LOGMEIN("JavascriptBooleanObject.cpp] 27\n");
        if (this->value == nullptr)
        {LOGMEIN("JavascriptBooleanObject.cpp] 29\n");
            return false;
        }
        return this->value->GetValue();
    }

    void JavascriptBooleanObject::Initialize(JavascriptBoolean* value)
    {LOGMEIN("JavascriptBooleanObject.cpp] 36\n");
        Assert(this->value == nullptr);

        this->value = value;
    }

    BOOL JavascriptBooleanObject::GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {LOGMEIN("JavascriptBooleanObject.cpp] 43\n");
        if (this->GetValue())
        {LOGMEIN("JavascriptBooleanObject.cpp] 45\n");
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

    BOOL JavascriptBooleanObject::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {LOGMEIN("JavascriptBooleanObject.cpp] 58\n");
        stringBuilder->AppendCppLiteral(_u("Boolean, (Object)"));
        return TRUE;
    }

#if ENABLE_TTD
    void JavascriptBooleanObject::SetValue_TTD(Js::Var val)
    {LOGMEIN("JavascriptBooleanObject.cpp] 65\n");
        TTDAssert(val == nullptr || Js::JavascriptBoolean::Is(val), "Only allowable values!");

        this->value = static_cast<Js::JavascriptBoolean*>(val);
    }

    void JavascriptBooleanObject::MarkVisitKindSpecificPtrs(TTD::SnapshotExtractor* extractor)
    {LOGMEIN("JavascriptBooleanObject.cpp] 72\n");
        if(this->value != nullptr)
        {LOGMEIN("JavascriptBooleanObject.cpp] 74\n");
            extractor->MarkVisitVar(this->value);
        }
    }

    TTD::NSSnapObjects::SnapObjectType JavascriptBooleanObject::GetSnapTag_TTD() const
    {LOGMEIN("JavascriptBooleanObject.cpp] 80\n");
        return TTD::NSSnapObjects::SnapObjectType::SnapBoxedValueObject;
    }

    void JavascriptBooleanObject::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {LOGMEIN("JavascriptBooleanObject.cpp] 85\n");
        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD::TTDVar, TTD::NSSnapObjects::SnapObjectType::SnapBoxedValueObject>(objData, this->value);
    }
#endif
} // namespace Js
