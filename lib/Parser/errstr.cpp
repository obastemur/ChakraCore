//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "ParserPch.h"
#include "errstr.h"

// scaffolding - get a g_hInstance from scrbgase.cpp
HANDLE g_hInstance;

// Used as a prefix to generate the resource dll name.
const CHAR_T g_wszPrefix[] = _u("js");

static BOOL FGetStringFromLibrary(HMODULE hlib, int istring, __out_ecount(cchMax) CHAR_T * psz, int cchMax)
{
    // NOTE - istring is expected to be HRESULT

    Assert(0 < cchMax);
    AssertArrMem(psz, cchMax);

    HGLOBAL hgl = NULL;
    CHAR_T * pchRes = NULL;
    HRSRC hrsrc;
    CHAR_T * pchCur;
    int cch;
    int cstring;
    DWORD cbRes;
    int itable = ((WORD)istring >> 4) + 1;
    istring &= 0x0F;
    BOOL fRet = FALSE;

#ifdef ENABLE_GLOBALIZATION
    psz[0] = '\0';

    if (NULL == hlib)
        goto LError;

    hrsrc = FindResourceEx((HMODULE)hlib, RT_STRING, MAKEINTRESOURCE(itable), 0);
    if (NULL == hrsrc)
        goto LError;

    hgl = LoadResource((HMODULE)hlib, hrsrc);
    if (NULL == hgl)
        goto LError;

    pchRes = (CHAR_T *)LockResource(hgl);
    if (NULL == pchRes)
        goto LError;

    cbRes = SizeofResource((HMODULE)hlib, hrsrc);

    if (cbRes < sizeof(WORD))
        goto LError;

    pchCur = pchRes;
    for (cstring = istring; cstring-- > 0;)
    {
        if (cbRes - sizeof(WORD) < sizeof(CHAR_T) * (pchCur - pchRes))
            goto LError;

        cch = (*(WORD *) pchCur) + 1;

        if (cch <= 0)
            goto LError;

        if (cbRes < sizeof(CHAR_T) * cch)
            goto LError;

        if (cbRes - sizeof(CHAR_T) * cch < sizeof(CHAR_T) * (pchCur - pchRes))
            goto LError;

        pchCur += cch;
    }

    if (cbRes - sizeof(WORD) < sizeof(CHAR_T) * (pchCur - pchRes))
        goto LError;
    cch = * (WORD *) pchCur;

    if (cch <= 0)
        goto LError;

    if (cbRes < sizeof(CHAR_T) * (cch + 1))
        goto LError;

    if (cbRes - sizeof(CHAR_T) * (cch + 1) < sizeof(CHAR_T) * (pchCur - pchRes))
        goto LError;

    if (cch > cchMax - 1)
        cch = cchMax - 1;

    js_memcpy_s(psz, cchMax * sizeof(CHAR_T), pchCur + 1, cch * sizeof(CHAR_T));
    psz[cch] = '\0';
    fRet = TRUE;

LError:

#if !_WIN32 && !_WIN64

    //
    // Unlock/FreeResource non-essential on win32/64.
    //
    if (NULL != hgl)
    {
        if (NULL != pchRes)
            UnlockResource(hgl);
        FreeResource(hgl);
    }

#endif
#endif // ENABLE_GLOBALIZATION
    return fRet;
}


BOOL FGetResourceString(int32 isz, __out_ecount(cchMax) OLECHAR *psz, int cchMax)
{
    return FGetStringFromLibrary((HINSTANCE)g_hInstance, isz, psz, cchMax);
}

// Get a bstr version of the error string
_NOINLINE // Don't inline. This function needs 2KB stack.
BSTR BstrGetResourceString(int32 isz)
{
    // NOTE - isz is expected to be HRESULT

#ifdef _WIN32
    OLECHAR szT[1024];

    if (!FGetResourceString(isz, szT,
        sizeof(szT) / sizeof(szT[0]) - 1))
    {
        return NULL;
    }
#else
    const CHAR_T* LoadResourceStr(UINT id);

    UINT id = (WORD)isz;
    const CHAR_T* szT = LoadResourceStr(id);
    if (!szT || !szT[0])
    {
        return NULL;
    }

#endif

    return SysAllocString(szT);
}
