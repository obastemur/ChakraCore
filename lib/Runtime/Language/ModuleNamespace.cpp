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
    {TRACE_IT(51822);

    }

    ModuleNamespace* ModuleNamespace::GetModuleNamespace(ModuleRecordBase* requestModule)
    {TRACE_IT(51823);
        Assert(requestModule->IsSourceTextModuleRecord());
        SourceTextModuleRecord* moduleRecord = SourceTextModuleRecord::FromHost(requestModule);
        ModuleNamespace* nsObject = moduleRecord->GetNamespace();
        if (nsObject != nullptr)
        {TRACE_IT(51824);
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
    {TRACE_IT(51825);
        ScriptContext* scriptContext = moduleRecord->GetRealm()->GetScriptContext();
        Recycler* recycler = scriptContext->GetRecycler();
        SourceTextModuleRecord* sourceTextModuleRecord = static_cast<SourceTextModuleRecord*>(
            static_cast<ModuleRecordBase*>(moduleRecord));
        JavascriptLibrary* library = GetLibrary();

        if (scriptContext->GetConfig()->IsES6ToStringTagEnabled())
        {TRACE_IT(51826);
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
        {TRACE_IT(51827);
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
                {TRACE_IT(51828);
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
        {TRACE_IT(51829);
            exportedNames->Map([&](PropertyId propertyId) {
                if (!moduleRecord->ResolveExport(propertyId, nullptr, nullptr, &moduleNameRecord))
                {TRACE_IT(51830);
                    // ignore ambigious resolution.
#if DBG
                    unresolvableExportsCount++;
#endif
                    return;
                }
                // non-ambiguous resolution.
                if (moduleNameRecord == nullptr)
                {TRACE_IT(51831);
                    JavascriptError::ThrowSyntaxError(scriptContext, JSERR_ResolveExportFailed, scriptContext->GetPropertyName(propertyId)->GetBuffer());
                }
                if (moduleNameRecord->module == moduleRecord)
                {TRACE_IT(51832);
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
    {TRACE_IT(51833);
        Recycler* recycler = GetScriptContext()->GetRecycler();
        if (unambiguousNonLocalExports == nullptr)
        {TRACE_IT(51834);
            unambiguousNonLocalExports = RecyclerNew(recycler, UnambiguousExportMap, recycler, 4);
        }
        // keep a local copy of the module/
        unambiguousNonLocalExports->AddNew(propertyId, *nonLocalExportNameRecord);
    }

    BOOL ModuleNamespace::HasProperty(PropertyId propertyId)
    {TRACE_IT(51835);
        SimpleDictionaryPropertyDescriptor<BigPropertyIndex> propertyDescriptor;
        const Js::PropertyRecord* propertyRecord = GetScriptContext()->GetThreadContext()->GetPropertyName(propertyId);
        if (propertyRecord->IsSymbol())
        {TRACE_IT(51836);
            return this->DynamicObject::HasProperty(propertyId);
        }
        if (propertyMap != nullptr && propertyMap->TryGetValue(propertyRecord, &propertyDescriptor))
        {TRACE_IT(51837);
            return TRUE;
        }
        if (unambiguousNonLocalExports != nullptr)
        {TRACE_IT(51838);
            return unambiguousNonLocalExports->ContainsKey(propertyId);
        }
        return FALSE;
    }

    BOOL ModuleNamespace::HasOwnProperty(PropertyId propertyId)
    {TRACE_IT(51839);
        return HasProperty(propertyId);
    }

    BOOL ModuleNamespace::GetProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(51840);
        SimpleDictionaryPropertyDescriptor<BigPropertyIndex> propertyDescriptor;
        const Js::PropertyRecord* propertyRecord = requestContext->GetThreadContext()->GetPropertyName(propertyId);
        if (propertyRecord->IsSymbol())
        {TRACE_IT(51841);
            return this->DynamicObject::GetProperty(originalInstance, propertyId, value, info, requestContext);
        }
        if (propertyMap != nullptr && propertyMap->TryGetValue(propertyRecord, &propertyDescriptor))
        {TRACE_IT(51842);
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
        {TRACE_IT(51843);
            ModuleNameRecord moduleNameRecord;
            // TODO: maybe we can cache the slot address & offset, instead of looking up everytime? We do need to look up the reference everytime.
            if (unambiguousNonLocalExports->TryGetValue(propertyId, &moduleNameRecord))
            {TRACE_IT(51844);
                return moduleNameRecord.module->GetNamespace()->GetProperty(originalInstance, moduleNameRecord.bindingName, value, info, requestContext);
            }
        }
        return FALSE;
    }

    BOOL ModuleNamespace::GetProperty(Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(51845);
        const PropertyRecord* propertyRecord = nullptr;
        GetScriptContext()->GetOrAddPropertyRecord(propertyNameString->GetString(), propertyNameString->GetLength(), &propertyRecord);
        return GetProperty(originalInstance, propertyRecord->GetPropertyId(), value, info, requestContext);
    }

    BOOL ModuleNamespace::GetInternalProperty(Var instance, PropertyId internalPropertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(51846);
        return FALSE;
    }

    BOOL ModuleNamespace::GetPropertyReference(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(51847);
        return GetProperty(originalInstance, propertyId, value, info, requestContext);
    }

    BOOL ModuleNamespace::GetEnumerator(JavascriptStaticEnumerator * enumerator, EnumeratorFlags flags, ScriptContext* requestContext, ForInCache * forInCache)
    {TRACE_IT(51848);
        ModuleNamespaceEnumerator* moduleEnumerator = ModuleNamespaceEnumerator::New(this, flags, requestContext, forInCache);
        if (moduleEnumerator == nullptr)
        {TRACE_IT(51849);
            return FALSE;
        }
        return enumerator->Initialize(moduleEnumerator, nullptr, nullptr, flags, requestContext, nullptr);
    }

    BOOL ModuleNamespace::DeleteProperty(PropertyId propertyId, PropertyOperationFlags flags)
    {TRACE_IT(51850);
        //Assert: IsPropertyKey(P) is true.
        //Let exports be O.[[Exports]].
        //If P is an element of exports, return false.
        //Return true.
        return !HasProperty(propertyId);
    }

    BOOL ModuleNamespace::DeleteProperty(JavascriptString *propertyNameString, PropertyOperationFlags flags)
    {TRACE_IT(51851);
        //Assert: IsPropertyKey(P) is true.
        //Let exports be O.[[Exports]].
        //If P is an element of exports, return false.
        //Return true.
        PropertyRecord const *propertyRecord = nullptr;
        if (JavascriptOperators::ShouldTryDeleteProperty(this, propertyNameString, &propertyRecord))
        {TRACE_IT(51852);
            Assert(propertyRecord);
            return DeleteProperty(propertyRecord->GetPropertyId(), flags);
        }

        return TRUE;
    }

    BOOL ModuleNamespace::GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {TRACE_IT(51853);
        stringBuilder->AppendCppLiteral(_u("{ModuleNamespaceObject}"));
        return TRUE;
    }

    BOOL ModuleNamespace::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {TRACE_IT(51854);
        stringBuilder->AppendCppLiteral(_u("Object, (ModuleNamespaceObject)"));
        return TRUE;
    }

    Var ModuleNamespace::GetNSSlot(BigPropertyIndex propertyIndex)
    {TRACE_IT(51855);
        Assert((uint)propertyIndex < static_cast<SourceTextModuleRecord*>(static_cast<ModuleRecordBase*>(moduleRecord))->GetLocalExportCount());
        return this->nsSlots[propertyIndex];
    }

    PropertyId ModuleNamespace::GetPropertyId(BigPropertyIndex index)
    {TRACE_IT(51856);
        SimpleDictionaryPropertyDescriptor<BigPropertyIndex> propertyDescriptor;
        if (propertyMap->TryGetValueAt(index, &propertyDescriptor))
        {TRACE_IT(51857);
            const PropertyRecord* propertyRecord = propertyMap->GetKeyAt(index);
            return propertyRecord->GetPropertyId();
        }
        return Constants::NoProperty;
    }

    BOOL ModuleNamespace::FindNextProperty(BigPropertyIndex& index, JavascriptString** propertyString, PropertyId* propertyId, PropertyAttributes* attributes, ScriptContext * requestContext) const
    {TRACE_IT(51858);
        if (index < propertyMap->Count())
        {TRACE_IT(51859);
            SimpleDictionaryPropertyDescriptor<BigPropertyIndex> propertyDescriptor(propertyMap->GetValueAt(index));
            Assert(propertyDescriptor.Attributes == PropertyModuleNamespaceDefault);
            const PropertyRecord* propertyRecord = propertyMap->GetKeyAt(index);
            *propertyId = propertyRecord->GetPropertyId();
            if (propertyString != nullptr)
            {TRACE_IT(51860);
                *propertyString = requestContext->GetPropertyString(*propertyId);
            }
            if (attributes != nullptr)
            {TRACE_IT(51861);
                *attributes = propertyDescriptor.Attributes;
            }
            return TRUE;
        }
        else
        {TRACE_IT(51862);
            *propertyId = Constants::NoProperty;
            if (propertyString != nullptr)
            {TRACE_IT(51863);
                *propertyString = nullptr;
            }
        }
        return FALSE;
    }

    // We will make sure the iterator will iterate through the exported properties in sorted order.
    // There is no such requirement for enumerator (forin).
    ListForListIterator* ModuleNamespace::EnsureSortedExportedNames()
    {TRACE_IT(51864);
        if (sortedExportedNames == nullptr)
        {TRACE_IT(51865);
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
                {TRACE_IT(51866);
                    return -1;
                }
                if (JavascriptString::LessThan(*rightString, *leftString))
                {TRACE_IT(51867);
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
        {TRACE_IT(51868);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedNamespace, _u("Namespace[Symbol.iterator]"));
        }

        if (JavascriptOperators::GetTypeId(args[0]) != TypeIds_ModuleNamespace)
        {TRACE_IT(51869);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedNamespace, _u("Namespace[Symbol.iterator]"));
        }

        ModuleNamespace* moduleNamespace = ModuleNamespace::FromVar(args[0]);
        ListForListIterator* sortedExportedNames = moduleNamespace->EnsureSortedExportedNames();
        return scriptContext->GetLibrary()->CreateListIterator(sortedExportedNames);
    }
}
