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
        static DWORD GetEntryPointInfoOffset() {TRACE_IT(67008); return offsetof(ScriptFunctionType, entryPointInfo); }
        ProxyEntryPointInfo * GetEntryPointInfo() const {TRACE_IT(67009); return entryPointInfo; }
        void SetEntryPointInfo(ProxyEntryPointInfo * entryPointInfo) {TRACE_IT(67010); this->entryPointInfo = entryPointInfo; }
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
