//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    class RejitException : public ExceptionBase
    {
    private:
        const RejitReason reason;

    public:
        RejitException(const RejitReason reason) : reason(reason)
        {TRACE_IT(22526);
        }

    public:
        RejitReason Reason() const
        {TRACE_IT(22527);
            return reason;
        }

        const char *ReasonName() const
        {TRACE_IT(22528);
            return RejitReasonNames[reason];
        }
    };
}
