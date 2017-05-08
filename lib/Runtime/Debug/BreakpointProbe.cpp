//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeDebugPch.h"

namespace Js
{
    BreakpointProbe::BreakpointProbe(DebugDocument* debugDocument, StatementLocation& statement, int breakpointId) :
        debugDocument(debugDocument),
        functionBody(statement.function),
        characterOffset(statement.statement.begin),
        byteOffset(statement.bytecodeSpan.begin),
        breakpointId(breakpointId)
    {TRACE_IT(42098);
    }

    bool BreakpointProbe::Install(ScriptContext* pScriptContext)
    {TRACE_IT(42099);
        Assert(this->functionBody);
        return functionBody->InstallProbe(byteOffset);
    }

    bool BreakpointProbe::Uninstall(ScriptContext* pScriptContext)
    {TRACE_IT(42100);
        Assert(this->functionBody);

        if (this->functionBody)
        {TRACE_IT(42101);
            Assert(this->debugDocument);
            this->debugDocument->RemoveBreakpointProbe(this);

            return functionBody->UninstallProbe(byteOffset);
        }

        return true;
    }

    bool BreakpointProbe::CanHalt(InterpreterHaltState* pHaltState)
    {TRACE_IT(42102);
        Assert(this->functionBody);

        FunctionBody* pCurrentFuncBody = pHaltState->GetFunction();
        int offset = pHaltState->GetCurrentOffset();

        if (functionBody == pCurrentFuncBody && byteOffset == offset)
        {TRACE_IT(42103);
            return true;
        }
        return false;
    }

    void BreakpointProbe::DispatchHalt(InterpreterHaltState* pHaltState)
    {TRACE_IT(42104);
        Assert(false);
    }

    void BreakpointProbe::CleanupHalt()
    {TRACE_IT(42105);
        Assert(this->functionBody);

        // Nothing to clean here
    }

    bool BreakpointProbe::Matches(FunctionBody* _pBody, int _characterOffset)
    {TRACE_IT(42106);
        Assert(this->functionBody);
        return _pBody == functionBody && _characterOffset == characterOffset;
    }

    bool BreakpointProbe::Matches(StatementLocation statement)
    {TRACE_IT(42107);
        return (this->GetCharacterOffset() == statement.statement.begin) && (this->byteOffset == statement.bytecodeSpan.begin);
    }

    bool BreakpointProbe::Matches(FunctionBody* _pBody, DebugDocument* debugDocument, int byteOffset)
    {TRACE_IT(42108);
        return (this->functionBody == _pBody) && (this->debugDocument == debugDocument) && (this->byteOffset == byteOffset);
    }

    void BreakpointProbe::GetStatementLocation(StatementLocation * statement)
    {TRACE_IT(42109);
        statement->bytecodeSpan.begin = this->byteOffset;
        statement->function = this->functionBody;
        statement->statement.begin = this->characterOffset;
    }
}
