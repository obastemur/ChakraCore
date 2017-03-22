//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "Backend.h"

int
DefaultComparer<CaseNode *>::Compare(CaseNode* caseNode1, CaseNode* caseNode2)
{LOGMEIN("CaseNode.cpp] 8\n");
    int caseVal1 = caseNode1->GetUpperBoundIntConst();
    int caseVal2 = caseNode2->GetUpperBoundIntConst();
    uint32 caseOffset1 = caseNode1->GetOffset();
    uint32 caseOffset2 = caseNode2->GetOffset();

    if (caseVal1 == caseVal2)
    {LOGMEIN("CaseNode.cpp] 15\n");
        return caseOffset1 - caseOffset2;
    }

    if (caseVal1 > caseVal2) return 1;
    return -1;
}

bool
DefaultComparer<CaseNode *>::Equals(CaseNode * caseNode1, CaseNode* caseNode2)
{LOGMEIN("CaseNode.cpp] 25\n");
    if(caseNode1->IsUpperBoundIntConst() && caseNode2->IsUpperBoundIntConst())
    {LOGMEIN("CaseNode.cpp] 27\n");
        int caseVal1 = caseNode1->GetUpperBoundIntConst();
        int caseVal2 = caseNode2->GetUpperBoundIntConst();
        return caseVal1 == caseVal2;
    }
    else if(caseNode1->IsUpperBoundStrConst() && caseNode2->IsUpperBoundStrConst())
    {LOGMEIN("CaseNode.cpp] 33\n");
        JITJavascriptString * caseVal1 = caseNode1->GetUpperBoundStrConst();
        JITJavascriptString * caseVal2 = caseNode2->GetUpperBoundStrConst();
        return JITJavascriptString::Equals(caseVal1, caseVal2);
    }
    else
    {
        AssertMsg(false, "Should not reach here. CaseNodes should store only string or integer case values");
        return false;
    }
}

uint
DefaultComparer<CaseNode *>::GetHashCode(CaseNode* caseNode)
{LOGMEIN("CaseNode.cpp] 47\n");
    return (uint)caseNode->GetUpperBoundIntConst();
}
