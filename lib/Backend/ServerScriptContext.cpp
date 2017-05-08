//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "Backend.h"

#if ENABLE_OOP_NATIVE_CODEGEN
#include "JITServer/JITServer.h"

ServerScriptContext::ThreadContextHolder::ThreadContextHolder(ServerThreadContext* threadContextInfo) : threadContextInfo(threadContextInfo)
{TRACE_IT(15316);
    threadContextInfo->AddRef();
}
ServerScriptContext::ThreadContextHolder::~ThreadContextHolder()
{TRACE_IT(15317);
    threadContextInfo->Release();
}

ServerScriptContext::ServerScriptContext(ScriptContextDataIDL * contextData, ServerThreadContext* threadContextInfo) :
    m_contextData(*contextData),
    threadContextHolder(threadContextInfo),
    m_isPRNGSeeded(false),
    m_sourceCodeArena(_u("JITSourceCodeArena"), threadContextInfo->GetForegroundPageAllocator(), Js::Throw::OutOfMemory, nullptr),
    m_interpreterThunkBufferManager(&m_sourceCodeArena, threadContextInfo->GetThunkPageAllocators(), nullptr, _u("Interpreter thunk buffer"), GetThreadContext()->GetProcessHandle()),
    m_asmJsInterpreterThunkBufferManager(&m_sourceCodeArena, threadContextInfo->GetThunkPageAllocators(), nullptr, _u("Asm.js interpreter thunk buffer"), GetThreadContext()->GetProcessHandle()),
    m_domFastPathHelperMap(nullptr),
    m_moduleRecords(&HeapAllocator::Instance),
    m_globalThisAddr(0),
#ifdef PROFILE_EXEC
    m_codeGenProfiler(nullptr),
#endif
    m_refCount(0),
    m_isClosed(false)
{TRACE_IT(15318);
#ifdef PROFILE_EXEC
    if (Js::Configuration::Global.flags.IsEnabled(Js::ProfileFlag))
    {TRACE_IT(15319);
        m_codeGenProfiler = HeapNew(Js::ScriptContextProfiler);
    }
#endif
    m_domFastPathHelperMap = HeapNew(JITDOMFastPathHelperMap, &HeapAllocator::Instance, 17);
}

ServerScriptContext::~ServerScriptContext()
{TRACE_IT(15320);
    HeapDelete(m_domFastPathHelperMap);
    m_moduleRecords.Map([](uint, Js::ServerSourceTextModuleRecord* record)
    {
        HeapDelete(record);
    });

#ifdef PROFILE_EXEC
    if (m_codeGenProfiler)
    {TRACE_IT(15321);
        HeapDelete(m_codeGenProfiler);
    }
#endif
}

intptr_t
ServerScriptContext::GetNullAddr() const
{TRACE_IT(15322);
    return m_contextData.nullAddr;
}

intptr_t
ServerScriptContext::GetUndefinedAddr() const
{TRACE_IT(15323);
    return m_contextData.undefinedAddr;
}

intptr_t
ServerScriptContext::GetTrueAddr() const
{TRACE_IT(15324);
    return m_contextData.trueAddr;
}

intptr_t
ServerScriptContext::GetFalseAddr() const
{TRACE_IT(15325);
    return m_contextData.falseAddr;
}

intptr_t
ServerScriptContext::GetUndeclBlockVarAddr() const
{TRACE_IT(15326);
    return m_contextData.undeclBlockVarAddr;
}

intptr_t
ServerScriptContext::GetEmptyStringAddr() const
{TRACE_IT(15327);
    return m_contextData.emptyStringAddr;
}

intptr_t
ServerScriptContext::GetNegativeZeroAddr() const
{TRACE_IT(15328);
    return m_contextData.negativeZeroAddr;
}

intptr_t
ServerScriptContext::GetNumberTypeStaticAddr() const
{TRACE_IT(15329);
    return m_contextData.numberTypeStaticAddr;
}

intptr_t
ServerScriptContext::GetStringTypeStaticAddr() const
{TRACE_IT(15330);
    return m_contextData.stringTypeStaticAddr;
}

intptr_t
ServerScriptContext::GetObjectTypeAddr() const
{TRACE_IT(15331);
    return m_contextData.objectTypeAddr;
}

intptr_t
ServerScriptContext::GetObjectHeaderInlinedTypeAddr() const
{TRACE_IT(15332);
    return m_contextData.objectHeaderInlinedTypeAddr;
}

intptr_t
ServerScriptContext::GetRegexTypeAddr() const
{TRACE_IT(15333);
    return m_contextData.regexTypeAddr;
}

intptr_t
ServerScriptContext::GetArrayTypeAddr() const
{TRACE_IT(15334);
    return m_contextData.arrayTypeAddr;
}

intptr_t
ServerScriptContext::GetNativeIntArrayTypeAddr() const
{TRACE_IT(15335);
    return m_contextData.nativeIntArrayTypeAddr;
}

intptr_t
ServerScriptContext::GetNativeFloatArrayTypeAddr() const
{TRACE_IT(15336);
    return m_contextData.nativeFloatArrayTypeAddr;
}

intptr_t
ServerScriptContext::GetArrayConstructorAddr() const
{TRACE_IT(15337);
    return m_contextData.arrayConstructorAddr;
}

intptr_t
ServerScriptContext::GetCharStringCacheAddr() const
{TRACE_IT(15338);
    return m_contextData.charStringCacheAddr;
}

intptr_t
ServerScriptContext::GetSideEffectsAddr() const
{TRACE_IT(15339);
    return m_contextData.sideEffectsAddr;
}

intptr_t
ServerScriptContext::GetArraySetElementFastPathVtableAddr() const
{TRACE_IT(15340);
    return m_contextData.arraySetElementFastPathVtableAddr;
}

intptr_t
ServerScriptContext::GetIntArraySetElementFastPathVtableAddr() const
{TRACE_IT(15341);
    return m_contextData.intArraySetElementFastPathVtableAddr;
}

intptr_t
ServerScriptContext::GetFloatArraySetElementFastPathVtableAddr() const
{TRACE_IT(15342);
    return m_contextData.floatArraySetElementFastPathVtableAddr;
}

intptr_t
ServerScriptContext::GetLibraryAddr() const
{TRACE_IT(15343);
    return m_contextData.libraryAddr;
}

intptr_t
ServerScriptContext::GetGlobalObjectAddr() const
{TRACE_IT(15344);
    return m_contextData.globalObjectAddr;
}

intptr_t
ServerScriptContext::GetGlobalObjectThisAddr() const
{TRACE_IT(15345);
    return m_globalThisAddr;
}

void
ServerScriptContext::UpdateGlobalObjectThisAddr(intptr_t globalThis)
{TRACE_IT(15346);
    m_globalThisAddr = globalThis;
}

intptr_t
ServerScriptContext::GetNumberAllocatorAddr() const
{TRACE_IT(15347);
    return m_contextData.numberAllocatorAddr;
}

intptr_t
ServerScriptContext::GetRecyclerAddr() const
{TRACE_IT(15348);
    return m_contextData.recyclerAddr;
}

bool
ServerScriptContext::GetRecyclerAllowNativeCodeBumpAllocation() const
{TRACE_IT(15349);
    return m_contextData.recyclerAllowNativeCodeBumpAllocation != 0;
}

bool
ServerScriptContext::IsSIMDEnabled() const
{TRACE_IT(15350);
    return m_contextData.isSIMDEnabled != 0;
}

intptr_t
ServerScriptContext::GetBuiltinFunctionsBaseAddr() const
{TRACE_IT(15351);
    return m_contextData.builtinFunctionsBaseAddr;
}

intptr_t
ServerScriptContext::GetAddr() const
{TRACE_IT(15352);
    return m_contextData.scriptContextAddr;
}

intptr_t
ServerScriptContext::GetVTableAddress(VTableValue vtableType) const
{TRACE_IT(15353);
    Assert(vtableType < VTableValue::Count);
    return m_contextData.vtableAddresses[vtableType];
}

bool
ServerScriptContext::IsRecyclerVerifyEnabled() const
{TRACE_IT(15354);
    return m_contextData.isRecyclerVerifyEnabled != FALSE;
}

uint
ServerScriptContext::GetRecyclerVerifyPad() const
{TRACE_IT(15355);
    return m_contextData.recyclerVerifyPad;
}

bool
ServerScriptContext::IsPRNGSeeded() const
{TRACE_IT(15356);
    return m_isPRNGSeeded;
}

intptr_t
ServerScriptContext::GetDebuggingFlagsAddr() const
{TRACE_IT(15357);
    return static_cast<intptr_t>(m_contextData.debuggingFlagsAddr);
}

intptr_t
ServerScriptContext::GetDebugStepTypeAddr() const
{TRACE_IT(15358);
    return static_cast<intptr_t>(m_contextData.debugStepTypeAddr);
}

intptr_t
ServerScriptContext::GetDebugFrameAddressAddr() const
{TRACE_IT(15359);
    return static_cast<intptr_t>(m_contextData.debugFrameAddressAddr);
}

intptr_t
ServerScriptContext::GetDebugScriptIdWhenSetAddr() const
{TRACE_IT(15360);
    return static_cast<intptr_t>(m_contextData.debugScriptIdWhenSetAddr);
}

bool
ServerScriptContext::IsClosed() const
{TRACE_IT(15361);
    return m_isClosed;
}

void
ServerScriptContext::AddToDOMFastPathHelperMap(intptr_t funcInfoAddr, IR::JnHelperMethod helper)
{TRACE_IT(15362);
    m_domFastPathHelperMap->Add(funcInfoAddr, helper);
}

ArenaAllocator *
ServerScriptContext::GetSourceCodeArena()
{TRACE_IT(15363);
    return &m_sourceCodeArena;
}

void
ServerScriptContext::DecommitEmitBufferManager(bool asmJsManager)
{TRACE_IT(15364);
    GetEmitBufferManager(asmJsManager)->Decommit();
}

OOPEmitBufferManager *
ServerScriptContext::GetEmitBufferManager(bool asmJsManager)
{TRACE_IT(15365);
    if (asmJsManager)
    {TRACE_IT(15366);
        return &m_asmJsInterpreterThunkBufferManager;
    }
    else
    {TRACE_IT(15367);
        return &m_interpreterThunkBufferManager;
    }
}

IR::JnHelperMethod
ServerScriptContext::GetDOMFastPathHelper(intptr_t funcInfoAddr)
{TRACE_IT(15368);
    IR::JnHelperMethod helper;

    m_domFastPathHelperMap->LockResize();
    bool found = m_domFastPathHelperMap->TryGetValue(funcInfoAddr, &helper);
    m_domFastPathHelperMap->UnlockResize();

    Assert(found);
    return helper;
}

void
ServerScriptContext::Close()
{TRACE_IT(15369);
    Assert(!IsClosed());
    m_isClosed = true;
    
#ifdef STACK_BACK_TRACE
    ServerContextManager::RecordCloseContext(this);
#endif
}

void
ServerScriptContext::AddRef()
{
    InterlockedExchangeAdd(&m_refCount, 1u);
}

void
ServerScriptContext::Release()
{
    InterlockedExchangeSubtract(&m_refCount, 1u);
    if (m_isClosed && m_refCount == 0)
    {TRACE_IT(15370);
        // Not freeing here, we'll expect explicit ServerCleanupScriptContext() call to do the free
        // otherwise after free, the CodeGen call can still get same scriptContext if there's another 
        // ServerInitializeScriptContext call
    }
}

Field(Js::Var)*
ServerScriptContext::GetModuleExportSlotArrayAddress(uint moduleIndex, uint slotIndex)
{TRACE_IT(15371);
    Assert(m_moduleRecords.ContainsKey(moduleIndex));
    auto record = m_moduleRecords.Item(moduleIndex);
    return record->localExportSlotsAddr;
}

void
ServerScriptContext::SetIsPRNGSeeded(bool value)
{TRACE_IT(15372);
    m_isPRNGSeeded = value;
}

void
ServerScriptContext::AddModuleRecordInfo(unsigned int moduleId, __int64 localExportSlotsAddr)
{TRACE_IT(15373);
    Js::ServerSourceTextModuleRecord* record = HeapNewStructZ(Js::ServerSourceTextModuleRecord);
    record->moduleId = moduleId;
    record->localExportSlotsAddr = (Field(Js::Var)*)localExportSlotsAddr;
    m_moduleRecords.Add(moduleId, record);
}

Js::ScriptContextProfiler *
ServerScriptContext::GetCodeGenProfiler() const
{TRACE_IT(15374);
#ifdef PROFILE_EXEC
    return m_codeGenProfiler;
#else
    return nullptr;
#endif
}

#endif
