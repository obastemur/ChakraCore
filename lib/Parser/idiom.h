//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#ifndef __IDIOM_H__
#define __IDIOM_H__

// cleanup if needed, and set to (null)

#ifndef DELETEARR
#define DELETEARR(arr) do {TRACE_IT(32995);if (arr){TRACE_IT(32996); delete [] (arr); (arr) = NULL; }} while (0)
#endif

#ifndef DELETEPTR
#define DELETEPTR(p) do {TRACE_IT(32997);if (p){TRACE_IT(32998); delete (p); (p) = NULL; }} while (0)
#endif

#ifndef FREEPTR
#define FREEPTR(p) do {TRACE_IT(32999);if (p){TRACE_IT(33000); free(p); (p) = NULL; }} while (0)
#endif

#ifndef SYSFREE
#define SYSFREE(p) do {TRACE_IT(33001);if (p){TRACE_IT(33002); ::SysFreeString(p); (p) = NULL; }} while (0)
#endif

#ifndef RELEASEPTR
#define RELEASEPTR(p) do {TRACE_IT(33003);if (p){TRACE_IT(33004); (p)->Release(); (p) = NULL; }} while (0)
#endif

#ifndef UNADVISERELEASE
#define UNADVISERELEASE(p, dwCookie) do {TRACE_IT(33005);if (p){TRACE_IT(33006); (p)->Unadvise(dwCookie); (p)->Release(); (p) = NULL; }} while (0)
#endif

#ifndef RELEASETYPEINFOATTR
#define RELEASETYPEINFOATTR(pinfo, pattr) do {TRACE_IT(33007); if (NULL != (pinfo)) {TRACE_IT(33008); if (NULL != (pattr)) {TRACE_IT(33009); (pinfo)->ReleaseTypeAttr(pattr); (pattr) = NULL; } (pinfo)->Release(); (pinfo) = NULL; } } while (0)
#endif

#ifndef REGCLOSE
#define REGCLOSE(hkey) do {TRACE_IT(33010);if (NULL != (hkey)){TRACE_IT(33011); RegCloseKey(hkey); (hkey) = NULL; }} while (0)
#endif

#ifndef CLOSEPTR
#define CLOSEPTR(p) do {TRACE_IT(33012);if (NULL != (p)) {TRACE_IT(33013); (p)->Close(); (p) = 0; }} while (0)
#endif
// check result, cleanup if failed

#ifndef IFNULLMEMGOLABEL
#define IFNULLMEMGOLABEL(p, label) do {TRACE_IT(33014);if (NULL == (p)){TRACE_IT(33015); hr = E_OUTOFMEMORY; goto label; }} while (0)
#endif

#ifndef IFNULLMEMGO
#define IFNULLMEMGO(p) IFNULLMEMGOLABEL(p, LReturn)
#endif

#ifndef IFNULLMEMRET
#define IFNULLMEMRET(p) do {TRACE_IT(33016);if (!(p)) return E_OUTOFMEMORY; } while (0)
#endif

#ifndef IFFAILGOLABEL
#define IFFAILGOLABEL(expr, label) do {TRACE_IT(33017);if (FAILED(hr = (expr))) goto label; } while (0)
#endif

#ifndef IFFAILGO
#define IFFAILGO(expr) IFFAILGOLABEL(expr, LReturn)
#endif

// If (expr) failed, go to LReturn with (code)
#ifndef IFFAILGORET
#define IFFAILGORET(expr, code) do {TRACE_IT(33018);if (FAILED(hr = (expr))) {TRACE_IT(33019); hr = (code); goto LReturn; }} while (0)
#endif

#ifndef FAILGO
#define FAILGO(hresult) do {TRACE_IT(33020); hr = (hresult); goto LReturn; } while (0)
#endif

#ifndef IFFAILWINERRGO
#define IFFAILWINERRGO(expr) do {TRACE_IT(33021); if (FAILED(hr = HRESULT_FROM_WIN32(expr))) goto LReturn; } while (0)
#endif

#ifndef FAILWINERRGO
#define FAILWINERRGO(expr) do {TRACE_IT(33022); hr = HRESULT_FROM_WIN32(expr); goto LReturn; } while (0)
#endif

#ifndef IFFAILRET
#define IFFAILRET(expr) do {TRACE_IT(33023);if (FAILED(hr = (expr))) return hr; } while (0)
#endif

#ifndef IFFAILLEAVE
#define IFFAILLEAVE(expr) do {TRACE_IT(33024);if (FAILED(hr = (expr))) __leave; } while (0)
#endif

#ifndef FAILLEAVE
#define FAILLEAVE(expr) do {TRACE_IT(33025); hr = (expr); __leave; } while (0)
#endif

// set optional return value

#ifndef SETRETVAL
#define SETRETVAL(ptr, val) do {TRACE_IT(33026); if (ptr) *(ptr) = (val); } while (0)
#endif

#ifndef CHECK_POINTER
#define CHECK_POINTER(p) do {TRACE_IT(33027); if (NULL == (p)) return E_POINTER; } while (0)
#endif

#ifndef EXPECT_POINTER
#define EXPECT_POINTER(p) do {TRACE_IT(33028); if (NULL == (p)) return E_UNEXPECTED; } while (0)
#endif

#ifndef ARG_POINTER
#define ARG_POINTER(p) do {TRACE_IT(33029); if (NULL == (p)) return E_INVALIDARG; } while (0)
#endif

#endif // __IDIOM_H__
