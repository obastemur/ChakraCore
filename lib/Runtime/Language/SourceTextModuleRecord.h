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
        IdentPtrList* GetRequestedModuleList() const {TRACE_IT(52847); return requestedModuleList; }
        ModuleImportOrExportEntryList* GetImportEntryList() const {TRACE_IT(52848); return importRecordList; }
        ModuleImportOrExportEntryList* GetLocalExportEntryList() const {TRACE_IT(52849); return localExportRecordList; }
        ModuleImportOrExportEntryList* GetIndirectExportEntryList() const {TRACE_IT(52850); return indirectExportRecordList; }
        ModuleImportOrExportEntryList* GetStarExportRecordList() const {TRACE_IT(52851); return starExportRecordList; }
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

        void* GetHostDefined() const {TRACE_IT(52852); return hostDefined; }
        void SetHostDefined(void* hostObj) {TRACE_IT(52853); hostDefined = hostObj; }

        void SetSpecifier(Var specifier) {TRACE_IT(52854); this->normalizedSpecifier = specifier; }
        Var GetSpecifier() const {TRACE_IT(52855); return normalizedSpecifier; }

        Var GetErrorObject() const {TRACE_IT(52856); return errorObject; }

        bool WasParsed() const {TRACE_IT(52857); return wasParsed; }
        void SetWasParsed() {TRACE_IT(52858); wasParsed = true; }
        bool WasDeclarationInitialized() const {TRACE_IT(52859); return wasDeclarationInitialized; }
        void SetWasDeclarationInitialized() {TRACE_IT(52860); wasDeclarationInitialized = true; }
        void SetIsRootModule() {TRACE_IT(52861); isRootModule = true; }

        void SetImportRecordList(ModuleImportOrExportEntryList* importList) {TRACE_IT(52862); importRecordList = importList; }
        void SetLocalExportRecordList(ModuleImportOrExportEntryList* localExports) {TRACE_IT(52863); localExportRecordList = localExports; }
        void SetIndirectExportRecordList(ModuleImportOrExportEntryList* indirectExports) {TRACE_IT(52864); indirectExportRecordList = indirectExports; }
        void SetStarExportRecordList(ModuleImportOrExportEntryList* starExports) {TRACE_IT(52865); starExportRecordList = starExports; }
        void SetrequestedModuleList(IdentPtrList* requestModules) {TRACE_IT(52866); requestedModuleList = requestModules; }

        ScriptContext* GetScriptContext() const {TRACE_IT(52867); return scriptContext; }
        HRESULT ParseSource(__in_bcount(sourceLength) byte* sourceText, uint32 sourceLength, SRCINFO * srcInfo, Var* exceptionVar, bool isUtf8);
        HRESULT OnHostException(void* errorVar);

        static SourceTextModuleRecord* FromHost(void* hostModuleRecord)
        {TRACE_IT(52868);
            SourceTextModuleRecord* moduleRecord = static_cast<SourceTextModuleRecord*>(hostModuleRecord);
            Assert((moduleRecord == nullptr) || (moduleRecord->magicNumber == moduleRecord->ModuleMagicNumber));
            return moduleRecord;
        }

        static bool Is(void* hostModuleRecord)
        {TRACE_IT(52869);
            SourceTextModuleRecord* moduleRecord = static_cast<SourceTextModuleRecord*>(hostModuleRecord);
            if (moduleRecord != nullptr && (moduleRecord->magicNumber == moduleRecord->ModuleMagicNumber))
            {TRACE_IT(52870);
                return true;
            }
            return false;
        }

        static SourceTextModuleRecord* Create(ScriptContext* scriptContext);

        uint GetLocalExportSlotIndexByExportName(PropertyId exportNameId);
        uint GetLocalExportSlotIndexByLocalName(PropertyId localNameId);
        Field(Var)* GetLocalExportSlots() const {TRACE_IT(52871); return localExportSlots; }
        uint GetLocalExportSlotCount() const {TRACE_IT(52872); return localSlotCount; }
        uint GetModuleId() const {TRACE_IT(52873); return moduleId; }
        uint GetLocalExportCount() const {TRACE_IT(52874); return localExportCount; }

        ModuleNameRecord* GetNamespaceNameRecord() {TRACE_IT(52875); return &namespaceRecord; }

        SourceTextModuleRecord* GetChildModuleRecord(LPCOLESTR specifier) const;

        void SetParent(SourceTextModuleRecord* parentRecord, LPCOLESTR moduleName);
        Utf8SourceInfo* GetSourceInfo() {TRACE_IT(52876); return this->pSourceInfo; }

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
        LocalExportMap* GetLocalExportMap() const {TRACE_IT(52877); return localExportMapByExportName; }
        LocalExportIndexList* GetLocalExportIndexList() const {TRACE_IT(52878); return localExportIndexList; }
        ResolvedExportMap* GetExportedNamesMap() const {TRACE_IT(52879); return resolvedExportMap; }
    };

    struct ServerSourceTextModuleRecord
    {
        uint moduleId;
        Field(Var)* localExportSlotsAddr;
    };
}