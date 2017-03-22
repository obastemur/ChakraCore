//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once
using namespace JsUtil;

/*
*   CaseNode - represents the case statements (not the case block) in the switch statement
*/
class CaseNode
{
private:
    uint32              offset;         // offset - indicates the bytecode offset of the case instruction
    uint32              targetOffset;   // targetOffset - indicates the bytecode offset of the target instruction (case block)
    IR::BranchInstr*    caseInstr;      // caseInstr - stores the case instruction
    IR::Opnd*           lowerBound;     // lowerBound - used for integer cases

    int32 GetIntConst(IR::Opnd* opnd)
    {LOGMEIN("CaseNode.h] 19\n");
        Assert(IsIntConst(opnd));
        return opnd->IsIntConstOpnd() ? opnd->AsIntConstOpnd()->AsInt32() : opnd->GetStackSym()->GetIntConstValue();
    }

    bool IsIntConst(IR::Opnd* opnd)
    {LOGMEIN("CaseNode.h] 25\n");
        return opnd->IsIntConstOpnd() || opnd->GetStackSym()->IsIntConst();
    }

    bool IsStrConst(IR::Opnd* opnd)
    {LOGMEIN("CaseNode.h] 30\n");
        return opnd->GetStackSym()->m_isStrConst;
    }

public:
    CaseNode(IR::BranchInstr* caseInstr, uint32 offset, uint32 targetOffset, IR::Opnd* lowerBound = nullptr)
        : caseInstr(caseInstr),
        offset(offset),
        targetOffset(targetOffset),
        lowerBound(lowerBound)
    {LOGMEIN("CaseNode.h] 40\n");
    }

    int32 GetUpperBoundIntConst()
    {LOGMEIN("CaseNode.h] 44\n");
        AssertMsg(IsUpperBoundIntConst(), "Source2 operand is not an integer constant");
        return GetIntConst(GetUpperBound());
    }

    JITJavascriptString* GetUpperBoundStringConstLocal()
    {LOGMEIN("CaseNode.h] 50\n");
        AssertMsg(IsUpperBoundStrConst(), "Upper bound operand is not a string constant");
        return JITJavascriptString::FromVar(GetUpperBound()->GetStackSym()->GetConstAddress(true));
    }

    JITJavascriptString* GetUpperBoundStrConst()
    {LOGMEIN("CaseNode.h] 56\n");
        AssertMsg(IsUpperBoundStrConst(), "Upper bound operand is not a string constant");
        return static_cast<JITJavascriptString*>(GetUpperBound()->GetStackSym()->GetConstAddress(false));
    }

    bool IsUpperBoundIntConst()
    {LOGMEIN("CaseNode.h] 62\n");
        return IsIntConst(GetUpperBound());
    }

    bool IsUpperBoundStrConst()
    {LOGMEIN("CaseNode.h] 67\n");
        return IsStrConst(GetUpperBound());
    }

    int32 GetLowerBoundIntConst()
    {LOGMEIN("CaseNode.h] 72\n");
        AssertMsg(IsLowerBoundIntConst(), "LowerBound is not an integer constant");
        return GetIntConst(lowerBound);
    }

    bool IsLowerBoundIntConst()
    {LOGMEIN("CaseNode.h] 78\n");
        return IsIntConst(lowerBound);
    }

    bool IsLowerBoundStrConst()
    {LOGMEIN("CaseNode.h] 83\n");
        return IsStrConst(lowerBound);
    }

    uint32 GetOffset()
    {LOGMEIN("CaseNode.h] 88\n");
        return offset;
    }

    uint32 GetTargetOffset()
    {LOGMEIN("CaseNode.h] 93\n");
        return targetOffset;
    }

    IR::Opnd* GetUpperBound()
    {LOGMEIN("CaseNode.h] 98\n");
        return caseInstr->GetSrc2();
    }

    IR::Opnd* GetLowerBound()
    {LOGMEIN("CaseNode.h] 103\n");
        return lowerBound;
    }

    void SetLowerBound(IR::Opnd* lowerBound)
    {LOGMEIN("CaseNode.h] 108\n");
        this->lowerBound = lowerBound;
    }

    IR::BranchInstr* GetCaseInstr()
    {LOGMEIN("CaseNode.h] 113\n");
        return caseInstr;
    }
};

template <>
struct DefaultComparer<CaseNode *>
{
public:
    static int Compare(CaseNode* caseNode1, CaseNode* caseNode2);
    static bool Equals(CaseNode* x, CaseNode* y);
    static uint GetHashCode(CaseNode * caseNode);
};

