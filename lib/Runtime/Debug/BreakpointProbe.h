//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    class BreakpointProbe : public Probe
    {
        int characterOffset;
        int byteOffset;
        DebugDocument* debugDocument;
        FunctionBody* functionBody;
        UINT breakpointId;

    public:
        BreakpointProbe(DebugDocument* debugDocument, StatementLocation& statement, int breakpointId);

        virtual bool Install(ScriptContext* pScriptContext);
        virtual bool Uninstall(ScriptContext* pScriptContext);
        virtual bool CanHalt(InterpreterHaltState* pHaltState);
        virtual void DispatchHalt(InterpreterHaltState* pHaltState);
        virtual void CleanupHalt();

        bool Matches(FunctionBody* _pBody, int characterPosition);
        bool Matches(StatementLocation statement);
        bool Matches(FunctionBody* _pBody, DebugDocument* debugDocument, int byteOffset);

        UINT GetId() const {LOGMEIN("BreakpointProbe.h] 29\n"); return this->breakpointId; }
        void GetStatementLocation(StatementLocation * statement);
        FunctionBody* GetFunctionBody() const {LOGMEIN("BreakpointProbe.h] 31\n"); return this->functionBody; }
        int GetBytecodeOffset() const {LOGMEIN("BreakpointProbe.h] 32\n"); return this->byteOffset; }

        DebugDocument* GetDbugDocument() {LOGMEIN("BreakpointProbe.h] 34\n"); return this->debugDocument; }
        int GetCharacterOffset() {LOGMEIN("BreakpointProbe.h] 35\n"); return this->characterOffset; }
    };

    typedef JsUtil::List<BreakpointProbe*, ArenaAllocator> BreakpointProbeList;
}
