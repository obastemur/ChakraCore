//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "WasmReaderPch.h"

#ifdef ENABLE_WASM
namespace Wasm
{
    SectionInfo::SectionInfo(
        SectionFlag flag,
        SectionCode precedent,
        const CHAR_T* name,
        const uint32 nameLength
    ): flag(flag), precedent(precedent), name(name), nameLength(nameLength) {}

    SectionInfo SectionInfo::All[bSectLimit] = {
#define WASM_SECTION(_, id, flag, precedent) {flag, bSect ## precedent, static_cast<const CHAR_T*>(_u(id)), sizeof(id)},
#include "WasmSections.h"
    };
}
#endif
