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
    {TRACE_IT(1470);
        Assert(IsIntConst(opnd));
        return opnd->IsIntConstOpnd() ? opnd->AsIntConstOpnd()->AsInt32() : opnd->GetStackSym()->GetIntConstValue();
    }

    bool IsIntConst(IR::Opnd* opnd)
    {TRACE_IT(1471);
        return opnd->IsIntConstOpnd() || opnd->GetStackSym()->IsIntConst();
    }

    bool IsStrConst(IR::Opnd* opnd)
    {TRACE_IT(1472);
        return opnd->GetStackSym()->m_isStrConst;
    }

public:
    CaseNode(IR::BranchInstr* caseInstr, uint32 offset, uint32 targetOffset, IR::Opnd* lowerBound = nullptr)
        : caseInstr(caseInstr),
        offset(offset),
        targetOffset(targetOffset),
        lowerBound(lowerBound)
    {TRACE_IT(1473);
    }

    int32 GetUpperBoundIntConst()
    {TRACE_IT(1474);
        AssertMsg(IsUpperBoundIntConst(), "Source2 operand is not an integer constant");
        return GetIntConst(GetUpperBound());
    }

    JITJavascriptString* GetUpperBoundStringConstLocal()
    {TRACE_IT(1475);
        AssertMsg(IsUpperBoundStrConst(), "Upper bound operand is not a string constant");
        return JITJavascriptString::FromVar(GetUpperBound()->GetStackSym()->GetConstAddress(true));
    }

    JITJavascriptString* GetUpperBoundStrConst()
    {TRACE_IT(1476);
        AssertMsg(IsUpperBoundStrConst(), "Upper bound operand is not a string constant");
        return static_cast<JITJavascriptString*>(GetUpperBound()->GetStackSym()->GetConstAddress(false));
    }

    bool IsUpperBoundIntConst()
    {TRACE_IT(1477);
        return IsIntConst(GetUpperBound());
    }

    bool IsUpperBoundStrConst()
    {TRACE_IT(1478);
        return IsStrConst(GetUpperBound());
    }

    int32 GetLowerBoundIntConst()
    {TRACE_IT(1479);
        AssertMsg(IsLowerBoundIntConst(), "LowerBound is not an integer constant");
        return GetIntConst(lowerBound);
    }

    bool IsLowerBoundIntConst()
    {TRACE_IT(1480);
        return IsIntConst(lowerBound);
    }

    bool IsLowerBoundStrConst()
    {TRACE_IT(1481);
        return IsStrConst(lowerBound);
    }

    uint32 GetOffset()
    {TRACE_IT(1482);
        return offset;
    }

    uint32 GetTargetOffset()
    {TRACE_IT(1483);
        return targetOffset;
    }

    IR::Opnd* GetUpperBound()
    {TRACE_IT(1484);
        return caseInstr->GetSrc2();
    }

    IR::Opnd* GetLowerBound()
    {TRACE_IT(1485);
        return lowerBound;
    }

    void SetLowerBound(IR::Opnd* lowerBound)
    {TRACE_IT(1486);
        this->lowerBound = lowerBound;
    }

    IR::BranchInstr* GetCaseInstr()
    {TRACE_IT(1487);
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

