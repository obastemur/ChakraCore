//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonDataStructuresPch.h"

namespace JsUtil
{
    FBVEnumerator::FBVEnumerator(BVUnit * iterStart, BVUnit * iterEnd):
        icur(iterStart), iend(iterEnd),
        curOffset(0)
    {TRACE_IT(21458);
        if(this->icur != this->iend)
        {TRACE_IT(21459);
            this->curUnit = *iterStart;
            this->MoveToNextBit();
        }
    }

    void
    FBVEnumerator::MoveToValidWord()
    {TRACE_IT(21460);
        while(curUnit.IsEmpty())
        {TRACE_IT(21461);
            this->icur++;
            if(this->icur == this->iend)
            {TRACE_IT(21462);
                return;
            }
            else
            {TRACE_IT(21463);
                this->curUnit    = *this->icur;
                this->curOffset += BVUnit::BitsPerWord;
            }
        }
    }

    void
    FBVEnumerator::MoveToNextBit()
    {TRACE_IT(21464);
        if(curUnit.IsEmpty())
        {TRACE_IT(21465);
            this->curOffset = BVUnit::Floor(curOffset);
            this->MoveToValidWord();
            if(this->End())
            {TRACE_IT(21466);
                return;
            }
        }

        BVIndex i = curUnit.GetNextBit();
        AssertMsg(BVInvalidIndex != i, "Fatal Exception. Error in Bitvector implementation");

        curOffset = BVUnit::Floor(curOffset) + i ;
        curUnit.Clear(i);
    }

    void
    FBVEnumerator::operator++(int)
    {TRACE_IT(21467);
        AssertMsg(this->icur != this->iend, "Iterator past the end of bit stream");
        this->MoveToNextBit();
    }

    BVIndex
    FBVEnumerator::GetCurrent() const
    {TRACE_IT(21468);
        return this->curOffset;
    }

    bool
    FBVEnumerator::End() const
    {TRACE_IT(21469);
        return this->icur == this->iend;
    }
}
