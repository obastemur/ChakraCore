//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "Backend.h"

#if ENABLE_OOP_NATIVE_CODEGEN
#include "JITServer/JITServer.h"

ServerScriptContext::ThreadContextHolder::ThreadContextHolder(ServerThreadContext* threadContextInfo) : threadContextInfo(threadContextInfo)
{LOGMEIN("ServerScriptContext.cpp] 11\n");
    threadContextInfo->AddRef();
}
ServerScriptContext::ThreadContextHolder::~ThreadContextHolder()
{LOGMEIN("ServerScriptContext.cpp] 15\n");
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
{LOGMEIN("ServerScriptContext.cpp] 34\n");
#ifdef PROFILE_EXEC
    if (Js::Configuration::Global.flags.IsEnabled(Js::ProfileFlag))
    {LOGMEIN("ServerScriptContext.cpp] 37\n");
        m_codeGenProfiler = HeapNew(Js::ScriptContextProfiler);
    }
#endif
    m_domFastPathHelperMap = HeapNew(JITDOMFastPathHelperMap, &HeapAllocator::Instance, 17);
}

ServerScriptContext::~ServerScriptContext()
{LOGMEIN("ServerScriptContext.cpp] 45\n");
    HeapDelete(m_domFastPathHelperMap);
    m_moduleRecords.Map([](uint, Js::ServerSourceTextModuleRecord* record)
    {
        HeapDelete(record);
    });

#ifdef PROFILE_EXEC
    if (m_codeGenProfiler)
    {LOGMEIN("ServerScriptContext.cpp] 54\n");
        HeapDelete(m_codeGenProfiler);
    }
#endif
}

intptr_t
ServerScriptContext::GetNullAddr() const
{LOGMEIN("ServerScriptContext.cpp] 62\n");
    return m_contextData.nullAddr;
}

intptr_t
ServerScriptContext::GetUndefinedAddr() const
{LOGMEIN("ServerScriptContext.cpp] 68\n");
    return m_contextData.undefinedAddr;
}

intptr_t
ServerScriptContext::GetTrueAddr() const
{LOGMEIN("ServerScriptContext.cpp] 74\n");
    return m_contextData.trueAddr;
}

intptr_t
ServerScriptContext::GetFalseAddr() const
{LOGMEIN("ServerScriptContext.cpp] 80\n");
    return m_contextData.falseAddr;
}

intptr_t
ServerScriptContext::GetUndeclBlockVarAddr() const
{LOGMEIN("ServerScriptContext.cpp] 86\n");
    return m_contextData.undeclBlockVarAddr;
}

intptr_t
ServerScriptContext::GetEmptyStringAddr() const
{LOGMEIN("ServerScriptContext.cpp] 92\n");
    return m_contextData.emptyStringAddr;
}

intptr_t
ServerScriptContext::GetNegativeZeroAddr() const
{LOGMEIN("ServerScriptContext.cpp] 98\n");
    return m_contextData.negativeZeroAddr;
}

intptr_t
ServerScriptContext::GetNumberTypeStaticAddr() const
{LOGMEIN("ServerScriptContext.cpp] 104\n");
    return m_contextData.numberTypeStaticAddr;
}

intptr_t
ServerScriptContext::GetStringTypeStaticAddr() const
{LOGMEIN("ServerScriptContext.cpp] 110\n");
    return m_contextData.stringTypeStaticAddr;
}

intptr_t
ServerScriptContext::GetObjectTypeAddr() const
{LOGMEIN("ServerScriptContext.cpp] 116\n");
    return m_contextData.objectTypeAddr;
}

intptr_t
ServerScriptContext::GetObjectHeaderInlinedTypeAddr() const
{LOGMEIN("ServerScriptContext.cpp] 122\n");
    return m_contextData.objectHeaderInlinedTypeAddr;
}

intptr_t
ServerScriptContext::GetRegexTypeAddr() const
{LOGMEIN("ServerScriptContext.cpp] 128\n");
    return m_contextData.regexTypeAddr;
}

intptr_t
ServerScriptContext::GetArrayTypeAddr() const
{LOGMEIN("ServerScriptContext.cpp] 134\n");
    return m_contextData.arrayTypeAddr;
}

intptr_t
ServerScriptContext::GetNativeIntArrayTypeAddr() const
{LOGMEIN("ServerScriptContext.cpp] 140\n");
    return m_contextData.nativeIntArrayTypeAddr;
}

intptr_t
ServerScriptContext::GetNativeFloatArrayTypeAddr() const
{LOGMEIN("ServerScriptContext.cpp] 146\n");
    return m_contextData.nativeFloatArrayTypeAddr;
}

intptr_t
ServerScriptContext::GetArrayConstructorAddr() const
{LOGMEIN("ServerScriptContext.cpp] 152\n");
    return m_contextData.arrayConstructorAddr;
}

intptr_t
ServerScriptContext::GetCharStringCacheAddr() const
{LOGMEIN("ServerScriptContext.cpp] 158\n");
    return m_contextData.charStringCacheAddr;
}

intptr_t
ServerScriptContext::GetSideEffectsAddr() const
{LOGMEIN("ServerScriptContext.cpp] 164\n");
    return m_contextData.sideEffectsAddr;
}

intptr_t
ServerScriptContext::GetArraySetElementFastPathVtableAddr() const
{LOGMEIN("ServerScriptContext.cpp] 170\n");
    return m_contextData.arraySetElementFastPathVtableAddr;
}

intptr_t
ServerScriptContext::GetIntArraySetElementFastPathVtableAddr() const
{LOGMEIN("ServerScriptContext.cpp] 176\n");
    return m_contextData.intArraySetElementFastPathVtableAddr;
}

intptr_t
ServerScriptContext::GetFloatArraySetElementFastPathVtableAddr() const
{LOGMEIN("ServerScriptContext.cpp] 182\n");
    return m_contextData.floatArraySetElementFastPathVtableAddr;
}

intptr_t
ServerScriptContext::GetLibraryAddr() const
{LOGMEIN("ServerScriptContext.cpp] 188\n");
    return m_contextData.libraryAddr;
}

intptr_t
ServerScriptContext::GetGlobalObjectAddr() const
{LOGMEIN("ServerScriptContext.cpp] 194\n");
    return m_contextData.globalObjectAddr;
}

intptr_t
ServerScriptContext::GetGlobalObjectThisAddr() const
{LOGMEIN("ServerScriptContext.cpp] 200\n");
    return m_globalThisAddr;
}

void
ServerScriptContext::UpdateGlobalObjectThisAddr(intptr_t globalThis)
{LOGMEIN("ServerScriptContext.cpp] 206\n");
    m_globalThisAddr = globalThis;
}

intptr_t
ServerScriptContext::GetNumberAllocatorAddr() const
{LOGMEIN("ServerScriptContext.cpp] 212\n");
    return m_contextData.numberAllocatorAddr;
}

intptr_t
ServerScriptContext::GetRecyclerAddr() const
{LOGMEIN("ServerScriptContext.cpp] 218\n");
    return m_contextData.recyclerAddr;
}

bool
ServerScriptContext::GetRecyclerAllowNativeCodeBumpAllocation() const
{LOGMEIN("ServerScriptContext.cpp] 224\n");
    return m_contextData.recyclerAllowNativeCodeBumpAllocation != 0;
}

bool
ServerScriptContext::IsSIMDEnabled() const
{LOGMEIN("ServerScriptContext.cpp] 230\n");
    return m_contextData.isSIMDEnabled != 0;
}

intptr_t
ServerScriptContext::GetBuiltinFunctionsBaseAddr() const
{LOGMEIN("ServerScriptContext.cpp] 236\n");
    return m_contextData.builtinFunctionsBaseAddr;
}

intptr_t
ServerScriptContext::GetAddr() const
{LOGMEIN("ServerScriptContext.cpp] 242\n");
    return m_contextData.scriptContextAddr;
}

intptr_t
ServerScriptContext::GetVTableAddress(VTableValue vtableType) const
{LOGMEIN("ServerScriptContext.cpp] 248\n");
    Assert(vtableType < VTableValue::Count);
    return m_contextData.vtableAddresses[vtableType];
}

bool
ServerScriptContext::IsRecyclerVerifyEnabled() const
{LOGMEIN("ServerScriptContext.cpp] 255\n");
    return m_contextData.isRecyclerVerifyEnabled != FALSE;
}

uint
ServerScriptContext::GetRecyclerVerifyPad() const
{LOGMEIN("ServerScriptContext.cpp] 261\n");
    return m_contextData.recyclerVerifyPad;
}

bool
ServerScriptContext::IsPRNGSeeded() const
{LOGMEIN("ServerScriptContext.cpp] 267\n");
    return m_isPRNGSeeded;
}

intptr_t
ServerScriptContext::GetDebuggingFlagsAddr() const
{LOGMEIN("ServerScriptContext.cpp] 273\n");
    return static_cast<intptr_t>(m_contextData.debuggingFlagsAddr);
}

intptr_t
ServerScriptContext::GetDebugStepTypeAddr() const
{LOGMEIN("ServerScriptContext.cpp] 279\n");
    return static_cast<intptr_t>(m_contextData.debugStepTypeAddr);
}

intptr_t
ServerScriptContext::GetDebugFrameAddressAddr() const
{LOGMEIN("ServerScriptContext.cpp] 285\n");
    return static_cast<intptr_t>(m_contextData.debugFrameAddressAddr);
}

intptr_t
ServerScriptContext::GetDebugScriptIdWhenSetAddr() const
{LOGMEIN("ServerScriptContext.cpp] 291\n");
    return static_cast<intptr_t>(m_contextData.debugScriptIdWhenSetAddr);
}

bool
ServerScriptContext::IsClosed() const
{LOGMEIN("ServerScriptContext.cpp] 297\n");
    return m_isClosed;
}

void
ServerScriptContext::AddToDOMFastPathHelperMap(intptr_t funcInfoAddr, IR::JnHelperMethod helper)
{LOGMEIN("ServerScriptContext.cpp] 303\n");
    m_domFastPathHelperMap->Add(funcInfoAddr, helper);
}

ArenaAllocator *
ServerScriptContext::GetSourceCodeArena()
{LOGMEIN("ServerScriptContext.cpp] 309\n");
    return &m_sourceCodeArena;
}

void
ServerScriptContext::DecommitEmitBufferManager(bool asmJsManager)
{LOGMEIN("ServerScriptContext.cpp] 315\n");
    GetEmitBufferManager(asmJsManager)->Decommit();
}

OOPEmitBufferManager *
ServerScriptContext::GetEmitBufferManager(bool asmJsManager)
{LOGMEIN("ServerScriptContext.cpp] 321\n");
    if (asmJsManager)
    {LOGMEIN("ServerScriptContext.cpp] 323\n");
        return &m_asmJsInterpreterThunkBufferManager;
    }
    else
    {
        return &m_interpreterThunkBufferManager;
    }
}

IR::JnHelperMethod
ServerScriptContext::GetDOMFastPathHelper(intptr_t funcInfoAddr)
{LOGMEIN("ServerScriptContext.cpp] 334\n");
    IR::JnHelperMethod helper;

    m_domFastPathHelperMap->LockResize();
    bool found = m_domFastPathHelperMap->TryGetValue(funcInfoAddr, &helper);
    m_domFastPathHelperMap->UnlockResize();

    Assert(found);
    return helper;
}

void
ServerScriptContext::Close()
{LOGMEIN("ServerScriptContext.cpp] 347\n");
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
    {LOGMEIN("ServerScriptContext.cpp] 367\n");
        // Not freeing here, we'll expect explicit ServerCleanupScriptContext() call to do the free
        // otherwise after free, the CodeGen call can still get same scriptContext if there's another 
        // ServerInitializeScriptContext call
    }
}

Field(Js::Var)*
ServerScriptContext::GetModuleExportSlotArrayAddress(uint moduleIndex, uint slotIndex)
{LOGMEIN("ServerScriptContext.cpp] 376\n");
    Assert(m_moduleRecords.ContainsKey(moduleIndex));
    auto record = m_moduleRecords.Item(moduleIndex);
    return record->localExportSlotsAddr;
}

void
ServerScriptContext::SetIsPRNGSeeded(bool value)
{LOGMEIN("ServerScriptContext.cpp] 384\n");
    m_isPRNGSeeded = value;
}

void
ServerScriptContext::AddModuleRecordInfo(unsigned int moduleId, __int64 localExportSlotsAddr)
{LOGMEIN("ServerScriptContext.cpp] 390\n");
    Js::ServerSourceTextModuleRecord* record = HeapNewStructZ(Js::ServerSourceTextModuleRecord);
    record->moduleId = moduleId;
    record->localExportSlotsAddr = (Field(Js::Var)*)localExportSlotsAddr;
    m_moduleRecords.Add(moduleId, record);
}

Js::ScriptContextProfiler *
ServerScriptContext::GetCodeGenProfiler() const
{LOGMEIN("ServerScriptContext.cpp] 399\n");
#ifdef PROFILE_EXEC
    return m_codeGenProfiler;
#else
    return nullptr;
#endif
}

#endif
