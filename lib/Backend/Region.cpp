//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "Backend.h"

Region *
Region::New(RegionType type, Region * parent, Func * func)
{TRACE_IT(15122);
    Region * region = JitAnew(func->m_alloc, Region);

    region->type = type;
    region->parent = parent;
    region->bailoutReturnThunkLabel = IR::LabelInstr::New(Js::OpCode::Label, func, true);
    if (type == RegionTypeTry)
    {TRACE_IT(15123);
        region->selfOrFirstTryAncestor = region;
    }
    return region;
}

void
Region::AllocateEHBailoutData(Func * func, IR::Instr * tryInstr)
{TRACE_IT(15124);
    if (this->GetType() == RegionTypeRoot)
    {TRACE_IT(15125);
        this->ehBailoutData = NativeCodeDataNew(func->GetNativeCodeDataAllocator(), Js::EHBailoutData, -1 /*nestingDepth*/, 0 /*catchOffset*/, nullptr /*parent*/);
    }
    else
    {TRACE_IT(15126);
        this->ehBailoutData = NativeCodeDataNew(func->GetNativeCodeDataAllocator(), Js::EHBailoutData, this->GetParent()->ehBailoutData->nestingDepth + 1, 0, this->GetParent()->ehBailoutData);
        if (this->GetType() == RegionTypeTry)
        {TRACE_IT(15127);
            Assert(tryInstr);
            if (tryInstr->m_opcode == Js::OpCode::TryCatch)
            {TRACE_IT(15128);
                this->ehBailoutData->catchOffset = tryInstr->AsBranchInstr()->GetTarget()->GetByteCodeOffset(); // ByteCode offset of the Catch
            }
        }
    }
}

Region *
Region::GetSelfOrFirstTryAncestor()
{TRACE_IT(15129);
    if (!this->selfOrFirstTryAncestor)
    {TRACE_IT(15130);
        Region* region = this;
        while (region && region->GetType() != RegionTypeTry)
        {TRACE_IT(15131);
            region = region->GetParent();
        }
        this->selfOrFirstTryAncestor = region;
    }
    return this->selfOrFirstTryAncestor;
}
