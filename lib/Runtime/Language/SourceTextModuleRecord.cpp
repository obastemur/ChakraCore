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
    {LOGMEIN("SourceTextModuleRecord.cpp] 45\n");
        namespaceRecord.module = this;
        namespaceRecord.bindingName = PropertyIds::star_;
    }

    SourceTextModuleRecord* SourceTextModuleRecord::Create(ScriptContext* scriptContext)
    {LOGMEIN("SourceTextModuleRecord.cpp] 51\n");
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
    {LOGMEIN("SourceTextModuleRecord.cpp] 66\n");
        parseTree = nullptr;
        requestedModuleList = nullptr;
        importRecordList = nullptr;
        localExportRecordList = nullptr;
        indirectExportRecordList = nullptr;
        starExportRecordList = nullptr;
        childrenModuleSet = nullptr;
        parentModuleList = nullptr;
        if (!isShutdown)
        {LOGMEIN("SourceTextModuleRecord.cpp] 76\n");
            if (parser != nullptr)
            {
                AllocatorDelete(ArenaAllocator, scriptContext->GeneralAllocator(), parser);
                parser = nullptr;
            }
        }
    }

    HRESULT SourceTextModuleRecord::ParseSource(__in_bcount(sourceLength) byte* sourceText, uint32 sourceLength, SRCINFO * srcInfo, Var* exceptionVar, bool isUtf8)
    {LOGMEIN("SourceTextModuleRecord.cpp] 86\n");
        Assert(!wasParsed);
        Assert(parser == nullptr);
        HRESULT hr = NOERROR;
        ScriptContext* scriptContext = GetScriptContext();
        CompileScriptException se;
        ArenaAllocator* allocator = scriptContext->GeneralAllocator();
        *exceptionVar = nullptr;
        if (!scriptContext->GetConfig()->IsES6ModuleEnabled())
        {LOGMEIN("SourceTextModuleRecord.cpp] 95\n");
            return E_NOTIMPL;
        }
        // Host indicates that the current module failed to load.
        if (sourceText == nullptr)
        {LOGMEIN("SourceTextModuleRecord.cpp] 100\n");
            Assert(sourceLength == 0);
            hr = E_FAIL;
            JavascriptError *pError = scriptContext->GetLibrary()->CreateError();
            JavascriptError::SetErrorMessageProperties(pError, hr, _u("host failed to download module"), scriptContext);
            *exceptionVar = pError;
        }
        else
        {
            try
            {LOGMEIN("SourceTextModuleRecord.cpp] 110\n");
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
                {LOGMEIN("SourceTextModuleRecord.cpp] 125\n");
                    hr = E_FAIL;
                }
            }
            catch (Js::OutOfMemoryException)
            {LOGMEIN("SourceTextModuleRecord.cpp] 130\n");
                hr = E_OUTOFMEMORY;
                se.ProcessError(nullptr, E_OUTOFMEMORY, nullptr);
            }
            catch (Js::StackOverflowException)
            {LOGMEIN("SourceTextModuleRecord.cpp] 135\n");
                hr = VBSERR_OutOfStack;
                se.ProcessError(nullptr, VBSERR_OutOfStack, nullptr);
            }
            if (SUCCEEDED(hr))
            {LOGMEIN("SourceTextModuleRecord.cpp] 140\n");
                hr = PostParseProcess();
                if (hr == S_OK && this->errorObject != nullptr && this->hadNotifyHostReady)
                {LOGMEIN("SourceTextModuleRecord.cpp] 143\n");
                    // This would be the case where the child module got error and current module has notified error already.
                    if (*exceptionVar == nullptr)
                    {LOGMEIN("SourceTextModuleRecord.cpp] 146\n");
                        *exceptionVar = this->errorObject;
                    }
                    return E_FAIL;
                }
            }
        }
        if (FAILED(hr))
        {LOGMEIN("SourceTextModuleRecord.cpp] 154\n");
            if (*exceptionVar == nullptr)
            {LOGMEIN("SourceTextModuleRecord.cpp] 156\n");
                *exceptionVar = JavascriptError::CreateFromCompileScriptException(scriptContext, &se);
            }
            if (this->parser)
            {LOGMEIN("SourceTextModuleRecord.cpp] 160\n");
                this->parseTree = nullptr;
                AllocatorDelete(ArenaAllocator, allocator, this->parser);
                this->parser = nullptr;
            }
            if (this->errorObject == nullptr)
            {LOGMEIN("SourceTextModuleRecord.cpp] 166\n");
                this->errorObject = *exceptionVar;
            }
            NotifyParentsAsNeeded();
        }
        return hr;
    }

    void SourceTextModuleRecord::NotifyParentsAsNeeded()
    {LOGMEIN("SourceTextModuleRecord.cpp] 175\n");
        // Notify the parent modules that this child module is either in fault state or finished.
        if (this->parentModuleList != nullptr)
        {LOGMEIN("SourceTextModuleRecord.cpp] 178\n");
            parentModuleList->Map([=](uint i, SourceTextModuleRecord* parentModule)
            {
                parentModule->OnChildModuleReady(this, this->errorObject);
            });
        }
    }

    void SourceTextModuleRecord::ImportModuleListsFromParser()
    {LOGMEIN("SourceTextModuleRecord.cpp] 187\n");
        Assert(scriptContext->GetConfig()->IsES6ModuleEnabled());
        PnModule* moduleParseNode = static_cast<PnModule*>(&this->parseTree->sxModule);
        SetrequestedModuleList(moduleParseNode->requestedModules);
        SetImportRecordList(moduleParseNode->importEntries);
        SetStarExportRecordList(moduleParseNode->starExportEntries);
        SetIndirectExportRecordList(moduleParseNode->indirectExportEntries);
        SetLocalExportRecordList(moduleParseNode->localExportEntries);
    }

    HRESULT SourceTextModuleRecord::PostParseProcess()
    {LOGMEIN("SourceTextModuleRecord.cpp] 198\n");
        HRESULT hr = NOERROR;
        SetWasParsed();
        ImportModuleListsFromParser();
        hr = ResolveExternalModuleDependencies();

        if (SUCCEEDED(hr))
        {LOGMEIN("SourceTextModuleRecord.cpp] 205\n");
            hr = PrepareForModuleDeclarationInitialization();
        }
        return hr;
    }

    HRESULT SourceTextModuleRecord::PrepareForModuleDeclarationInitialization()
    {LOGMEIN("SourceTextModuleRecord.cpp] 212\n");
        HRESULT hr = NO_ERROR;

        if (numUnInitializedChildrenModule == 0)
        {LOGMEIN("SourceTextModuleRecord.cpp] 216\n");
            NotifyParentsAsNeeded();

            if (!WasDeclarationInitialized() && isRootModule)
            {LOGMEIN("SourceTextModuleRecord.cpp] 220\n");
                // TODO: move this as a promise call? if parser is called from a different thread
                // We'll need to call the bytecode gen in the main thread as we are accessing GC.
                ScriptContext* scriptContext = GetScriptContext();
                Assert(!scriptContext->GetThreadContext()->IsScriptActive());

                ModuleDeclarationInstantiation();
                if (!hadNotifyHostReady)
                {LOGMEIN("SourceTextModuleRecord.cpp] 228\n");
                    hr = scriptContext->GetHostScriptContext()->NotifyHostAboutModuleReady(this, this->errorObject);
                    hadNotifyHostReady = true;
                }
            }
        }
        return hr;
    }

    HRESULT SourceTextModuleRecord::OnChildModuleReady(SourceTextModuleRecord* childModule, Var childException)
    {LOGMEIN("SourceTextModuleRecord.cpp] 238\n");
        HRESULT hr = NOERROR;
        if (childException != nullptr)
        {LOGMEIN("SourceTextModuleRecord.cpp] 241\n");
            // propagate the error up as needed.
            if (this->errorObject == nullptr)
            {LOGMEIN("SourceTextModuleRecord.cpp] 244\n");
                this->errorObject = childException;
            }
            NotifyParentsAsNeeded();
            if (isRootModule && !hadNotifyHostReady)
            {LOGMEIN("SourceTextModuleRecord.cpp] 249\n");
                hr = scriptContext->GetHostScriptContext()->NotifyHostAboutModuleReady(this, this->errorObject);
                hadNotifyHostReady = true;
            }
        }
        else
        {
            if (numUnInitializedChildrenModule == 0)
            {LOGMEIN("SourceTextModuleRecord.cpp] 257\n");
                return NOERROR; // this is only in case of recursive module reference. Let the higher stack frame handle this module.
            }
            numUnInitializedChildrenModule--;

            hr = PrepareForModuleDeclarationInitialization();
        }
        return hr;
    }

    ModuleNamespace* SourceTextModuleRecord::GetNamespace()
    {LOGMEIN("SourceTextModuleRecord.cpp] 268\n");
        Assert(localExportSlots != nullptr);
        Assert(PointerValue(localExportSlots[GetLocalExportSlotCount()]) == __super::GetNamespace());
        return (ModuleNamespace*)(void*)(localExportSlots[GetLocalExportSlotCount()]);
    }

    void SourceTextModuleRecord::SetNamespace(ModuleNamespace* moduleNamespace)
    {LOGMEIN("SourceTextModuleRecord.cpp] 275\n");
        Assert(localExportSlots != nullptr);
        __super::SetNamespace(moduleNamespace);
        localExportSlots[GetLocalExportSlotCount()] = moduleNamespace;
    }


    ExportedNames* SourceTextModuleRecord::GetExportedNames(ExportModuleRecordList* exportStarSet)
    {LOGMEIN("SourceTextModuleRecord.cpp] 283\n");
        if (exportedNames != nullptr)
        {LOGMEIN("SourceTextModuleRecord.cpp] 285\n");
            return exportedNames;
        }
        ArenaAllocator* allocator = scriptContext->GeneralAllocator();
        if (exportStarSet == nullptr)
        {LOGMEIN("SourceTextModuleRecord.cpp] 290\n");
            exportStarSet = (ExportModuleRecordList*)AllocatorNew(ArenaAllocator, allocator, ExportModuleRecordList, allocator);
        }
        if (exportStarSet->Has(this))
        {LOGMEIN("SourceTextModuleRecord.cpp] 294\n");
            return nullptr;
        }
        exportStarSet->Prepend(this);
        ExportedNames* tempExportedNames = nullptr;
        if (this->localExportRecordList != nullptr)
        {LOGMEIN("SourceTextModuleRecord.cpp] 300\n");
            tempExportedNames = (ExportedNames*)AllocatorNew(ArenaAllocator, allocator, ExportedNames, allocator);
            this->localExportRecordList->Map([=](ModuleImportOrExportEntry exportEntry) {
                PropertyId exportNameId = EnsurePropertyIdForIdentifier(exportEntry.exportName);
                tempExportedNames->Prepend(exportNameId);
            });
        }
        if (this->indirectExportRecordList != nullptr)
        {LOGMEIN("SourceTextModuleRecord.cpp] 308\n");
            if (tempExportedNames == nullptr)
            {LOGMEIN("SourceTextModuleRecord.cpp] 310\n");
                tempExportedNames = (ExportedNames*)AllocatorNew(ArenaAllocator, allocator, ExportedNames, allocator);
            }
            this->indirectExportRecordList->Map([=](ModuleImportOrExportEntry exportEntry) {
                PropertyId exportedNameId = EnsurePropertyIdForIdentifier(exportEntry.exportName);
                tempExportedNames->Prepend(exportedNameId);
            });
        }
        if (this->starExportRecordList != nullptr)
        {LOGMEIN("SourceTextModuleRecord.cpp] 319\n");
            if (tempExportedNames == nullptr)
            {LOGMEIN("SourceTextModuleRecord.cpp] 321\n");
                tempExportedNames = (ExportedNames*)AllocatorNew(ArenaAllocator, allocator, ExportedNames, allocator);
            }
            this->starExportRecordList->Map([=](ModuleImportOrExportEntry exportEntry) {
                Assert(exportEntry.moduleRequest != nullptr);
                SourceTextModuleRecord* moduleRecord;
                if (this->childrenModuleSet->TryGetValue(exportEntry.moduleRequest->Psz(), &moduleRecord))
                {LOGMEIN("SourceTextModuleRecord.cpp] 328\n");
                    Assert(moduleRecord->WasParsed());
                    Assert(moduleRecord->WasDeclarationInitialized()); // we should be half way during initialization
                    Assert(!moduleRecord->WasEvaluated());
                    ExportedNames* starExportedNames = moduleRecord->GetExportedNames(exportStarSet);
                    // We are not rejecting ambiguous resolution at this time.
                    if (starExportedNames != nullptr)
                    {LOGMEIN("SourceTextModuleRecord.cpp] 335\n");
                        starExportedNames->Map([&](PropertyId propertyId) {
                            if (propertyId != PropertyIds::default_ && !tempExportedNames->Has(propertyId))
                            {LOGMEIN("SourceTextModuleRecord.cpp] 338\n");
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
    {LOGMEIN("SourceTextModuleRecord.cpp] 357\n");
        *importRecord = nullptr;

        importRecordList->MapUntil([&](ModuleImportOrExportEntry& importEntry) {
            Js::PropertyId localNamePid = EnsurePropertyIdForIdentifier(importEntry.localName);
            if (localNamePid == localName)
            {LOGMEIN("SourceTextModuleRecord.cpp] 363\n");
                SourceTextModuleRecord* childModule = this->GetChildModuleRecord(importEntry.moduleRequest->Psz());
                Js::PropertyId importName = EnsurePropertyIdForIdentifier(importEntry.importName);
                if (importName == Js::PropertyIds::star_)
                {LOGMEIN("SourceTextModuleRecord.cpp] 367\n");
                    *importRecord = childModule->GetNamespaceNameRecord();
                }
                else
                {
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
    {LOGMEIN("SourceTextModuleRecord.cpp] 385\n");
        ArenaAllocator* allocator = scriptContext->GeneralAllocator();
        if (resolvedExportMap == nullptr)
        {LOGMEIN("SourceTextModuleRecord.cpp] 388\n");
            resolvedExportMap = AllocatorNew(ArenaAllocator, allocator, ResolvedExportMap, allocator);
        }
        if (resolvedExportMap->TryGetReference(exportName, exportRecord))
        {LOGMEIN("SourceTextModuleRecord.cpp] 392\n");
            return true;
        }
        // TODO: use per-call/loop allocator?
        if (exportStarSet == nullptr)
        {LOGMEIN("SourceTextModuleRecord.cpp] 397\n");
            exportStarSet = (ExportModuleRecordList*)AllocatorNew(ArenaAllocator, allocator, ExportModuleRecordList, allocator);
        }
        if (resolveSet == nullptr)
        {LOGMEIN("SourceTextModuleRecord.cpp] 401\n");
            resolveSet = (ResolveSet*)AllocatorNew(ArenaAllocator, allocator, ResolveSet, allocator);
        }

        *exportRecord = nullptr;
        bool hasCircularRef = false;
        resolveSet->MapUntil([&](ModuleNameRecord moduleNameRecord) {
            if (moduleNameRecord.module == this && moduleNameRecord.bindingName == exportName)
            {LOGMEIN("SourceTextModuleRecord.cpp] 409\n");
                *exportRecord = nullptr;
                hasCircularRef = true;
                return true;
            }
            return false;
        });
        if (hasCircularRef)
        {LOGMEIN("SourceTextModuleRecord.cpp] 417\n");
            Assert(*exportRecord == nullptr);
            return true;
        }
        resolveSet->Prepend(ModuleNameRecord(this, exportName));

        if (localExportRecordList != nullptr)
        {LOGMEIN("SourceTextModuleRecord.cpp] 424\n");
            PropertyId localNameId = Js::Constants::NoProperty;
            localExportRecordList->MapUntil([&](ModuleImportOrExportEntry exportEntry) {
                PropertyId exportNameId = EnsurePropertyIdForIdentifier(exportEntry.exportName);
                if (exportNameId == exportName)
                {LOGMEIN("SourceTextModuleRecord.cpp] 429\n");
                    localNameId = EnsurePropertyIdForIdentifier(exportEntry.localName);
                    return true;
                }
                return false;
            });
            if (localNameId != Js::Constants::NoProperty)
            {LOGMEIN("SourceTextModuleRecord.cpp] 436\n");
                // Check to see if we are exporting something we imported from another module without using a re-export.
                // ex: import { foo } from 'module'; export { foo };
                ModuleRecordBase* sourceModule = this;
                ModuleNameRecord* importRecord = nullptr;
                if (this->importRecordList != nullptr
                    && this->ResolveImport(localNameId, &importRecord)
                    && importRecord != nullptr)
                {LOGMEIN("SourceTextModuleRecord.cpp] 444\n");
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
        {LOGMEIN("SourceTextModuleRecord.cpp] 456\n");
            bool isAmbiguous = false;
            indirectExportRecordList->MapUntil([&](ModuleImportOrExportEntry exportEntry) {
                PropertyId reexportNameId = EnsurePropertyIdForIdentifier(exportEntry.exportName);
                if (exportName != reexportNameId)
                {LOGMEIN("SourceTextModuleRecord.cpp] 461\n");
                    return false;
                }

                PropertyId importNameId = EnsurePropertyIdForIdentifier(exportEntry.importName);
                SourceTextModuleRecord* childModuleRecord = GetChildModuleRecord(exportEntry.moduleRequest->Psz());
                if (childModuleRecord == nullptr)
                {LOGMEIN("SourceTextModuleRecord.cpp] 468\n");
                    JavascriptError::ThrowReferenceError(scriptContext, JSERR_CannotResolveModule, exportEntry.moduleRequest->Psz());
                }
                else
                {
                    isAmbiguous = !childModuleRecord->ResolveExport(importNameId, resolveSet, exportStarSet, exportRecord);
                    if (isAmbiguous)
                    {LOGMEIN("SourceTextModuleRecord.cpp] 475\n");
                        // ambiguous; don't need to search further
                        return true;
                    }
                    else
                    {
                        // found a resolution. done;
                        if (*exportRecord != nullptr)
                        {LOGMEIN("SourceTextModuleRecord.cpp] 483\n");
                            return true;
                        }
                    }
                }
                return false;
            });
            if (isAmbiguous)
            {LOGMEIN("SourceTextModuleRecord.cpp] 491\n");
                return false;
            }
            if (*exportRecord != nullptr)
            {LOGMEIN("SourceTextModuleRecord.cpp] 495\n");
                return true;
            }
        }

        if (exportName == PropertyIds::default_)
        {LOGMEIN("SourceTextModuleRecord.cpp] 501\n");
            JavascriptError* errorObj = scriptContext->GetLibrary()->CreateSyntaxError();
            JavascriptError::SetErrorMessage(errorObj, JSERR_ModuleResolveExport, scriptContext->GetPropertyName(exportName)->GetBuffer(), scriptContext);
            this->errorObject = errorObj;
            return false;
        }

        if (exportStarSet->Has(this))
        {LOGMEIN("SourceTextModuleRecord.cpp] 509\n");
            *exportRecord = nullptr;
            return true;
        }

        exportStarSet->Prepend(this);
        bool ambiguousResolution = false;
        if (this->starExportRecordList != nullptr)
        {LOGMEIN("SourceTextModuleRecord.cpp] 517\n");
            ModuleNameRecord* starResolution = nullptr;
            starExportRecordList->MapUntil([&](ModuleImportOrExportEntry starExportEntry) {
                ModuleNameRecord* currentResolution = nullptr;
                SourceTextModuleRecord* childModule = GetChildModuleRecord(starExportEntry.moduleRequest->Psz());
                if (childModule == nullptr)
                {LOGMEIN("SourceTextModuleRecord.cpp] 523\n");
                    JavascriptError::ThrowReferenceError(GetScriptContext(), JSERR_CannotResolveModule, starExportEntry.moduleRequest->Psz());
                }
                if (childModule->errorObject != nullptr)
                {LOGMEIN("SourceTextModuleRecord.cpp] 527\n");
                    JavascriptExceptionOperators::Throw(childModule->errorObject, GetScriptContext());
                }

                // if ambigious, return "ambigious"
                if (!childModule->ResolveExport(exportName, resolveSet, exportStarSet, &currentResolution))
                {LOGMEIN("SourceTextModuleRecord.cpp] 533\n");
                    ambiguousResolution = true;
                    return true;
                }
                if (currentResolution != nullptr)
                {LOGMEIN("SourceTextModuleRecord.cpp] 538\n");
                    if (starResolution == nullptr)
                    {LOGMEIN("SourceTextModuleRecord.cpp] 540\n");
                        starResolution = currentResolution;
                    }
                    else
                    {
                        if (currentResolution->bindingName != starResolution->bindingName ||
                            currentResolution->module != starResolution->module)
                        {LOGMEIN("SourceTextModuleRecord.cpp] 547\n");
                            ambiguousResolution = true;
                        }
                        return true;
                    }
                }
                return false;
            });
            if (!ambiguousResolution)
            {LOGMEIN("SourceTextModuleRecord.cpp] 556\n");
                *exportRecord = starResolution;
            }
        }
        return !ambiguousResolution;
    }

    void SourceTextModuleRecord::SetParent(SourceTextModuleRecord* parentRecord, LPCOLESTR moduleName)
    {LOGMEIN("SourceTextModuleRecord.cpp] 564\n");
        Assert(parentRecord != nullptr);
        Assert(parentRecord->childrenModuleSet != nullptr);
        if (!parentRecord->childrenModuleSet->ContainsKey(moduleName))
        {LOGMEIN("SourceTextModuleRecord.cpp] 568\n");
            parentRecord->childrenModuleSet->AddNew(moduleName, this);

            if (this->parentModuleList == nullptr)
            {LOGMEIN("SourceTextModuleRecord.cpp] 572\n");
                Recycler* recycler = GetScriptContext()->GetRecycler();
                this->parentModuleList = RecyclerNew(recycler, ModuleRecordList, recycler);
            }
            bool contains = this->parentModuleList->Contains(parentRecord);
            Assert(!contains);
            if (!contains)
            {LOGMEIN("SourceTextModuleRecord.cpp] 579\n");
                this->parentModuleList->Add(parentRecord);
                if (!this->WasDeclarationInitialized())
                {LOGMEIN("SourceTextModuleRecord.cpp] 582\n");
                    parentRecord->numUnInitializedChildrenModule++;
                }
            }
        }
    }

    HRESULT SourceTextModuleRecord::ResolveExternalModuleDependencies()
    {LOGMEIN("SourceTextModuleRecord.cpp] 590\n");
        ScriptContext* scriptContext = GetScriptContext();

        HRESULT hr = NOERROR;
        if (requestedModuleList != nullptr)
        {LOGMEIN("SourceTextModuleRecord.cpp] 595\n");
            if (nullptr == childrenModuleSet)
            {LOGMEIN("SourceTextModuleRecord.cpp] 597\n");
                ArenaAllocator* allocator = scriptContext->GeneralAllocator();
                childrenModuleSet = (ChildModuleRecordSet*)AllocatorNew(ArenaAllocator, allocator, ChildModuleRecordSet, allocator);
            }
            requestedModuleList->MapUntil([&](IdentPtr specifier) {
                ModuleRecordBase* moduleRecordBase = nullptr;
                SourceTextModuleRecord* moduleRecord = nullptr;
                LPCOLESTR moduleName = specifier->Psz();
                bool itemFound = childrenModuleSet->TryGetValue(moduleName, &moduleRecord);
                if (!itemFound)
                {LOGMEIN("SourceTextModuleRecord.cpp] 607\n");
                    hr = scriptContext->GetHostScriptContext()->FetchImportedModule(this, moduleName, &moduleRecordBase);
                    if (FAILED(hr))
                    {LOGMEIN("SourceTextModuleRecord.cpp] 610\n");
                        return true;
                    }
                    moduleRecord = SourceTextModuleRecord::FromHost(moduleRecordBase);
                    moduleRecord->SetParent(this, moduleName);
                }
                return false;
            });
            if (FAILED(hr))
            {LOGMEIN("SourceTextModuleRecord.cpp] 619\n");
                JavascriptError *error = scriptContext->GetLibrary()->CreateError();
                JavascriptError::SetErrorMessageProperties(error, hr, _u("fetch import module failed"), scriptContext);
                this->errorObject = error;
                NotifyParentsAsNeeded();
            }
        }
        return hr;
    }

    void SourceTextModuleRecord::CleanupBeforeExecution()
    {LOGMEIN("SourceTextModuleRecord.cpp] 630\n");
        // zero out fields is more a defense in depth as those fields are not needed anymore
        Assert(wasParsed);
        Assert(wasEvaluated);
        Assert(wasDeclarationInitialized);
        // Debugger can reparse the source and generate the byte code again. Don't cleanup the
        // helper information for now.
    }

    void SourceTextModuleRecord::ModuleDeclarationInstantiation()
    {LOGMEIN("SourceTextModuleRecord.cpp] 640\n");
        ScriptContext* scriptContext = GetScriptContext();

        if (this->WasDeclarationInitialized())
        {LOGMEIN("SourceTextModuleRecord.cpp] 644\n");
            return;
        }

        try
        {LOGMEIN("SourceTextModuleRecord.cpp] 649\n");
            AUTO_NESTED_HANDLED_EXCEPTION_TYPE((ExceptionType)(ExceptionType_OutOfMemory|ExceptionType_JavascriptException));
            InitializeLocalExports();

            InitializeLocalImports();

            InitializeIndirectExports();
        }
        catch (const JavascriptException& err)
        {LOGMEIN("SourceTextModuleRecord.cpp] 658\n");
            this->errorObject = err.GetAndClear()->GetThrownObject(scriptContext);
        }

        if (this->errorObject != nullptr)
        {LOGMEIN("SourceTextModuleRecord.cpp] 663\n");
            NotifyParentsAsNeeded();
            return;
        }

        SetWasDeclarationInitialized();
        if (childrenModuleSet != nullptr)
        {LOGMEIN("SourceTextModuleRecord.cpp] 670\n");
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
        {LOGMEIN("SourceTextModuleRecord.cpp] 684\n");
            this->errorObject = JavascriptError::CreateFromCompileScriptException(scriptContext, &se);
            NotifyParentsAsNeeded();
        }
        else
        {
            scriptContext->GetDebugContext()->RegisterFunction(this->rootFunction->GetFunctionBody(), nullptr);
        }
    }

    Var SourceTextModuleRecord::ModuleEvaluation()
    {LOGMEIN("SourceTextModuleRecord.cpp] 695\n");
#if DBG
        if (childrenModuleSet != nullptr)
        {LOGMEIN("SourceTextModuleRecord.cpp] 698\n");
            childrenModuleSet->EachValue([=](SourceTextModuleRecord* childModuleRecord)
            {
                AssertMsg(childModuleRecord->WasParsed(), "child module needs to have been parsed");
                AssertMsg(childModuleRecord->WasDeclarationInitialized(), "child module needs to have been initialized.");
            });
        }
#endif
        if (!scriptContext->GetConfig()->IsES6ModuleEnabled())
        {LOGMEIN("SourceTextModuleRecord.cpp] 707\n");
            return nullptr;
        }
        Assert(this->errorObject == nullptr);
        if (this->errorObject != nullptr)
        {LOGMEIN("SourceTextModuleRecord.cpp] 712\n");
            JavascriptExceptionOperators::Throw(errorObject, scriptContext);
        }
        Assert(!WasEvaluated());
        SetWasEvaluated();
        // we shouldn't evaluate if there are existing failure. This is defense in depth as the host shouldn't
        // call into evaluation if there was previous fialure on the module.
        if (this->errorObject)
        {LOGMEIN("SourceTextModuleRecord.cpp] 720\n");
            return this->errorObject;
        }
        if (childrenModuleSet != nullptr)
        {LOGMEIN("SourceTextModuleRecord.cpp] 724\n");
            childrenModuleSet->EachValue([=](SourceTextModuleRecord* childModuleRecord)
            {
                if (!childModuleRecord->WasEvaluated())
                {LOGMEIN("SourceTextModuleRecord.cpp] 728\n");
                    childModuleRecord->ModuleEvaluation();
                }
            });
        }
        CleanupBeforeExecution();

        Arguments outArgs(CallInfo(CallFlags_Value, 0), nullptr);
        return rootFunction->CallRootFunction(outArgs, scriptContext, true);
    }

    HRESULT SourceTextModuleRecord::OnHostException(void* errorVar)
    {LOGMEIN("SourceTextModuleRecord.cpp] 740\n");
        if (!RecyclableObject::Is(errorVar))
        {LOGMEIN("SourceTextModuleRecord.cpp] 742\n");
            return E_INVALIDARG;
        }
        AssertMsg(!WasParsed(), "shouldn't be called after a module is parsed");
        if (WasParsed())
        {LOGMEIN("SourceTextModuleRecord.cpp] 747\n");
            return E_INVALIDARG;
        }
        this->errorObject = errorVar;

        // a sub module failed to download. we need to notify that the parent that sub module failed and we need to report back.
        return NOERROR;
    }

    SourceTextModuleRecord* SourceTextModuleRecord::GetChildModuleRecord(LPCOLESTR specifier) const
    {LOGMEIN("SourceTextModuleRecord.cpp] 757\n");
        SourceTextModuleRecord* childModuleRecord = nullptr;
        if (childrenModuleSet == nullptr)
        {
            AssertMsg(false, "We should have some child modulerecords first before trying to get child modulerecord.");
            return nullptr;
        }
        if (!childrenModuleSet->TryGetValue(specifier, &childModuleRecord))
        {LOGMEIN("SourceTextModuleRecord.cpp] 765\n");
            return nullptr;
        }
        return childModuleRecord;
    }

    void SourceTextModuleRecord::InitializeLocalImports()
    {LOGMEIN("SourceTextModuleRecord.cpp] 772\n");
        if (importRecordList != nullptr)
        {LOGMEIN("SourceTextModuleRecord.cpp] 774\n");
            importRecordList->Map([&](ModuleImportOrExportEntry& importEntry) {
                Js::PropertyId importName = EnsurePropertyIdForIdentifier(importEntry.importName);

                SourceTextModuleRecord* childModule = this->GetChildModuleRecord(importEntry.moduleRequest->Psz());
                ModuleNameRecord* importRecord = nullptr;
                // We don't need to initialize anything for * import.
                if (importName != Js::PropertyIds::star_)
                {LOGMEIN("SourceTextModuleRecord.cpp] 782\n");
                    if (!childModule->ResolveExport(importName, nullptr, nullptr, &importRecord)
                        || importRecord == nullptr)
                    {LOGMEIN("SourceTextModuleRecord.cpp] 785\n");
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
    {LOGMEIN("SourceTextModuleRecord.cpp] 798\n");
        Recycler* recycler = scriptContext->GetRecycler();
        Var undefineValue = scriptContext->GetLibrary()->GetUndefined();
        if (localSlotCount == InvalidSlotCount)
        {LOGMEIN("SourceTextModuleRecord.cpp] 802\n");
            uint currentSlotCount = 0;
            if (localExportRecordList != nullptr)
            {LOGMEIN("SourceTextModuleRecord.cpp] 805\n");
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
                    {LOGMEIN("SourceTextModuleRecord.cpp] 823\n");
                        return;
                    }

                    // 2G is too big already.
                    if (localExportCount >= INT_MAX)
                    {LOGMEIN("SourceTextModuleRecord.cpp] 829\n");
                        JavascriptError::ThrowRangeError(scriptContext, JSERR_TooManyImportExports);
                    }
                    localExportCount++;
                    uint exportSlot = UINT_MAX;

                    for (uint i = 0; i < (uint)localExportIndexList->Count(); i++)
                    {LOGMEIN("SourceTextModuleRecord.cpp] 836\n");
                        if (localExportIndexList->Item(i) == localNameId)
                        {LOGMEIN("SourceTextModuleRecord.cpp] 838\n");
                            exportSlot = i;
                            break;
                        }
                    }

                    if (exportSlot == UINT_MAX)
                    {LOGMEIN("SourceTextModuleRecord.cpp] 845\n");
                        exportSlot = currentSlotCount;
                        localExportMapByLocalName->Add(localNameId, exportSlot);
                        localExportIndexList->Add(localNameId);
                        Assert(localExportIndexList->Item(currentSlotCount) == localNameId);
                        currentSlotCount++;
                        if (currentSlotCount >= INT_MAX)
                        {LOGMEIN("SourceTextModuleRecord.cpp] 852\n");
                            JavascriptError::ThrowRangeError(scriptContext, JSERR_TooManyImportExports);
                        }
                    }

                    localExportMapByExportName->Add(exportNameId, exportSlot);

                });
            }
            // Namespace object will be added to the end of the array though invisible through namespace object itself.
            localExportSlots = RecyclerNewArray(recycler, Field(Var), currentSlotCount + 1);
            for (uint i = 0; i < currentSlotCount; i++)
            {LOGMEIN("SourceTextModuleRecord.cpp] 864\n");
                localExportSlots[i] = undefineValue;
            }
            localExportSlots[currentSlotCount] = nullptr;

            localSlotCount = currentSlotCount;

#if ENABLE_NATIVE_CODEGEN
            if (JITManager::GetJITManager()->IsOOPJITEnabled() && JITManager::GetJITManager()->IsConnected())
            {LOGMEIN("SourceTextModuleRecord.cpp] 873\n");
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
    {LOGMEIN("SourceTextModuleRecord.cpp] 885\n");
        PropertyId propertyId = pid->GetPropertyId();
        if (propertyId == Js::Constants::NoProperty)
        {LOGMEIN("SourceTextModuleRecord.cpp] 888\n");
            propertyId = scriptContext->GetOrAddPropertyIdTracked(pid->Psz(), pid->Cch());
            pid->SetPropertyId(propertyId);
        }
        return propertyId;
    }

    void SourceTextModuleRecord::InitializeIndirectExports()
    {LOGMEIN("SourceTextModuleRecord.cpp] 896\n");
        ModuleNameRecord* exportRecord = nullptr;
        if (indirectExportRecordList != nullptr)
        {LOGMEIN("SourceTextModuleRecord.cpp] 899\n");
            indirectExportRecordList->Map([&](ModuleImportOrExportEntry exportEntry)
            {
                PropertyId propertyId = EnsurePropertyIdForIdentifier(exportEntry.importName);
                SourceTextModuleRecord* childModuleRecord = GetChildModuleRecord(exportEntry.moduleRequest->Psz());
                if (childModuleRecord == nullptr)
                {LOGMEIN("SourceTextModuleRecord.cpp] 905\n");
                    JavascriptError* errorObj = scriptContext->GetLibrary()->CreateReferenceError();
                    JavascriptError::SetErrorMessage(errorObj, JSERR_CannotResolveModule, exportEntry.moduleRequest->Psz(), scriptContext);
                    this->errorObject = errorObj;
                    return;
                }
                if (!childModuleRecord->ResolveExport(propertyId, nullptr, nullptr, &exportRecord) ||
                    (exportRecord == nullptr))
                {LOGMEIN("SourceTextModuleRecord.cpp] 913\n");
                    JavascriptError* errorObj = scriptContext->GetLibrary()->CreateSyntaxError();
                    JavascriptError::SetErrorMessage(errorObj, JSERR_ModuleResolveExport, exportEntry.exportName->Psz(), scriptContext);
                    this->errorObject = errorObj;
                    return;
                }
            });
        }
    }

    uint SourceTextModuleRecord::GetLocalExportSlotIndexByExportName(PropertyId exportNameId)
    {LOGMEIN("SourceTextModuleRecord.cpp] 924\n");
        Assert(localSlotCount != 0);
        Assert(localExportSlots != nullptr);
        uint slotIndex = InvalidSlotIndex;
        if (!localExportMapByExportName->TryGetValue(exportNameId, &slotIndex))
        {
            AssertMsg(false, "exportNameId is not in local export list");
            return InvalidSlotIndex;
        }
        else
        {
            return slotIndex;
        }
    }

    uint SourceTextModuleRecord::GetLocalExportSlotIndexByLocalName(PropertyId localNameId)
    {LOGMEIN("SourceTextModuleRecord.cpp] 940\n");
        Assert(localSlotCount != 0 || localNameId == PropertyIds::star_);
        Assert(localExportSlots != nullptr);
        uint slotIndex = InvalidSlotIndex;
        if (localNameId == PropertyIds::star_)
        {LOGMEIN("SourceTextModuleRecord.cpp] 945\n");
            return localSlotCount;  // namespace is put on the last slot.
        } else if (!localExportMapByLocalName->TryGetValue(localNameId, &slotIndex))
        {
            AssertMsg(false, "exportNameId is not in local export list");
            return InvalidSlotIndex;
        }
        return slotIndex;
    }
}