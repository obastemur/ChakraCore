//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonExceptionsPch.h"

#ifdef EXCEPTION_CHECK
#include "ExceptionCheck.h"

THREAD_LOCAL ExceptionCheck::Data ExceptionCheck::data;

BOOL ExceptionCheck::IsEmpty()
{TRACE_IT(22506);
    return (data.handledExceptionType == ExceptionType_None);
}

ExceptionCheck::Data ExceptionCheck::Save()
{TRACE_IT(22507);
    ExceptionCheck::Data savedData = data;
    data = ExceptionCheck::Data();
    return savedData;
}

void ExceptionCheck::Restore(ExceptionCheck::Data& savedData)
{TRACE_IT(22508);
    Assert(IsEmpty());
    data = savedData;
}

ExceptionCheck::Data ExceptionCheck::GetData()
{TRACE_IT(22509);
    return data;
}

BOOL ExceptionCheck::CanHandleOutOfMemory()
{TRACE_IT(22510);
    return (data.handledExceptionType == ExceptionType_DisableCheck) ||
        JsUtil::ExternalApi::IsScriptActiveOnCurrentThreadContext() ||
        (data.handledExceptionType & ExceptionType_OutOfMemory);
}

BOOL ExceptionCheck::HasStackProbe()
{TRACE_IT(22511);
    return  (data.handledExceptionType & ExceptionType_HasStackProbe);
}


BOOL ExceptionCheck::CanHandleStackOverflow(bool isExternal)
{TRACE_IT(22512);
    return (JsUtil::ExternalApi::IsScriptActiveOnCurrentThreadContext() || isExternal) ||
        (data.handledExceptionType & ExceptionType_StackOverflow) ||
        (data.handledExceptionType == ExceptionType_DisableCheck);
}
void ExceptionCheck::SetHandledExceptionType(ExceptionType e)
{TRACE_IT(22513);
    Assert((e & ExceptionType_DisableCheck) == 0 || e == ExceptionType_DisableCheck);
    Assert(IsEmpty());
#if DBG
    if(!(e == ExceptionType_None ||
         e == ExceptionType_DisableCheck ||
         !JsUtil::ExternalApi::IsScriptActiveOnCurrentThreadContext() ||
         (e & ExceptionType_JavascriptException) == ExceptionType_JavascriptException ||
         e == ExceptionType_HasStackProbe))
    {TRACE_IT(22514);
        Assert(false);
    }
#endif
    data.handledExceptionType = e;
}

ExceptionType ExceptionCheck::ClearHandledExceptionType()
{TRACE_IT(22515);
    ExceptionType exceptionType = data.handledExceptionType;
    data.handledExceptionType = ExceptionType_None;
    Assert(IsEmpty());
    return exceptionType;
}

AutoHandledExceptionType::AutoHandledExceptionType(ExceptionType e)
{TRACE_IT(22516);
    ExceptionCheck::SetHandledExceptionType(e);
}

AutoHandledExceptionType::~AutoHandledExceptionType()
{TRACE_IT(22517);
    Assert(ExceptionCheck::GetData().handledExceptionType == ExceptionType_DisableCheck ||
        !JsUtil::ExternalApi::IsScriptActiveOnCurrentThreadContext() ||
        ExceptionCheck::GetData().handledExceptionType == ExceptionType_HasStackProbe ||
        (ExceptionCheck::GetData().handledExceptionType & ExceptionType_JavascriptException) == ExceptionType_JavascriptException);
    ExceptionCheck::ClearHandledExceptionType();
}

AutoNestedHandledExceptionType::AutoNestedHandledExceptionType(ExceptionType e)
{TRACE_IT(22518);
    savedData = ExceptionCheck::Save();
    ExceptionCheck::SetHandledExceptionType(e);
}
AutoNestedHandledExceptionType::~AutoNestedHandledExceptionType()
{TRACE_IT(22519);
    Assert(ExceptionCheck::GetData().handledExceptionType == ExceptionType_DisableCheck ||
        !JsUtil::ExternalApi::IsScriptActiveOnCurrentThreadContext() ||
        ExceptionCheck::GetData().handledExceptionType == ExceptionType_HasStackProbe ||
        (ExceptionCheck::GetData().handledExceptionType & ExceptionType_JavascriptException) == ExceptionType_JavascriptException);
    ExceptionCheck::ClearHandledExceptionType();
    ExceptionCheck::Restore(savedData);
}

AutoFilterExceptionRegion::AutoFilterExceptionRegion(ExceptionType e)
{TRACE_IT(22520);
    savedData = ExceptionCheck::Save();
    ExceptionCheck::SetHandledExceptionType((ExceptionType)(~e & savedData.handledExceptionType));
}
AutoFilterExceptionRegion::~AutoFilterExceptionRegion()
{TRACE_IT(22521);
    ExceptionCheck::ClearHandledExceptionType();
    ExceptionCheck::Restore(savedData);
}
#endif
