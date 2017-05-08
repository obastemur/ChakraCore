//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#pragma once

namespace Wasm
{
    class WasmFunctionInfo
    {
    public:
        WasmFunctionInfo(ArenaAllocator* alloc, WasmSignature* signature, uint32 number);

        void AddLocal(WasmTypes::WasmType type, uint count = 1);
        Local GetLocal(uint index) const;
        Local GetParam(uint index) const;
        WasmTypes::WasmType GetResultType() const;

        uint32 GetLocalCount() const;
        uint32 GetParamCount() const;

        void SetName(const char16* name, uint32 nameLength) {TRACE_IT(68414); m_name = name; m_nameLength = nameLength; }
        const char16* GetName() const {TRACE_IT(68415); return m_name; }
        uint32 GetNameLength() const {TRACE_IT(68416); return m_nameLength; }

        uint32 GetNumber() const {TRACE_IT(68417); return m_number; }
        WasmSignature* GetSignature() const {TRACE_IT(68418); return m_signature; }

        void SetExitLabel(Js::ByteCodeLabel label) {TRACE_IT(68419); m_ExitLabel = label; }
        Js::ByteCodeLabel GetExitLabel() const {TRACE_IT(68420); return m_ExitLabel; }
        Js::FunctionBody* GetBody() const {TRACE_IT(68421); return m_body; }
        void SetBody(Js::FunctionBody* val) {TRACE_IT(68422); m_body = val; }

        WasmReaderBase* GetCustomReader() const {TRACE_IT(68423); return m_customReader; }
        void SetCustomReader(WasmReaderBase* customReader) {TRACE_IT(68424); m_customReader = customReader; }
#if DBG_DUMP
        FieldNoBarrier(WasmImport*) importedFunctionReference;
#endif

        Field(FunctionBodyReaderInfo) m_readerInfo;
    private:

        FieldNoBarrier(ArenaAllocator*) m_alloc;
        typedef JsUtil::GrowingArray<Local, ArenaAllocator> WasmTypeArray;
        Field(WasmTypeArray) m_locals;
        Field(Js::FunctionBody*) m_body;
        Field(WasmSignature*) m_signature;
        Field(Js::ByteCodeLabel) m_ExitLabel;
        Field(WasmReaderBase*) m_customReader;
        Field(const char16*) m_name;
        Field(uint32) m_nameLength;
        Field(uint32) m_number;
    };
} // namespace Wasm
