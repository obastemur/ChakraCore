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

        UINT GetId() const {TRACE_IT(42110); return this->breakpointId; }
        void GetStatementLocation(StatementLocation * statement);
        FunctionBody* GetFunctionBody() const {TRACE_IT(42111); return this->functionBody; }
        int GetBytecodeOffset() const {TRACE_IT(42112); return this->byteOffset; }

        DebugDocument* GetDbugDocument() {TRACE_IT(42113); return this->debugDocument; }
        int GetCharacterOffset() {TRACE_IT(42114); return this->characterOffset; }
    };

    typedef JsUtil::List<BreakpointProbe*, ArenaAllocator> BreakpointProbeList;
}
