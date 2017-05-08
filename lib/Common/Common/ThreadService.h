//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

// Note, these definitions must match the definitions in jsrt.h.

// Callback for a background work item.
namespace JsUtil
{
    class ThreadService
    {
    public:
        typedef void (CALLBACK *BackgroundWorkItemCallback)(void * callbackData);

        // Callback for the thread service itself, to which we can submit background work items.

        typedef bool (CALLBACK *ThreadServiceCallback)(BackgroundWorkItemCallback callback, void * callbackData);

    private:
        ThreadServiceCallback threadService;
        bool isInCallback;

    public:
        ThreadService(ThreadServiceCallback threadService) :
            threadService(threadService),
            isInCallback(false)
        {TRACE_IT(19470);
        }

        bool Invoke(BackgroundWorkItemCallback callback, void * callbackData)
        {TRACE_IT(19471);
            isInCallback = true;
            bool result = threadService(callback, callbackData);
            isInCallback = false;
            return result;
        }

        bool HasCallback() const
        {TRACE_IT(19472);
            return this != nullptr && threadService != nullptr;
        }

        bool IsInCallback() const
        {TRACE_IT(19473);
            return isInCallback;
        }
    };
};
