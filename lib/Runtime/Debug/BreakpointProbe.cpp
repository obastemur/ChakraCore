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
    {LOGMEIN("BreakpointProbe.cpp] 14\n");
    }

    bool BreakpointProbe::Install(ScriptContext* pScriptContext)
    {LOGMEIN("BreakpointProbe.cpp] 18\n");
        Assert(this->functionBody);
        return functionBody->InstallProbe(byteOffset);
    }

    bool BreakpointProbe::Uninstall(ScriptContext* pScriptContext)
    {LOGMEIN("BreakpointProbe.cpp] 24\n");
        Assert(this->functionBody);

        if (this->functionBody)
        {LOGMEIN("BreakpointProbe.cpp] 28\n");
            Assert(this->debugDocument);
            this->debugDocument->RemoveBreakpointProbe(this);

            return functionBody->UninstallProbe(byteOffset);
        }

        return true;
    }

    bool BreakpointProbe::CanHalt(InterpreterHaltState* pHaltState)
    {LOGMEIN("BreakpointProbe.cpp] 39\n");
        Assert(this->functionBody);

        FunctionBody* pCurrentFuncBody = pHaltState->GetFunction();
        int offset = pHaltState->GetCurrentOffset();

        if (functionBody == pCurrentFuncBody && byteOffset == offset)
        {LOGMEIN("BreakpointProbe.cpp] 46\n");
            return true;
        }
        return false;
    }

    void BreakpointProbe::DispatchHalt(InterpreterHaltState* pHaltState)
    {LOGMEIN("BreakpointProbe.cpp] 53\n");
        Assert(false);
    }

    void BreakpointProbe::CleanupHalt()
    {LOGMEIN("BreakpointProbe.cpp] 58\n");
        Assert(this->functionBody);

        // Nothing to clean here
    }

    bool BreakpointProbe::Matches(FunctionBody* _pBody, int _characterOffset)
    {LOGMEIN("BreakpointProbe.cpp] 65\n");
        Assert(this->functionBody);
        return _pBody == functionBody && _characterOffset == characterOffset;
    }

    bool BreakpointProbe::Matches(StatementLocation statement)
    {LOGMEIN("BreakpointProbe.cpp] 71\n");
        return (this->GetCharacterOffset() == statement.statement.begin) && (this->byteOffset == statement.bytecodeSpan.begin);
    }

    bool BreakpointProbe::Matches(FunctionBody* _pBody, DebugDocument* debugDocument, int byteOffset)
    {LOGMEIN("BreakpointProbe.cpp] 76\n");
        return (this->functionBody == _pBody) && (this->debugDocument == debugDocument) && (this->byteOffset == byteOffset);
    }

    void BreakpointProbe::GetStatementLocation(StatementLocation * statement)
    {LOGMEIN("BreakpointProbe.cpp] 81\n");
        statement->bytecodeSpan.begin = this->byteOffset;
        statement->function = this->functionBody;
        statement->statement.begin = this->characterOffset;
    }
}
