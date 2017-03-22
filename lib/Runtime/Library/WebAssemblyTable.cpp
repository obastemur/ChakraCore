//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "RuntimeLibraryPch.h"

#ifdef ENABLE_WASM

namespace Js
{

WebAssemblyTable::WebAssemblyTable(Var * values, uint32 currentLength, uint32 initialLength, uint32 maxLength, DynamicType * type) :
    DynamicObject(type),
    m_values((Field(Var)*)values),
    m_currentLength(currentLength),
    m_initialLength(initialLength),
    m_maxLength(maxLength)
{LOGMEIN("WebAssemblyTable.cpp] 18\n");
}

/* static */
bool
WebAssemblyTable::Is(Var value)
{LOGMEIN("WebAssemblyTable.cpp] 24\n");
    return JavascriptOperators::GetTypeId(value) == TypeIds_WebAssemblyTable;
}

/* static */
WebAssemblyTable *
WebAssemblyTable::FromVar(Var value)
{LOGMEIN("WebAssemblyTable.cpp] 31\n");
    Assert(WebAssemblyTable::Is(value));
    return static_cast<WebAssemblyTable*>(value);
}

Var
WebAssemblyTable::NewInstance(RecyclableObject* function, CallInfo callInfo, ...)
{
    PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

    ARGUMENTS(args, callInfo);
    ScriptContext* scriptContext = function->GetScriptContext();

    AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");

    Var newTarget = callInfo.Flags & CallFlags_NewTarget ? args.Values[args.Info.Count] : args[0];
    bool isCtorSuperCall = (callInfo.Flags & CallFlags_New) && newTarget != nullptr && !JavascriptOperators::IsUndefined(newTarget);
    Assert(isCtorSuperCall || !(callInfo.Flags & CallFlags_New) || args[0] == nullptr);

    if (!(callInfo.Flags & CallFlags_New) || (newTarget && JavascriptOperators::IsUndefinedObject(newTarget)))
    {LOGMEIN("WebAssemblyTable.cpp] 51\n");
        JavascriptError::ThrowTypeError(scriptContext, JSERR_ClassConstructorCannotBeCalledWithoutNew, _u("WebAssembly.Table"));
    }

    if (args.Info.Count < 2 || !JavascriptOperators::IsObject(args[1]))
    {LOGMEIN("WebAssemblyTable.cpp] 56\n");
        JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedObject, _u("tableDescriptor"));
    }
    DynamicObject * tableDescriptor = JavascriptObject::FromVar(args[1]);

    Var elementVar = JavascriptOperators::OP_GetProperty(tableDescriptor, PropertyIds::element, scriptContext);
    if (!JavascriptOperators::StrictEqualString(elementVar, scriptContext->GetLibrary()->CreateStringFromCppLiteral(_u("anyfunc"))))
    {LOGMEIN("WebAssemblyTable.cpp] 63\n");
        JavascriptError::ThrowTypeError(scriptContext, WASMERR_ExpectedAnyFunc, _u("tableDescriptor.element"));
    }

    Var initVar = JavascriptOperators::OP_GetProperty(tableDescriptor, PropertyIds::initial, scriptContext);
    uint32 initial = WebAssembly::ToNonWrappingUint32(initVar, scriptContext);

    uint32 maximum = UINT_MAX;
    if (JavascriptOperators::OP_HasProperty(tableDescriptor, PropertyIds::maximum, scriptContext))
    {LOGMEIN("WebAssemblyTable.cpp] 72\n");
        Var maxVar = JavascriptOperators::OP_GetProperty(tableDescriptor, PropertyIds::maximum, scriptContext);
        maximum = WebAssembly::ToNonWrappingUint32(maxVar, scriptContext);
    }
    return Create(initial, maximum, scriptContext);
}

Var
WebAssemblyTable::EntryGetterLength(RecyclableObject* function, CallInfo callInfo, ...)
{
    PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

    ARGUMENTS(args, callInfo);
    ScriptContext* scriptContext = function->GetScriptContext();

    Assert(!(callInfo.Flags & CallFlags_New));

    if (args.Info.Count == 0 || !WebAssemblyTable::Is(args[0]))
    {LOGMEIN("WebAssemblyTable.cpp] 90\n");
        JavascriptError::ThrowTypeError(scriptContext, WASMERR_NeedTableObject);
    }
    WebAssemblyTable * table = WebAssemblyTable::FromVar(args[0]);
    return JavascriptNumber::ToVar(table->m_currentLength, scriptContext);
}

Var
WebAssemblyTable::EntryGrow(RecyclableObject* function, CallInfo callInfo, ...)
{
    PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

    ARGUMENTS(args, callInfo);
    ScriptContext* scriptContext = function->GetScriptContext();

    Assert(!(callInfo.Flags & CallFlags_New));

    if (args.Info.Count == 0 || !WebAssemblyTable::Is(args[0]))
    {LOGMEIN("WebAssemblyTable.cpp] 108\n");
        JavascriptError::ThrowTypeError(scriptContext, WASMERR_NeedTableObject);
    }
    WebAssemblyTable * table = WebAssemblyTable::FromVar(args[0]);

    Var deltaVar = scriptContext->GetLibrary()->GetUndefined();
    if (args.Info.Count >= 2)
    {LOGMEIN("WebAssemblyTable.cpp] 115\n");
        deltaVar = args[1];
    }
    uint32 delta = WebAssembly::ToNonWrappingUint32(deltaVar, scriptContext);
    if ((uint64)table->m_currentLength + delta > (uint64)table->m_maxLength)
    {LOGMEIN("WebAssemblyTable.cpp] 120\n");
        JavascriptError::ThrowRangeError(scriptContext, JSERR_ArgumentOutOfRange);
    }
    CompileAssert(sizeof(table->m_maxLength) == sizeof(uint32));

    uint32 newLength = table->m_currentLength + delta;
    Field(Var) * newValues = RecyclerNewArrayZ(scriptContext->GetRecycler(), Field(Var), newLength);
    CopyArray(newValues, newLength, table->m_values, table->m_currentLength);

    table->m_values = newValues;
    table->m_currentLength = newLength;

    return scriptContext->GetLibrary()->GetUndefined();
}

Var
WebAssemblyTable::EntryGet(RecyclableObject* function, CallInfo callInfo, ...)
{
    PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

    ARGUMENTS(args, callInfo);
    ScriptContext* scriptContext = function->GetScriptContext();

    Assert(!(callInfo.Flags & CallFlags_New));

    if (args.Info.Count == 0 || !WebAssemblyTable::Is(args[0]))
    {LOGMEIN("WebAssemblyTable.cpp] 146\n");
        JavascriptError::ThrowTypeError(scriptContext, WASMERR_NeedTableObject);
    }
    WebAssemblyTable * table = WebAssemblyTable::FromVar(args[0]);

    Var indexVar = scriptContext->GetLibrary()->GetUndefined();
    if (args.Info.Count >= 2)
    {LOGMEIN("WebAssemblyTable.cpp] 153\n");
        indexVar = args[1];
    }
    uint32 index = WebAssembly::ToNonWrappingUint32(indexVar, scriptContext);
    if (index >= table->m_currentLength)
    {LOGMEIN("WebAssemblyTable.cpp] 158\n");
        JavascriptError::ThrowRangeError(scriptContext, JSERR_ArgumentOutOfRange);
    }
    if (!table->m_values[index])
    {LOGMEIN("WebAssemblyTable.cpp] 162\n");
        return scriptContext->GetLibrary()->GetNull();
    }

    return table->m_values[index];
}

Var
WebAssemblyTable::EntrySet(RecyclableObject* function, CallInfo callInfo, ...)
{
    PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

    ARGUMENTS(args, callInfo);
    ScriptContext* scriptContext = function->GetScriptContext();

    Assert(!(callInfo.Flags & CallFlags_New));

    if (args.Info.Count == 0 || !WebAssemblyTable::Is(args[0]))
    {LOGMEIN("WebAssemblyTable.cpp] 180\n");
        JavascriptError::ThrowTypeError(scriptContext, WASMERR_NeedTableObject);
    }
    WebAssemblyTable * table = WebAssemblyTable::FromVar(args[0]);

    if (args.Info.Count < 3)
    {LOGMEIN("WebAssemblyTable.cpp] 186\n");
        JavascriptError::ThrowTypeError(scriptContext, WASMERR_NeedWebAssemblyFunc);
    }
    Var indexVar = args[1];
    Var value = args[2];
    
    if (JavascriptOperators::IsNull(value))
    {LOGMEIN("WebAssemblyTable.cpp] 193\n");
        value = nullptr;
    }
    else if (!AsmJsScriptFunction::IsWasmScriptFunction(args[2]))
    {LOGMEIN("WebAssemblyTable.cpp] 197\n");
        JavascriptError::ThrowTypeError(scriptContext, WASMERR_NeedWebAssemblyFunc);
    }

    uint32 index = WebAssembly::ToNonWrappingUint32(indexVar, scriptContext);
    if (index >= table->m_currentLength)
    {LOGMEIN("WebAssemblyTable.cpp] 203\n");
        JavascriptError::ThrowRangeError(scriptContext, JSERR_ArgumentOutOfRange);
    }

    table->m_values[index] = value;

    return scriptContext->GetLibrary()->GetUndefined();
}

WebAssemblyTable *
WebAssemblyTable::Create(uint32 initial, uint32 maximum, ScriptContext * scriptContext)
{LOGMEIN("WebAssemblyTable.cpp] 214\n");
    Field(Var) * values = nullptr;
    if (initial > 0)
    {LOGMEIN("WebAssemblyTable.cpp] 217\n");
        values = RecyclerNewArrayZ(scriptContext->GetRecycler(), Field(Var), initial);
    }
    return RecyclerNew(scriptContext->GetRecycler(), WebAssemblyTable, (Var*)values, initial, initial, maximum, scriptContext->GetLibrary()->GetWebAssemblyTableType());
}

void
WebAssemblyTable::DirectSetValue(uint index, Var val)
{LOGMEIN("WebAssemblyTable.cpp] 225\n");
    Assert(index < m_currentLength);
    Assert(!val || AsmJsScriptFunction::Is(val));
    m_values[index] = val;
}

Var
WebAssemblyTable::DirectGetValue(uint index) const
{LOGMEIN("WebAssemblyTable.cpp] 233\n");
    Assert(index < m_currentLength);
    Var val = m_values[index];
    Assert(!val || AsmJsScriptFunction::Is(val));
    return val;
}

Var *
WebAssemblyTable::GetValues() const
{LOGMEIN("WebAssemblyTable.cpp] 242\n");
    return (Var*)PointerValue(m_values);
}

uint32
WebAssemblyTable::GetCurrentLength() const
{LOGMEIN("WebAssemblyTable.cpp] 248\n");
    return m_currentLength;
}

uint32
WebAssemblyTable::GetInitialLength() const
{LOGMEIN("WebAssemblyTable.cpp] 254\n");
    return m_initialLength;
}

uint32
WebAssemblyTable::GetMaximumLength() const
{LOGMEIN("WebAssemblyTable.cpp] 260\n");
    return m_maxLength;
}

} // namespace Js

#endif // ENABLE_WASM
