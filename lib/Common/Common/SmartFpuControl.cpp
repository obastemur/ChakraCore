//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonCommonPch.h"
#include "Common/SmartFpuControl.h"

#if _WIN32
#include <float.h>
#endif

//
// Floating point unit utility functions
//

static errno_t SetFPUControlDefault(void)
{TRACE_IT(19452);
#if _WIN32
#if _M_AMD64 || _M_ARM
    return _controlfp_s(0, _RC_NEAR + _DN_SAVE + _EM_INVALID + _EM_ZERODIVIDE +
        _EM_OVERFLOW + _EM_UNDERFLOW + _EM_INEXACT,
        _MCW_EM | _MCW_DN | _MCW_RC);
#elif _M_IX86
    _control87(_CW_DEFAULT, _MCW_EM | _MCW_DN | _MCW_PC | _MCW_RC | _MCW_IC);
    return 0;
#else
    return _controlfp_s(0, _CW_DEFAULT, _MCW_EM | _MCW_DN | _MCW_PC | _MCW_RC | _MCW_IC);
#endif
#else //!_Win32
    return 0;
#endif
}

static errno_t GetFPUControl(unsigned int *pctrl)
{TRACE_IT(19453);
    Assert(pctrl != nullptr);
#if _WIN32
#if _M_IX86
    *pctrl = _control87(0, 0);
    return 0;
#else
    return _controlfp_s(pctrl, 0, 0);
#endif
#else //!_Win32
    return 0;
#endif
}

static errno_t SetFPUControl(unsigned int fpctrl)
{TRACE_IT(19454);
#if _WIN32
#if _M_AMD64 || _M_ARM
    return _controlfp_s(0, fpctrl, _MCW_EM | _MCW_DN | _MCW_RC);
#elif _M_IX86
    _control87(fpctrl, (unsigned int)(-1));
    return 0;
#else
    return _controlfp_s(0, fpctrl, (unsigned int)(-1));
#endif
#else //!_Win32
    return 0;
#endif
}

static void ClearFPUStatus(void)
{TRACE_IT(19455);
#if _WIN32
    // WinSE 187789
    // _clearfp gives up the thread's time slice, so clear only if flags are set
    if (_statusfp())
        _clearfp();
#endif
}

template <bool enabled>
SmartFPUControlT<enabled>::SmartFPUControlT()
{TRACE_IT(19456);
    if (enabled)
    {TRACE_IT(19457);
        m_oldFpuControl = INVALID_FPUCONTROL;
        ClearFPUStatus(); // Clear pending exception status first (blue 555235)
        m_err = GetFPUControl(&m_oldFpuControl);
        if (m_err == 0)
        {TRACE_IT(19458);
            m_err = SetFPUControlDefault();
        }
    }
#if DBG
    else
    {TRACE_IT(19459);
        m_oldFpuControl = INVALID_FPUCONTROL;
        m_err = GetFPUControl(&m_oldFpuControl);
        m_oldFpuControlForConsistencyCheck = m_oldFpuControl;
    }
#endif
}

template <bool enabled>
SmartFPUControlT<enabled>::~SmartFPUControlT()
{TRACE_IT(19460);
    if (enabled)
    {TRACE_IT(19461);
        RestoreFPUControl();
    }
#if DBG
    else
    {TRACE_IT(19462);
        uint currentFpuControl;
        m_err = GetFPUControl(&currentFpuControl);
        if (m_err == 0 && m_oldFpuControlForConsistencyCheck != INVALID_FPUCONTROL)
        {TRACE_IT(19463);
            Assert(m_oldFpuControlForConsistencyCheck == currentFpuControl);
        }
    }
#endif
}

template <bool enabled>
void
SmartFPUControlT<enabled>::RestoreFPUControl()
{TRACE_IT(19464);
    if (enabled)
    {TRACE_IT(19465);
        if (m_oldFpuControl != INVALID_FPUCONTROL)
        {TRACE_IT(19466);
            m_err = SetFPUControl(m_oldFpuControl);
            m_oldFpuControl = INVALID_FPUCONTROL; // Only restore once
        }
    }
    else
    {TRACE_IT(19467);
        // Shouldn't restore if this is not enabled
        Assert(false);
    }
}

// Explicit instantiation
template class SmartFPUControlT<true>;
template class SmartFPUControlT<false>;
