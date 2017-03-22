//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    class ScriptFunctionType : public DynamicType
    {
    public:
        static ScriptFunctionType * New(FunctionProxy * proxy, bool isShared);
        static DWORD GetEntryPointInfoOffset() {LOGMEIN("ScriptFunctionType.h] 12\n"); return offsetof(ScriptFunctionType, entryPointInfo); }
        ProxyEntryPointInfo * GetEntryPointInfo() const {LOGMEIN("ScriptFunctionType.h] 13\n"); return entryPointInfo; }
        void SetEntryPointInfo(ProxyEntryPointInfo * entryPointInfo) {LOGMEIN("ScriptFunctionType.h] 14\n"); this->entryPointInfo = entryPointInfo; }
    private:
        ScriptFunctionType(ScriptFunctionType * type);
        ScriptFunctionType(ScriptContext* scriptContext, RecyclableObject* prototype,
            JavascriptMethod entryPoint, ProxyEntryPointInfo * entryPointInfo, DynamicTypeHandler * typeHandler,
            bool isLocked, bool isShared);

        Field(ProxyEntryPointInfo *) entryPointInfo;

        friend class ScriptFunction;
        friend class JavascriptLibrary;
    };
};
