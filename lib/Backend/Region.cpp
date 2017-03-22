//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "Backend.h"

Region *
Region::New(RegionType type, Region * parent, Func * func)
{LOGMEIN("Region.cpp] 8\n");
    Region * region = JitAnew(func->m_alloc, Region);

    region->type = type;
    region->parent = parent;
    region->bailoutReturnThunkLabel = IR::LabelInstr::New(Js::OpCode::Label, func, true);
    if (type == RegionTypeTry)
    {LOGMEIN("Region.cpp] 15\n");
        region->selfOrFirstTryAncestor = region;
    }
    return region;
}

void
Region::AllocateEHBailoutData(Func * func, IR::Instr * tryInstr)
{LOGMEIN("Region.cpp] 23\n");
    if (this->GetType() == RegionTypeRoot)
    {LOGMEIN("Region.cpp] 25\n");
        this->ehBailoutData = NativeCodeDataNew(func->GetNativeCodeDataAllocator(), Js::EHBailoutData, -1 /*nestingDepth*/, 0 /*catchOffset*/, nullptr /*parent*/);
    }
    else
    {
        this->ehBailoutData = NativeCodeDataNew(func->GetNativeCodeDataAllocator(), Js::EHBailoutData, this->GetParent()->ehBailoutData->nestingDepth + 1, 0, this->GetParent()->ehBailoutData);
        if (this->GetType() == RegionTypeTry)
        {LOGMEIN("Region.cpp] 32\n");
            Assert(tryInstr);
            if (tryInstr->m_opcode == Js::OpCode::TryCatch)
            {LOGMEIN("Region.cpp] 35\n");
                this->ehBailoutData->catchOffset = tryInstr->AsBranchInstr()->GetTarget()->GetByteCodeOffset(); // ByteCode offset of the Catch
            }
        }
    }
}

Region *
Region::GetSelfOrFirstTryAncestor()
{LOGMEIN("Region.cpp] 44\n");
    if (!this->selfOrFirstTryAncestor)
    {LOGMEIN("Region.cpp] 46\n");
        Region* region = this;
        while (region && region->GetType() != RegionTypeTry)
        {LOGMEIN("Region.cpp] 49\n");
            region = region->GetParent();
        }
        this->selfOrFirstTryAncestor = region;
    }
    return this->selfOrFirstTryAncestor;
}
