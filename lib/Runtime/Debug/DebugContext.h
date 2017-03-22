//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

class HostDebugContext
{
public:
    HostDebugContext(Js::ScriptContext* inScriptContext) {LOGMEIN("DebugContext.h] 9\n"); this->scriptContext = inScriptContext; }
    virtual void Delete() = 0;
    virtual DWORD_PTR GetHostSourceContext(Js::Utf8SourceInfo * sourceInfo) = 0;
    virtual HRESULT SetThreadDescription(__in LPCWSTR url) = 0;
    virtual HRESULT DbgRegisterFunction(Js::ScriptContext * scriptContext, Js::FunctionBody * functionBody, DWORD_PTR dwDebugSourceContext, LPCWSTR title) = 0;
    virtual void ReParentToCaller(Js::Utf8SourceInfo* sourceInfo) = 0;
    virtual void SortMembersList(JsUtil::List<Js::DebuggerPropertyDisplayInfo *, ArenaAllocator> * pMembersList, Js::ScriptContext* scriptContext) {LOGMEIN("DebugContext.h] 15\n");/*Do nothing*/}

    Js::ScriptContext* GetScriptContext() {LOGMEIN("DebugContext.h] 17\n"); return scriptContext; }

private:
    Js::ScriptContext* scriptContext;
};

namespace Js
{
    // Represents the different modes that the debugger can be placed into.
    enum DebuggerMode
    {
        // The debugger is not running so the engine can be running
        // in JITed mode.
        NotDebugging,

        // The debugger is not running but PDM has been created and
        // source rundown was performed to register script documents.
        SourceRundown,

        // The debugger is running which means that the engine is
        // running in interpreted mode.
        Debugging,
    };

    class DebugContext
    {
    public:
        DebugContext(Js::ScriptContext * scriptContext);
        ~DebugContext();
        void Initialize();
        HRESULT RundownSourcesAndReparse(bool shouldPerformSourceRundown, bool shouldReparseFunctions);
        void RegisterFunction(Js::ParseableFunctionInfo * func, LPCWSTR title);
        void Close();
        void SetHostDebugContext(HostDebugContext * hostDebugContext);

        void SetDebuggerMode(DebuggerMode mode);
        bool IsDebugContextInNonDebugMode() const {LOGMEIN("DebugContext.h] 53\n"); return this->debuggerMode == DebuggerMode::NotDebugging; }
        bool IsDebugContextInDebugMode() const {LOGMEIN("DebugContext.h] 54\n"); return this->debuggerMode == DebuggerMode::Debugging; }
        bool IsDebugContextInSourceRundownMode() const {LOGMEIN("DebugContext.h] 55\n"); return this->debuggerMode == DebuggerMode::SourceRundown; }
        bool IsDebugContextInSourceRundownOrDebugMode() const {LOGMEIN("DebugContext.h] 56\n"); return IsDebugContextInSourceRundownMode() || IsDebugContextInDebugMode(); }

        ProbeContainer* GetProbeContainer() const {LOGMEIN("DebugContext.h] 58\n"); return this->diagProbesContainer; }

        HostDebugContext * GetHostDebugContext() const {LOGMEIN("DebugContext.h] 60\n"); return hostDebugContext; }

    private:
        ScriptContext * scriptContext;
        HostDebugContext* hostDebugContext;
        DebuggerMode debuggerMode;
        ProbeContainer* diagProbesContainer;

        // Private Functions
        void WalkAndAddUtf8SourceInfo(Js::Utf8SourceInfo* sourceInfo, JsUtil::List<Js::Utf8SourceInfo *, Recycler, false, Js::CopyRemovePolicy, RecyclerPointerComparer> *utf8SourceInfoList);
        bool CanRegisterFunction() const;
        void RegisterFunction(Js::ParseableFunctionInfo * func, DWORD_PTR dwDebugSourceContext, LPCWSTR title);
        void RegisterFunction(Js::FunctionBody * functionBody, DWORD_PTR dwDebugSourceContext, LPCWSTR title);

        template<class TMapFunction>
        void MapUTF8SourceInfoUntil(TMapFunction map);
    };
}
