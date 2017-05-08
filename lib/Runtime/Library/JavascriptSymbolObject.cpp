//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    JavascriptSymbolObject::JavascriptSymbolObject(JavascriptSymbol* value, DynamicType * type)
        : DynamicObject(type), value(value)
    {TRACE_IT(62173);
        Assert(type->GetTypeId() == TypeIds_SymbolObject);
    }

    bool JavascriptSymbolObject::Is(Var aValue)
    {TRACE_IT(62174);
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_SymbolObject;
    }

    JavascriptSymbolObject* JavascriptSymbolObject::FromVar(Js::Var aValue)
    {
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptSymbolObject'");

        return static_cast<JavascriptSymbolObject *>(RecyclableObject::FromVar(aValue));
    }

    BOOL JavascriptSymbolObject::GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {TRACE_IT(62175);
        if (this->GetValue())
        {TRACE_IT(62176);
            stringBuilder->Append(this->GetValue()->GetBuffer(), this->GetValue()->GetLength());
        }
        return TRUE;
    }

    BOOL JavascriptSymbolObject::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {TRACE_IT(62177);
        stringBuilder->AppendCppLiteral(_u("Symbol, (Object)"));
        return TRUE;
    }

#if ENABLE_TTD
    void JavascriptSymbolObject::SetValue_TTD(Js::Var val)
    {TRACE_IT(62178);
        AssertMsg(val == nullptr || Js::JavascriptSymbol::Is(val), "Only allowable values!");

        this->value = static_cast<Js::JavascriptSymbol*>(val);
    }

    void JavascriptSymbolObject::MarkVisitKindSpecificPtrs(TTD::SnapshotExtractor* extractor)
    {TRACE_IT(62179);
        if(this->value != nullptr)
        {TRACE_IT(62180);
            extractor->MarkVisitVar(this->value);
        }
    }

    TTD::NSSnapObjects::SnapObjectType JavascriptSymbolObject::GetSnapTag_TTD() const
    {TRACE_IT(62181);
        return TTD::NSSnapObjects::SnapObjectType::SnapBoxedValueObject;
    }

    void JavascriptSymbolObject::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {TRACE_IT(62182);
        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD::TTDVar, TTD::NSSnapObjects::SnapObjectType::SnapBoxedValueObject>(objData, this->value);
    }
#endif
} // namespace Js
