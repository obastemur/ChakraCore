//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once
template <class Fn, bool mapRest>
void MapFormalsImpl(ParseNode *pnodeFunc, Fn fn)
{TRACE_IT(29187);
    for (ParseNode *pnode = pnodeFunc->sxFnc.pnodeParams; pnode != nullptr; pnode = pnode->GetFormalNext())
    {TRACE_IT(29188);
        fn(pnode);
    }
    if (mapRest && pnodeFunc->sxFnc.pnodeRest != nullptr)
    {TRACE_IT(29189);
        fn(pnodeFunc->sxFnc.pnodeRest);
    }
}

template <class Fn>
void MapFormalsWithoutRest(ParseNode *pnodeFunc, Fn fn)
{TRACE_IT(29190);
    return MapFormalsImpl<Fn, false>(pnodeFunc, fn);
}

template <class Fn>
void MapFormals(ParseNode *pnodeFunc, Fn fn)
{TRACE_IT(29191);
    return MapFormalsImpl<Fn, true>(pnodeFunc, fn);
}

template <class Fn>
void MapFormalsFromPattern(ParseNode *pnodeFunc, Fn fn)
{TRACE_IT(29192);
    for (ParseNode *pnode = pnodeFunc->sxFnc.pnodeParams; pnode != nullptr; pnode = pnode->GetFormalNext())
    {TRACE_IT(29193);
        if (pnode->nop == knopParamPattern)
        {TRACE_IT(29194);
            Parser::MapBindIdentifier(pnode->sxParamPattern.pnode1, fn);
        }
    }
}

