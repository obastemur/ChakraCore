//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    class SourceTextModuleRecord;
    typedef JsUtil::BaseDictionary<LPCOLESTR, SourceTextModuleRecord*, ArenaAllocator, PowerOf2SizePolicy> ChildModuleRecordSet;
    typedef JsUtil::BaseDictionary<SourceTextModuleRecord*, SourceTextModuleRecord*, ArenaAllocator, PowerOf2SizePolicy> ParentModuleRecordSet;
    typedef JsUtil::BaseDictionary<PropertyId, uint, ArenaAllocator, PowerOf2SizePolicy> LocalExportMap;
    typedef JsUtil::BaseDictionary<PropertyId, ModuleNameRecord, ArenaAllocator, PowerOf2SizePolicy> ResolvedExportMap;
    typedef JsUtil::List<PropertyId, ArenaAllocator> LocalExportIndexList;

    class SourceTextModuleRecord : public ModuleRecordBase
    {
    public:
        friend class ModuleNamespace;

        SourceTextModuleRecord(ScriptContext* scriptContext);
        IdentPtrList* GetRequestedModuleList() const {LOGMEIN("SourceTextModuleRecord.h] 21\n"); return requestedModuleList; }
        ModuleImportOrExportEntryList* GetImportEntryList() const {LOGMEIN("SourceTextModuleRecord.h] 22\n"); return importRecordList; }
        ModuleImportOrExportEntryList* GetLocalExportEntryList() const {LOGMEIN("SourceTextModuleRecord.h] 23\n"); return localExportRecordList; }
        ModuleImportOrExportEntryList* GetIndirectExportEntryList() const {LOGMEIN("SourceTextModuleRecord.h] 24\n"); return indirectExportRecordList; }
        ModuleImportOrExportEntryList* GetStarExportRecordList() const {LOGMEIN("SourceTextModuleRecord.h] 25\n"); return starExportRecordList; }
        virtual ExportedNames* GetExportedNames(ExportModuleRecordList* exportStarSet) override;
        virtual bool IsSourceTextModuleRecord() override { return true; } // we don't really have other kind of modulerecord at this time.

        // return false when "ambiguous". 
        // otherwise nullptr means "null" where we have circular reference/cannot resolve.
        bool ResolveExport(PropertyId exportName, ResolveSet* resolveSet, ExportModuleRecordList* exportStarSet, ModuleNameRecord** exportRecord) override;
        bool ResolveImport(PropertyId localName, ModuleNameRecord** importRecord);
        void ModuleDeclarationInstantiation() override;
        Var ModuleEvaluation() override;
        virtual ModuleNamespace* GetNamespace();
        virtual void SetNamespace(ModuleNamespace* moduleNamespace);

        void Finalize(bool isShutdown) override;
        void Dispose(bool isShutdown) override { return; }
        void Mark(Recycler * recycler) override { return; }

        HRESULT ResolveExternalModuleDependencies();

        void* GetHostDefined() const {LOGMEIN("SourceTextModuleRecord.h] 44\n"); return hostDefined; }
        void SetHostDefined(void* hostObj) {LOGMEIN("SourceTextModuleRecord.h] 45\n"); hostDefined = hostObj; }

        void SetSpecifier(Var specifier) {LOGMEIN("SourceTextModuleRecord.h] 47\n"); this->normalizedSpecifier = specifier; }
        Var GetSpecifier() const {LOGMEIN("SourceTextModuleRecord.h] 48\n"); return normalizedSpecifier; }

        Var GetErrorObject() const {LOGMEIN("SourceTextModuleRecord.h] 50\n"); return errorObject; }

        bool WasParsed() const {LOGMEIN("SourceTextModuleRecord.h] 52\n"); return wasParsed; }
        void SetWasParsed() {LOGMEIN("SourceTextModuleRecord.h] 53\n"); wasParsed = true; }
        bool WasDeclarationInitialized() const {LOGMEIN("SourceTextModuleRecord.h] 54\n"); return wasDeclarationInitialized; }
        void SetWasDeclarationInitialized() {LOGMEIN("SourceTextModuleRecord.h] 55\n"); wasDeclarationInitialized = true; }
        void SetIsRootModule() {LOGMEIN("SourceTextModuleRecord.h] 56\n"); isRootModule = true; }

        void SetImportRecordList(ModuleImportOrExportEntryList* importList) {LOGMEIN("SourceTextModuleRecord.h] 58\n"); importRecordList = importList; }
        void SetLocalExportRecordList(ModuleImportOrExportEntryList* localExports) {LOGMEIN("SourceTextModuleRecord.h] 59\n"); localExportRecordList = localExports; }
        void SetIndirectExportRecordList(ModuleImportOrExportEntryList* indirectExports) {LOGMEIN("SourceTextModuleRecord.h] 60\n"); indirectExportRecordList = indirectExports; }
        void SetStarExportRecordList(ModuleImportOrExportEntryList* starExports) {LOGMEIN("SourceTextModuleRecord.h] 61\n"); starExportRecordList = starExports; }
        void SetrequestedModuleList(IdentPtrList* requestModules) {LOGMEIN("SourceTextModuleRecord.h] 62\n"); requestedModuleList = requestModules; }

        ScriptContext* GetScriptContext() const {LOGMEIN("SourceTextModuleRecord.h] 64\n"); return scriptContext; }
        HRESULT ParseSource(__in_bcount(sourceLength) byte* sourceText, uint32 sourceLength, SRCINFO * srcInfo, Var* exceptionVar, bool isUtf8);
        HRESULT OnHostException(void* errorVar);

        static SourceTextModuleRecord* FromHost(void* hostModuleRecord)
        {LOGMEIN("SourceTextModuleRecord.h] 69\n");
            SourceTextModuleRecord* moduleRecord = static_cast<SourceTextModuleRecord*>(hostModuleRecord);
            Assert((moduleRecord == nullptr) || (moduleRecord->magicNumber == moduleRecord->ModuleMagicNumber));
            return moduleRecord;
        }

        static bool Is(void* hostModuleRecord)
        {LOGMEIN("SourceTextModuleRecord.h] 76\n");
            SourceTextModuleRecord* moduleRecord = static_cast<SourceTextModuleRecord*>(hostModuleRecord);
            if (moduleRecord != nullptr && (moduleRecord->magicNumber == moduleRecord->ModuleMagicNumber))
            {LOGMEIN("SourceTextModuleRecord.h] 79\n");
                return true;
            }
            return false;
        }

        static SourceTextModuleRecord* Create(ScriptContext* scriptContext);

        uint GetLocalExportSlotIndexByExportName(PropertyId exportNameId);
        uint GetLocalExportSlotIndexByLocalName(PropertyId localNameId);
        Field(Var)* GetLocalExportSlots() const {LOGMEIN("SourceTextModuleRecord.h] 89\n"); return localExportSlots; }
        uint GetLocalExportSlotCount() const {LOGMEIN("SourceTextModuleRecord.h] 90\n"); return localSlotCount; }
        uint GetModuleId() const {LOGMEIN("SourceTextModuleRecord.h] 91\n"); return moduleId; }
        uint GetLocalExportCount() const {LOGMEIN("SourceTextModuleRecord.h] 92\n"); return localExportCount; }

        ModuleNameRecord* GetNamespaceNameRecord() {LOGMEIN("SourceTextModuleRecord.h] 94\n"); return &namespaceRecord; }

        SourceTextModuleRecord* GetChildModuleRecord(LPCOLESTR specifier) const;

        void SetParent(SourceTextModuleRecord* parentRecord, LPCOLESTR moduleName);
        Utf8SourceInfo* GetSourceInfo() {LOGMEIN("SourceTextModuleRecord.h] 99\n"); return this->pSourceInfo; }

    private:
        const static uint InvalidModuleIndex = 0xffffffff;
        const static uint InvalidSlotCount = 0xffffffff;
        const static uint InvalidSlotIndex = 0xffffffff;
        // TODO: move non-GC fields out to avoid false reference?
        // This is the parsed tree resulted from compilation. 
        Field(bool) wasParsed;
        Field(bool) wasDeclarationInitialized;
        Field(bool) isRootModule;
        Field(bool) hadNotifyHostReady;
        Field(ParseNodePtr) parseTree;
        Field(Utf8SourceInfo*) pSourceInfo;
        Field(uint) sourceIndex;
        FieldNoBarrier(Parser*) parser;  // we'll need to keep the parser around till we are done with bytecode gen.
        Field(ScriptContext*) scriptContext;
        Field(IdentPtrList*) requestedModuleList;
        Field(ModuleImportOrExportEntryList*) importRecordList;
        Field(ModuleImportOrExportEntryList*) localExportRecordList;
        Field(ModuleImportOrExportEntryList*) indirectExportRecordList;
        Field(ModuleImportOrExportEntryList*) starExportRecordList;
        Field(ChildModuleRecordSet*) childrenModuleSet;
        Field(ModuleRecordList*) parentModuleList;
        Field(LocalExportMap*) localExportMapByExportName;  // from propertyId to index map: for bytecode gen.
        Field(LocalExportMap*) localExportMapByLocalName;  // from propertyId to index map: for bytecode gen.
        Field(LocalExportIndexList*) localExportIndexList; // from index to propertyId: for typehandler.
        Field(uint) numUnInitializedChildrenModule;
        Field(ExportedNames*) exportedNames;
        Field(ResolvedExportMap*) resolvedExportMap;

        Field(Js::JavascriptFunction*) rootFunction;
        Field(void*) hostDefined;
        Field(Var) normalizedSpecifier;
        Field(Var) errorObject;
        Field(Field(Var)*) localExportSlots;
        Field(uint) localSlotCount;

        // module export allows aliasing, like export {foo as foo1, foo2, foo3}.
        Field(uint) localExportCount;
        Field(uint) moduleId;

        Field(ModuleNameRecord) namespaceRecord;

        HRESULT PostParseProcess();
        HRESULT PrepareForModuleDeclarationInitialization();
        void ImportModuleListsFromParser();
        HRESULT OnChildModuleReady(SourceTextModuleRecord* childModule, Var errorObj);
        void NotifyParentsAsNeeded();
        void CleanupBeforeExecution();
        void InitializeLocalImports();
        void InitializeLocalExports();
        void InitializeIndirectExports();
        PropertyId EnsurePropertyIdForIdentifier(IdentPtr pid);
        LocalExportMap* GetLocalExportMap() const {LOGMEIN("SourceTextModuleRecord.h] 153\n"); return localExportMapByExportName; }
        LocalExportIndexList* GetLocalExportIndexList() const {LOGMEIN("SourceTextModuleRecord.h] 154\n"); return localExportIndexList; }
        ResolvedExportMap* GetExportedNamesMap() const {LOGMEIN("SourceTextModuleRecord.h] 155\n"); return resolvedExportMap; }
    };

    struct ServerSourceTextModuleRecord
    {
        uint moduleId;
        Field(Var)* localExportSlotsAddr;
    };
}