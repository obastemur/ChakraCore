//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "RuntimeLanguagePch.h"
#include "Types/PropertyIndexRanges.h"
#include "Types/SimpleDictionaryPropertyDescriptor.h"
#include "Types/SimpleDictionaryTypeHandler.h"
#include "Types/NullTypeHandler.h"
#include "ModuleNamespace.h"
#include "ModuleNamespaceEnumerator.h"

namespace Js
{
    Js::FunctionInfo ModuleNamespace::EntryInfo::SymbolIterator(FORCE_NO_WRITE_BARRIER_TAG(ModuleNamespace::EntrySymbolIterator));

    ModuleNamespace::ModuleNamespace(ModuleRecordBase* moduleRecord, DynamicType* type) :
        moduleRecord(moduleRecord), DynamicObject(type), unambiguousNonLocalExports(nullptr),
        sortedExportedNames(nullptr), nsSlots(nullptr)
    {LOGMEIN("ModuleNamespace.cpp] 20\n");

    }

    ModuleNamespace* ModuleNamespace::GetModuleNamespace(ModuleRecordBase* requestModule)
    {LOGMEIN("ModuleNamespace.cpp] 25\n");
        Assert(requestModule->IsSourceTextModuleRecord());
        SourceTextModuleRecord* moduleRecord = SourceTextModuleRecord::FromHost(requestModule);
        ModuleNamespace* nsObject = moduleRecord->GetNamespace();
        if (nsObject != nullptr)
        {LOGMEIN("ModuleNamespace.cpp] 30\n");
            return nsObject;
        }
        ScriptContext* scriptContext = moduleRecord->GetRealm()->GetScriptContext();
        Recycler* recycler = scriptContext->GetRecycler();

        nsObject = RecyclerNew(recycler, ModuleNamespace, moduleRecord, scriptContext->GetLibrary()->GetModuleNamespaceType());
        nsObject->Initialize();

        moduleRecord->SetNamespace(nsObject);
        return nsObject;
    }

    void ModuleNamespace::Initialize()
    {LOGMEIN("ModuleNamespace.cpp] 44\n");
        ScriptContext* scriptContext = moduleRecord->GetRealm()->GetScriptContext();
        Recycler* recycler = scriptContext->GetRecycler();
        SourceTextModuleRecord* sourceTextModuleRecord = static_cast<SourceTextModuleRecord*>(
            static_cast<ModuleRecordBase*>(moduleRecord));
        JavascriptLibrary* library = GetLibrary();

        if (scriptContext->GetConfig()->IsES6ToStringTagEnabled())
        {LOGMEIN("ModuleNamespace.cpp] 52\n");
            DynamicObject::SetPropertyWithAttributes(PropertyIds::_symbolToStringTag, library->GetModuleTypeDisplayString(),
                PropertyConfigurable, nullptr);
        }

        DynamicType* type = library->CreateFunctionWithLengthType(&EntryInfo::SymbolIterator);
        RuntimeFunction* iteratorFunction = RecyclerNewEnumClass(scriptContext->GetRecycler(),
            JavascriptLibrary::EnumFunctionClass, RuntimeFunction,
            type, &EntryInfo::SymbolIterator);
        DynamicObject::SetPropertyWithAttributes(PropertyIds::_symbolIterator, iteratorFunction, PropertyBuiltInMethodDefaults, nullptr);

        ModuleImportOrExportEntryList* localExportList = sourceTextModuleRecord->GetLocalExportEntryList();
        // We don't have a type handler that can handle ModuleNamespace object. We have properties that could be aliased
        // like {export foo as foo1, foo2, foo3}, and external properties as reexport from current module. The problem with aliasing
        // is that multiple propertyId can be associated with one slotIndex. We need to build from PropertyMap directly here.
        // there is one instance of ModuleNamespace per module file; we can always use the BigPropertyIndex for security.
        propertyMap = RecyclerNew(recycler, SimplePropertyDescriptorMap, recycler, sourceTextModuleRecord->GetLocalExportCount());
        if (localExportList != nullptr)
        {LOGMEIN("ModuleNamespace.cpp] 70\n");
            localExportList->Map([=](ModuleImportOrExportEntry exportEntry) {
                PropertyId exportNameId = exportEntry.exportName->GetPropertyId();
                PropertyId localNameId = exportEntry.localName->GetPropertyId();
                const Js::PropertyRecord* exportNameRecord = scriptContext->GetThreadContext()->GetPropertyName(exportNameId);
                ModuleNameRecord* importRecord = nullptr;
                AssertMsg(exportNameId != Js::Constants::NoProperty, "should have been initialized already");
                // ignore local exports that are actually indirect exports.
                if (sourceTextModuleRecord->GetImportEntryList() == nullptr ||
                    (sourceTextModuleRecord->ResolveImport(localNameId, &importRecord)
                    && importRecord == nullptr))
                {LOGMEIN("ModuleNamespace.cpp] 81\n");
                    BigPropertyIndex index = sourceTextModuleRecord->GetLocalExportSlotIndexByExportName(exportNameId);
                    Assert((uint)index < sourceTextModuleRecord->GetLocalExportCount());
                    SimpleDictionaryPropertyDescriptor<BigPropertyIndex> propertyDescriptor = { index, PropertyModuleNamespaceDefault };
                    propertyMap->Add(exportNameRecord, propertyDescriptor);
                }
            });
        }
        // update the local slot to use the storage for local exports.
        SetNSSlotsForModuleNS(sourceTextModuleRecord->GetLocalExportSlots());

        // For items that are not in the local export list, we need to resolve them to get it
        ExportedNames* exportedNames = sourceTextModuleRecord->GetExportedNames(nullptr);
        ModuleNameRecord* moduleNameRecord = nullptr;
#if DBG
        uint unresolvableExportsCount = 0;
        uint localExportCount = 0;
#endif
        if (exportedNames != nullptr)
        {LOGMEIN("ModuleNamespace.cpp] 100\n");
            exportedNames->Map([&](PropertyId propertyId) {
                if (!moduleRecord->ResolveExport(propertyId, nullptr, nullptr, &moduleNameRecord))
                {LOGMEIN("ModuleNamespace.cpp] 103\n");
                    // ignore ambigious resolution.
#if DBG
                    unresolvableExportsCount++;
#endif
                    return;
                }
                // non-ambiguous resolution.
                if (moduleNameRecord == nullptr)
                {LOGMEIN("ModuleNamespace.cpp] 112\n");
                    JavascriptError::ThrowSyntaxError(scriptContext, JSERR_ResolveExportFailed, scriptContext->GetPropertyName(propertyId)->GetBuffer());
                }
                if (moduleNameRecord->module == moduleRecord)
                {LOGMEIN("ModuleNamespace.cpp] 116\n");
                    // skip local exports as they are covered in the localExportSlots.
#if DBG
                    localExportCount++;
#endif
                    return;
                }
                Assert(moduleNameRecord->module != moduleRecord);
                this->AddUnambiguousNonLocalExport(propertyId, moduleNameRecord);
            });
        }
#if DBG
        uint totalExportCount = exportedNames != nullptr ? exportedNames->Count() : 0;
        uint unambiguousNonLocalCount = (this->GetUnambiguousNonLocalExports() != nullptr) ? this->GetUnambiguousNonLocalExports()->Count() : 0;
        Assert(totalExportCount == localExportCount + unambiguousNonLocalCount + unresolvableExportsCount);
#endif
        BOOL result = this->PreventExtensions();
        Assert(result);
    }

    void ModuleNamespace::AddUnambiguousNonLocalExport(PropertyId propertyId, ModuleNameRecord* nonLocalExportNameRecord)
    {LOGMEIN("ModuleNamespace.cpp] 137\n");
        Recycler* recycler = GetScriptContext()->GetRecycler();
        if (unambiguousNonLocalExports == nullptr)
        {LOGMEIN("ModuleNamespace.cpp] 140\n");
            unambiguousNonLocalExports = RecyclerNew(recycler, UnambiguousExportMap, recycler, 4);
        }
        // keep a local copy of the module/
        unambiguousNonLocalExports->AddNew(propertyId, *nonLocalExportNameRecord);
    }

    BOOL ModuleNamespace::HasProperty(PropertyId propertyId)
    {LOGMEIN("ModuleNamespace.cpp] 148\n");
        SimpleDictionaryPropertyDescriptor<BigPropertyIndex> propertyDescriptor;
        const Js::PropertyRecord* propertyRecord = GetScriptContext()->GetThreadContext()->GetPropertyName(propertyId);
        if (propertyRecord->IsSymbol())
        {LOGMEIN("ModuleNamespace.cpp] 152\n");
            return this->DynamicObject::HasProperty(propertyId);
        }
        if (propertyMap != nullptr && propertyMap->TryGetValue(propertyRecord, &propertyDescriptor))
        {LOGMEIN("ModuleNamespace.cpp] 156\n");
            return TRUE;
        }
        if (unambiguousNonLocalExports != nullptr)
        {LOGMEIN("ModuleNamespace.cpp] 160\n");
            return unambiguousNonLocalExports->ContainsKey(propertyId);
        }
        return FALSE;
    }

    BOOL ModuleNamespace::HasOwnProperty(PropertyId propertyId)
    {LOGMEIN("ModuleNamespace.cpp] 167\n");
        return HasProperty(propertyId);
    }

    BOOL ModuleNamespace::GetProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("ModuleNamespace.cpp] 172\n");
        SimpleDictionaryPropertyDescriptor<BigPropertyIndex> propertyDescriptor;
        const Js::PropertyRecord* propertyRecord = requestContext->GetThreadContext()->GetPropertyName(propertyId);
        if (propertyRecord->IsSymbol())
        {LOGMEIN("ModuleNamespace.cpp] 176\n");
            return this->DynamicObject::GetProperty(originalInstance, propertyId, value, info, requestContext);
        }
        if (propertyMap != nullptr && propertyMap->TryGetValue(propertyRecord, &propertyDescriptor))
        {LOGMEIN("ModuleNamespace.cpp] 180\n");
            Assert((uint)propertyDescriptor.propertyIndex < ((SourceTextModuleRecord*)static_cast<ModuleRecordBase*>(moduleRecord))->GetLocalExportCount());
            PropertyValueInfo::SetNoCache(info, this); // Disable inlinecache for localexport slot for now.
            //if ((PropertyIndex)propertyDescriptor.propertyIndex == propertyDescriptor.propertyIndex)
            //{
            //    PropertyValueInfo::Set(info, this, (PropertyIndex)propertyDescriptor.propertyIndex, propertyDescriptor.Attributes);
            //}
            *value = this->GetNSSlot(propertyDescriptor.propertyIndex);
            return TRUE;
        }
        if (unambiguousNonLocalExports != nullptr)
        {LOGMEIN("ModuleNamespace.cpp] 191\n");
            ModuleNameRecord moduleNameRecord;
            // TODO: maybe we can cache the slot address & offset, instead of looking up everytime? We do need to look up the reference everytime.
            if (unambiguousNonLocalExports->TryGetValue(propertyId, &moduleNameRecord))
            {LOGMEIN("ModuleNamespace.cpp] 195\n");
                return moduleNameRecord.module->GetNamespace()->GetProperty(originalInstance, moduleNameRecord.bindingName, value, info, requestContext);
            }
        }
        return FALSE;
    }

    BOOL ModuleNamespace::GetProperty(Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("ModuleNamespace.cpp] 203\n");
        const PropertyRecord* propertyRecord = nullptr;
        GetScriptContext()->GetOrAddPropertyRecord(propertyNameString->GetString(), propertyNameString->GetLength(), &propertyRecord);
        return GetProperty(originalInstance, propertyRecord->GetPropertyId(), value, info, requestContext);
    }

    BOOL ModuleNamespace::GetInternalProperty(Var instance, PropertyId internalPropertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("ModuleNamespace.cpp] 210\n");
        return FALSE;
    }

    BOOL ModuleNamespace::GetPropertyReference(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("ModuleNamespace.cpp] 215\n");
        return GetProperty(originalInstance, propertyId, value, info, requestContext);
    }

    BOOL ModuleNamespace::GetEnumerator(JavascriptStaticEnumerator * enumerator, EnumeratorFlags flags, ScriptContext* requestContext, ForInCache * forInCache)
    {LOGMEIN("ModuleNamespace.cpp] 220\n");
        ModuleNamespaceEnumerator* moduleEnumerator = ModuleNamespaceEnumerator::New(this, flags, requestContext, forInCache);
        if (moduleEnumerator == nullptr)
        {LOGMEIN("ModuleNamespace.cpp] 223\n");
            return FALSE;
        }
        return enumerator->Initialize(moduleEnumerator, nullptr, nullptr, flags, requestContext, nullptr);
    }

    BOOL ModuleNamespace::DeleteProperty(PropertyId propertyId, PropertyOperationFlags flags)
    {LOGMEIN("ModuleNamespace.cpp] 230\n");
        //Assert: IsPropertyKey(P) is true.
        //Let exports be O.[[Exports]].
        //If P is an element of exports, return false.
        //Return true.
        return !HasProperty(propertyId);
    }

    BOOL ModuleNamespace::DeleteProperty(JavascriptString *propertyNameString, PropertyOperationFlags flags)
    {LOGMEIN("ModuleNamespace.cpp] 239\n");
        //Assert: IsPropertyKey(P) is true.
        //Let exports be O.[[Exports]].
        //If P is an element of exports, return false.
        //Return true.
        PropertyRecord const *propertyRecord = nullptr;
        if (JavascriptOperators::ShouldTryDeleteProperty(this, propertyNameString, &propertyRecord))
        {LOGMEIN("ModuleNamespace.cpp] 246\n");
            Assert(propertyRecord);
            return DeleteProperty(propertyRecord->GetPropertyId(), flags);
        }

        return TRUE;
    }

    BOOL ModuleNamespace::GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {LOGMEIN("ModuleNamespace.cpp] 255\n");
        stringBuilder->AppendCppLiteral(_u("{ModuleNamespaceObject}"));
        return TRUE;
    }

    BOOL ModuleNamespace::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {LOGMEIN("ModuleNamespace.cpp] 261\n");
        stringBuilder->AppendCppLiteral(_u("Object, (ModuleNamespaceObject)"));
        return TRUE;
    }

    Var ModuleNamespace::GetNSSlot(BigPropertyIndex propertyIndex)
    {LOGMEIN("ModuleNamespace.cpp] 267\n");
        Assert((uint)propertyIndex < static_cast<SourceTextModuleRecord*>(static_cast<ModuleRecordBase*>(moduleRecord))->GetLocalExportCount());
        return this->nsSlots[propertyIndex];
    }

    PropertyId ModuleNamespace::GetPropertyId(BigPropertyIndex index)
    {LOGMEIN("ModuleNamespace.cpp] 273\n");
        SimpleDictionaryPropertyDescriptor<BigPropertyIndex> propertyDescriptor;
        if (propertyMap->TryGetValueAt(index, &propertyDescriptor))
        {LOGMEIN("ModuleNamespace.cpp] 276\n");
            const PropertyRecord* propertyRecord = propertyMap->GetKeyAt(index);
            return propertyRecord->GetPropertyId();
        }
        return Constants::NoProperty;
    }

    BOOL ModuleNamespace::FindNextProperty(BigPropertyIndex& index, JavascriptString** propertyString, PropertyId* propertyId, PropertyAttributes* attributes, ScriptContext * requestContext) const
    {LOGMEIN("ModuleNamespace.cpp] 284\n");
        if (index < propertyMap->Count())
        {LOGMEIN("ModuleNamespace.cpp] 286\n");
            SimpleDictionaryPropertyDescriptor<BigPropertyIndex> propertyDescriptor(propertyMap->GetValueAt(index));
            Assert(propertyDescriptor.Attributes == PropertyModuleNamespaceDefault);
            const PropertyRecord* propertyRecord = propertyMap->GetKeyAt(index);
            *propertyId = propertyRecord->GetPropertyId();
            if (propertyString != nullptr)
            {LOGMEIN("ModuleNamespace.cpp] 292\n");
                *propertyString = requestContext->GetPropertyString(*propertyId);
            }
            if (attributes != nullptr)
            {LOGMEIN("ModuleNamespace.cpp] 296\n");
                *attributes = propertyDescriptor.Attributes;
            }
            return TRUE;
        }
        else
        {
            *propertyId = Constants::NoProperty;
            if (propertyString != nullptr)
            {LOGMEIN("ModuleNamespace.cpp] 305\n");
                *propertyString = nullptr;
            }
        }
        return FALSE;
    }

    // We will make sure the iterator will iterate through the exported properties in sorted order.
    // There is no such requirement for enumerator (forin).
    ListForListIterator* ModuleNamespace::EnsureSortedExportedNames()
    {LOGMEIN("ModuleNamespace.cpp] 315\n");
        if (sortedExportedNames == nullptr)
        {LOGMEIN("ModuleNamespace.cpp] 317\n");
            ExportedNames* exportedNames = moduleRecord->GetExportedNames(nullptr);
            ScriptContext* scriptContext = GetScriptContext();
            sortedExportedNames = ListForListIterator::New(scriptContext->GetRecycler());
            exportedNames->Map([&](PropertyId propertyId) {
                JavascriptString* propertyString = scriptContext->GetPropertyString(propertyId);
                sortedExportedNames->Add(propertyString);
            });
            sortedExportedNames->Sort([](void* context, const void* left, const void* right) ->int {
                JavascriptString** leftString = (JavascriptString**) (left);
                JavascriptString** rightString = (JavascriptString**) (right);
                if (JavascriptString::LessThan(*leftString, *rightString))
                {LOGMEIN("ModuleNamespace.cpp] 329\n");
                    return -1;
                }
                if (JavascriptString::LessThan(*rightString, *leftString))
                {LOGMEIN("ModuleNamespace.cpp] 333\n");
                    return 1;
                }
                return 0;
            }, nullptr);
        }
        return sortedExportedNames;
    }

    Var ModuleNamespace::EntrySymbolIterator(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0)
        {LOGMEIN("ModuleNamespace.cpp] 352\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedNamespace, _u("Namespace[Symbol.iterator]"));
        }

        if (JavascriptOperators::GetTypeId(args[0]) != TypeIds_ModuleNamespace)
        {LOGMEIN("ModuleNamespace.cpp] 357\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedNamespace, _u("Namespace[Symbol.iterator]"));
        }

        ModuleNamespace* moduleNamespace = ModuleNamespace::FromVar(args[0]);
        ListForListIterator* sortedExportedNames = moduleNamespace->EnsureSortedExportedNames();
        return scriptContext->GetLibrary()->CreateListIterator(sortedExportedNames);
    }
}
