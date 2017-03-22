//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#pragma once

#include "../WasmReader/WasmParseTree.h"

namespace Wasm
{
    class WasmSignature;
    class WasmFunctionInfo;
    class WasmBinaryReader;
    class WasmDataSegment;
    class WasmElementSegment;
    class WasmGlobal;
    struct WasmImport;
    struct WasmExport;
    struct CustomSection;
}

namespace Js
{
class WebAssemblyModule : public DynamicObject
{
public:

    class EntryInfo
    {
    public:
        static FunctionInfo NewInstance;
        static FunctionInfo Exports;
        static FunctionInfo Imports;
        static FunctionInfo CustomSections;
    };

    static Var NewInstance(RecyclableObject* function, CallInfo callInfo, ...);
    static Var EntryExports(RecyclableObject* function, CallInfo callInfo, ...);
    static Var EntryImports(RecyclableObject* function, CallInfo callInfo, ...);
    static Var EntryCustomSections(RecyclableObject* function, CallInfo callInfo, ...);

    static bool Is(Var aValue);
    static WebAssemblyModule * FromVar(Var aValue);

    static WebAssemblyModule * CreateModule(
        ScriptContext* scriptContext,
        const byte* buffer,
        const uint lengthBytes);

    static bool ValidateModule(
        ScriptContext* scriptContext,
        const byte* buffer,
        const uint lengthBytes);

public:
    WebAssemblyModule(Js::ScriptContext* scriptContext, const byte* binaryBuffer, uint binaryBufferLength, DynamicType * type);

    // The index used by those methods is the function index as describe by the WebAssembly design, ie: imports first then wasm functions
    uint32 GetMaxFunctionIndex() const;
    Wasm::FunctionIndexTypes::Type GetFunctionIndexType(uint32 funcIndex) const;

    void InitializeMemory(uint32 minSize, uint32 maxSize);
    WebAssemblyMemory * CreateMemory() const;
    bool HasMemory() const {LOGMEIN("WebAssemblyModule.h] 64\n"); return m_hasMemory; }
    bool HasMemoryImport() const {LOGMEIN("WebAssemblyModule.h] 65\n"); return m_memImport != nullptr; }
    uint32 GetMemoryInitSize() const {LOGMEIN("WebAssemblyModule.h] 66\n"); return m_memoryInitSize; }
    uint32 GetMemoryMaxSize() const {LOGMEIN("WebAssemblyModule.h] 67\n"); return m_memoryMaxSize; }

    Wasm::WasmSignature * GetSignatures() const;
    Wasm::WasmSignature* GetSignature(uint32 index) const;
    void SetSignatureCount(uint32 count);
    uint32 GetSignatureCount() const;

    uint32 GetEquivalentSignatureId(uint32 sigId) const;

    void InitializeTable(uint32 minEntries, uint32 maxEntries);
    WebAssemblyTable * CreateTable() const;
    bool HasTable() const {LOGMEIN("WebAssemblyModule.h] 78\n"); return m_hasTable; }
    bool HasTableImport() const {LOGMEIN("WebAssemblyModule.h] 79\n"); return m_tableImport != nullptr; }
    bool IsValidTableImport(const WebAssemblyTable * table) const;

    uint GetWasmFunctionCount() const;
    Wasm::WasmFunctionInfo* AddWasmFunctionInfo(Wasm::WasmSignature* funsig);
    Wasm::WasmFunctionInfo* GetWasmFunctionInfo(uint index) const;

    void AllocateFunctionExports(uint32 entries);
    uint GetExportCount() const {LOGMEIN("WebAssemblyModule.h] 87\n"); return m_exportCount; }
    void SetExport(uint32 iExport, uint32 funcIndex, const char16* exportName, uint32 nameLength, Wasm::ExternalKinds::ExternalKind kind);
    Wasm::WasmExport* GetExport(uint32 iExport) const;

    uint32 GetImportCount() const;
    Wasm::WasmImport* GetImport(uint32 i) const;
    void AddFunctionImport(uint32 sigId, const char16* modName, uint32 modNameLen, const char16* fnName, uint32 fnNameLen);
    void AddGlobalImport(const char16* modName, uint32 modNameLen, const char16* importName, uint32 importNameLen);
    void AddMemoryImport(const char16* modName, uint32 modNameLen, const char16* importName, uint32 importNameLen);
    void AddTableImport(const char16* modName, uint32 modNameLen, const char16* importName, uint32 importNameLen);
    Wasm::WasmImport * GetMemoryImport() const {LOGMEIN("WebAssemblyModule.h] 97\n"); return m_memImport; }
    Wasm::WasmImport * GetTableImport() const {LOGMEIN("WebAssemblyModule.h] 98\n"); return m_tableImport; }
    uint32 GetImportedFunctionCount() const {LOGMEIN("WebAssemblyModule.h] 99\n"); return m_importedFunctionCount; }

    uint GetOffsetFromInit(const Wasm::WasmNode& initExpr, const class WebAssemblyEnvironment* env) const;
    void ValidateInitExportForOffset(const Wasm::WasmNode& initExpr) const;

    void AddGlobal(Wasm::GlobalReferenceTypes::Type refType, Wasm::WasmTypes::WasmType type, bool isMutable, Wasm::WasmNode init);
    uint32 GetGlobalCount() const;
    Wasm::WasmGlobal* GetGlobal(uint32 index) const;

    void AllocateDataSegs(uint32 count);
    void SetDataSeg(Wasm::WasmDataSegment* seg, uint32 index);
    Wasm::WasmDataSegment* GetDataSeg(uint32 index) const;
    uint32 GetDataSegCount() const {LOGMEIN("WebAssemblyModule.h] 111\n"); return m_datasegCount; }

    void AllocateElementSegs(uint32 count);
    void SetElementSeg(Wasm::WasmElementSegment* seg, uint32 index);
    Wasm::WasmElementSegment* GetElementSeg(uint32 index) const;
    uint32 GetElementSegCount() const {LOGMEIN("WebAssemblyModule.h] 116\n"); return m_elementsegCount; }

    void SetStartFunction(uint32 i);
    uint32 GetStartFunction() const;

    uint32 GetModuleEnvironmentSize() const;

    // elements at known offsets
    static uint GetMemoryOffset() {LOGMEIN("WebAssemblyModule.h] 124\n"); return 0; }
    static uint GetImportFuncOffset() {LOGMEIN("WebAssemblyModule.h] 125\n"); return GetMemoryOffset() + 1; }

    // elements at instance dependent offsets
    uint GetFuncOffset() const {LOGMEIN("WebAssemblyModule.h] 128\n"); return GetImportFuncOffset() + GetImportedFunctionCount(); }
    uint GetTableEnvironmentOffset() const {LOGMEIN("WebAssemblyModule.h] 129\n"); return GetFuncOffset() + GetWasmFunctionCount(); }
    uint GetGlobalOffset() const {LOGMEIN("WebAssemblyModule.h] 130\n"); return GetTableEnvironmentOffset() + 1; }
    uint GetOffsetForGlobal(Wasm::WasmGlobal* global) const;
    uint AddGlobalByteSizeToOffset(Wasm::WasmTypes::WasmType type, uint32 offset) const;
    uint GetGlobalsByteSize() const;

    void AddCustomSection(Wasm::CustomSection customSection);
    uint32 GetCustomSectionCount() const;
    Wasm::CustomSection GetCustomSection(uint32 index) const;

    Wasm::WasmBinaryReader* GetReader() const {LOGMEIN("WebAssemblyModule.h] 139\n"); return m_reader; }

    virtual void Finalize(bool isShutdown) override;
    virtual void Dispose(bool isShutdown) override;
    virtual void Mark(Recycler * recycler) override;

private:
    static JavascriptString * GetExternalKindString(ScriptContext * scriptContext, Wasm::ExternalKinds::ExternalKind kind);

    Field(bool) m_hasTable;
    Field(bool) m_hasMemory;
    // The binary buffer is recycler allocated, tied the lifetime of the buffer to the module
    Field(const byte*) m_binaryBuffer;
    Field(uint32) m_memoryInitSize;
    Field(uint32) m_memoryMaxSize;
    Field(uint32) m_tableInitSize;
    Field(uint32) m_tableMaxSize;
    Field(Wasm::WasmSignature*) m_signatures;
    Field(uint32*) m_indirectfuncs;
    Field(Wasm::WasmElementSegment**) m_elementsegs;
    typedef JsUtil::List<Wasm::WasmFunctionInfo*, Recycler> WasmFunctionInfosList;
    Field(WasmFunctionInfosList*) m_functionsInfo;
    Field(Wasm::WasmExport*) m_exports;
    typedef JsUtil::List<Wasm::WasmImport*, ArenaAllocator> WasmImportsList;
    Field(WasmImportsList*) m_imports;
    Field(Wasm::WasmImport*) m_memImport;
    Field(Wasm::WasmImport*) m_tableImport;
    Field(uint32) m_importedFunctionCount;
    Field(Wasm::WasmDataSegment**) m_datasegs;
    Field(Wasm::WasmBinaryReader*) m_reader;
    Field(uint32*) m_equivalentSignatureMap;
    typedef JsUtil::List<Wasm::CustomSection, ArenaAllocator> CustomSectionsList;
    Field(CustomSectionsList*) m_customSections;

    Field(uint) m_globalCounts[Wasm::WasmTypes::Limit];
    typedef JsUtil::List<Wasm::WasmGlobal*, ArenaAllocator> WasmGlobalsList;
    Field(WasmGlobalsList *) m_globals;

    Field(uint) m_signaturesCount;
    Field(uint) m_exportCount;
    Field(uint32) m_datasegCount;
    Field(uint32) m_elementsegCount;

    Field(uint32) m_startFuncIndex;

    FieldNoBarrier(ArenaAllocator) m_alloc;
};

} // namespace Js
