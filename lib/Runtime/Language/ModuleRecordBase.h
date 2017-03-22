//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    class ModuleRecordBase;
    class ModuleNamespace;
    typedef SList<PropertyId> ExportedNames;
    typedef SList<ModuleRecordBase*> ExportModuleRecordList;
    struct ModuleNameRecord
    {
        ModuleNameRecord(const ModuleNameRecord& other)
            :module(other.module), bindingName(other.bindingName)
        {LOGMEIN("ModuleRecordBase.h] 16\n");}
        ModuleNameRecord(ModuleRecordBase* module, PropertyId bindingName) 
            :module(module), bindingName(bindingName) 
        {LOGMEIN("ModuleRecordBase.h] 19\n");}
        ModuleNameRecord() {LOGMEIN("ModuleRecordBase.h] 20\n");}
        Field(ModuleRecordBase*) module;
        Field(PropertyId) bindingName;
    };
    typedef SList<ModuleNameRecord> ResolveSet;

    // ModuleRecord need to keep rootFunction etc. alive.
    class ModuleRecordBase : public FinalizableObject
    {
    public:
        static const uint32 ModuleMagicNumber;
        ModuleRecordBase(JavascriptLibrary* library) :
            namespaceObject(nullptr), wasEvaluated(false),
            javascriptLibrary(library),  magicNumber(ModuleMagicNumber){LOGMEIN("ModuleRecordBase.h] 33\n");};
        bool WasEvaluated() {LOGMEIN("ModuleRecordBase.h] 34\n"); return wasEvaluated; }
        void SetWasEvaluated() {LOGMEIN("ModuleRecordBase.h] 35\n"); Assert(!wasEvaluated); wasEvaluated = true; }
        JavascriptLibrary* GetRealm() {LOGMEIN("ModuleRecordBase.h] 36\n"); return javascriptLibrary; }  // TODO: do we need to provide this method ?
        virtual ModuleNamespace* GetNamespace() {LOGMEIN("ModuleRecordBase.h] 37\n"); return namespaceObject; }
        virtual void SetNamespace(ModuleNamespace* moduleNamespace) {LOGMEIN("ModuleRecordBase.h] 38\n"); namespaceObject = moduleNamespace; }

        virtual ExportedNames* GetExportedNames(ExportModuleRecordList* exportStarSet) = 0;
        // return false when "ambiguous".
        // otherwise nullptr means "null" where we have circular reference/cannot resolve.
        virtual bool ResolveExport(PropertyId exportName, ResolveSet* resolveSet, ExportModuleRecordList* exportStarSet, ModuleNameRecord** exportRecord) = 0;
        virtual void ModuleDeclarationInstantiation() = 0;
        virtual Var ModuleEvaluation() = 0;
        virtual bool IsSourceTextModuleRecord() {LOGMEIN("ModuleRecordBase.h] 46\n"); return false; }

    protected:
        Field(uint32) magicNumber;
        Field(ModuleNamespace*) namespaceObject;
        Field(bool) wasEvaluated;
        Field(JavascriptLibrary*) javascriptLibrary;
    };
}