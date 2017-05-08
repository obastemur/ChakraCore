//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{

class ScriptContextOptimizationOverrideInfo
{
public:
    ScriptContextOptimizationOverrideInfo();
    ~ScriptContextOptimizationOverrideInfo();

    static DWORD GetSideEffectsOffset() {TRACE_IT(36834); return offsetof(ScriptContextOptimizationOverrideInfo, sideEffects); }
    static DWORD GetArraySetElementFastPathVtableOffset() {TRACE_IT(36835); return offsetof(ScriptContextOptimizationOverrideInfo, arraySetElementFastPathVtable); }
    static DWORD GetIntArraySetElementFastPathVtableOffset() {TRACE_IT(36836); return offsetof(ScriptContextOptimizationOverrideInfo, intArraySetElementFastPathVtable); }
    static DWORD GetFloatArraySetElementFastPathVtableOffset() {TRACE_IT(36837); return offsetof(ScriptContextOptimizationOverrideInfo, floatArraySetElementFastPathVtable); }

    void SetSideEffects(SideEffects se);
    SideEffects GetSideEffects() {TRACE_IT(36838); return sideEffects; }
    intptr_t GetAddressOfSideEffects() const {TRACE_IT(36839); return (intptr_t)&sideEffects; }

    bool IsEnabledArraySetElementFastPath() const;
    void DisableArraySetElementFastPath();
    INT_PTR GetArraySetElementFastPathVtable() const;
    INT_PTR GetArraySetElementFastPathVtableAddr() const;
    INT_PTR GetIntArraySetElementFastPathVtableAddr() const;
    INT_PTR GetFloatArraySetElementFastPathVtableAddr() const;
    void * GetAddressOfArraySetElementFastPathVtable();
    void * GetAddressOfIntArraySetElementFastPathVtable();
    void * GetAddressOfFloatArraySetElementFastPathVtable();

    void Merge(ScriptContextOptimizationOverrideInfo * info);

    // Use a small integer so JIT'ed code can encode in a smaller instruction
    static const INT_PTR InvalidVtable = (INT_PTR)1;
private:
    // Optimization overrides
    SideEffects sideEffects;
    INT_PTR arraySetElementFastPathVtable;
    INT_PTR intArraySetElementFastPathVtable;
    INT_PTR floatArraySetElementFastPathVtable;

    // Cross site tracking
    ScriptContextOptimizationOverrideInfo * crossSiteRoot;
    ScriptContextOptimizationOverrideInfo * crossSitePrev;
    ScriptContextOptimizationOverrideInfo * crossSiteNext;

    template <typename Fn>
    void ForEachCrossSiteInfo(Fn fn);
    template <typename Fn>
    void ForEachEditingCrossSiteInfo(Fn fn);
    void Update(ScriptContextOptimizationOverrideInfo * info);
    void CopyTo(ScriptContextOptimizationOverrideInfo * info);
    void Insert(ScriptContextOptimizationOverrideInfo * info);

#if DBG
    void Verify();
#endif
};

};
