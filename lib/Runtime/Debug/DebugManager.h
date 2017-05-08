//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    struct InterpreterHaltState;

    class DebugManager
    {
        friend class RecyclableObjectDisplay;
        friend class RecyclableArrayWalker;
        template <typename TData> friend class RecyclableCollectionObjectWalker;
        template <typename TData> friend class RecyclableCollectionObjectDisplay;
        friend class RecyclableKeyValueDisplay;
        friend class ProbeContainer;

    private:
        InterpreterHaltState* pCurrentInterpreterLocation; // NULL if not Halted at a Probe
        DWORD_PTR secondaryCurrentSourceContext;           // For resolving ambiguity among generated files, e.g. eval, anonymous, etc.
        uint32 debugSessionNumber;                          // A unique number, which will be used to sync all probecontainer when on break
        RecyclerRootPtr<Js::DynamicObject> pConsoleScope;
        ThreadContext* pThreadContext;
        bool isAtDispatchHalt;
        PageAllocator diagnosticPageAllocator;

        int evalCodeRegistrationCount;
        int anonymousCodeRegistrationCount;
        int jscriptBlockRegistrationCount;
        bool isDebuggerAttaching;
        DebuggingFlags debuggingFlags;
        UINT nextBreakPointId;
        DWORD localsDisplayFlags;
        void * dispatchHaltFrameAddress;
    public:
        StepController stepController;
        AsyncBreakController asyncBreakController;
        PropertyId mutationNewValuePid;                    // Holds the property id of $newValue$ property for object mutation breakpoint
        PropertyId mutationPropertyNamePid;                // Holds the property id of $propertyName$ property for object mutation breakpoint
        PropertyId mutationTypePid;                        // Holds the property id of $mutationType$ property for object mutation breakpoint

        DebugManager(ThreadContext* _pThreadContext, AllocationPolicyManager * allocationPolicyManager);
        ~DebugManager();
        void Close();

        DebuggingFlags* GetDebuggingFlags();
        intptr_t GetDebuggingFlagsAddr() const;

        bool IsAtDispatchHalt() const {TRACE_IT(42248); return this->isAtDispatchHalt; }
        void SetDispatchHalt(bool set) {TRACE_IT(42249); this->isAtDispatchHalt = set; }

        ReferencedArenaAdapter* GetDiagnosticArena();
        DWORD_PTR AllocateSecondaryHostSourceContext();
        void SetCurrentInterpreterLocation(InterpreterHaltState* pHaltState);
        void UnsetCurrentInterpreterLocation();
        bool IsMatchTopFrameStackAddress(DiagStackFrame* frame) const;
        uint32 GetDebugSessionNumber() const {TRACE_IT(42250); return debugSessionNumber; }
#ifdef ENABLE_MUTATION_BREAKPOINT
        MutationBreakpoint* GetActiveMutationBreakpoint() const;
#endif
        DynamicObject* GetConsoleScope(ScriptContext* scriptContext);
        FrameDisplay *GetFrameDisplay(ScriptContext* scriptContext, DynamicObject* scopeAtZero, DynamicObject* scopeAtOne);
        void UpdateConsoleScope(DynamicObject* copyFromScope, ScriptContext* scriptContext);
        PageAllocator * GetDiagnosticPageAllocator() {TRACE_IT(42251); return &this->diagnosticPageAllocator; }
        void SetDispatchHaltFrameAddress(void * returnAddress) {TRACE_IT(42252); this->dispatchHaltFrameAddress = returnAddress; }
        DWORD_PTR GetDispatchHaltFrameAddress() const {TRACE_IT(42253); return (DWORD_PTR)this->dispatchHaltFrameAddress; }
#if DBG
        void ValidateDebugAPICall();
#endif
        void SetDebuggerAttaching(bool attaching) {TRACE_IT(42254); this->isDebuggerAttaching = attaching; }
        bool IsDebuggerAttaching() const {TRACE_IT(42255); return this->isDebuggerAttaching; }

        enum DynamicFunctionType
        {
            DFT_EvalCode,
            DFT_AnonymousCode,
            DFT_JScriptBlock
        };

        int GetNextId(DynamicFunctionType eFunc)
        {TRACE_IT(42256);
            switch (eFunc)
            {
            case DFT_EvalCode: return ++evalCodeRegistrationCount;
            case DFT_AnonymousCode: return ++anonymousCodeRegistrationCount;
            case DFT_JScriptBlock: return ++jscriptBlockRegistrationCount;
            }

            return -1;
        }

        UINT GetNextBreakpointId()
        {TRACE_IT(42257);
            return ++nextBreakPointId;
        }

        enum LocalsDisplayFlags
        {
            LocalsDisplayFlags_None = 0x0,
            LocalsDisplayFlags_NoGroupMethods = 0x1
        };

        void SetLocalsDisplayFlags(LocalsDisplayFlags localsDisplayFlags)
        {TRACE_IT(42258);
            this->localsDisplayFlags |= localsDisplayFlags;
        }

        bool IsLocalsDisplayFlagsSet(LocalsDisplayFlags localsDisplayFlags)
        {TRACE_IT(42259);
            return (this->localsDisplayFlags & localsDisplayFlags) == (DWORD)localsDisplayFlags;
        }
    };
}

class AutoSetDispatchHaltFlag
{
public:
    AutoSetDispatchHaltFlag(Js::ScriptContext *scriptContext, ThreadContext *threadContext);
    ~AutoSetDispatchHaltFlag();
private:
    // Primary reason for caching both because once we break to debugger our engine is open for re-entrancy. That means the
    // connection to scriptcontet to threadcontext can go away (imagine the GC is called when we are broken)
    Js::ScriptContext * m_scriptContext;
    ThreadContext * m_threadContext;
};

