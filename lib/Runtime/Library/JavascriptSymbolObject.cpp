//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    JavascriptSymbolObject::JavascriptSymbolObject(JavascriptSymbol* value, DynamicType * type)
        : DynamicObject(type), value(value)
    {LOGMEIN("JavascriptSymbolObject.cpp] 10\n");
        Assert(type->GetTypeId() == TypeIds_SymbolObject);
    }

    bool JavascriptSymbolObject::Is(Var aValue)
    {LOGMEIN("JavascriptSymbolObject.cpp] 15\n");
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_SymbolObject;
    }

    JavascriptSymbolObject* JavascriptSymbolObject::FromVar(Js::Var aValue)
    {
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptSymbolObject'");

        return static_cast<JavascriptSymbolObject *>(RecyclableObject::FromVar(aValue));
    }

    BOOL JavascriptSymbolObject::GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {LOGMEIN("JavascriptSymbolObject.cpp] 27\n");
        if (this->GetValue())
        {LOGMEIN("JavascriptSymbolObject.cpp] 29\n");
            stringBuilder->Append(this->GetValue()->GetBuffer(), this->GetValue()->GetLength());
        }
        return TRUE;
    }

    BOOL JavascriptSymbolObject::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {LOGMEIN("JavascriptSymbolObject.cpp] 36\n");
        stringBuilder->AppendCppLiteral(_u("Symbol, (Object)"));
        return TRUE;
    }

#if ENABLE_TTD
    void JavascriptSymbolObject::SetValue_TTD(Js::Var val)
    {LOGMEIN("JavascriptSymbolObject.cpp] 43\n");
        AssertMsg(val == nullptr || Js::JavascriptSymbol::Is(val), "Only allowable values!");

        this->value = static_cast<Js::JavascriptSymbol*>(val);
    }

    void JavascriptSymbolObject::MarkVisitKindSpecificPtrs(TTD::SnapshotExtractor* extractor)
    {LOGMEIN("JavascriptSymbolObject.cpp] 50\n");
        if(this->value != nullptr)
        {LOGMEIN("JavascriptSymbolObject.cpp] 52\n");
            extractor->MarkVisitVar(this->value);
        }
    }

    TTD::NSSnapObjects::SnapObjectType JavascriptSymbolObject::GetSnapTag_TTD() const
    {LOGMEIN("JavascriptSymbolObject.cpp] 58\n");
        return TTD::NSSnapObjects::SnapObjectType::SnapBoxedValueObject;
    }

    void JavascriptSymbolObject::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {LOGMEIN("JavascriptSymbolObject.cpp] 63\n");
        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD::TTDVar, TTD::NSSnapObjects::SnapObjectType::SnapBoxedValueObject>(objData, this->value);
    }
#endif
} // namespace Js
