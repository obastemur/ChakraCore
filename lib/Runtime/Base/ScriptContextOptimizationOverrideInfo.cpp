//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeBasePch.h"

namespace Js
{
ScriptContextOptimizationOverrideInfo::ScriptContextOptimizationOverrideInfo()
    : sideEffects(SideEffects_None),
    arraySetElementFastPathVtable(VirtualTableInfo<Js::JavascriptArray>::Address),
    intArraySetElementFastPathVtable(VirtualTableInfo<Js::JavascriptNativeIntArray>::Address),
    floatArraySetElementFastPathVtable(VirtualTableInfo<Js::JavascriptNativeFloatArray>::Address),
    crossSiteRoot(nullptr), crossSitePrev(nullptr), crossSiteNext(nullptr)
{TRACE_IT(36799);
}

ScriptContextOptimizationOverrideInfo::~ScriptContextOptimizationOverrideInfo()
{TRACE_IT(36800);
    ScriptContextOptimizationOverrideInfo * next = crossSiteNext;
    if (next != nullptr)
    {TRACE_IT(36801);
        ScriptContextOptimizationOverrideInfo * root = crossSiteRoot;
        Assert(root != nullptr);
        if (this == root)
        {TRACE_IT(36802);
            // Change the root
            ForEachCrossSiteInfo([next](ScriptContextOptimizationOverrideInfo * info)
            {
                info->crossSiteRoot = next;
            });
        }
        ScriptContextOptimizationOverrideInfo * prev = crossSitePrev;
        Assert(prev != nullptr);
        next->crossSitePrev = prev;
        prev->crossSiteNext = next;
    }
}

template <typename Fn>
void ScriptContextOptimizationOverrideInfo::ForEachCrossSiteInfo(Fn fn)
{TRACE_IT(36803);
    Assert(crossSiteRoot != nullptr);
    ScriptContextOptimizationOverrideInfo * current = this;
    do
    {TRACE_IT(36804);
        fn(current);
        current = current->crossSiteNext;
    }
    while (current != this);

}

template <typename Fn>
void ScriptContextOptimizationOverrideInfo::ForEachEditingCrossSiteInfo(Fn fn)
{TRACE_IT(36805);
    Assert(crossSiteRoot != nullptr);
    ScriptContextOptimizationOverrideInfo * current = this;
    do
    {TRACE_IT(36806);
        ScriptContextOptimizationOverrideInfo * next = current->crossSiteNext;
        fn(current);
        current = next;
    }
    while (current != this);
}

void
ScriptContextOptimizationOverrideInfo::Merge(ScriptContextOptimizationOverrideInfo * info)
{TRACE_IT(36807);
    ScriptContextOptimizationOverrideInfo * thisRoot = this->crossSiteRoot;
    ScriptContextOptimizationOverrideInfo * infoRoot = info->crossSiteRoot;
    if (thisRoot == infoRoot)
    {TRACE_IT(36808);
        if (thisRoot != nullptr)
        {TRACE_IT(36809);
            // Both info is already in the same info group
            return;
        }

        // Both of them are null, just group them

        // Update this to be the template
        this->Update(info);

        // Initialize the cross site list
        this->crossSiteRoot = this;
        this->crossSitePrev = this;
        this->crossSiteNext = this;

        // Insert the info to the list
        this->Insert(info);
    }
    else
    {TRACE_IT(36810);
        if (thisRoot == nullptr)
        {TRACE_IT(36811);
            thisRoot = infoRoot;
            infoRoot = nullptr;
            info = this;
        }

        thisRoot->Update(info);

        // Spread the information on the current group
        thisRoot->ForEachCrossSiteInfo([thisRoot](ScriptContextOptimizationOverrideInfo * i)
        {
            thisRoot->CopyTo(i);
        });

        if (infoRoot == nullptr)
        {TRACE_IT(36812);
            thisRoot->Insert(info);
        }
        else
        {TRACE_IT(36813);
            // Insert the other group
            info->ForEachEditingCrossSiteInfo([thisRoot](ScriptContextOptimizationOverrideInfo * i)
            {
                thisRoot->Insert(i);
            });
        }
    }

    DebugOnly(Verify());
}

void
ScriptContextOptimizationOverrideInfo::CopyTo(ScriptContextOptimizationOverrideInfo * info)
{TRACE_IT(36814);
    info->arraySetElementFastPathVtable = this->arraySetElementFastPathVtable;
    info->intArraySetElementFastPathVtable = this->intArraySetElementFastPathVtable;
    info->floatArraySetElementFastPathVtable = this->floatArraySetElementFastPathVtable;
    info->sideEffects = this->sideEffects;
}

void
ScriptContextOptimizationOverrideInfo::Insert(ScriptContextOptimizationOverrideInfo * info)
{TRACE_IT(36815);
    // Copy the information
    this->CopyTo(info);

    // Insert
    // Only insert at the root
    Assert(this == this->crossSiteRoot);
    info->crossSiteRoot = this;
    info->crossSiteNext = this;
    info->crossSitePrev = this->crossSitePrev;
    this->crossSitePrev->crossSiteNext = info;
    this->crossSitePrev = info;
}

void
ScriptContextOptimizationOverrideInfo::Update(ScriptContextOptimizationOverrideInfo * info)
{TRACE_IT(36816);
    if (!info->IsEnabledArraySetElementFastPath())
    {TRACE_IT(36817);
        this->DisableArraySetElementFastPath();
    }

    this->sideEffects = (SideEffects)(this->sideEffects | info->sideEffects);
}

void
ScriptContextOptimizationOverrideInfo::SetSideEffects(SideEffects se)
{TRACE_IT(36818);
    if (this->crossSiteRoot == nullptr)
    {TRACE_IT(36819);
        sideEffects = (SideEffects)(sideEffects | se);
    }
    else if ((sideEffects & se) != se)
    {TRACE_IT(36820);
        ForEachCrossSiteInfo([se](ScriptContextOptimizationOverrideInfo * info)
        {
            Assert((info->sideEffects & se) != se);
            info->sideEffects = (SideEffects)(info->sideEffects | se);
        });
    }
}
bool
ScriptContextOptimizationOverrideInfo::IsEnabledArraySetElementFastPath() const
{TRACE_IT(36821);
    return arraySetElementFastPathVtable != InvalidVtable;
}

void
ScriptContextOptimizationOverrideInfo::DisableArraySetElementFastPath()
{TRACE_IT(36822);
    if (this->crossSiteRoot == nullptr)
    {TRACE_IT(36823);
        arraySetElementFastPathVtable = InvalidVtable;
        intArraySetElementFastPathVtable = InvalidVtable;
        floatArraySetElementFastPathVtable = InvalidVtable;
    }
    else if (IsEnabledArraySetElementFastPath())
    {TRACE_IT(36824);
        // disable for all script context in the cross site group
        ForEachCrossSiteInfo([](ScriptContextOptimizationOverrideInfo * info)
        {
            Assert(info->IsEnabledArraySetElementFastPath());
            info->arraySetElementFastPathVtable = InvalidVtable;
            info->intArraySetElementFastPathVtable = InvalidVtable;
            info->floatArraySetElementFastPathVtable = InvalidVtable;
        });
    }
}

INT_PTR
ScriptContextOptimizationOverrideInfo::GetArraySetElementFastPathVtable() const
{TRACE_IT(36825);
    return arraySetElementFastPathVtable;
}

INT_PTR
ScriptContextOptimizationOverrideInfo::GetArraySetElementFastPathVtableAddr() const
{TRACE_IT(36826);
    return (INT_PTR)&arraySetElementFastPathVtable;
}

INT_PTR
ScriptContextOptimizationOverrideInfo::GetIntArraySetElementFastPathVtableAddr() const
{TRACE_IT(36827);
    return (INT_PTR)&intArraySetElementFastPathVtable;
}

INT_PTR
ScriptContextOptimizationOverrideInfo::GetFloatArraySetElementFastPathVtableAddr() const
{TRACE_IT(36828);
    return (INT_PTR)&floatArraySetElementFastPathVtable;
}

void *
ScriptContextOptimizationOverrideInfo::GetAddressOfArraySetElementFastPathVtable()
{TRACE_IT(36829);
    return &arraySetElementFastPathVtable;
}

void *
ScriptContextOptimizationOverrideInfo::GetAddressOfIntArraySetElementFastPathVtable()
{TRACE_IT(36830);
    return &intArraySetElementFastPathVtable;
}

void *
ScriptContextOptimizationOverrideInfo::GetAddressOfFloatArraySetElementFastPathVtable()
{TRACE_IT(36831);
    return &floatArraySetElementFastPathVtable;
}

#if DBG
void
ScriptContextOptimizationOverrideInfo::Verify()
{TRACE_IT(36832);
    if (this->crossSiteRoot == nullptr)
    {TRACE_IT(36833);
        return;
    }

    this->ForEachCrossSiteInfo([this](ScriptContextOptimizationOverrideInfo * i)
    {
        Assert(i->crossSiteRoot == this->crossSiteRoot);
        Assert(i->sideEffects == this->sideEffects);
        Assert(i->arraySetElementFastPathVtable == this->arraySetElementFastPathVtable);
        Assert(i->intArraySetElementFastPathVtable == this->intArraySetElementFastPathVtable);
        Assert(i->floatArraySetElementFastPathVtable == this->floatArraySetElementFastPathVtable);
    });
}
#endif
};
