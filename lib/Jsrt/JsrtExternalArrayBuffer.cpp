//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "JsrtPch.h"
#include "JsrtExternalArrayBuffer.h"

namespace Js
{
    JsrtExternalArrayBuffer::JsrtExternalArrayBuffer(byte *buffer, uint32 length, JsFinalizeCallback finalizeCallback, void *callbackState, DynamicType *type)
        : ExternalArrayBuffer(buffer, length, type), finalizeCallback(finalizeCallback), callbackState(callbackState)
    {TRACE_IT(28380);
    }

    JsrtExternalArrayBuffer* JsrtExternalArrayBuffer::New(byte *buffer, uint32 length, JsFinalizeCallback finalizeCallback, void *callbackState, DynamicType *type)
    {TRACE_IT(28381);
        Recycler* recycler = type->GetScriptContext()->GetRecycler();
        return RecyclerNewFinalized(recycler, JsrtExternalArrayBuffer, buffer, length, finalizeCallback, callbackState, type);
    }

    void JsrtExternalArrayBuffer::Finalize(bool isShutdown)
    {TRACE_IT(28382);
        if (finalizeCallback != nullptr)
        {TRACE_IT(28383);
            finalizeCallback(callbackState);
        }
    }
}
