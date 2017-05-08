//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLanguagePch.h"
#include "Types/PropertyIndexRanges.h"
#include "Types/SimpleDictionaryPropertyDescriptor.h"
#include "Types/SimpleDictionaryTypeHandler.h"
#include "ModuleNamespace.h"

namespace Js
{
    const uint32 ModuleRecordBase::ModuleMagicNumber = *(const uint32*)"Mode";

    SourceTextModuleRecord::SourceTextModuleRecord(ScriptContext* scriptContext) :
        ModuleRecordBase(scriptContext->GetLibrary()),
        scriptContext(scriptContext),
        parseTree(nullptr),
        parser(nullptr),
        pSourceInfo(nullptr),
        rootFunction(nullptr),
        requestedModuleList(nullptr),
        importRecordList(nullptr),
        localExportRecordList(nullptr),
        indirectExportRecordList(nullptr),
        starExportRecordList(nullptr),
        childrenModuleSet(nullptr),
        parentModuleList(nullptr),
        localExportMapByExportName(nullptr),
        localExportMapByLocalName(nullptr),
        localExportIndexList(nullptr),
        normalizedSpecifier(nullptr),
        errorObject(nullptr),
        hostDefined(nullptr),
        exportedNames(nullptr),
        resolvedExportMap(nullptr),
        wasParsed(false),
        wasDeclarationInitialized(false),
        isRootModule(false),
        hadNotifyHostReady(false),
        localExportSlots(nullptr),
        numUnInitializedChildrenModule(0),
        moduleId(InvalidModuleIndex),
        localSlotCount(InvalidSlotCount),
        localExportCount(0)
    {TRACE_IT(52707);
        namespaceRecord.module = this;
        namespaceRecord.bindingName = PropertyIds::star_;
    }

    SourceTextModuleRecord* SourceTextModuleRecord::Create(ScriptContext* scriptContext)
    {TRACE_IT(52708);
        Recycler* recycler = scriptContext->GetRecycler();
        SourceTextModuleRecord* childModuleRecord;
        childModuleRecord = RecyclerNewFinalized(recycler, Js::SourceTextModuleRecord, scriptContext);
        // There is no real reference to lifetime management in ecmascript
        // The life time of a module record should be controlled by the module registry as defined in WHATWG module loader spec
        // in practice the modulerecord lifetime should be the same as the scriptcontext so it could be retrieved for the same
        // site. Host might hold a reference to the module as well after initializing the module.
        // In our implementation, we'll use the moduleId in bytecode to identify the module.
        childModuleRecord->moduleId = scriptContext->GetLibrary()->EnsureModuleRecordList()->Add(childModuleRecord);

        return childModuleRecord;
    }

    void SourceTextModuleRecord::Finalize(bool isShutdown)
    {TRACE_IT(52709);
        parseTree = nullptr;
        requestedModuleList = nullptr;
        importRecordList = nullptr;
        localExportRecordList = nullptr;
        indirectExportRecordList = nullptr;
        starExportRecordList = nullptr;
        childrenModuleSet = nullptr;
        parentModuleList = nullptr;
        if (!isShutdown)
        {TRACE_IT(52710);
            if (parser != nullptr)
            {
                AllocatorDelete(ArenaAllocator, scriptContext->GeneralAllocator(), parser);
                parser = nullptr;
            }
        }
    }

    HRESULT SourceTextModuleRecord::ParseSource(__in_bcount(sourceLength) byte* sourceText, uint32 sourceLength, SRCINFO * srcInfo, Var* exceptionVar, bool isUtf8)
    {TRACE_IT(52711);
        Assert(!wasParsed);
        Assert(parser == nullptr);
        HRESULT hr = NOERROR;
        ScriptContext* scriptContext = GetScriptContext();
        CompileScriptException se;
        ArenaAllocator* allocator = scriptContext->GeneralAllocator();
        *exceptionVar = nullptr;
        if (!scriptContext->GetConfig()->IsES6ModuleEnabled())
        {TRACE_IT(52712);
            return E_NOTIMPL;
        }
        // Host indicates that the current module failed to load.
        if (sourceText == nullptr)
        {TRACE_IT(52713);
            Assert(sourceLength == 0);
            hr = E_FAIL;
            JavascriptError *pError = scriptContext->GetLibrary()->CreateError();
            JavascriptError::SetErrorMessageProperties(pError, hr, _u("host failed to download module"), scriptContext);
            *exceptionVar = pError;
        }
        else
        {TRACE_IT(52714);
            try
            {TRACE_IT(52715);
                AUTO_NESTED_HANDLED_EXCEPTION_TYPE((ExceptionType)(ExceptionType_OutOfMemory | ExceptionType_StackOverflow));
                this->parser = (Parser*)AllocatorNew(ArenaAllocator, allocator, Parser, scriptContext);
                srcInfo->moduleID = moduleId;

                LoadScriptFlag loadScriptFlag = (LoadScriptFlag)(LoadScriptFlag_Expression | LoadScriptFlag_Module |
                    (isUtf8 ? LoadScriptFlag_Utf8Source : LoadScriptFlag_None));

                Utf8SourceInfo* pResultSourceInfo = nullptr;
                this->parseTree = scriptContext->ParseScript(parser, sourceText,
                    sourceLength, srcInfo, &se, &pResultSourceInfo, _u("module"),
                    loadScriptFlag, &sourceIndex, nullptr);
                this->pSourceInfo = pResultSourceInfo;

                if (parseTree == nullptr)
                {TRACE_IT(52716);
                    hr = E_FAIL;
                }
            }
            catch (Js::OutOfMemoryException)
            {TRACE_IT(52717);
                hr = E_OUTOFMEMORY;
                se.ProcessError(nullptr, E_OUTOFMEMORY, nullptr);
            }
            catch (Js::StackOverflowException)
            {TRACE_IT(52718);
                hr = VBSERR_OutOfStack;
                se.ProcessError(nullptr, VBSERR_OutOfStack, nullptr);
            }
            if (SUCCEEDED(hr))
            {TRACE_IT(52719);
                hr = PostParseProcess();
                if (hr == S_OK && this->errorObject != nullptr && this->hadNotifyHostReady)
                {TRACE_IT(52720);
                    // This would be the case where the child module got error and current module has notified error already.
                    if (*exceptionVar == nullptr)
                    {TRACE_IT(52721);
                        *exceptionVar = this->errorObject;
                    }
                    return E_FAIL;
                }
            }
        }
        if (FAILED(hr))
        {TRACE_IT(52722);
            if (*exceptionVar == nullptr)
            {TRACE_IT(52723);
                *exceptionVar = JavascriptError::CreateFromCompileScriptException(scriptContext, &se);
            }
            if (this->parser)
            {TRACE_IT(52724);
                this->parseTree = nullptr;
                AllocatorDelete(ArenaAllocator, allocator, this->parser);
                this->parser = nullptr;
            }
            if (this->errorObject == nullptr)
            {TRACE_IT(52725);
                this->errorObject = *exceptionVar;
            }
            NotifyParentsAsNeeded();
        }
        return hr;
    }

    void SourceTextModuleRecord::NotifyParentsAsNeeded()
    {TRACE_IT(52726);
        // Notify the parent modules that this child module is either in fault state or finished.
        if (this->parentModuleList != nullptr)
        {TRACE_IT(52727);
            parentModuleList->Map([=](uint i, SourceTextModuleRecord* parentModule)
            {
                parentModule->OnChildModuleReady(this, this->errorObject);
            });
        }
    }

    void SourceTextModuleRecord::ImportModuleListsFromParser()
    {TRACE_IT(52728);
        Assert(scriptContext->GetConfig()->IsES6ModuleEnabled());
        PnModule* moduleParseNode = static_cast<PnModule*>(&this->parseTree->sxModule);
        SetrequestedModuleList(moduleParseNode->requestedModules);
        SetImportRecordList(moduleParseNode->importEntries);
        SetStarExportRecordList(moduleParseNode->starExportEntries);
        SetIndirectExportRecordList(moduleParseNode->indirectExportEntries);
        SetLocalExportRecordList(moduleParseNode->localExportEntries);
    }

    HRESULT SourceTextModuleRecord::PostParseProcess()
    {TRACE_IT(52729);
        HRESULT hr = NOERROR;
        SetWasParsed();
        ImportModuleListsFromParser();
        hr = ResolveExternalModuleDependencies();

        if (SUCCEEDED(hr))
        {TRACE_IT(52730);
            hr = PrepareForModuleDeclarationInitialization();
        }
        return hr;
    }

    HRESULT SourceTextModuleRecord::PrepareForModuleDeclarationInitialization()
    {TRACE_IT(52731);
        HRESULT hr = NO_ERROR;

        if (numUnInitializedChildrenModule == 0)
        {TRACE_IT(52732);
            NotifyParentsAsNeeded();

            if (!WasDeclarationInitialized() && isRootModule)
            {TRACE_IT(52733);
                // TODO: move this as a promise call? if parser is called from a different thread
                // We'll need to call the bytecode gen in the main thread as we are accessing GC.
                ScriptContext* scriptContext = GetScriptContext();
                Assert(!scriptContext->GetThreadContext()->IsScriptActive());

                ModuleDeclarationInstantiation();
                if (!hadNotifyHostReady)
                {TRACE_IT(52734);
                    hr = scriptContext->GetHostScriptContext()->NotifyHostAboutModuleReady(this, this->errorObject);
                    hadNotifyHostReady = true;
                }
            }
        }
        return hr;
    }

    HRESULT SourceTextModuleRecord::OnChildModuleReady(SourceTextModuleRecord* childModule, Var childException)
    {TRACE_IT(52735);
        HRESULT hr = NOERROR;
        if (childException != nullptr)
        {TRACE_IT(52736);
            // propagate the error up as needed.
            if (this->errorObject == nullptr)
            {TRACE_IT(52737);
                this->errorObject = childException;
            }
            NotifyParentsAsNeeded();
            if (isRootModule && !hadNotifyHostReady)
            {TRACE_IT(52738);
                hr = scriptContext->GetHostScriptContext()->NotifyHostAboutModuleReady(this, this->errorObject);
                hadNotifyHostReady = true;
            }
        }
        else
        {TRACE_IT(52739);
            if (numUnInitializedChildrenModule == 0)
            {TRACE_IT(52740);
                return NOERROR; // this is only in case of recursive module reference. Let the higher stack frame handle this module.
            }
            numUnInitializedChildrenModule--;

            hr = PrepareForModuleDeclarationInitialization();
        }
        return hr;
    }

    ModuleNamespace* SourceTextModuleRecord::GetNamespace()
    {TRACE_IT(52741);
        Assert(localExportSlots != nullptr);
        Assert(PointerValue(localExportSlots[GetLocalExportSlotCount()]) == __super::GetNamespace());
        return (ModuleNamespace*)(void*)(localExportSlots[GetLocalExportSlotCount()]);
    }

    void SourceTextModuleRecord::SetNamespace(ModuleNamespace* moduleNamespace)
    {TRACE_IT(52742);
        Assert(localExportSlots != nullptr);
        __super::SetNamespace(moduleNamespace);
        localExportSlots[GetLocalExportSlotCount()] = moduleNamespace;
    }


    ExportedNames* SourceTextModuleRecord::GetExportedNames(ExportModuleRecordList* exportStarSet)
    {TRACE_IT(52743);
        if (exportedNames != nullptr)
        {TRACE_IT(52744);
            return exportedNames;
        }
        ArenaAllocator* allocator = scriptContext->GeneralAllocator();
        if (exportStarSet == nullptr)
        {TRACE_IT(52745);
            exportStarSet = (ExportModuleRecordList*)AllocatorNew(ArenaAllocator, allocator, ExportModuleRecordList, allocator);
        }
        if (exportStarSet->Has(this))
        {TRACE_IT(52746);
            return nullptr;
        }
        exportStarSet->Prepend(this);
        ExportedNames* tempExportedNames = nullptr;
        if (this->localExportRecordList != nullptr)
        {TRACE_IT(52747);
            tempExportedNames = (ExportedNames*)AllocatorNew(ArenaAllocator, allocator, ExportedNames, allocator);
            this->localExportRecordList->Map([=](ModuleImportOrExportEntry exportEntry) {
                PropertyId exportNameId = EnsurePropertyIdForIdentifier(exportEntry.exportName);
                tempExportedNames->Prepend(exportNameId);
            });
        }
        if (this->indirectExportRecordList != nullptr)
        {TRACE_IT(52748);
            if (tempExportedNames == nullptr)
            {TRACE_IT(52749);
                tempExportedNames = (ExportedNames*)AllocatorNew(ArenaAllocator, allocator, ExportedNames, allocator);
            }
            this->indirectExportRecordList->Map([=](ModuleImportOrExportEntry exportEntry) {
                PropertyId exportedNameId = EnsurePropertyIdForIdentifier(exportEntry.exportName);
                tempExportedNames->Prepend(exportedNameId);
            });
        }
        if (this->starExportRecordList != nullptr)
        {TRACE_IT(52750);
            if (tempExportedNames == nullptr)
            {TRACE_IT(52751);
                tempExportedNames = (ExportedNames*)AllocatorNew(ArenaAllocator, allocator, ExportedNames, allocator);
            }
            this->starExportRecordList->Map([=](ModuleImportOrExportEntry exportEntry) {
                Assert(exportEntry.moduleRequest != nullptr);
                SourceTextModuleRecord* moduleRecord;
                if (this->childrenModuleSet->TryGetValue(exportEntry.moduleRequest->Psz(), &moduleRecord))
                {TRACE_IT(52752);
                    Assert(moduleRecord->WasParsed());
                    Assert(moduleRecord->WasDeclarationInitialized()); // we should be half way during initialization
                    Assert(!moduleRecord->WasEvaluated());
                    ExportedNames* starExportedNames = moduleRecord->GetExportedNames(exportStarSet);
                    // We are not rejecting ambiguous resolution at this time.
                    if (starExportedNames != nullptr)
                    {TRACE_IT(52753);
                        starExportedNames->Map([&](PropertyId propertyId) {
                            if (propertyId != PropertyIds::default_ && !tempExportedNames->Has(propertyId))
                            {TRACE_IT(52754);
                                tempExportedNames->Prepend(propertyId);
                            }
                        });
                    }
                }
#if DBG
                else
                {
                    AssertMsg(false, "dependent modules should have been initialized");
                }
#endif
            });
        }
        exportedNames = tempExportedNames;
        return tempExportedNames;
    }

    bool SourceTextModuleRecord::ResolveImport(PropertyId localName, ModuleNameRecord** importRecord)
    {TRACE_IT(52755);
        *importRecord = nullptr;

        importRecordList->MapUntil([&](ModuleImportOrExportEntry& importEntry) {
            Js::PropertyId localNamePid = EnsurePropertyIdForIdentifier(importEntry.localName);
            if (localNamePid == localName)
            {TRACE_IT(52756);
                SourceTextModuleRecord* childModule = this->GetChildModuleRecord(importEntry.moduleRequest->Psz());
                Js::PropertyId importName = EnsurePropertyIdForIdentifier(importEntry.importName);
                if (importName == Js::PropertyIds::star_)
                {TRACE_IT(52757);
                    *importRecord = childModule->GetNamespaceNameRecord();
                }
                else
                {TRACE_IT(52758);
                    childModule->ResolveExport(importName, nullptr, nullptr, importRecord);
                }
                return true;
            }
            return false;
        });

        return *importRecord != nullptr;
    }

    // return false when "ambiguous".
    // otherwise nullptr means "null" where we have circular reference/cannot resolve.
    bool SourceTextModuleRecord::ResolveExport(PropertyId exportName, ResolveSet* resolveSet, ExportModuleRecordList* exportStarSet, ModuleNameRecord** exportRecord)
    {TRACE_IT(52759);
        ArenaAllocator* allocator = scriptContext->GeneralAllocator();
        if (resolvedExportMap == nullptr)
        {TRACE_IT(52760);
            resolvedExportMap = AllocatorNew(ArenaAllocator, allocator, ResolvedExportMap, allocator);
        }
        if (resolvedExportMap->TryGetReference(exportName, exportRecord))
        {TRACE_IT(52761);
            return true;
        }
        // TODO: use per-call/loop allocator?
        if (exportStarSet == nullptr)
        {TRACE_IT(52762);
            exportStarSet = (ExportModuleRecordList*)AllocatorNew(ArenaAllocator, allocator, ExportModuleRecordList, allocator);
        }
        if (resolveSet == nullptr)
        {TRACE_IT(52763);
            resolveSet = (ResolveSet*)AllocatorNew(ArenaAllocator, allocator, ResolveSet, allocator);
        }

        *exportRecord = nullptr;
        bool hasCircularRef = false;
        resolveSet->MapUntil([&](ModuleNameRecord moduleNameRecord) {
            if (moduleNameRecord.module == this && moduleNameRecord.bindingName == exportName)
            {TRACE_IT(52764);
                *exportRecord = nullptr;
                hasCircularRef = true;
                return true;
            }
            return false;
        });
        if (hasCircularRef)
        {TRACE_IT(52765);
            Assert(*exportRecord == nullptr);
            return true;
        }
        resolveSet->Prepend(ModuleNameRecord(this, exportName));

        if (localExportRecordList != nullptr)
        {TRACE_IT(52766);
            PropertyId localNameId = Js::Constants::NoProperty;
            localExportRecordList->MapUntil([&](ModuleImportOrExportEntry exportEntry) {
                PropertyId exportNameId = EnsurePropertyIdForIdentifier(exportEntry.exportName);
                if (exportNameId == exportName)
                {TRACE_IT(52767);
                    localNameId = EnsurePropertyIdForIdentifier(exportEntry.localName);
                    return true;
                }
                return false;
            });
            if (localNameId != Js::Constants::NoProperty)
            {TRACE_IT(52768);
                // Check to see if we are exporting something we imported from another module without using a re-export.
                // ex: import { foo } from 'module'; export { foo };
                ModuleRecordBase* sourceModule = this;
                ModuleNameRecord* importRecord = nullptr;
                if (this->importRecordList != nullptr
                    && this->ResolveImport(localNameId, &importRecord)
                    && importRecord != nullptr)
                {TRACE_IT(52769);
                    sourceModule = importRecord->module;
                    localNameId = importRecord->bindingName;
                }
                resolvedExportMap->AddNew(exportName, { sourceModule, localNameId });
                // return the address from Map buffer.
                resolvedExportMap->TryGetReference(exportName, exportRecord);
                return true;
            }
        }

        if (indirectExportRecordList != nullptr)
        {TRACE_IT(52770);
            bool isAmbiguous = false;
            indirectExportRecordList->MapUntil([&](ModuleImportOrExportEntry exportEntry) {
                PropertyId reexportNameId = EnsurePropertyIdForIdentifier(exportEntry.exportName);
                if (exportName != reexportNameId)
                {TRACE_IT(52771);
                    return false;
                }

                PropertyId importNameId = EnsurePropertyIdForIdentifier(exportEntry.importName);
                SourceTextModuleRecord* childModuleRecord = GetChildModuleRecord(exportEntry.moduleRequest->Psz());
                if (childModuleRecord == nullptr)
                {TRACE_IT(52772);
                    JavascriptError::ThrowReferenceError(scriptContext, JSERR_CannotResolveModule, exportEntry.moduleRequest->Psz());
                }
                else
                {TRACE_IT(52773);
                    isAmbiguous = !childModuleRecord->ResolveExport(importNameId, resolveSet, exportStarSet, exportRecord);
                    if (isAmbiguous)
                    {TRACE_IT(52774);
                        // ambiguous; don't need to search further
                        return true;
                    }
                    else
                    {TRACE_IT(52775);
                        // found a resolution. done;
                        if (*exportRecord != nullptr)
                        {TRACE_IT(52776);
                            return true;
                        }
                    }
                }
                return false;
            });
            if (isAmbiguous)
            {TRACE_IT(52777);
                return false;
            }
            if (*exportRecord != nullptr)
            {TRACE_IT(52778);
                return true;
            }
        }

        if (exportName == PropertyIds::default_)
        {TRACE_IT(52779);
            JavascriptError* errorObj = scriptContext->GetLibrary()->CreateSyntaxError();
            JavascriptError::SetErrorMessage(errorObj, JSERR_ModuleResolveExport, scriptContext->GetPropertyName(exportName)->GetBuffer(), scriptContext);
            this->errorObject = errorObj;
            return false;
        }

        if (exportStarSet->Has(this))
        {TRACE_IT(52780);
            *exportRecord = nullptr;
            return true;
        }

        exportStarSet->Prepend(this);
        bool ambiguousResolution = false;
        if (this->starExportRecordList != nullptr)
        {TRACE_IT(52781);
            ModuleNameRecord* starResolution = nullptr;
            starExportRecordList->MapUntil([&](ModuleImportOrExportEntry starExportEntry) {
                ModuleNameRecord* currentResolution = nullptr;
                SourceTextModuleRecord* childModule = GetChildModuleRecord(starExportEntry.moduleRequest->Psz());
                if (childModule == nullptr)
                {TRACE_IT(52782);
                    JavascriptError::ThrowReferenceError(GetScriptContext(), JSERR_CannotResolveModule, starExportEntry.moduleRequest->Psz());
                }
                if (childModule->errorObject != nullptr)
                {TRACE_IT(52783);
                    JavascriptExceptionOperators::Throw(childModule->errorObject, GetScriptContext());
                }

                // if ambigious, return "ambigious"
                if (!childModule->ResolveExport(exportName, resolveSet, exportStarSet, &currentResolution))
                {TRACE_IT(52784);
                    ambiguousResolution = true;
                    return true;
                }
                if (currentResolution != nullptr)
                {TRACE_IT(52785);
                    if (starResolution == nullptr)
                    {TRACE_IT(52786);
                        starResolution = currentResolution;
                    }
                    else
                    {TRACE_IT(52787);
                        if (currentResolution->bindingName != starResolution->bindingName ||
                            currentResolution->module != starResolution->module)
                        {TRACE_IT(52788);
                            ambiguousResolution = true;
                        }
                        return true;
                    }
                }
                return false;
            });
            if (!ambiguousResolution)
            {TRACE_IT(52789);
                *exportRecord = starResolution;
            }
        }
        return !ambiguousResolution;
    }

    void SourceTextModuleRecord::SetParent(SourceTextModuleRecord* parentRecord, LPCOLESTR moduleName)
    {TRACE_IT(52790);
        Assert(parentRecord != nullptr);
        Assert(parentRecord->childrenModuleSet != nullptr);
        if (!parentRecord->childrenModuleSet->ContainsKey(moduleName))
        {TRACE_IT(52791);
            parentRecord->childrenModuleSet->AddNew(moduleName, this);

            if (this->parentModuleList == nullptr)
            {TRACE_IT(52792);
                Recycler* recycler = GetScriptContext()->GetRecycler();
                this->parentModuleList = RecyclerNew(recycler, ModuleRecordList, recycler);
            }
            bool contains = this->parentModuleList->Contains(parentRecord);
            Assert(!contains);
            if (!contains)
            {TRACE_IT(52793);
                this->parentModuleList->Add(parentRecord);
                if (!this->WasDeclarationInitialized())
                {TRACE_IT(52794);
                    parentRecord->numUnInitializedChildrenModule++;
                }
            }
        }
    }

    HRESULT SourceTextModuleRecord::ResolveExternalModuleDependencies()
    {TRACE_IT(52795);
        ScriptContext* scriptContext = GetScriptContext();

        HRESULT hr = NOERROR;
        if (requestedModuleList != nullptr)
        {TRACE_IT(52796);
            if (nullptr == childrenModuleSet)
            {TRACE_IT(52797);
                ArenaAllocator* allocator = scriptContext->GeneralAllocator();
                childrenModuleSet = (ChildModuleRecordSet*)AllocatorNew(ArenaAllocator, allocator, ChildModuleRecordSet, allocator);
            }
            requestedModuleList->MapUntil([&](IdentPtr specifier) {
                ModuleRecordBase* moduleRecordBase = nullptr;
                SourceTextModuleRecord* moduleRecord = nullptr;
                LPCOLESTR moduleName = specifier->Psz();
                bool itemFound = childrenModuleSet->TryGetValue(moduleName, &moduleRecord);
                if (!itemFound)
                {TRACE_IT(52798);
                    hr = scriptContext->GetHostScriptContext()->FetchImportedModule(this, moduleName, &moduleRecordBase);
                    if (FAILED(hr))
                    {TRACE_IT(52799);
                        return true;
                    }
                    moduleRecord = SourceTextModuleRecord::FromHost(moduleRecordBase);
                    moduleRecord->SetParent(this, moduleName);
                }
                return false;
            });
            if (FAILED(hr))
            {TRACE_IT(52800);
                JavascriptError *error = scriptContext->GetLibrary()->CreateError();
                JavascriptError::SetErrorMessageProperties(error, hr, _u("fetch import module failed"), scriptContext);
                this->errorObject = error;
                NotifyParentsAsNeeded();
            }
        }
        return hr;
    }

    void SourceTextModuleRecord::CleanupBeforeExecution()
    {TRACE_IT(52801);
        // zero out fields is more a defense in depth as those fields are not needed anymore
        Assert(wasParsed);
        Assert(wasEvaluated);
        Assert(wasDeclarationInitialized);
        // Debugger can reparse the source and generate the byte code again. Don't cleanup the
        // helper information for now.
    }

    void SourceTextModuleRecord::ModuleDeclarationInstantiation()
    {TRACE_IT(52802);
        ScriptContext* scriptContext = GetScriptContext();

        if (this->WasDeclarationInitialized())
        {TRACE_IT(52803);
            return;
        }

        try
        {TRACE_IT(52804);
            AUTO_NESTED_HANDLED_EXCEPTION_TYPE((ExceptionType)(ExceptionType_OutOfMemory|ExceptionType_JavascriptException));
            InitializeLocalExports();

            InitializeLocalImports();

            InitializeIndirectExports();
        }
        catch (const JavascriptException& err)
        {TRACE_IT(52805);
            this->errorObject = err.GetAndClear()->GetThrownObject(scriptContext);
        }

        if (this->errorObject != nullptr)
        {TRACE_IT(52806);
            NotifyParentsAsNeeded();
            return;
        }

        SetWasDeclarationInitialized();
        if (childrenModuleSet != nullptr)
        {TRACE_IT(52807);
            childrenModuleSet->Map([](LPCOLESTR specifier, SourceTextModuleRecord* moduleRecord)
            {
                Assert(moduleRecord->WasParsed());
                moduleRecord->ModuleDeclarationInstantiation();
            });
        }

        ModuleNamespace::GetModuleNamespace(this);
        Js::AutoDynamicCodeReference dynamicFunctionReference(scriptContext);
        Assert(this == scriptContext->GetLibrary()->GetModuleRecord(this->pSourceInfo->GetSrcInfo()->moduleID));
        CompileScriptException se;
        this->rootFunction = scriptContext->GenerateRootFunction(parseTree, sourceIndex, this->parser, this->pSourceInfo->GetParseFlags(), &se, _u("module"));
        if (rootFunction == nullptr)
        {TRACE_IT(52808);
            this->errorObject = JavascriptError::CreateFromCompileScriptException(scriptContext, &se);
            NotifyParentsAsNeeded();
        }
        else
        {TRACE_IT(52809);
            scriptContext->GetDebugContext()->RegisterFunction(this->rootFunction->GetFunctionBody(), nullptr);
        }
    }

    Var SourceTextModuleRecord::ModuleEvaluation()
    {TRACE_IT(52810);
#if DBG
        if (childrenModuleSet != nullptr)
        {TRACE_IT(52811);
            childrenModuleSet->EachValue([=](SourceTextModuleRecord* childModuleRecord)
            {
                AssertMsg(childModuleRecord->WasParsed(), "child module needs to have been parsed");
                AssertMsg(childModuleRecord->WasDeclarationInitialized(), "child module needs to have been initialized.");
            });
        }
#endif
        if (!scriptContext->GetConfig()->IsES6ModuleEnabled())
        {TRACE_IT(52812);
            return nullptr;
        }
        Assert(this->errorObject == nullptr);
        if (this->errorObject != nullptr)
        {TRACE_IT(52813);
            JavascriptExceptionOperators::Throw(errorObject, scriptContext);
        }
        Assert(!WasEvaluated());
        SetWasEvaluated();
        // we shouldn't evaluate if there are existing failure. This is defense in depth as the host shouldn't
        // call into evaluation if there was previous fialure on the module.
        if (this->errorObject)
        {TRACE_IT(52814);
            return this->errorObject;
        }
        if (childrenModuleSet != nullptr)
        {TRACE_IT(52815);
            childrenModuleSet->EachValue([=](SourceTextModuleRecord* childModuleRecord)
            {
                if (!childModuleRecord->WasEvaluated())
                {TRACE_IT(52816);
                    childModuleRecord->ModuleEvaluation();
                }
            });
        }
        CleanupBeforeExecution();

        Arguments outArgs(CallInfo(CallFlags_Value, 0), nullptr);
        return rootFunction->CallRootFunction(outArgs, scriptContext, true);
    }

    HRESULT SourceTextModuleRecord::OnHostException(void* errorVar)
    {TRACE_IT(52817);
        if (!RecyclableObject::Is(errorVar))
        {TRACE_IT(52818);
            return E_INVALIDARG;
        }
        AssertMsg(!WasParsed(), "shouldn't be called after a module is parsed");
        if (WasParsed())
        {TRACE_IT(52819);
            return E_INVALIDARG;
        }
        this->errorObject = errorVar;

        // a sub module failed to download. we need to notify that the parent that sub module failed and we need to report back.
        return NOERROR;
    }

    SourceTextModuleRecord* SourceTextModuleRecord::GetChildModuleRecord(LPCOLESTR specifier) const
    {TRACE_IT(52820);
        SourceTextModuleRecord* childModuleRecord = nullptr;
        if (childrenModuleSet == nullptr)
        {
            AssertMsg(false, "We should have some child modulerecords first before trying to get child modulerecord.");
            return nullptr;
        }
        if (!childrenModuleSet->TryGetValue(specifier, &childModuleRecord))
        {TRACE_IT(52821);
            return nullptr;
        }
        return childModuleRecord;
    }

    void SourceTextModuleRecord::InitializeLocalImports()
    {TRACE_IT(52822);
        if (importRecordList != nullptr)
        {TRACE_IT(52823);
            importRecordList->Map([&](ModuleImportOrExportEntry& importEntry) {
                Js::PropertyId importName = EnsurePropertyIdForIdentifier(importEntry.importName);

                SourceTextModuleRecord* childModule = this->GetChildModuleRecord(importEntry.moduleRequest->Psz());
                ModuleNameRecord* importRecord = nullptr;
                // We don't need to initialize anything for * import.
                if (importName != Js::PropertyIds::star_)
                {TRACE_IT(52824);
                    if (!childModule->ResolveExport(importName, nullptr, nullptr, &importRecord)
                        || importRecord == nullptr)
                    {TRACE_IT(52825);
                        JavascriptError* errorObj = scriptContext->GetLibrary()->CreateSyntaxError();
                        JavascriptError::SetErrorMessage(errorObj, JSERR_ModuleResolveImport, importEntry.importName->Psz(), scriptContext);
                        this->errorObject = errorObj;
                        return;
                    }
                }
            });
        }
    }

    // Local exports are stored in the slotarray in the SourceTextModuleRecord.
    void SourceTextModuleRecord::InitializeLocalExports()
    {TRACE_IT(52826);
        Recycler* recycler = scriptContext->GetRecycler();
        Var undefineValue = scriptContext->GetLibrary()->GetUndefined();
        if (localSlotCount == InvalidSlotCount)
        {TRACE_IT(52827);
            uint currentSlotCount = 0;
            if (localExportRecordList != nullptr)
            {TRACE_IT(52828);
                ArenaAllocator* allocator = scriptContext->GeneralAllocator();
                localExportMapByExportName = AllocatorNew(ArenaAllocator, allocator, LocalExportMap, allocator);
                localExportMapByLocalName = AllocatorNew(ArenaAllocator, allocator, LocalExportMap, allocator);
                localExportIndexList = AllocatorNew(ArenaAllocator, allocator, LocalExportIndexList, allocator);
                localExportRecordList->Map([&](ModuleImportOrExportEntry exportEntry)
                {
                    Assert(exportEntry.moduleRequest == nullptr);
                    Assert(exportEntry.importName == nullptr);
                    PropertyId exportNameId = EnsurePropertyIdForIdentifier(exportEntry.exportName);
                    PropertyId localNameId = EnsurePropertyIdForIdentifier(exportEntry.localName);

                    // We could have exports that look local but actually exported from other module
                    // import {foo} from "module1.js"; export {foo};
                    ModuleNameRecord* importRecord = nullptr;
                    if (this->GetImportEntryList() != nullptr
                        && this->ResolveImport(localNameId, &importRecord)
                        && importRecord != nullptr)
                    {TRACE_IT(52829);
                        return;
                    }

                    // 2G is too big already.
                    if (localExportCount >= INT_MAX)
                    {TRACE_IT(52830);
                        JavascriptError::ThrowRangeError(scriptContext, JSERR_TooManyImportExports);
                    }
                    localExportCount++;
                    uint exportSlot = UINT_MAX;

                    for (uint i = 0; i < (uint)localExportIndexList->Count(); i++)
                    {TRACE_IT(52831);
                        if (localExportIndexList->Item(i) == localNameId)
                        {TRACE_IT(52832);
                            exportSlot = i;
                            break;
                        }
                    }

                    if (exportSlot == UINT_MAX)
                    {TRACE_IT(52833);
                        exportSlot = currentSlotCount;
                        localExportMapByLocalName->Add(localNameId, exportSlot);
                        localExportIndexList->Add(localNameId);
                        Assert(localExportIndexList->Item(currentSlotCount) == localNameId);
                        currentSlotCount++;
                        if (currentSlotCount >= INT_MAX)
                        {TRACE_IT(52834);
                            JavascriptError::ThrowRangeError(scriptContext, JSERR_TooManyImportExports);
                        }
                    }

                    localExportMapByExportName->Add(exportNameId, exportSlot);

                });
            }
            // Namespace object will be added to the end of the array though invisible through namespace object itself.
            localExportSlots = RecyclerNewArray(recycler, Field(Var), currentSlotCount + 1);
            for (uint i = 0; i < currentSlotCount; i++)
            {TRACE_IT(52835);
                localExportSlots[i] = undefineValue;
            }
            localExportSlots[currentSlotCount] = nullptr;

            localSlotCount = currentSlotCount;

#if ENABLE_NATIVE_CODEGEN
            if (JITManager::GetJITManager()->IsOOPJITEnabled() && JITManager::GetJITManager()->IsConnected())
            {TRACE_IT(52836);
                HRESULT hr = JITManager::GetJITManager()->AddModuleRecordInfo(
                    scriptContext->GetRemoteScriptAddr(),
                    this->GetModuleId(),
                    (intptr_t)this->GetLocalExportSlots());
                JITManager::HandleServerCallResult(hr, RemoteCallType::StateUpdate);
            }
#endif
        }
    }

    PropertyId SourceTextModuleRecord::EnsurePropertyIdForIdentifier(IdentPtr pid)
    {TRACE_IT(52837);
        PropertyId propertyId = pid->GetPropertyId();
        if (propertyId == Js::Constants::NoProperty)
        {TRACE_IT(52838);
            propertyId = scriptContext->GetOrAddPropertyIdTracked(pid->Psz(), pid->Cch());
            pid->SetPropertyId(propertyId);
        }
        return propertyId;
    }

    void SourceTextModuleRecord::InitializeIndirectExports()
    {TRACE_IT(52839);
        ModuleNameRecord* exportRecord = nullptr;
        if (indirectExportRecordList != nullptr)
        {TRACE_IT(52840);
            indirectExportRecordList->Map([&](ModuleImportOrExportEntry exportEntry)
            {
                PropertyId propertyId = EnsurePropertyIdForIdentifier(exportEntry.importName);
                SourceTextModuleRecord* childModuleRecord = GetChildModuleRecord(exportEntry.moduleRequest->Psz());
                if (childModuleRecord == nullptr)
                {TRACE_IT(52841);
                    JavascriptError* errorObj = scriptContext->GetLibrary()->CreateReferenceError();
                    JavascriptError::SetErrorMessage(errorObj, JSERR_CannotResolveModule, exportEntry.moduleRequest->Psz(), scriptContext);
                    this->errorObject = errorObj;
                    return;
                }
                if (!childModuleRecord->ResolveExport(propertyId, nullptr, nullptr, &exportRecord) ||
                    (exportRecord == nullptr))
                {TRACE_IT(52842);
                    JavascriptError* errorObj = scriptContext->GetLibrary()->CreateSyntaxError();
                    JavascriptError::SetErrorMessage(errorObj, JSERR_ModuleResolveExport, exportEntry.exportName->Psz(), scriptContext);
                    this->errorObject = errorObj;
                    return;
                }
            });
        }
    }

    uint SourceTextModuleRecord::GetLocalExportSlotIndexByExportName(PropertyId exportNameId)
    {TRACE_IT(52843);
        Assert(localSlotCount != 0);
        Assert(localExportSlots != nullptr);
        uint slotIndex = InvalidSlotIndex;
        if (!localExportMapByExportName->TryGetValue(exportNameId, &slotIndex))
        {
            AssertMsg(false, "exportNameId is not in local export list");
            return InvalidSlotIndex;
        }
        else
        {TRACE_IT(52844);
            return slotIndex;
        }
    }

    uint SourceTextModuleRecord::GetLocalExportSlotIndexByLocalName(PropertyId localNameId)
    {TRACE_IT(52845);
        Assert(localSlotCount != 0 || localNameId == PropertyIds::star_);
        Assert(localExportSlots != nullptr);
        uint slotIndex = InvalidSlotIndex;
        if (localNameId == PropertyIds::star_)
        {TRACE_IT(52846);
            return localSlotCount;  // namespace is put on the last slot.
        } else if (!localExportMapByLocalName->TryGetValue(localNameId, &slotIndex))
        {
            AssertMsg(false, "exportNameId is not in local export list");
            return InvalidSlotIndex;
        }
        return slotIndex;
    }
}