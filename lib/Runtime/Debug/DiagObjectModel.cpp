//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeDebugPch.h"

// Parser includes
#include "CharClassifier.h"
// TODO: clean up the need of these regex related header here just for GroupInfo needed in JavascriptRegExpConstructor
#include "RegexCommon.h"

// Runtime includes
#include "Library/ObjectPrototypeObject.h"
#include "Library/JavascriptNumberObject.h"
#include "Library/BoundFunction.h"
#include "Library/JavascriptRegExpConstructor.h"
#include "Library/SameValueComparer.h"
#include "Library/MapOrSetDataList.h"
#include "Library/JavascriptPromise.h"
#include "Library/JavascriptProxy.h"
#include "Library/JavascriptMap.h"
#include "Library/JavascriptSet.h"
#include "Library/JavascriptWeakMap.h"
#include "Library/JavascriptWeakSet.h"
#include "Library/ArgumentsObject.h"

#include "Types/DynamicObjectPropertyEnumerator.h"
#include "Types/JavascriptStaticEnumerator.h"
#include "Library/ForInObjectEnumerator.h"
#include "Library/ES5Array.h"
#include "Library/SimdLib.h"

namespace Js
{
#define RETURN_VALUE_MAX_NAME   255
#define PENDING_MUTATION_VALUE_MAX_NAME   255

    ArenaAllocator *GetArenaFromContext(ScriptContext *scriptContext)
    {LOGMEIN("DiagObjectModel.cpp] 38\n");
        Assert(scriptContext);
        return scriptContext->GetThreadContext()->GetDebugManager()->GetDiagnosticArena()->Arena();
    }

    template <class T>
    WeakArenaReference<IDiagObjectModelWalkerBase>* CreateAWalker(ScriptContext * scriptContext, Var instance, Var originalInstance)
    {LOGMEIN("DiagObjectModel.cpp] 45\n");
        ReferencedArenaAdapter* pRefArena = scriptContext->GetThreadContext()->GetDebugManager()->GetDiagnosticArena();
        if (pRefArena)
        {LOGMEIN("DiagObjectModel.cpp] 48\n");
            IDiagObjectModelWalkerBase* pOMWalker = Anew(pRefArena->Arena(), T, scriptContext, instance, originalInstance);
            return HeapNew(WeakArenaReference<IDiagObjectModelWalkerBase>,pRefArena, pOMWalker);
        }
        return nullptr;
    }
    //-----------------------
    // ResolvedObject


    WeakArenaReference<IDiagObjectModelDisplay>* ResolvedObject::GetObjectDisplay()
    {LOGMEIN("DiagObjectModel.cpp] 59\n");
        AssertMsg(typeId != TypeIds_HostDispatch, "Bad usage of ResolvedObject::GetObjectDisplay");

        IDiagObjectModelDisplay* pOMDisplay = (this->objectDisplay != nullptr) ? this->objectDisplay : CreateDisplay();
        Assert(pOMDisplay);

        return HeapNew(WeakArenaReference<IDiagObjectModelDisplay>, scriptContext->GetThreadContext()->GetDebugManager()->GetDiagnosticArena(), pOMDisplay);
    }

    IDiagObjectModelDisplay * ResolvedObject::CreateDisplay()
    {LOGMEIN("DiagObjectModel.cpp] 69\n");
        IDiagObjectModelDisplay* pOMDisplay = nullptr;
        ReferencedArenaAdapter* pRefArena = scriptContext->GetThreadContext()->GetDebugManager()->GetDiagnosticArena();

        if (Js::TypedArrayBase::Is(obj))
        {LOGMEIN("DiagObjectModel.cpp] 74\n");
            pOMDisplay = Anew(pRefArena->Arena(), RecyclableTypedArrayDisplay, this);
        }
        else if (Js::ES5Array::Is(obj))
        {LOGMEIN("DiagObjectModel.cpp] 78\n");
            pOMDisplay = Anew(pRefArena->Arena(), RecyclableES5ArrayDisplay, this);
        }
        else if (Js::JavascriptArray::Is(obj))
        {LOGMEIN("DiagObjectModel.cpp] 82\n");
            // DisableJIT-TODO: Review- is this correct?
#if ENABLE_COPYONACCESS_ARRAY
            // Make sure any NativeIntArrays are converted
            Js::JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(obj);
#endif
            pOMDisplay = Anew(pRefArena->Arena(), RecyclableArrayDisplay, this);
        }
#ifdef ENABLE_SIMDJS
        else if (Js::JavascriptSIMDInt32x4::Is(obj))
        {LOGMEIN("DiagObjectModel.cpp] 92\n");
            pOMDisplay = Anew(pRefArena->Arena(), RecyclableSimdInt32x4ObjectDisplay, this);
        }
        else if (Js::JavascriptSIMDFloat32x4::Is(obj))
        {LOGMEIN("DiagObjectModel.cpp] 96\n");
            pOMDisplay = Anew(pRefArena->Arena(), RecyclableSimdFloat32x4ObjectDisplay, this);
        }
        else if (Js::JavascriptSIMDInt8x16::Is(obj))
        {LOGMEIN("DiagObjectModel.cpp] 100\n");
            pOMDisplay = Anew(pRefArena->Arena(), RecyclableSimdInt8x16ObjectDisplay, this);
        }
        else if (Js::JavascriptSIMDInt16x8::Is(obj))
        {LOGMEIN("DiagObjectModel.cpp] 104\n");
            pOMDisplay = Anew(pRefArena->Arena(), RecyclableSimdInt16x8ObjectDisplay, this);
        }
        else if (Js::JavascriptSIMDBool32x4::Is(obj))
        {LOGMEIN("DiagObjectModel.cpp] 108\n");
            pOMDisplay = Anew(pRefArena->Arena(), RecyclableSimdBool32x4ObjectDisplay, this);
        }
        else if (Js::JavascriptSIMDBool8x16::Is(obj))
        {LOGMEIN("DiagObjectModel.cpp] 112\n");
            pOMDisplay = Anew(pRefArena->Arena(), RecyclableSimdBool8x16ObjectDisplay, this);
        }
        else if (Js::JavascriptSIMDBool16x8::Is(obj))
        {LOGMEIN("DiagObjectModel.cpp] 116\n");
            pOMDisplay = Anew(pRefArena->Arena(), RecyclableSimdBool16x8ObjectDisplay, this);
        }
        else if (Js::JavascriptSIMDUint32x4::Is(obj))
        {LOGMEIN("DiagObjectModel.cpp] 120\n");
            pOMDisplay = Anew(pRefArena->Arena(), RecyclableSimdUint32x4ObjectDisplay, this);
        }
        else if (Js::JavascriptSIMDUint8x16::Is(obj))
        {LOGMEIN("DiagObjectModel.cpp] 124\n");
            pOMDisplay = Anew(pRefArena->Arena(), RecyclableSimdUint8x16ObjectDisplay, this);
        }
        else if (Js::JavascriptSIMDUint16x8::Is(obj))
        {LOGMEIN("DiagObjectModel.cpp] 128\n");
            pOMDisplay = Anew(pRefArena->Arena(), RecyclableSimdUint16x8ObjectDisplay, this);
        }
#endif
        else
        {
            pOMDisplay = Anew(pRefArena->Arena(), RecyclableObjectDisplay, this);
        }

        if (this->isConst || this->propId == Js::PropertyIds::_superReferenceSymbol || this->propId == Js::PropertyIds::_superCtorReferenceSymbol)
        {LOGMEIN("DiagObjectModel.cpp] 138\n");
            pOMDisplay->SetDefaultTypeAttribute(DBGPROP_ATTRIB_VALUE_READONLY);
        }

        return pOMDisplay;
    }

    bool ResolvedObject::IsInDeadZone() const
    {LOGMEIN("DiagObjectModel.cpp] 146\n");
        Assert(scriptContext);
        return this->obj == scriptContext->GetLibrary()->GetDebuggerDeadZoneBlockVariableString();
    }

    //-----------------------
    // LocalsDisplay


    LocalsDisplay::LocalsDisplay(DiagStackFrame* _frame)
        : pFrame(_frame)
    {LOGMEIN("DiagObjectModel.cpp] 157\n");
    }

    LPCWSTR LocalsDisplay::Name()
    {LOGMEIN("DiagObjectModel.cpp] 161\n");
        return _u("Locals");
    }

    LPCWSTR LocalsDisplay::Type()
    {LOGMEIN("DiagObjectModel.cpp] 166\n");
        return _u("");
    }

    LPCWSTR LocalsDisplay::Value(int radix)
    {LOGMEIN("DiagObjectModel.cpp] 171\n");
        return _u("Locals");
    }

    BOOL LocalsDisplay::HasChildren()
    {LOGMEIN("DiagObjectModel.cpp] 176\n");
        Js::JavascriptFunction* func = pFrame->GetJavascriptFunction();

        FunctionBody* function = func->GetFunctionBody();
        return function && function->GetLocalsCount() != 0;
    }

    DBGPROP_ATTRIB_FLAGS LocalsDisplay::GetTypeAttribute()
    {LOGMEIN("DiagObjectModel.cpp] 184\n");
        return DBGPROP_ATTRIB_NO_ATTRIB;
    }

    BOOL LocalsDisplay::Set(Var updateObject)
    {LOGMEIN("DiagObjectModel.cpp] 189\n");
        // This is the hidden root object for Locals it doesn't get updated.
        return FALSE;
    }

    WeakArenaReference<IDiagObjectModelWalkerBase>* LocalsDisplay::CreateWalker()
    {LOGMEIN("DiagObjectModel.cpp] 195\n");
        ReferencedArenaAdapter* pRefArena = pFrame->GetScriptContext()->GetThreadContext()->GetDebugManager()->GetDiagnosticArena();
        if (pRefArena)
        {LOGMEIN("DiagObjectModel.cpp] 198\n");
            IDiagObjectModelWalkerBase * pOMWalker = nullptr;

            IGNORE_STACKWALK_EXCEPTION(scriptContext);
            pOMWalker = Anew(pRefArena->Arena(), LocalsWalker, pFrame, FrameWalkerFlags::FW_MakeGroups);

            return HeapNew(WeakArenaReference<IDiagObjectModelWalkerBase>,pRefArena, pOMWalker);
        }
        return nullptr;
    }

    // Variables on the scope or in current function.

    /*static*/
    BOOL VariableWalkerBase::GetExceptionObject(int &index, DiagStackFrame* frame, ResolvedObject* pResolvedObject)
    {LOGMEIN("DiagObjectModel.cpp] 213\n");
        Assert(pResolvedObject);
        Assert(pResolvedObject->scriptContext);
        Assert(frame);
        Assert(index >= 0);

        if (HasExceptionObject(frame))
        {LOGMEIN("DiagObjectModel.cpp] 220\n");
            if (index == 0)
            {LOGMEIN("DiagObjectModel.cpp] 222\n");
                pResolvedObject->name          = _u("{exception}");
                pResolvedObject->typeId        = TypeIds_Error;
                pResolvedObject->address       = nullptr;
                pResolvedObject->obj           = pResolvedObject->scriptContext->GetDebugContext()->GetProbeContainer()->GetExceptionObject();

                if (pResolvedObject->obj == nullptr)
                {LOGMEIN("DiagObjectModel.cpp] 229\n");
                    Assert(false);
                    pResolvedObject->obj = pResolvedObject->scriptContext->GetLibrary()->GetUndefined();
                }
                return TRUE;
            }

            // Adjust the index
            index -= 1;
        }

        return FALSE;
    }

    /*static*/
    bool VariableWalkerBase::HasExceptionObject(DiagStackFrame* frame)
    {LOGMEIN("DiagObjectModel.cpp] 245\n");
        Assert(frame);
        Assert(frame->GetScriptContext());

        return frame->GetScriptContext()->GetDebugContext()->GetProbeContainer()->GetExceptionObject() != nullptr;
    }

    /*static*/
    void VariableWalkerBase::GetReturnedValueResolvedObject(ReturnedValue * returnValue, DiagStackFrame* frame, ResolvedObject* pResolvedObject)
    {LOGMEIN("DiagObjectModel.cpp] 254\n");
        DBGPROP_ATTRIB_FLAGS defaultAttributes = DBGPROP_ATTRIB_VALUE_IS_RETURN_VALUE | DBGPROP_ATTRIB_VALUE_IS_FAKE;
        WCHAR * finalName = AnewArray(GetArenaFromContext(pResolvedObject->scriptContext), WCHAR, RETURN_VALUE_MAX_NAME);
        if (returnValue->isValueOfReturnStatement)
        {
            swprintf_s(finalName, RETURN_VALUE_MAX_NAME, _u("[Return value]"));
            pResolvedObject->obj = frame->GetRegValue(Js::FunctionBody::ReturnValueRegSlot);
            pResolvedObject->address = Anew(frame->GetArena(), LocalObjectAddressForRegSlot, frame, Js::FunctionBody::ReturnValueRegSlot, pResolvedObject->obj);
        }
        else
        {
            if (returnValue->calledFunction->IsScriptFunction())
            {
                swprintf_s(finalName, RETURN_VALUE_MAX_NAME, _u("[%s returned]"), returnValue->calledFunction->GetFunctionBody()->GetDisplayName());
            }
            else
            {
                Js::JavascriptString *builtInName = returnValue->calledFunction->GetDisplayName();
                swprintf_s(finalName, RETURN_VALUE_MAX_NAME, _u("[%s returned]"), builtInName->GetSz());
            }
            pResolvedObject->obj = returnValue->returnedValue;
            defaultAttributes |= DBGPROP_ATTRIB_VALUE_READONLY;
            pResolvedObject->address = nullptr;
        }
        Assert(pResolvedObject->obj != nullptr);

        pResolvedObject->name = finalName;
        pResolvedObject->typeId = TypeIds_Object;

        pResolvedObject->objectDisplay = pResolvedObject->CreateDisplay();
        pResolvedObject->objectDisplay->SetDefaultTypeAttribute(defaultAttributes);
    }

    /*static*/
    BOOL VariableWalkerBase::GetReturnedValue(int &index, DiagStackFrame* frame, ResolvedObject* pResolvedObject)
    {LOGMEIN("DiagObjectModel.cpp] 289\n");
        Assert(pResolvedObject);
        Assert(pResolvedObject->scriptContext);
        Assert(frame);
        Assert(index >= 0);
        ReturnedValueList *returnedValueList = frame->GetScriptContext()->GetDebugContext()->GetProbeContainer()->GetReturnedValueList();

        if (returnedValueList != nullptr && returnedValueList->Count() > 0 && frame->IsTopFrame())
        {LOGMEIN("DiagObjectModel.cpp] 297\n");
            if (index < returnedValueList->Count())
            {LOGMEIN("DiagObjectModel.cpp] 299\n");
                ReturnedValue * returnValue = returnedValueList->Item(index);
                VariableWalkerBase::GetReturnedValueResolvedObject(returnValue, frame, pResolvedObject);
                return TRUE;
            }

            // Adjust the index
            index -= returnedValueList->Count();
        }

        return FALSE;
    }

    /*static*/
    int  VariableWalkerBase::GetReturnedValueCount(DiagStackFrame* frame)
    {LOGMEIN("DiagObjectModel.cpp] 314\n");
        Assert(frame);
        Assert(frame->GetScriptContext());

        ReturnedValueList *returnedValueList = frame->GetScriptContext()->GetDebugContext()->GetProbeContainer()->GetReturnedValueList();
        return returnedValueList != nullptr && frame->IsTopFrame() ? returnedValueList->Count() : 0;
    }

#ifdef ENABLE_MUTATION_BREAKPOINT
    BOOL VariableWalkerBase::GetBreakMutationBreakpointValue(int &index, DiagStackFrame* frame, ResolvedObject* pResolvedObject)
    {LOGMEIN("DiagObjectModel.cpp] 324\n");
        Assert(pResolvedObject);
        Assert(pResolvedObject->scriptContext);
        Assert(frame);
        Assert(index >= 0);

        Js::MutationBreakpoint *mutationBreakpoint = frame->GetScriptContext()->GetDebugContext()->GetProbeContainer()->GetDebugManager()->GetActiveMutationBreakpoint();

        if (mutationBreakpoint != nullptr)
        {LOGMEIN("DiagObjectModel.cpp] 333\n");
            if (index == 0)
            {LOGMEIN("DiagObjectModel.cpp] 335\n");
                pResolvedObject->name = _u("[Pending Mutation]");
                pResolvedObject->typeId = TypeIds_Object;
                pResolvedObject->address = nullptr;
                pResolvedObject->obj = mutationBreakpoint->GetMutationObjectVar();
                ReferencedArenaAdapter* pRefArena = pResolvedObject->scriptContext->GetThreadContext()->GetDebugManager()->GetDiagnosticArena();
                pResolvedObject->objectDisplay = Anew(pRefArena->Arena(), PendingMutationBreakpointDisplay, pResolvedObject, mutationBreakpoint->GetBreakMutationType());
                pResolvedObject->objectDisplay->SetDefaultTypeAttribute(DBGPROP_ATTRIB_VALUE_PENDING_MUTATION | DBGPROP_ATTRIB_VALUE_READONLY | DBGPROP_ATTRIB_VALUE_IS_FAKE);
                return TRUE;
            }
            index -= 1; // Adjust the index
        }

        return FALSE;
    }

    uint  VariableWalkerBase::GetBreakMutationBreakpointsCount(DiagStackFrame* frame)
    {LOGMEIN("DiagObjectModel.cpp] 352\n");
        Assert(frame);
        Assert(frame->GetScriptContext());

        return frame->GetScriptContext()->GetDebugContext()->GetProbeContainer()->GetDebugManager()->GetActiveMutationBreakpoint() != nullptr ? 1 : 0;
    }
#endif
    BOOL VariableWalkerBase::Get(int i, ResolvedObject* pResolvedObject)
    {
        AssertMsg(pResolvedObject, "Bad usage of VariableWalkerBase::Get");

        Assert(pFrame);
        pResolvedObject->scriptContext    = pFrame->GetScriptContext();

        if (i < 0)
        {LOGMEIN("DiagObjectModel.cpp] 367\n");
            return FALSE;
        }

        if (GetMemberCount() > i)
        {LOGMEIN("DiagObjectModel.cpp] 372\n");
            pResolvedObject->propId           = pMembersList->Item(i)->propId;
            Assert(pResolvedObject->propId != Js::Constants::NoProperty);
            Assert(!Js::IsInternalPropertyId(pResolvedObject->propId));

            if (pResolvedObject->propId == Js::PropertyIds::_superReferenceSymbol || pResolvedObject->propId == Js::PropertyIds::_superCtorReferenceSymbol)
            {LOGMEIN("DiagObjectModel.cpp] 378\n");
                pResolvedObject->name         = _u("super");
            }
            else
            {
                const Js::PropertyRecord* propertyRecord = pResolvedObject->scriptContext->GetPropertyName(pResolvedObject->propId);
                pResolvedObject->name         = propertyRecord->GetBuffer();
            }


            pResolvedObject->obj              = GetVarObjectAt(i);
            Assert(pResolvedObject->obj);

            pResolvedObject->typeId           = JavascriptOperators::GetTypeId(pResolvedObject->obj);

            pResolvedObject->address          = GetObjectAddress(i);
            pResolvedObject->isConst          = IsConstAt(i);

            pResolvedObject->objectDisplay    = nullptr;
            return TRUE;
        }

        return FALSE;
    }

    Var VariableWalkerBase::GetVarObjectAt(int index)
    {LOGMEIN("DiagObjectModel.cpp] 404\n");
        Assert(index < pMembersList->Count());
        return pMembersList->Item(index)->aVar;
    }

    bool VariableWalkerBase::IsConstAt(int index)
    {LOGMEIN("DiagObjectModel.cpp] 410\n");
        Assert(index < pMembersList->Count());
        DebuggerPropertyDisplayInfo* displayInfo = pMembersList->Item(index);

        // Dead zone variables are also displayed as read only.
        return displayInfo->IsConst() || displayInfo->IsInDeadZone();
    }

    uint32 VariableWalkerBase::GetChildrenCount()
    {LOGMEIN("DiagObjectModel.cpp] 419\n");
        PopulateMembers();
        return GetMemberCount();
    }

    BOOL VariableWalkerBase::GetGroupObject(ResolvedObject* pResolvedObject)
    {LOGMEIN("DiagObjectModel.cpp] 425\n");
        if (!IsInGroup()) return FALSE;

        Assert(pResolvedObject);

        // This is fake [Methods] object.
        pResolvedObject->name           = groupType == UIGroupType_Scope ? _u("[Scope]") : _u("[Globals]");
        pResolvedObject->obj            = Js::RecyclableObject::FromVar(instance);
        pResolvedObject->typeId         = TypeIds_Function;
        pResolvedObject->address        = nullptr;  // Scope object should not be editable

        ArenaAllocator *arena = GetArenaFromContext(pResolvedObject->scriptContext);
        Assert(arena);

        if (groupType == UIGroupType_Scope)
        {LOGMEIN("DiagObjectModel.cpp] 440\n");
            pResolvedObject->objectDisplay = Anew(arena, ScopeVariablesGroupDisplay, this, pResolvedObject);
        }
        else
        {
            pResolvedObject->objectDisplay = Anew(arena, GlobalsScopeVariablesGroupDisplay, this, pResolvedObject);
        }

        return TRUE;
    }

    IDiagObjectAddress *VariableWalkerBase::FindPropertyAddress(PropertyId propId, bool& isConst)
    {LOGMEIN("DiagObjectModel.cpp] 452\n");
        PopulateMembers();
        if (pMembersList)
        {LOGMEIN("DiagObjectModel.cpp] 455\n");
            for (int i = 0; i < pMembersList->Count(); i++)
            {LOGMEIN("DiagObjectModel.cpp] 457\n");
                DebuggerPropertyDisplayInfo *pair = pMembersList->Item(i);
                Assert(pair);
                if (pair->propId == propId)
                {LOGMEIN("DiagObjectModel.cpp] 461\n");
                    isConst = pair->IsConst();
                    return GetObjectAddress(i);
                }
            }
        }
        return nullptr;
    }

    // Determines if the given property is valid for display in the locals window.
    // Cases in which the property is valid are:
    // 1. It is not represented by an internal property.
    // 2. It is a var property.
    // 3. It is a let/const property in scope and is not in a dead zone (assuming isInDeadZone is nullptr).
    // (Determines if the given property is currently in block scope and not in a dead zone.)
    bool VariableWalkerBase::IsPropertyValid(PropertyId propertyId, RegSlot location, bool *isPropertyInDebuggerScope, bool* isConst, bool* isInDeadZone) const
    {LOGMEIN("DiagObjectModel.cpp] 477\n");
        Assert(isPropertyInDebuggerScope);
        Assert(isConst);
        *isPropertyInDebuggerScope = false;

        // Default to writable (for the case of vars and internal properties).
        *isConst = false;


        if (!allowLexicalThis && (propertyId == Js::PropertyIds::_lexicalThisSlotSymbol || propertyId == Js::PropertyIds::_lexicalNewTargetSymbol))
        {LOGMEIN("DiagObjectModel.cpp] 487\n");
            return false;
        }

        if (!allowSuperReference && (propertyId == Js::PropertyIds::_superReferenceSymbol || propertyId == Js::PropertyIds::_superCtorReferenceSymbol))
        {LOGMEIN("DiagObjectModel.cpp] 492\n");
            return false;
        }

        if (Js::IsInternalPropertyId(propertyId))
        {LOGMEIN("DiagObjectModel.cpp] 497\n");
            return false;
        }

        Assert(pFrame);
        Js::FunctionBody *pFBody = pFrame->GetJavascriptFunction()->GetFunctionBody();

        if (pFBody && pFBody->GetScopeObjectChain())
        {LOGMEIN("DiagObjectModel.cpp] 505\n");
            int offset = GetAdjustedByteCodeOffset();

            if (pFBody->GetScopeObjectChain()->TryGetDebuggerScopePropertyInfo(
                propertyId,
                location,
                offset,
                isPropertyInDebuggerScope,
                isConst,
                isInDeadZone))
            {LOGMEIN("DiagObjectModel.cpp] 515\n");
                return true;
            }
        }

        // If the register was not found in any scopes, then it's a var and should be in scope.
        return !*isPropertyInDebuggerScope;
    }

    int VariableWalkerBase::GetAdjustedByteCodeOffset() const
    {LOGMEIN("DiagObjectModel.cpp] 525\n");
        return LocalsWalker::GetAdjustedByteCodeOffset(pFrame);
    }

    DebuggerScope * VariableWalkerBase::GetScopeWhenHaltAtFormals()
    {LOGMEIN("DiagObjectModel.cpp] 530\n");
        if (IsWalkerForCurrentFrame())
        {LOGMEIN("DiagObjectModel.cpp] 532\n");
            return LocalsWalker::GetScopeWhenHaltAtFormals(pFrame);
        }

        return nullptr;
    }

    // Allocates and returns a property display info.
    DebuggerPropertyDisplayInfo* VariableWalkerBase::AllocateNewPropertyDisplayInfo(PropertyId propertyId, Var value, bool isConst, bool isInDeadZone)
    {LOGMEIN("DiagObjectModel.cpp] 541\n");
        Assert(pFrame);
        Assert(value);
        Assert(isInDeadZone || !pFrame->GetScriptContext()->IsUndeclBlockVar(value));

        DWORD flags = DebuggerPropertyDisplayInfoFlags_None;
        flags |= isConst ? DebuggerPropertyDisplayInfoFlags_Const : 0;
        flags |= isInDeadZone ? DebuggerPropertyDisplayInfoFlags_InDeadZone : 0;

        ArenaAllocator *arena = pFrame->GetArena();

        if (isInDeadZone)
        {LOGMEIN("DiagObjectModel.cpp] 553\n");
            value = pFrame->GetScriptContext()->GetLibrary()->GetDebuggerDeadZoneBlockVariableString();
        }

        return Anew(arena, DebuggerPropertyDisplayInfo, propertyId, value, flags);
    }

    /// Slot array

    void SlotArrayVariablesWalker::PopulateMembers()
    {LOGMEIN("DiagObjectModel.cpp] 563\n");
        if (pMembersList == nullptr && instance != nullptr)
        {LOGMEIN("DiagObjectModel.cpp] 565\n");
            ArenaAllocator *arena = pFrame->GetArena();

            ScopeSlots slotArray = GetSlotArray();

            if (slotArray.IsFunctionScopeSlotArray())
            {LOGMEIN("DiagObjectModel.cpp] 571\n");
                DebuggerScope *formalScope = GetScopeWhenHaltAtFormals();
                Js::FunctionBody *pFBody = slotArray.GetFunctionInfo()->GetFunctionBody();
                uint slotArrayCount = slotArray.GetCount();

                if (formalScope != nullptr && !pFBody->IsParamAndBodyScopeMerged())
                {LOGMEIN("DiagObjectModel.cpp] 577\n");
                    Assert(pFBody->paramScopeSlotArraySize > 0);
                    pMembersList = JsUtil::List<DebuggerPropertyDisplayInfo *, ArenaAllocator>::New(arena, slotArrayCount);

                    for (uint32 i = 0; i < slotArrayCount; i++)
                    {LOGMEIN("DiagObjectModel.cpp] 582\n");
                        Js::DebuggerScopeProperty scopeProperty = formalScope->scopeProperties->Item(i);

                        Var value = slotArray.Get(i);
                        bool isInDeadZone = pFrame->GetScriptContext()->IsUndeclBlockVar(value);

                        DebuggerPropertyDisplayInfo *pair = AllocateNewPropertyDisplayInfo(
                            scopeProperty.propId,
                            value,
                            false/*isConst*/,
                            isInDeadZone);

                        Assert(pair != nullptr);
                        pMembersList->Add(pair);
                    }
                }
                else if (pFBody->GetPropertyIdsForScopeSlotArray() != nullptr)
                {LOGMEIN("DiagObjectModel.cpp] 599\n");
                    pMembersList = JsUtil::List<DebuggerPropertyDisplayInfo *, ArenaAllocator>::New(arena, slotArrayCount);

                    for (uint32 i = 0; i < slotArrayCount; i++)
                    {LOGMEIN("DiagObjectModel.cpp] 603\n");
                        Js::PropertyId propertyId = pFBody->GetPropertyIdsForScopeSlotArray()[i];
                        bool isConst = false;
                        bool isPropertyInDebuggerScope = false;
                        bool isInDeadZone = false;
                        if (propertyId != Js::Constants::NoProperty && IsPropertyValid(propertyId, i, &isPropertyInDebuggerScope, &isConst, &isInDeadZone))
                        {LOGMEIN("DiagObjectModel.cpp] 609\n");
                            if (formalScope == nullptr || formalScope->HasProperty(propertyId))
                            {LOGMEIN("DiagObjectModel.cpp] 611\n");
                                Var value = slotArray.Get(i);

                                if (pFrame->GetScriptContext()->IsUndeclBlockVar(value))
                                {LOGMEIN("DiagObjectModel.cpp] 615\n");
                                    isInDeadZone = true;
                                }

                                DebuggerPropertyDisplayInfo *pair = AllocateNewPropertyDisplayInfo(
                                    propertyId,
                                    value,
                                    isConst,
                                    isInDeadZone);

                                Assert(pair != nullptr);
                                pMembersList->Add(pair);
                            }
                        }
                    }
                }
            }
            else
            {
                DebuggerScope* debuggerScope = slotArray.GetDebuggerScope();

                AssertMsg(debuggerScope, "Slot array debugger scope is missing but should be created.");
                pMembersList = JsUtil::List<DebuggerPropertyDisplayInfo *, ArenaAllocator>::New(arena);
                if (debuggerScope->HasProperties())
                {LOGMEIN("DiagObjectModel.cpp] 639\n");
                    debuggerScope->scopeProperties->Map([&] (int i, Js::DebuggerScopeProperty& scopeProperty)
                    {
                        Var value = slotArray.Get(scopeProperty.location);
                        bool isConst = scopeProperty.IsConst();
                        bool isInDeadZone = false;

                        if (pFrame->GetScriptContext()->IsUndeclBlockVar(value))
                        {LOGMEIN("DiagObjectModel.cpp] 647\n");
                            isInDeadZone = true;
                        }

                        DebuggerPropertyDisplayInfo *pair = AllocateNewPropertyDisplayInfo(
                            scopeProperty.propId,
                            value,
                            isConst,
                            isInDeadZone);

                        Assert(pair != nullptr);
                        pMembersList->Add(pair);
                    });
                }
            }
        }
    }

    IDiagObjectAddress * SlotArrayVariablesWalker::GetObjectAddress(int index)
    {LOGMEIN("DiagObjectModel.cpp] 666\n");
        Assert(index < pMembersList->Count());
        ScopeSlots slotArray = GetSlotArray();
        return Anew(pFrame->GetArena(), LocalObjectAddressForSlot, slotArray, index, pMembersList->Item(index)->aVar);
    }

    // Regslot

    void RegSlotVariablesWalker::PopulateMembers()
    {LOGMEIN("DiagObjectModel.cpp] 675\n");
        if (pMembersList == nullptr)
        {LOGMEIN("DiagObjectModel.cpp] 677\n");
            Js::FunctionBody *pFBody = pFrame->GetJavascriptFunction()->GetFunctionBody();
            ArenaAllocator *arena = pFrame->GetArena();

            PropertyIdOnRegSlotsContainer *propIdContainer = pFBody->GetPropertyIdOnRegSlotsContainer();

            DebuggerScope *formalScope = GetScopeWhenHaltAtFormals();

            // this container can be nullptr if there is no locals in current function.
            if (propIdContainer != nullptr)
            {LOGMEIN("DiagObjectModel.cpp] 687\n");
                pMembersList = JsUtil::List<DebuggerPropertyDisplayInfo *, ArenaAllocator>::New(arena);
                for (uint i = 0; i < propIdContainer->length; i++)
                {LOGMEIN("DiagObjectModel.cpp] 690\n");
                    Js::PropertyId propertyId;
                    RegSlot reg;
                    propIdContainer->FetchItemAt(i, pFBody, &propertyId, &reg);
                    bool shouldInsert = false;
                    bool isConst = false;
                    bool isInDeadZone = false;

                    if (this->debuggerScope)
                    {LOGMEIN("DiagObjectModel.cpp] 699\n");
                        DebuggerScopeProperty debuggerScopeProperty;
                        if (this->debuggerScope->TryGetValidProperty(propertyId, reg, GetAdjustedByteCodeOffset(), &debuggerScopeProperty, &isInDeadZone))
                        {LOGMEIN("DiagObjectModel.cpp] 702\n");
                            isConst = debuggerScopeProperty.IsConst();
                            shouldInsert = true;
                        }
                    }
                    else
                    {
                        bool isPropertyInDebuggerScope = false;
                        shouldInsert = IsPropertyValid(propertyId, reg, &isPropertyInDebuggerScope, &isConst, &isInDeadZone) && !isPropertyInDebuggerScope;
                    }

                    if (shouldInsert && formalScope != nullptr)
                    {LOGMEIN("DiagObjectModel.cpp] 714\n");
                        shouldInsert = formalScope->HasProperty(propertyId);
                    }

                    if (shouldInsert)
                    {LOGMEIN("DiagObjectModel.cpp] 719\n");
                        Var value = pFrame->GetRegValue(reg);

                        // If the user didn't supply an arguments object, a fake one will
                        // be created when evaluating LocalsWalker::ShouldInsertFakeArguments().
                        if (!(propertyId == PropertyIds::arguments && value == nullptr))
                        {LOGMEIN("DiagObjectModel.cpp] 725\n");
                            if (pFrame->GetScriptContext()->IsUndeclBlockVar(value))
                            {LOGMEIN("DiagObjectModel.cpp] 727\n");
                                isInDeadZone = true;
                            }

                            DebuggerPropertyDisplayInfo *info = AllocateNewPropertyDisplayInfo(
                                propertyId,
                                (Var)reg,
                                isConst,
                                isInDeadZone);

                            Assert(info != nullptr);
                            pMembersList->Add(info);
                        }
                    }
                }
            }
        }
    }

    Var RegSlotVariablesWalker::GetVarObjectAndRegAt(int index, RegSlot* reg /*= nullptr*/)
    {LOGMEIN("DiagObjectModel.cpp] 747\n");
        Assert(index < pMembersList->Count());

        Var returnedVar = nullptr;
        RegSlot returnedReg = Js::Constants::NoRegister;

        DebuggerPropertyDisplayInfo* displayInfo = pMembersList->Item(index);
        if (displayInfo->IsInDeadZone())
        {LOGMEIN("DiagObjectModel.cpp] 755\n");
            // The uninitialized string is already set in the var for the dead zone display.
            Assert(JavascriptString::Is(displayInfo->aVar));
            returnedVar = displayInfo->aVar;
        }
        else
        {
            returnedReg = ::Math::PointerCastToIntegral<RegSlot>(displayInfo->aVar);
            returnedVar = pFrame->GetRegValue(returnedReg);
        }

        if (reg != nullptr)
        {LOGMEIN("DiagObjectModel.cpp] 767\n");
            *reg = returnedReg;
        }

        AssertMsg(returnedVar, "Var should be replaced with the dead zone string object.");
        return returnedVar;
    }

    Var RegSlotVariablesWalker::GetVarObjectAt(int index)
    {LOGMEIN("DiagObjectModel.cpp] 776\n");
        return GetVarObjectAndRegAt(index);
    }

    IDiagObjectAddress * RegSlotVariablesWalker::GetObjectAddress(int index)
    {LOGMEIN("DiagObjectModel.cpp] 781\n");
        RegSlot reg = Js::Constants::NoRegister;
        Var obj = GetVarObjectAndRegAt(index, &reg);

        return Anew(pFrame->GetArena(), LocalObjectAddressForRegSlot, pFrame, reg, obj);
    }

    // For an activation object.

    void ObjectVariablesWalker::PopulateMembers()
    {LOGMEIN("DiagObjectModel.cpp] 791\n");
        if (pMembersList == nullptr && instance != nullptr)
        {LOGMEIN("DiagObjectModel.cpp] 793\n");
            ScriptContext * scriptContext = pFrame->GetScriptContext();
            ArenaAllocator *arena = GetArenaFromContext(scriptContext);

            Assert(Js::RecyclableObject::Is(instance));

            Js::RecyclableObject* object = Js::RecyclableObject::FromVar(instance);
            Assert(JavascriptOperators::IsObject(object));

            int count = object->GetPropertyCount();
            pMembersList = JsUtil::List<DebuggerPropertyDisplayInfo *, ArenaAllocator>::New(arena, count);

            AddObjectProperties(count, object);
        }
    }

    void ObjectVariablesWalker::AddObjectProperties(int count, Js::RecyclableObject* object)
    {LOGMEIN("DiagObjectModel.cpp] 810\n");
        ScriptContext * scriptContext = pFrame->GetScriptContext();

        DebuggerScope *formalScope = GetScopeWhenHaltAtFormals();

        // For the scopes and locals only enumerable properties will be shown.
        for (int i = 0; i < count; i++)
        {LOGMEIN("DiagObjectModel.cpp] 817\n");
            Js::PropertyId propertyId = object->GetPropertyId((PropertyIndex)i);

            bool isConst = false;
            bool isPropertyInDebuggerScope = false;
            bool isInDeadZone = false;
            if (propertyId != Js::Constants::NoProperty
                && IsPropertyValid(propertyId, Js::Constants::NoRegister, &isPropertyInDebuggerScope, &isConst, &isInDeadZone)
                && object->IsEnumerable(propertyId))
            {LOGMEIN("DiagObjectModel.cpp] 826\n");
                Var itemObj = RecyclableObjectWalker::GetObject(object, object, propertyId, scriptContext);
                if (itemObj == nullptr)
                {LOGMEIN("DiagObjectModel.cpp] 829\n");
                    itemObj = scriptContext->GetLibrary()->GetUndefined();
                }

                if (formalScope == nullptr || formalScope->HasProperty(propertyId))
                {LOGMEIN("DiagObjectModel.cpp] 834\n");
                    if (formalScope != nullptr && pFrame->GetScriptContext()->IsUndeclBlockVar(itemObj))
                    {LOGMEIN("DiagObjectModel.cpp] 836\n");
                        itemObj = scriptContext->GetLibrary()->GetUndefined();
                    }

                    AssertMsg(!RootObjectBase::Is(object) || !isConst, "root object shouldn't produce const properties through IsPropertyValid");

                    DebuggerPropertyDisplayInfo *info = AllocateNewPropertyDisplayInfo(
                        propertyId,
                        itemObj,
                        isConst,
                        isInDeadZone);

                    Assert(info);
                    pMembersList->Add(info);
                }
            }
        }
    }

    IDiagObjectAddress * ObjectVariablesWalker::GetObjectAddress(int index)
    {LOGMEIN("DiagObjectModel.cpp] 856\n");
        Assert(index < pMembersList->Count());

        DebuggerPropertyDisplayInfo* info = pMembersList->Item(index);
        return Anew(pFrame->GetArena(), RecyclableObjectAddress, instance, info->propId, info->aVar, info->IsInDeadZone() ? TRUE : FALSE);
    }

    // For root access on the Global object (adds let/const variables before properties)

    void RootObjectVariablesWalker::PopulateMembers()
    {LOGMEIN("DiagObjectModel.cpp] 866\n");
        if (pMembersList == nullptr && instance != nullptr)
        {LOGMEIN("DiagObjectModel.cpp] 868\n");
            ScriptContext * scriptContext = pFrame->GetScriptContext();
            ArenaAllocator *arena = GetArenaFromContext(scriptContext);

            Assert(Js::RootObjectBase::Is(instance));
            Js::RootObjectBase* object = Js::RootObjectBase::FromVar(instance);

            int count = object->GetPropertyCount();
            pMembersList = JsUtil::List<DebuggerPropertyDisplayInfo *, ArenaAllocator>::New(arena, count);

            // Add let/const globals first so that they take precedence over the global properties.  Then
            // VariableWalkerBase::FindPropertyAddress will correctly find let/const globals that shadow
            // global properties of the same name.
            object->MapLetConstGlobals([&](const PropertyRecord* propertyRecord, Var value, bool isConst) {
                if (!scriptContext->IsUndeclBlockVar(value))
                {LOGMEIN("DiagObjectModel.cpp] 883\n");
                    // Let/const are always enumerable and valid
                    DebuggerPropertyDisplayInfo *info = AllocateNewPropertyDisplayInfo(propertyRecord->GetPropertyId(), value, isConst, false /*isInDeadZone*/);
                    pMembersList->Add(info);
                }
            });

            AddObjectProperties(count, object);
        }
    }

    // DiagScopeVariablesWalker

    DiagScopeVariablesWalker::DiagScopeVariablesWalker(DiagStackFrame* _pFrame, Var _instance, IDiagObjectModelWalkerBase* innerWalker)
        : VariableWalkerBase(_pFrame, _instance, UIGroupType_InnerScope, /* allowLexicalThis */ false)
    {LOGMEIN("DiagObjectModel.cpp] 898\n");
        ScriptContext * scriptContext = _pFrame->GetScriptContext();
        ArenaAllocator *arena = GetArenaFromContext(scriptContext);
        pDiagScopeObjects = JsUtil::List<IDiagObjectModelWalkerBase *, ArenaAllocator>::New(arena);
        pDiagScopeObjects->Add(innerWalker);
        diagScopeVarCount = innerWalker->GetChildrenCount();
        scopeIsInitialized = true;
    }

    uint32 DiagScopeVariablesWalker::GetChildrenCount()
    {LOGMEIN("DiagObjectModel.cpp] 908\n");
        if (scopeIsInitialized)
        {LOGMEIN("DiagObjectModel.cpp] 910\n");
            return diagScopeVarCount;
        }
        Assert(pFrame);
        Js::FunctionBody *pFBody = pFrame->GetJavascriptFunction()->GetFunctionBody();

        if (pFBody->GetScopeObjectChain())
        {LOGMEIN("DiagObjectModel.cpp] 917\n");
            int bytecodeOffset = GetAdjustedByteCodeOffset();
            ScriptContext * scriptContext = pFrame->GetScriptContext();
            ArenaAllocator *arena = GetArenaFromContext(scriptContext);
            pDiagScopeObjects = JsUtil::List<IDiagObjectModelWalkerBase *, ArenaAllocator>::New(arena);

            // Look for catch/with/block scopes which encompass current offset (skip block scopes as
            // they are only used for lookup within the RegSlotVariablesWalker).

            // Go the reverse way so that we find the innermost scope first;
            Js::ScopeObjectChain * pScopeObjectChain = pFBody->GetScopeObjectChain();
            for (int i = pScopeObjectChain->pScopeChain->Count() - 1 ; i >= 0; i--)
            {LOGMEIN("DiagObjectModel.cpp] 929\n");
                Js::DebuggerScope *debuggerScope = pScopeObjectChain->pScopeChain->Item(i);
                bool isScopeInRange = debuggerScope->IsOffsetInScope(bytecodeOffset);
                if (isScopeInRange
                    && !debuggerScope->IsParamScope()
                    && (debuggerScope->IsOwnScope() || (debuggerScope->scopeType == DiagBlockScopeDirect && debuggerScope->HasProperties())))
                {LOGMEIN("DiagObjectModel.cpp] 935\n");
                    switch (debuggerScope->scopeType)
                    {LOGMEIN("DiagObjectModel.cpp] 937\n");
                    case DiagWithScope:
                        {LOGMEIN("DiagObjectModel.cpp] 939\n");
                            if (enumWithScopeAlso)
                            {LOGMEIN("DiagObjectModel.cpp] 941\n");
                                RecyclableObjectWalker* recylableObjectWalker = Anew(arena, RecyclableObjectWalker, scriptContext,
                                    (Var)pFrame->GetRegValue(debuggerScope->GetLocation(), true));
                                pDiagScopeObjects->Add(recylableObjectWalker);
                                diagScopeVarCount += recylableObjectWalker->GetChildrenCount();
                            }
                        }
                        break;
                    case DiagCatchScopeDirect:
                    case DiagCatchScopeInObject:
                        {LOGMEIN("DiagObjectModel.cpp] 951\n");
                            CatchScopeWalker* catchScopeWalker = Anew(arena, CatchScopeWalker, pFrame, debuggerScope);
                            pDiagScopeObjects->Add(catchScopeWalker);
                            diagScopeVarCount += catchScopeWalker->GetChildrenCount();
                        }
                        break;
                    case DiagCatchScopeInSlot:
                    case DiagBlockScopeInSlot:
                        {LOGMEIN("DiagObjectModel.cpp] 959\n");
                            SlotArrayVariablesWalker* blockScopeWalker = Anew(arena, SlotArrayVariablesWalker, pFrame,
                                (Var)pFrame->GetInnerScopeFromRegSlot(debuggerScope->GetLocation()), UIGroupType_InnerScope, /* allowLexicalThis */ false);
                            pDiagScopeObjects->Add(blockScopeWalker);
                            diagScopeVarCount += blockScopeWalker->GetChildrenCount();
                        }
                        break;
                    case DiagBlockScopeDirect:
                        {LOGMEIN("DiagObjectModel.cpp] 967\n");
                            RegSlotVariablesWalker *pObjWalker = Anew(arena, RegSlotVariablesWalker, pFrame, debuggerScope, UIGroupType_InnerScope);
                            pDiagScopeObjects->Add(pObjWalker);
                            diagScopeVarCount += pObjWalker->GetChildrenCount();
                        }
                        break;
                    case DiagBlockScopeInObject:
                        {LOGMEIN("DiagObjectModel.cpp] 974\n");
                            ObjectVariablesWalker* objectVariablesWalker = Anew(arena, ObjectVariablesWalker, pFrame, pFrame->GetInnerScopeFromRegSlot(debuggerScope->GetLocation()), UIGroupType_InnerScope, /* allowLexicalThis */ false);
                            pDiagScopeObjects->Add(objectVariablesWalker);
                            diagScopeVarCount += objectVariablesWalker->GetChildrenCount();
                        }
                        break;
                    default:
                        Assert(false);
                    }
                }
            }
        }
        scopeIsInitialized = true;
        return diagScopeVarCount;
    }

    BOOL DiagScopeVariablesWalker::Get(int i, ResolvedObject* pResolvedObject)
    {LOGMEIN("DiagObjectModel.cpp] 991\n");
        if (i >= 0 && i < (int)diagScopeVarCount)
        {LOGMEIN("DiagObjectModel.cpp] 993\n");
            for (int j = 0; j < pDiagScopeObjects->Count(); j++)
            {LOGMEIN("DiagObjectModel.cpp] 995\n");
                IDiagObjectModelWalkerBase *pObjWalker = pDiagScopeObjects->Item(j);
                if (i < (int)pObjWalker->GetChildrenCount())
                {LOGMEIN("DiagObjectModel.cpp] 998\n");
                    return pObjWalker->Get(i, pResolvedObject);
                }
                i -= (int)pObjWalker->GetChildrenCount();
                Assert(i >=0);
            }
        }

        return FALSE;
    }

    IDiagObjectAddress * DiagScopeVariablesWalker::FindPropertyAddress(PropertyId propId, bool& isConst)
    {LOGMEIN("DiagObjectModel.cpp] 1010\n");
        IDiagObjectAddress * address = nullptr;

        // Ensure that children are fetched.
        GetChildrenCount();

        if (pDiagScopeObjects)
        {LOGMEIN("DiagObjectModel.cpp] 1017\n");
            for (int j = 0; j < pDiagScopeObjects->Count(); j++)
            {LOGMEIN("DiagObjectModel.cpp] 1019\n");
                IDiagObjectModelWalkerBase *pObjWalker = pDiagScopeObjects->Item(j);
                Assert(pObjWalker);

                address = pObjWalker->FindPropertyAddress(propId, isConst);
                if (address != nullptr)
                {LOGMEIN("DiagObjectModel.cpp] 1025\n");
                    break;
                }
            }
        }

        return address;
    }


    // Locals walker

    LocalsWalker::LocalsWalker(DiagStackFrame* _frame, DWORD _frameWalkerFlags)
        :  pFrame(_frame), frameWalkerFlags(_frameWalkerFlags), pVarWalkers(nullptr), totalLocalsCount(0), hasUserNotDefinedArguments(false)
    {LOGMEIN("DiagObjectModel.cpp] 1039\n");
        Js::FunctionBody *pFBody = pFrame->GetJavascriptFunction()->GetFunctionBody();
        if (pFBody && !pFBody->GetUtf8SourceInfo()->GetIsLibraryCode())
        {LOGMEIN("DiagObjectModel.cpp] 1042\n");
            // Allocate the container of all walkers.
            ArenaAllocator *arena = pFrame->GetArena();
            pVarWalkers = JsUtil::List<VariableWalkerBase *, ArenaAllocator>::New(arena);

            VariableWalkerBase *pVarWalker = nullptr;

            // Top most function will have one of these regslot, slotarray or activation object.

            FrameDisplay * pDisplay = pFrame->GetFrameDisplay();
            uint scopeCount = (uint)(pDisplay ? pDisplay->GetLength() : 0);

            uint nextStartIndex = 0;

            // Add the catch/with/block expression scope objects.
            if (pFBody->GetScopeObjectChain())
            {LOGMEIN("DiagObjectModel.cpp] 1058\n");
                pVarWalkers->Add(Anew(arena, DiagScopeVariablesWalker, pFrame, nullptr, !!(frameWalkerFlags & FrameWalkerFlags::FW_EnumWithScopeAlso)));
            }

            // In the eval function, we will not show global items directly, instead they should go as a group node.
            bool shouldAddGlobalItemsDirectly = pFBody->GetIsGlobalFunc() && !pFBody->IsEval();
            bool dontAddGlobalsDirectly = (frameWalkerFlags & FrameWalkerFlags::FW_DontAddGlobalsDirectly) == FrameWalkerFlags::FW_DontAddGlobalsDirectly;
            if (shouldAddGlobalItemsDirectly && !dontAddGlobalsDirectly)
            {LOGMEIN("DiagObjectModel.cpp] 1066\n");
                // Global properties will be enumerated using RootObjectVariablesWalker
                pVarWalkers->Add(Anew(arena, RootObjectVariablesWalker, pFrame, pFrame->GetRootObject(), UIGroupType_None));
            }

            DebuggerScope *formalScope = GetScopeWhenHaltAtFormals(pFrame);

            // If we are halted at formal place, and param and body scopes are splitted we need to make use of formal debugger scope to walk those variables.
            if (!pFBody->IsParamAndBodyScopeMerged() && formalScope != nullptr)
            {LOGMEIN("DiagObjectModel.cpp] 1075\n");
                Assert(scopeCount > 0);
                if (formalScope->scopeType == Js::DiagParamScopeInObject)
                {LOGMEIN("DiagObjectModel.cpp] 1078\n");
                    pVarWalker = Anew(arena, ObjectVariablesWalker, pFrame, (Js::Var *)pDisplay->GetItem(nextStartIndex++), UIGroupType_None, !!(frameWalkerFlags & FrameWalkerFlags::FW_AllowLexicalThis), !!(frameWalkerFlags & FrameWalkerFlags::FW_AllowSuperReference));
                }
                else
                {
                    Assert(pFBody->paramScopeSlotArraySize > 0);
                    pVarWalker = Anew(arena, SlotArrayVariablesWalker, pFrame, (Js::Var *)pDisplay->GetItem(nextStartIndex++), UIGroupType_None, !!(frameWalkerFlags & FrameWalkerFlags::FW_AllowLexicalThis), !!(frameWalkerFlags & FrameWalkerFlags::FW_AllowSuperReference));
                }
            }
            else
            {
                DWORD localsType = GetCurrentFramesLocalsType(pFrame);
                if (localsType & FramesLocalType::LocalType_Reg)
                {LOGMEIN("DiagObjectModel.cpp] 1091\n");
                    pVarWalkers->Add(Anew(arena, RegSlotVariablesWalker, pFrame, nullptr /*not debugger scope*/, UIGroupType_None, !!(frameWalkerFlags & FrameWalkerFlags::FW_AllowSuperReference)));
                }
                if (localsType & FramesLocalType::LocalType_InObject)
                {LOGMEIN("DiagObjectModel.cpp] 1095\n");
                    Assert(scopeCount > 0);
                    pVarWalker = Anew(arena, ObjectVariablesWalker, pFrame, pDisplay->GetItem(nextStartIndex++), UIGroupType_None, !!(frameWalkerFlags & FrameWalkerFlags::FW_AllowLexicalThis), !!(frameWalkerFlags & FrameWalkerFlags::FW_AllowSuperReference));
                }
                else if (localsType & FramesLocalType::LocalType_InSlot)
                {LOGMEIN("DiagObjectModel.cpp] 1100\n");
                    Assert(scopeCount > 0);
                    pVarWalker = Anew(arena, SlotArrayVariablesWalker, pFrame, (Js::Var *)pDisplay->GetItem(nextStartIndex++), UIGroupType_None, !!(frameWalkerFlags & FrameWalkerFlags::FW_AllowLexicalThis), !!(frameWalkerFlags & FrameWalkerFlags::FW_AllowSuperReference));
                }
                else if (scopeCount > 0 && pFBody->GetFrameDisplayRegister() != 0 )
                {LOGMEIN("DiagObjectModel.cpp] 1105\n");
                    Assert((Var)pDisplay->GetItem(0) == pFrame->GetScriptContext()->GetLibrary()->GetNull() || !pFBody->IsParamAndBodyScopeMerged());

                    // A dummy scope with nullptr register is created. Skip this.
                    nextStartIndex++;
                }
            }

            if (pVarWalker)
            {LOGMEIN("DiagObjectModel.cpp] 1114\n");
                pVarWalkers->Add(pVarWalker);
            }

            const Js::Var nullVar = pFrame->GetScriptContext()->GetLibrary()->GetNull();
            for (uint i = nextStartIndex; i < (uint)scopeCount; i++)
            {LOGMEIN("DiagObjectModel.cpp] 1120\n");
                Var currentScopeObject = pDisplay->GetItem(i);
                if (currentScopeObject != nullptr && currentScopeObject != nullVar) // Skip nullptr (dummy scope)
                {LOGMEIN("DiagObjectModel.cpp] 1123\n");
                    ScopeType scopeType = FrameDisplay::GetScopeType(currentScopeObject);
                    switch(scopeType)
                    {LOGMEIN("DiagObjectModel.cpp] 1126\n");
                    case ScopeType_ActivationObject:
                        pVarWalker = Anew(arena, ObjectVariablesWalker, pFrame, currentScopeObject, UIGroupType_Scope, !!(frameWalkerFlags & FrameWalkerFlags::FW_AllowLexicalThis), !!(frameWalkerFlags & FrameWalkerFlags::FW_AllowSuperReference));
                        pVarWalkers->Add(pVarWalker);
                        break;
                    case ScopeType_SlotArray:
                        pVarWalker = Anew(arena, SlotArrayVariablesWalker, pFrame, currentScopeObject, UIGroupType_Scope, !!(frameWalkerFlags & FrameWalkerFlags::FW_AllowLexicalThis), !!(frameWalkerFlags & FrameWalkerFlags::FW_AllowSuperReference));
                        pVarWalkers->Add(pVarWalker);
                        break;
                    case ScopeType_WithScope:
                        if( (frameWalkerFlags & FrameWalkerFlags::FW_EnumWithScopeAlso) == FrameWalkerFlags::FW_EnumWithScopeAlso)
                        {LOGMEIN("DiagObjectModel.cpp] 1137\n");
                            RecyclableObjectWalker* withScopeWalker = Anew(arena, RecyclableObjectWalker, pFrame->GetScriptContext(), currentScopeObject);
                            pVarWalker = Anew(arena, DiagScopeVariablesWalker, pFrame, currentScopeObject, withScopeWalker);
                            pVarWalkers->Add(pVarWalker);
                        }
                        break;
                    default:
                        Assert(false);
                    }
                }
            }

            // No need to add global properties if this is a global function, as it is already done above.
            if (!shouldAddGlobalItemsDirectly && !dontAddGlobalsDirectly)
            {LOGMEIN("DiagObjectModel.cpp] 1151\n");
                pVarWalker = Anew(arena, RootObjectVariablesWalker, pFrame, pFrame->GetRootObject(),  UIGroupType_Globals);
                pVarWalkers->Add(pVarWalker);
            }
        }
    }

    BOOL LocalsWalker::CreateArgumentsObject(ResolvedObject* pResolvedObject)
    {LOGMEIN("DiagObjectModel.cpp] 1159\n");
        Assert(pResolvedObject);
        Assert(pResolvedObject->scriptContext);

        Assert(hasUserNotDefinedArguments);

        pResolvedObject->name = _u("arguments");
        pResolvedObject->propId = Js::PropertyIds::arguments;
        pResolvedObject->typeId = TypeIds_Arguments;

        Js::FunctionBody *pFBody = pFrame->GetJavascriptFunction()->GetFunctionBody();
        Assert(pFBody);

        pResolvedObject->obj = pFrame->GetArgumentsObject();
        if (pResolvedObject->obj == nullptr)
        {LOGMEIN("DiagObjectModel.cpp] 1174\n");
            pResolvedObject->obj = pFrame->CreateHeapArguments();
            Assert(pResolvedObject->obj);

            pResolvedObject->objectDisplay = Anew(pFrame->GetArena(), RecyclableArgumentsObjectDisplay, pResolvedObject, this);
            ExpandArgumentsObject(pResolvedObject->objectDisplay);
        }

        pResolvedObject->address = Anew(GetArenaFromContext(pResolvedObject->scriptContext),
            RecyclableObjectAddress,
            pResolvedObject->scriptContext->GetGlobalObject(),
            Js::PropertyIds::arguments,
            pResolvedObject->obj,
            false /*isInDeadZone*/);

        return TRUE;
    }

    BOOL LocalsWalker::Get(int i, ResolvedObject* pResolvedObject)
    {LOGMEIN("DiagObjectModel.cpp] 1193\n");
        if (i >= (int)totalLocalsCount)
        {LOGMEIN("DiagObjectModel.cpp] 1195\n");
            return FALSE;
        }

        pResolvedObject->scriptContext = pFrame->GetScriptContext();

        if (VariableWalkerBase::GetExceptionObject(i, pFrame, pResolvedObject))
        {LOGMEIN("DiagObjectModel.cpp] 1202\n");
            return TRUE;
        }

#ifdef ENABLE_MUTATION_BREAKPOINT
        // Pending mutation display should be before any return value
        if (VariableWalkerBase::GetBreakMutationBreakpointValue(i, pFrame, pResolvedObject))
        {LOGMEIN("DiagObjectModel.cpp] 1209\n");
            return TRUE;
        }
#endif

        if (VariableWalkerBase::GetReturnedValue(i, pFrame, pResolvedObject))
        {LOGMEIN("DiagObjectModel.cpp] 1215\n");
            return TRUE;
        }

        if (hasUserNotDefinedArguments)
        {LOGMEIN("DiagObjectModel.cpp] 1220\n");
            if (i == 0)
            {LOGMEIN("DiagObjectModel.cpp] 1222\n");
                return CreateArgumentsObject(pResolvedObject);
            }
            i--;
        }

        if (!pVarWalkers || pVarWalkers->Count() == 0)
        {LOGMEIN("DiagObjectModel.cpp] 1229\n");
            return FALSE;
        }

        // In the case of not making groups, all variables will be arranged
        // as one int32 list in the locals window.
        if (!ShouldMakeGroups())
        {LOGMEIN("DiagObjectModel.cpp] 1236\n");
            for (int j = 0; j < pVarWalkers->Count(); j++)
            {LOGMEIN("DiagObjectModel.cpp] 1238\n");
                int count = pVarWalkers->Item(j)->GetChildrenCount();
                if (i < count)
                {LOGMEIN("DiagObjectModel.cpp] 1241\n");
                    return pVarWalkers->Item(j)->Get(i, pResolvedObject);
                }
                i-= count;
            }

            Assert(FALSE);
            return FALSE;
        }

        int startScopeIndex = 0;

        // Need to determine what range of local variables we're in for the requested index.
        // Non-grouped local variables are organized with reg slot coming first, then followed by
        // scope slot/activation object variables. Catch and with variables follow next
        // and group variables are stored last which come from upper scopes that
        // are accessed in this function (those passed down as part of a closure).
        // Note that all/any/none of these walkers may be present.
        // Example variable layout:
        // [0-2] - Reg slot vars.
        // [3-4] - Scope slot array vars.
        // [5-8] - Global vars (stored on the global object as properties).
        for (int j = 0; j < pVarWalkers->Count(); ++j)
        {LOGMEIN("DiagObjectModel.cpp] 1264\n");
            VariableWalkerBase *variableWalker = pVarWalkers->Item(j);
            if (!variableWalker->IsInGroup())
            {LOGMEIN("DiagObjectModel.cpp] 1267\n");
                int count = variableWalker->GetChildrenCount();

                if (i < count)
                {LOGMEIN("DiagObjectModel.cpp] 1271\n");
                    return variableWalker->Get(i, pResolvedObject);
                }
                i-= count;
                startScopeIndex++;
            }
            else
            {
                // We've finished with all walkers for the current locals level so
                // break out in order to handle the groups.
                break;
            }
        }

        // Handle groups.
        Assert((i + startScopeIndex) < pVarWalkers->Count());
        VariableWalkerBase *variableWalker = pVarWalkers->Item(i + startScopeIndex);
        return variableWalker->GetGroupObject(pResolvedObject);
    }

    bool LocalsWalker::ShouldInsertFakeArguments()
    {LOGMEIN("DiagObjectModel.cpp] 1292\n");
        JavascriptFunction* func = pFrame->GetJavascriptFunction();
        if (func->IsScriptFunction()
            && !func->GetFunctionBody()->GetUtf8SourceInfo()->GetIsLibraryCode()
            && !func->GetFunctionBody()->GetIsGlobalFunc())
        {LOGMEIN("DiagObjectModel.cpp] 1297\n");
            bool isConst = false;
            hasUserNotDefinedArguments  = (nullptr == FindPropertyAddress(PropertyIds::arguments, false /*walkers on the current frame*/, isConst));
        }
        return hasUserNotDefinedArguments;
    }

    uint32 LocalsWalker::GetChildrenCount()
    {LOGMEIN("DiagObjectModel.cpp] 1305\n");
        if (totalLocalsCount == 0)
        {LOGMEIN("DiagObjectModel.cpp] 1307\n");
            if (pVarWalkers)
            {LOGMEIN("DiagObjectModel.cpp] 1309\n");
                int groupWalkersStartIndex = 0;
                for (int i = 0; i < pVarWalkers->Count(); i++)
                {LOGMEIN("DiagObjectModel.cpp] 1312\n");
                    VariableWalkerBase* variableWalker = pVarWalkers->Item(i);

                    // In the case of making groups, we want to include any variables that aren't
                    // part of a group as part of the local variable count.
                    if (!ShouldMakeGroups() || !variableWalker->IsInGroup())
                    {LOGMEIN("DiagObjectModel.cpp] 1318\n");
                        ++groupWalkersStartIndex;
                        totalLocalsCount += variableWalker->GetChildrenCount();
                    }
                }

                // Add on the number of groups to display in locals
                // (group walkers come after function local walkers).
                totalLocalsCount += (pVarWalkers->Count() - groupWalkersStartIndex);
            }

            if (VariableWalkerBase::HasExceptionObject(pFrame))
            {LOGMEIN("DiagObjectModel.cpp] 1330\n");
                totalLocalsCount++;
            }

#ifdef ENABLE_MUTATION_BREAKPOINT
            totalLocalsCount += VariableWalkerBase::GetBreakMutationBreakpointsCount(pFrame);
#endif
            totalLocalsCount += VariableWalkerBase::GetReturnedValueCount(pFrame);

            // Check if needed to add fake arguments.
            if (ShouldInsertFakeArguments())
            {LOGMEIN("DiagObjectModel.cpp] 1341\n");
                // In this case we need to create arguments object explicitly.
                totalLocalsCount++;
            }
        }
        return totalLocalsCount;
    }

    uint32 LocalsWalker::GetLocalVariablesCount()
    {LOGMEIN("DiagObjectModel.cpp] 1350\n");
        uint32 localsCount = 0;
        if (pVarWalkers)
        {LOGMEIN("DiagObjectModel.cpp] 1353\n");
            for (int i = 0; i < pVarWalkers->Count(); i++)
            {LOGMEIN("DiagObjectModel.cpp] 1355\n");
                VariableWalkerBase* variableWalker = pVarWalkers->Item(i);

                // In the case of making groups, we want to include any variables that aren't
                // part of a group as part of the local variable count.
                if (!ShouldMakeGroups() || !variableWalker->IsInGroup())
                {LOGMEIN("DiagObjectModel.cpp] 1361\n");
                    localsCount += variableWalker->GetChildrenCount();
                }
            }
        }
        return localsCount;
    }

    BOOL LocalsWalker::GetLocal(int i, ResolvedObject* pResolvedObject)
    {LOGMEIN("DiagObjectModel.cpp] 1370\n");
        if (!pVarWalkers || pVarWalkers->Count() == 0)
        {LOGMEIN("DiagObjectModel.cpp] 1372\n");
            return FALSE;
        }

        for (int j = 0; j < pVarWalkers->Count(); ++j)
        {LOGMEIN("DiagObjectModel.cpp] 1377\n");
            VariableWalkerBase *variableWalker = pVarWalkers->Item(j);

            if (!ShouldMakeGroups() || !variableWalker->IsInGroup())
            {LOGMEIN("DiagObjectModel.cpp] 1381\n");
                int count = variableWalker->GetChildrenCount();

                if (i < count)
                {LOGMEIN("DiagObjectModel.cpp] 1385\n");
                    return variableWalker->Get(i, pResolvedObject);
                }
                i -= count;
            }
            else
            {
                // We've finished with all walkers for the current locals level so
                // break out in order to handle the groups.
                break;
            }
        }

        return FALSE;
    }

    BOOL LocalsWalker::GetGroupObject(Js::UIGroupType uiGroupType, int i, ResolvedObject* pResolvedObject)
    {LOGMEIN("DiagObjectModel.cpp] 1402\n");
        if (pVarWalkers)
        {LOGMEIN("DiagObjectModel.cpp] 1404\n");
            int scopeCount = 0;
            for (int j = 0; j < pVarWalkers->Count(); j++)
            {LOGMEIN("DiagObjectModel.cpp] 1407\n");
                VariableWalkerBase* variableWalker = pVarWalkers->Item(j);

                if (variableWalker->groupType == uiGroupType)
                {LOGMEIN("DiagObjectModel.cpp] 1411\n");
                    scopeCount++;
                    if (i < scopeCount)
                    {LOGMEIN("DiagObjectModel.cpp] 1414\n");
                        return variableWalker->GetGroupObject(pResolvedObject);
                    }
                }
            }
        }
        return FALSE;
    }

    BOOL LocalsWalker::GetScopeObject(int i, ResolvedObject* pResolvedObject)
    {LOGMEIN("DiagObjectModel.cpp] 1424\n");
        return this->GetGroupObject(Js::UIGroupType::UIGroupType_Scope, i, pResolvedObject);
    }

    BOOL LocalsWalker::GetGlobalsObject(ResolvedObject* pResolvedObject)
    {LOGMEIN("DiagObjectModel.cpp] 1429\n");
        int i = 0;
        return this->GetGroupObject(Js::UIGroupType::UIGroupType_Globals, i, pResolvedObject);
    }

    /*static*/
    DebuggerScope * LocalsWalker::GetScopeWhenHaltAtFormals(DiagStackFrame* frame)
    {LOGMEIN("DiagObjectModel.cpp] 1436\n");
        Js::ScopeObjectChain * scopeObjectChain = frame->GetJavascriptFunction()->GetFunctionBody()->GetScopeObjectChain();

        if (scopeObjectChain != nullptr && scopeObjectChain->pScopeChain != nullptr)
        {LOGMEIN("DiagObjectModel.cpp] 1440\n");
            int currentOffset = GetAdjustedByteCodeOffset(frame);
            for (int i = 0; i < scopeObjectChain->pScopeChain->Count(); i++)
            {LOGMEIN("DiagObjectModel.cpp] 1443\n");
                Js::DebuggerScope * scope = scopeObjectChain->pScopeChain->Item(i);
                if (scope->IsParamScope() && scope->GetEnd() > currentOffset)
                {LOGMEIN("DiagObjectModel.cpp] 1446\n");
                    return scope;
                }
            }
        }

        return nullptr;
    }

    // Gets an adjusted offset for the current bytecode location based on which stack frame we're in.
    // If we're in the top frame (leaf node), then the byte code offset should remain as is, to reflect
    // the current position of the instruction pointer.  If we're not in the top frame, we need to subtract
    // 1 as the byte code location will be placed at the next statement to be executed at the top frame.
    // In the case of block scoping, this is an inaccurate location for viewing variables since the next
    // statement could be beyond the current block scope.  For inspection, we want to remain in the
    // current block that the function was called from.
    // An example is this:
    // function foo() { ... }   // Frame 0 (with breakpoint inside)
    // function bar() {         // Frame 1
    //     {
    //         let a = 0;
    //         foo(); // <-- Inspecting here, foo is already evaluated.
    //     }
    //     foo(); // <-- Byte code offset is now here, so we need to -1 to get back in the block scope.
    int LocalsWalker::GetAdjustedByteCodeOffset(DiagStackFrame* frame)
    {LOGMEIN("DiagObjectModel.cpp] 1471\n");
        int offset = frame->GetByteCodeOffset();
        if (!frame->IsTopFrame() && frame->IsInterpreterFrame())
        {LOGMEIN("DiagObjectModel.cpp] 1474\n");
            // Native frames are already adjusted so just need to adjust interpreted
            // frames that are not the top frame.
            --offset;
        }

        return offset;
    }

    /*static*/
    DWORD LocalsWalker::GetCurrentFramesLocalsType(DiagStackFrame* frame)
    {LOGMEIN("DiagObjectModel.cpp] 1485\n");
        Assert(frame);

        FunctionBody *pFBody = frame->GetJavascriptFunction()->GetFunctionBody();
        Assert(pFBody);

        DWORD localType = FramesLocalType::LocalType_None;

        if (pFBody->GetFrameDisplayRegister() != 0)
        {LOGMEIN("DiagObjectModel.cpp] 1494\n");
            if (pFBody->GetObjectRegister() != 0)
            {LOGMEIN("DiagObjectModel.cpp] 1496\n");
                // current scope is activation object
                localType = FramesLocalType::LocalType_InObject;
            }
            else
            {
                if (pFBody->scopeSlotArraySize > 0)
                {LOGMEIN("DiagObjectModel.cpp] 1503\n");
                    localType = FramesLocalType::LocalType_InSlot;
                }
            }
        }

        if (pFBody->GetPropertyIdOnRegSlotsContainer() && pFBody->GetPropertyIdOnRegSlotsContainer()->length > 0)
        {LOGMEIN("DiagObjectModel.cpp] 1510\n");
           localType |= FramesLocalType::LocalType_Reg;
        }

        return localType;
    }

    IDiagObjectAddress * LocalsWalker::FindPropertyAddress(PropertyId propId, bool& isConst)
    {LOGMEIN("DiagObjectModel.cpp] 1518\n");
        return FindPropertyAddress(propId, true, isConst);
    }

    IDiagObjectAddress * LocalsWalker::FindPropertyAddress(PropertyId propId, bool enumerateGroups, bool& isConst)
    {LOGMEIN("DiagObjectModel.cpp] 1523\n");
        isConst = false;
        if (propId == PropertyIds::arguments && hasUserNotDefinedArguments)
        {LOGMEIN("DiagObjectModel.cpp] 1526\n");
            ResolvedObject resolveObject;
            resolveObject.scriptContext = pFrame->GetScriptContext();
            if (CreateArgumentsObject(&resolveObject))
            {LOGMEIN("DiagObjectModel.cpp] 1530\n");
                return resolveObject.address;
            }
        }

        if (pVarWalkers)
        {LOGMEIN("DiagObjectModel.cpp] 1536\n");
            for (int i = 0; i < pVarWalkers->Count(); i++)
            {LOGMEIN("DiagObjectModel.cpp] 1538\n");
                VariableWalkerBase *pVarWalker = pVarWalkers->Item(i);
                if (!enumerateGroups && !pVarWalker->IsWalkerForCurrentFrame())
                {LOGMEIN("DiagObjectModel.cpp] 1541\n");
                    continue;
                }

                IDiagObjectAddress *address = pVarWalkers->Item(i)->FindPropertyAddress(propId, isConst);
                if (address != nullptr)
                {LOGMEIN("DiagObjectModel.cpp] 1547\n");
                    return address;
                }
            }
        }

        return nullptr;
    }

    void LocalsWalker::ExpandArgumentsObject(IDiagObjectModelDisplay * argumentsDisplay)
    {LOGMEIN("DiagObjectModel.cpp] 1557\n");
        Assert(argumentsDisplay != nullptr);

        WeakArenaReference<Js::IDiagObjectModelWalkerBase>* argumentsObjectWalkerRef = argumentsDisplay->CreateWalker();
        Assert(argumentsObjectWalkerRef != nullptr);

        IDiagObjectModelWalkerBase * walker = argumentsObjectWalkerRef->GetStrongReference();
        int count = (int)walker->GetChildrenCount();
        Js::ResolvedObject tempResolvedObj;
        for (int i = 0; i < count; i++)
        {LOGMEIN("DiagObjectModel.cpp] 1567\n");
            walker->Get(i, &tempResolvedObj);
        }
        argumentsObjectWalkerRef->ReleaseStrongReference();
        HeapDelete(argumentsObjectWalkerRef);
    }

    //--------------------------
    // LocalObjectAddressForSlot


    LocalObjectAddressForSlot::LocalObjectAddressForSlot(ScopeSlots _pSlotArray, int _slotIndex, Js::Var _value)
        : slotArray(_pSlotArray),
          slotIndex(_slotIndex),
          value(_value)
    {LOGMEIN("DiagObjectModel.cpp] 1582\n");
    }

    BOOL LocalObjectAddressForSlot::Set(Var updateObject)
    {LOGMEIN("DiagObjectModel.cpp] 1586\n");
        if (IsInDeadZone())
        {
            AssertMsg(FALSE, "Should not be able to set the value of a slot in a dead zone.");
            return FALSE;
        }

        slotArray.Set(slotIndex, updateObject);
        return TRUE;
    }

    Var LocalObjectAddressForSlot::GetValue(BOOL fUpdated)
    {LOGMEIN("DiagObjectModel.cpp] 1598\n");
        if (!fUpdated || IsInDeadZone())
        {LOGMEIN("DiagObjectModel.cpp] 1600\n");
#if DBG
            if (IsInDeadZone())
            {LOGMEIN("DiagObjectModel.cpp] 1603\n");
                // If we're in a dead zone, the value will be the
                // [Uninitialized block variable] string.
                Assert(JavascriptString::Is(value));
            }
#endif // DBG

            return value;
        }

        return slotArray.Get(slotIndex);
    }

    BOOL LocalObjectAddressForSlot::IsInDeadZone() const
    {LOGMEIN("DiagObjectModel.cpp] 1617\n");
        Var value = slotArray.Get(slotIndex);
        if (!RecyclableObject::Is(value))
        {LOGMEIN("DiagObjectModel.cpp] 1620\n");
            return FALSE;
        }

        RecyclableObject* obj = RecyclableObject::FromVar(value);
        ScriptContext* scriptContext = obj->GetScriptContext();
        return scriptContext->IsUndeclBlockVar(obj) ? TRUE : FALSE;
    }

    //--------------------------
    // LocalObjectAddressForSlot


    LocalObjectAddressForRegSlot::LocalObjectAddressForRegSlot(DiagStackFrame* _pFrame, RegSlot _regSlot, Js::Var _value)
        : pFrame(_pFrame),
          regSlot(_regSlot),
          value(_value)
    {LOGMEIN("DiagObjectModel.cpp] 1637\n");
    }

    BOOL LocalObjectAddressForRegSlot::IsInDeadZone() const
    {LOGMEIN("DiagObjectModel.cpp] 1641\n");
        return regSlot == Js::Constants::NoRegister;
    }

    BOOL LocalObjectAddressForRegSlot::Set(Var updateObject)
    {LOGMEIN("DiagObjectModel.cpp] 1646\n");
        Assert(pFrame);

        if (IsInDeadZone())
        {
            AssertMsg(FALSE, "Should not be able to set the value of a register in a dead zone.");
            return FALSE;
        }

        pFrame->SetRegValue(regSlot, updateObject);

        return TRUE;
    }

    Var LocalObjectAddressForRegSlot::GetValue(BOOL fUpdated)
    {LOGMEIN("DiagObjectModel.cpp] 1661\n");
        if (!fUpdated || IsInDeadZone())
        {LOGMEIN("DiagObjectModel.cpp] 1663\n");
#if DBG
            if (IsInDeadZone())
            {LOGMEIN("DiagObjectModel.cpp] 1666\n");
                // If we're in a dead zone, the value will be the
                // [Uninitialized block variable] string.
                Assert(JavascriptString::Is(value));
            }
#endif // DBG

            return value;
        }

        Assert(pFrame);
        return pFrame->GetRegValue(regSlot);
    }

    //
    // CatchScopeWalker

    BOOL CatchScopeWalker::Get(int i, ResolvedObject* pResolvedObject)
    {LOGMEIN("DiagObjectModel.cpp] 1684\n");
        Assert(pResolvedObject);

        Assert(pFrame);
        pResolvedObject->scriptContext = pFrame->GetScriptContext();
        Assert(i < (int)GetChildrenCount());
        Js::DebuggerScopeProperty scopeProperty = debuggerScope->scopeProperties->Item(i);

        pResolvedObject->propId = scopeProperty.propId;

        const Js::PropertyRecord* propertyRecord = pResolvedObject->scriptContext->GetPropertyName(pResolvedObject->propId);

        // TODO: If this is a symbol-keyed property, we should indicate that in the name - "Symbol (description)"
        pResolvedObject->name = propertyRecord->GetBuffer();

        FetchValueAndAddress(scopeProperty, &pResolvedObject->obj, &pResolvedObject->address);

        Assert(pResolvedObject->obj);

        pResolvedObject->typeId = JavascriptOperators::GetTypeId(pResolvedObject->obj);
        pResolvedObject->objectDisplay =  Anew(pFrame->GetArena(), RecyclableObjectDisplay, pResolvedObject);

        return TRUE;
    }

    uint32 CatchScopeWalker::GetChildrenCount()
    {LOGMEIN("DiagObjectModel.cpp] 1710\n");
        return debuggerScope->scopeProperties->Count();
    }

    void CatchScopeWalker::FetchValueAndAddress(DebuggerScopeProperty &scopeProperty, _Out_opt_ Var *pValue, _Out_opt_ IDiagObjectAddress ** ppAddress)
    {LOGMEIN("DiagObjectModel.cpp] 1715\n");
        Assert(pValue != nullptr || ppAddress != nullptr);

        ArenaAllocator* arena = pFrame->GetArena();
        Var outValue;
        IDiagObjectAddress * pAddress = nullptr;

        ScriptContext* scriptContext = pFrame->GetScriptContext();
        if (debuggerScope->scopeType == Js::DiagCatchScopeInObject)
        {LOGMEIN("DiagObjectModel.cpp] 1724\n");
            Var obj = pFrame->GetInnerScopeFromRegSlot(debuggerScope->GetLocation());
            Assert(RecyclableObject::Is(obj));

            outValue = RecyclableObjectWalker::GetObject(RecyclableObject::FromVar(obj), RecyclableObject::FromVar(obj), scopeProperty.propId, scriptContext);
            bool isInDeadZone = scriptContext->IsUndeclBlockVar(outValue);
            if (isInDeadZone)
            {LOGMEIN("DiagObjectModel.cpp] 1731\n");
                outValue = scriptContext->GetLibrary()->GetDebuggerDeadZoneBlockVariableString();

            }
            pAddress = Anew(arena, RecyclableObjectAddress, obj, scopeProperty.propId, outValue, isInDeadZone);
        }
        else
        {
            outValue = pFrame->GetRegValue(scopeProperty.location);
            bool isInDeadZone = scriptContext->IsUndeclBlockVar(outValue);
            if (isInDeadZone)
            {LOGMEIN("DiagObjectModel.cpp] 1742\n");
                outValue = scriptContext->GetLibrary()->GetDebuggerDeadZoneBlockVariableString();

            }
            pAddress = Anew(arena, LocalObjectAddressForRegSlot, pFrame, scopeProperty.location, outValue);
        }

        if (pValue)
        {LOGMEIN("DiagObjectModel.cpp] 1750\n");
            *pValue = outValue;
        }

        if (ppAddress)
        {LOGMEIN("DiagObjectModel.cpp] 1755\n");
            *ppAddress = pAddress;
        }
    }

    IDiagObjectAddress *CatchScopeWalker::FindPropertyAddress(PropertyId _propId, bool& isConst)
    {LOGMEIN("DiagObjectModel.cpp] 1761\n");
        isConst = false;
        IDiagObjectAddress * address = nullptr;
        auto properties = debuggerScope->scopeProperties;
        for (int i = 0; i < properties->Count(); i++)
        {LOGMEIN("DiagObjectModel.cpp] 1766\n");
            if (properties->Item(i).propId == _propId)
            {LOGMEIN("DiagObjectModel.cpp] 1768\n");
                FetchValueAndAddress(properties->Item(i), nullptr, &address);
                break;
            }
        }

        return address;
    }

    //--------------------------
    // RecyclableObjectAddress

    RecyclableObjectAddress::RecyclableObjectAddress(Var _parentObj, Js::PropertyId _propId, Js::Var _value, BOOL _isInDeadZone)
        : parentObj(_parentObj),
          propId(_propId),
          value(_value),
          isInDeadZone(_isInDeadZone)
    {LOGMEIN("DiagObjectModel.cpp] 1785\n");
        parentObj = ((RecyclableObject*)parentObj)->GetThisObjectOrUnWrap();
    }

    BOOL RecyclableObjectAddress::IsInDeadZone() const
    {LOGMEIN("DiagObjectModel.cpp] 1790\n");
        return isInDeadZone;
    }

    BOOL RecyclableObjectAddress::Set(Var updateObject)
    {LOGMEIN("DiagObjectModel.cpp] 1795\n");
        if (Js::RecyclableObject::Is(parentObj))
        {LOGMEIN("DiagObjectModel.cpp] 1797\n");
            Js::RecyclableObject* obj = Js::RecyclableObject::FromVar(parentObj);

            ScriptContext* requestContext = obj->GetScriptContext(); //TODO: real requestContext
            return Js::JavascriptOperators::SetProperty(obj, obj, propId, updateObject, requestContext);
        }
        return FALSE;
    }

    BOOL RecyclableObjectAddress::IsWritable()
    {LOGMEIN("DiagObjectModel.cpp] 1807\n");
        if (Js::RecyclableObject::Is(parentObj))
        {LOGMEIN("DiagObjectModel.cpp] 1809\n");
            Js::RecyclableObject* obj = Js::RecyclableObject::FromVar(parentObj);

            return obj->IsWritable(propId);
        }

        return TRUE;
    }

    Var RecyclableObjectAddress::GetValue(BOOL fUpdated)
    {LOGMEIN("DiagObjectModel.cpp] 1819\n");
        if (!fUpdated)
        {LOGMEIN("DiagObjectModel.cpp] 1821\n");
            return value;
        }

        if (Js::RecyclableObject::Is(parentObj))
        {LOGMEIN("DiagObjectModel.cpp] 1826\n");
            Js::RecyclableObject* obj = Js::RecyclableObject::FromVar(parentObj);

            ScriptContext* requestContext = obj->GetScriptContext();
            Var objValue = nullptr;

#if ENABLE_TTD
            bool suppressGetterForTTDebug = requestContext->GetThreadContext()->IsRuntimeInTTDMode() && requestContext->GetThreadContext()->TTDLog->ShouldDoGetterInvocationSupression();
            TTD::TTModeStackAutoPopper suppressModeAutoPopper(requestContext->GetThreadContext()->TTDLog);
            if(suppressGetterForTTDebug)
            {LOGMEIN("DiagObjectModel.cpp] 1836\n");
                suppressModeAutoPopper.PushModeAndSetToAutoPop(TTD::TTDMode::DebuggerSuppressGetter);
            }
#endif

            if (Js::JavascriptOperators::GetProperty(obj, propId, &objValue, requestContext))
            {LOGMEIN("DiagObjectModel.cpp] 1842\n");
                return objValue;
            }
        }

        return nullptr;
    }

    //--------------------------
    // RecyclableObjectDisplay


    RecyclableObjectDisplay::RecyclableObjectDisplay(ResolvedObject* resolvedObject, DBGPROP_ATTRIB_FLAGS defaultAttributes)
        : scriptContext(resolvedObject->scriptContext),
          instance(resolvedObject->obj),
          originalInstance(resolvedObject->originalObj != nullptr ? resolvedObject->originalObj : resolvedObject->obj), // If we don't have it set it means originalInstance should point to object itself
          name(resolvedObject->name),
          pObjAddress(resolvedObject->address),
          defaultAttributes(defaultAttributes),
          propertyId(resolvedObject->propId)
    {LOGMEIN("DiagObjectModel.cpp] 1862\n");
    }

    bool RecyclableObjectDisplay::IsLiteralProperty() const
    {LOGMEIN("DiagObjectModel.cpp] 1866\n");
        Assert(this->scriptContext);

        if (this->propertyId != Constants::NoProperty)
        {LOGMEIN("DiagObjectModel.cpp] 1870\n");
            Js::PropertyRecord const * propertyRecord = this->scriptContext->GetThreadContext()->GetPropertyName(this->propertyId);
            const WCHAR* startOfPropertyName = propertyRecord->GetBuffer();
            const WCHAR* endOfIdentifier = this->scriptContext->GetCharClassifier()->SkipIdentifier((LPCOLESTR)propertyRecord->GetBuffer());
            return (charcount_t)(endOfIdentifier - startOfPropertyName) == propertyRecord->GetLength();
        }
        else
        {
            return true;
        }
    }


    bool RecyclableObjectDisplay::IsSymbolProperty()
    {LOGMEIN("DiagObjectModel.cpp] 1884\n");
        Assert(this->scriptContext);

        if (this->propertyId != Constants::NoProperty)
        {LOGMEIN("DiagObjectModel.cpp] 1888\n");
            Js::PropertyRecord const * propertyRecord = this->scriptContext->GetThreadContext()->GetPropertyName(this->propertyId);
            return propertyRecord->IsSymbol();
        }

        return false;
    }

    LPCWSTR RecyclableObjectDisplay::Name()
    {LOGMEIN("DiagObjectModel.cpp] 1897\n");
        return name;
    }

    LPCWSTR RecyclableObjectDisplay::Type()
    {LOGMEIN("DiagObjectModel.cpp] 1902\n");
        LPCWSTR typeStr;

        if(Js::TaggedInt::Is(instance) || Js::JavascriptNumber::Is(instance))
        {LOGMEIN("DiagObjectModel.cpp] 1906\n");
            typeStr = _u("Number");
        }
        else
        {
            Js::RecyclableObject* obj = Js::RecyclableObject::FromVar(instance);

            StringBuilder<ArenaAllocator>* builder = scriptContext->GetThreadContext()->GetDebugManager()->pCurrentInterpreterLocation->stringBuilder;
            builder->Reset();

            // For the RecyclableObject try to find out the constructor, which will be shown as type for the object.
            // This case is to handle the user defined function, built in objects have dedicated classes to handle.

            Var value = nullptr;
            TypeId typeId = obj->GetTypeId();
            if (typeId == TypeIds_Object && GetPropertyWithScriptEnter(obj, obj, PropertyIds::constructor, &value, scriptContext))
            {LOGMEIN("DiagObjectModel.cpp] 1922\n");
                builder->AppendCppLiteral(_u("Object"));
                if (Js::JavascriptFunction::Is(value))
                {LOGMEIN("DiagObjectModel.cpp] 1925\n");
                    Js::JavascriptFunction *pfunction = Js::JavascriptFunction::FromVar(value);
                    // For an odd chance that the constructor wasn't called to create the object.
                    Js::ParseableFunctionInfo *pFuncBody = pfunction->GetFunctionProxy() != nullptr ? pfunction->GetFunctionProxy()->EnsureDeserialized() : nullptr;
                    if (pFuncBody)
                    {LOGMEIN("DiagObjectModel.cpp] 1930\n");
                        const char16* pDisplayName = pFuncBody->GetDisplayName();
                        if (pDisplayName)
                        {LOGMEIN("DiagObjectModel.cpp] 1933\n");
                            builder->AppendCppLiteral(_u(", ("));
                            builder->AppendSz(pDisplayName);
                            builder->Append(_u(')'));
                        }
                    }
                }
                typeStr = builder->Detach();
            }
            else if (obj->GetDiagTypeString(builder, scriptContext))
            {LOGMEIN("DiagObjectModel.cpp] 1943\n");
                typeStr = builder->Detach();
            }
            else
            {
                typeStr = _u("Undefined");
            }
        }

        return typeStr;
    }

    Var RecyclableObjectDisplay::GetVarValue(BOOL fUpdated)
    {LOGMEIN("DiagObjectModel.cpp] 1956\n");
        if (pObjAddress)
        {LOGMEIN("DiagObjectModel.cpp] 1958\n");
            return pObjAddress->GetValue(fUpdated);
        }
        return instance;
    }

    LPCWSTR RecyclableObjectDisplay::Value(int radix)
    {LOGMEIN("DiagObjectModel.cpp] 1965\n");
        LPCWSTR valueStr = _u("");

        if(Js::TaggedInt::Is(instance)
            || Js::JavascriptNumber::Is(instance)
            || Js::JavascriptNumberObject::Is(instance)
            || Js::JavascriptOperators::GetTypeId(instance) == TypeIds_Int64Number
            || Js::JavascriptOperators::GetTypeId(instance) == TypeIds_UInt64Number)
        {LOGMEIN("DiagObjectModel.cpp] 1973\n");
            double value;
            if (Js::TaggedInt::Is(instance))
            {LOGMEIN("DiagObjectModel.cpp] 1976\n");
                value = TaggedInt::ToDouble(instance);
            }
            else if (Js::JavascriptNumber::Is(instance))
            {LOGMEIN("DiagObjectModel.cpp] 1980\n");
                value = Js::JavascriptNumber::GetValue(instance);
            }
            else if (Js::JavascriptOperators::GetTypeId(instance) == TypeIds_Int64Number)
            {LOGMEIN("DiagObjectModel.cpp] 1984\n");
                value = (double)JavascriptInt64Number::FromVar(instance)->GetValue();
            }
            else if (Js::JavascriptOperators::GetTypeId(instance) == TypeIds_UInt64Number)
            {LOGMEIN("DiagObjectModel.cpp] 1988\n");
                value = (double)JavascriptUInt64Number::FromVar(instance)->GetValue();
            }
            else
            {
                Js::JavascriptNumberObject* numobj = Js::JavascriptNumberObject::FromVar(instance);
                value = numobj->GetValue();
            }

            // For fractional values, radix is ignored.
            int32 l = (int32)value;
            bool isZero = JavascriptNumber::IsZero(value - (double)l);

            if (radix == 10 || !isZero)
            {LOGMEIN("DiagObjectModel.cpp] 2002\n");
                if (Js::JavascriptNumber::IsNegZero(value))
                {LOGMEIN("DiagObjectModel.cpp] 2004\n");
                    // In debugger, we wanted to show negative zero explicitly
                    valueStr = _u("-0");
                }
                else
                {
                    valueStr = Js::JavascriptNumber::ToStringRadix10(value, scriptContext)->GetSz();
                }
            }
            else if (radix >= 2 && radix <= 36)
            {LOGMEIN("DiagObjectModel.cpp] 2014\n");
                if (radix == 16)
                {LOGMEIN("DiagObjectModel.cpp] 2016\n");
                    if (value < 0)
                    {LOGMEIN("DiagObjectModel.cpp] 2018\n");
                        // On the tools side we show unsigned value.
                        uint32 ul = static_cast<uint32>(static_cast<int32>(value)); // ARM: casting negative value to uint32 gives 0
                        value = (double)ul;
                    }
                    valueStr = Js::JavascriptString::Concat(scriptContext->GetLibrary()->CreateStringFromCppLiteral(_u("0x")),
                                                            Js::JavascriptNumber::ToStringRadixHelper(value, radix, scriptContext))->GetSz();
                }
                else
                {
                    valueStr = Js::JavascriptNumber::ToStringRadixHelper(value, radix, scriptContext)->GetSz();
                }
            }
        }
        else
        {
            Js::RecyclableObject* obj = Js::RecyclableObject::FromVar(instance);

            StringBuilder<ArenaAllocator>* builder = scriptContext->GetThreadContext()->GetDebugManager()->pCurrentInterpreterLocation->stringBuilder;
            builder->Reset();

            if (obj->GetDiagValueString(builder, scriptContext))
            {LOGMEIN("DiagObjectModel.cpp] 2040\n");
                valueStr = builder->Detach();
            }
            else
            {
                valueStr = _u("undefined");
            }
        }

        return valueStr;
    }

    BOOL RecyclableObjectDisplay::HasChildren()
    {LOGMEIN("DiagObjectModel.cpp] 2053\n");
        if (Js::RecyclableObject::Is(instance))
        {LOGMEIN("DiagObjectModel.cpp] 2055\n");
            Js::RecyclableObject* object = Js::RecyclableObject::FromVar(instance);

            if (JavascriptOperators::IsObject(object))
            {LOGMEIN("DiagObjectModel.cpp] 2059\n");
                if (JavascriptOperators::GetTypeId(object) == TypeIds_HostDispatch)
                {LOGMEIN("DiagObjectModel.cpp] 2061\n");
                    return TRUE;
                }

                try
                {LOGMEIN("DiagObjectModel.cpp] 2066\n");
                    auto funcPtr = [&]()
                    {
                        IGNORE_STACKWALK_EXCEPTION(scriptContext);
                        if (object->CanHaveInterceptors())
                        {LOGMEIN("DiagObjectModel.cpp] 2071\n");
                            Js::ForInObjectEnumerator enumerator(object, object->GetScriptContext(), /* enumSymbols */ true);
                            Js::PropertyId propertyId;
                            if (enumerator.MoveAndGetNext(propertyId))
                            {LOGMEIN("DiagObjectModel.cpp] 2075\n");
                                enumerator.Clear();
                                return TRUE;
                            }
                        }
                        else if (object->GetPropertyCount() > 0 || (JavascriptOperators::GetTypeId(object->GetPrototype()) != TypeIds_Null))
                        {LOGMEIN("DiagObjectModel.cpp] 2081\n");
                            return TRUE;
                        }

                        return FALSE;
                    };

                    BOOL autoFuncReturn = FALSE;

                    if (!scriptContext->GetThreadContext()->IsScriptActive())
                    {
                        BEGIN_JS_RUNTIME_CALL_EX(scriptContext, false)
                        {LOGMEIN("DiagObjectModel.cpp] 2093\n");
                            autoFuncReturn = funcPtr();
                        }
                        END_JS_RUNTIME_CALL(scriptContext);
                    }
                    else
                    {
                        autoFuncReturn = funcPtr();
                    }

                    if (autoFuncReturn == TRUE)
                    {LOGMEIN("DiagObjectModel.cpp] 2104\n");
                        return TRUE;
                    }
                }
                catch (const JavascriptException& err)
                {LOGMEIN("DiagObjectModel.cpp] 2109\n");
                    // The For in enumerator can throw an exception and we will use the error object as a child in that case.
                    Var error = err.GetAndClear()->GetThrownObject(scriptContext);
                    if (error != nullptr && Js::JavascriptError::Is(error))
                    {LOGMEIN("DiagObjectModel.cpp] 2113\n");
                        return TRUE;
                    }
                    return FALSE;
                }
            }
        }

        return FALSE;
    }

    BOOL RecyclableObjectDisplay::Set(Var updateObject)
    {LOGMEIN("DiagObjectModel.cpp] 2125\n");
        if (pObjAddress)
        {LOGMEIN("DiagObjectModel.cpp] 2127\n");
            return pObjAddress->Set(updateObject);
        }
        return FALSE;
    }

    DBGPROP_ATTRIB_FLAGS RecyclableObjectDisplay::GetTypeAttribute()
    {LOGMEIN("DiagObjectModel.cpp] 2134\n");
        DBGPROP_ATTRIB_FLAGS flag = defaultAttributes;

        if (Js::RecyclableObject::Is(instance))
        {LOGMEIN("DiagObjectModel.cpp] 2138\n");
            if (instance == scriptContext->GetLibrary()->GetDebuggerDeadZoneBlockVariableString())
            {LOGMEIN("DiagObjectModel.cpp] 2140\n");
                flag |= DBGPROP_ATTRIB_VALUE_IS_INVALID;
            }
            else if (JavascriptOperators::GetTypeId(instance) == TypeIds_Function)
            {LOGMEIN("DiagObjectModel.cpp] 2144\n");
                flag |= DBGPROP_ATTRIB_VALUE_IS_METHOD;
            }
            else if (JavascriptOperators::GetTypeId(instance) == TypeIds_String
                || JavascriptOperators::GetTypeId(instance) == TypeIds_StringObject)
            {LOGMEIN("DiagObjectModel.cpp] 2149\n");
                flag |= DBGPROP_ATTRIB_VALUE_IS_RAW_STRING;
            }
        }

        auto checkWriteableFunction = [&]()
        {
            if (pObjAddress && !pObjAddress->IsWritable())
            {LOGMEIN("DiagObjectModel.cpp] 2157\n");
                flag |= DBGPROP_ATTRIB_VALUE_READONLY;
            }
        };

        if (!scriptContext->GetThreadContext()->IsScriptActive())
        {
            BEGIN_JS_RUNTIME_CALL_EX(scriptContext, false);
            {LOGMEIN("DiagObjectModel.cpp] 2165\n");
                IGNORE_STACKWALK_EXCEPTION(scriptContext);
                checkWriteableFunction();
            }
            END_JS_RUNTIME_CALL(scriptContext);
        }
        else
        {
            checkWriteableFunction();
        }
        // TODO : need to identify Events explicitly for fastDOM

        return flag;
    }


    /* static */
    BOOL RecyclableObjectDisplay::GetPropertyWithScriptEnter(RecyclableObject* originalInstance, RecyclableObject* instance, PropertyId propertyId, Var* value, ScriptContext* scriptContext)
    {LOGMEIN("DiagObjectModel.cpp] 2183\n");
        BOOL retValue = FALSE;

#if ENABLE_TTD
        bool suppressGetterForTTDebug = scriptContext->GetThreadContext()->IsRuntimeInTTDMode() && scriptContext->GetThreadContext()->TTDLog->ShouldDoGetterInvocationSupression();
        TTD::TTModeStackAutoPopper suppressModeAutoPopper(scriptContext->GetThreadContext()->TTDLog);
        if(suppressGetterForTTDebug)
        {LOGMEIN("DiagObjectModel.cpp] 2190\n");
            suppressModeAutoPopper.PushModeAndSetToAutoPop(TTD::TTDMode::DebuggerSuppressGetter);
        }
#endif

        if(!scriptContext->GetThreadContext()->IsScriptActive())
        {
            BEGIN_JS_RUNTIME_CALL_EX(scriptContext, false)
            {LOGMEIN("DiagObjectModel.cpp] 2198\n");
                IGNORE_STACKWALK_EXCEPTION(scriptContext);
                retValue = Js::JavascriptOperators::GetProperty(originalInstance, instance, propertyId, value, scriptContext);
            }
            END_JS_RUNTIME_CALL(scriptContext);
        }
        else
        {
            retValue = Js::JavascriptOperators::GetProperty(originalInstance, instance, propertyId, value, scriptContext);
        }

        return retValue;
    }


    WeakArenaReference<IDiagObjectModelWalkerBase>* RecyclableObjectDisplay::CreateWalker()
    {LOGMEIN("DiagObjectModel.cpp] 2214\n");
        return CreateAWalker<RecyclableObjectWalker>(scriptContext, instance, originalInstance);
    }

    StringBuilder<ArenaAllocator>* RecyclableObjectDisplay::GetStringBuilder()
    {LOGMEIN("DiagObjectModel.cpp] 2219\n");
        return scriptContext->GetThreadContext()->GetDebugManager()->pCurrentInterpreterLocation->stringBuilder;
    }

    PropertyId RecyclableObjectDisplay::GetPropertyId() const
    {LOGMEIN("DiagObjectModel.cpp] 2224\n");
        return this->propertyId;
    }

    // ------------------------------------
    // RecyclableObjectWalker

    RecyclableObjectWalker::RecyclableObjectWalker(ScriptContext* _scriptContext, Var _slot)
        : scriptContext(_scriptContext),
        instance(_slot),
        originalInstance(_slot),
        pMembersList(nullptr),
        innerArrayObjectWalker(nullptr),
        fakeGroupObjectWalkerList(nullptr)
    {LOGMEIN("DiagObjectModel.cpp] 2238\n");
    }

    RecyclableObjectWalker::RecyclableObjectWalker(ScriptContext* _scriptContext, Var _slot, Var _originalInstance)
        : scriptContext(_scriptContext),
          instance(_slot),
          originalInstance(_originalInstance),
          pMembersList(nullptr),
          innerArrayObjectWalker(nullptr),
          fakeGroupObjectWalkerList(nullptr)
    {LOGMEIN("DiagObjectModel.cpp] 2248\n");
    }

    BOOL RecyclableObjectWalker::Get(int index, ResolvedObject* pResolvedObject)
    {
        AssertMsg(pResolvedObject, "Bad usage of RecyclableObjectWalker::Get");

        int fakeObjCount = fakeGroupObjectWalkerList ? fakeGroupObjectWalkerList->Count() : 0;
        int arrayItemCount = innerArrayObjectWalker ? innerArrayObjectWalker->GetChildrenCount() : 0;

        if (index < 0 || !pMembersList || index >= (pMembersList->Count() + arrayItemCount + fakeObjCount))
        {LOGMEIN("DiagObjectModel.cpp] 2259\n");
            return FALSE;
        }

        int nonArrayElementCount = Js::RecyclableObject::Is(instance) ? pMembersList->Count() : 0;

        // First the virtual groups
        if (index < fakeObjCount)
        {LOGMEIN("DiagObjectModel.cpp] 2267\n");
            Assert(fakeGroupObjectWalkerList);
            return fakeGroupObjectWalkerList->Item(index)->GetGroupObject(pResolvedObject);
        }

        index -= fakeObjCount;

        if (index < nonArrayElementCount)
        {LOGMEIN("DiagObjectModel.cpp] 2275\n");
            Assert(Js::RecyclableObject::Is(instance));

            pResolvedObject->propId = pMembersList->Item(index)->propId;

            if (pResolvedObject->propId == Js::Constants::NoProperty || Js::IsInternalPropertyId(pResolvedObject->propId))
            {LOGMEIN("DiagObjectModel.cpp] 2281\n");
                Assert(FALSE);
                return FALSE;
            }

            Js::DebuggerPropertyDisplayInfo* displayInfo = pMembersList->Item(index);
            const Js::PropertyRecord* propertyRecord = scriptContext->GetPropertyName(pResolvedObject->propId);

            pResolvedObject->name = propertyRecord->GetBuffer();
            pResolvedObject->obj = displayInfo->aVar;
            Assert(pResolvedObject->obj);

            pResolvedObject->scriptContext = scriptContext;
            pResolvedObject->typeId = JavascriptOperators::GetTypeId(pResolvedObject->obj);

            pResolvedObject->address = Anew(GetArenaFromContext(scriptContext),
                RecyclableObjectAddress,
                instance,
                pResolvedObject->propId,
                pResolvedObject->obj,
                displayInfo->IsInDeadZone() ? TRUE : FALSE);

            pResolvedObject->isConst = displayInfo->IsConst();

            return TRUE;
        }

        index -= nonArrayElementCount;

        if (index < arrayItemCount)
        {LOGMEIN("DiagObjectModel.cpp] 2311\n");
            Assert(innerArrayObjectWalker);
            return innerArrayObjectWalker->Get(index, pResolvedObject);
        }

        Assert(false);
        return FALSE;
    }

    void RecyclableObjectWalker::EnsureFakeGroupObjectWalkerList()
    {LOGMEIN("DiagObjectModel.cpp] 2321\n");
        if (fakeGroupObjectWalkerList == nullptr)
        {LOGMEIN("DiagObjectModel.cpp] 2323\n");
            ArenaAllocator *arena = GetArenaFromContext(scriptContext);
            fakeGroupObjectWalkerList = JsUtil::List<IDiagObjectModelWalkerBase *, ArenaAllocator>::New(arena);
        }
    }

    IDiagObjectAddress *RecyclableObjectWalker::FindPropertyAddress(PropertyId propertyId, bool& isConst)
    {LOGMEIN("DiagObjectModel.cpp] 2330\n");
        GetChildrenCount(); // Ensure to populate members

        if (pMembersList != nullptr)
        {LOGMEIN("DiagObjectModel.cpp] 2334\n");
            for (int i = 0; i < pMembersList->Count(); i++)
            {LOGMEIN("DiagObjectModel.cpp] 2336\n");
                DebuggerPropertyDisplayInfo *pair = pMembersList->Item(i);
                Assert(pair);
                if (pair->propId == propertyId)
                {LOGMEIN("DiagObjectModel.cpp] 2340\n");
                    isConst = pair->IsConst();
                    return Anew(GetArenaFromContext(scriptContext),
                        RecyclableObjectAddress,
                        instance,
                        propertyId,
                        pair->aVar,
                        pair->IsInDeadZone() ? TRUE : FALSE);
                }
            }
        }

        // Following is for "with object" scope lookup. We may have members in [Methods] group or prototype chain that need to
        // be exposed to expression evaluation.
        if (fakeGroupObjectWalkerList != nullptr)
        {LOGMEIN("DiagObjectModel.cpp] 2355\n");
            // WARNING: Following depends on [Methods] group being before [prototype] group. We need to check local [Methods] group
            // first for local properties before going to prototype chain.
            for (int i = 0; i < fakeGroupObjectWalkerList->Count(); i++)
            {LOGMEIN("DiagObjectModel.cpp] 2359\n");
                IDiagObjectAddress* address = fakeGroupObjectWalkerList->Item(i)->FindPropertyAddress(propertyId, isConst);
                if (address != nullptr)
                {LOGMEIN("DiagObjectModel.cpp] 2362\n");
                    return address;
                }
            }
        }

        return nullptr;
    }

    uint32 RecyclableObjectWalker::GetChildrenCount()
    {LOGMEIN("DiagObjectModel.cpp] 2372\n");
        if (pMembersList == nullptr)
        {LOGMEIN("DiagObjectModel.cpp] 2374\n");
            ArenaAllocator *arena = GetArenaFromContext(scriptContext);

            pMembersList = JsUtil::List<DebuggerPropertyDisplayInfo *, ArenaAllocator>::New(arena);

            RecyclableMethodsGroupWalker *pMethodsGroupWalker = nullptr;

            if (Js::RecyclableObject::Is(instance))
            {LOGMEIN("DiagObjectModel.cpp] 2382\n");
                Js::RecyclableObject* object = Js::RecyclableObject::FromVar(instance);
                // If we are walking a prototype, we'll use its instance for property names enumeration, but originalInstance to get values
                Js::RecyclableObject* originalObject = (originalInstance != nullptr) ? Js::RecyclableObject::FromVar(originalInstance) : object;
                const Js::TypeId typeId = JavascriptOperators::GetTypeId(instance);

                if (JavascriptOperators::IsObject(object))
                {LOGMEIN("DiagObjectModel.cpp] 2389\n");
                    if (object->CanHaveInterceptors() || JavascriptOperators::GetTypeId(object) == TypeIds_Proxy)
                    {LOGMEIN("DiagObjectModel.cpp] 2391\n");
                        try
                        {LOGMEIN("DiagObjectModel.cpp] 2393\n");
                            ScriptContext * objectContext = object->GetScriptContext();
                            JavascriptStaticEnumerator enumerator;
                            if (object->GetEnumerator(&enumerator, EnumeratorFlags::EnumNonEnumerable | EnumeratorFlags::EnumSymbols, objectContext))
                            {LOGMEIN("DiagObjectModel.cpp] 2397\n");
                                Js::PropertyId propertyId;
                                Var obj;

                                while ((obj = enumerator.MoveAndGetNext(propertyId)) != nullptr)
                                {LOGMEIN("DiagObjectModel.cpp] 2402\n");
                                    if (!JavascriptString::Is(obj))
                                    {LOGMEIN("DiagObjectModel.cpp] 2404\n");
                                        continue;
                                    }

                                    if (propertyId == Constants::NoProperty)
                                    {LOGMEIN("DiagObjectModel.cpp] 2409\n");
                                        JavascriptString *pString = JavascriptString::FromVar(obj);
                                        if (VirtualTableInfo<Js::PropertyString>::HasVirtualTable(pString))
                                        {LOGMEIN("DiagObjectModel.cpp] 2412\n");
                                            // If we have a property string, it is assumed that the propertyId is being
                                            // kept alive with the object
                                            PropertyString * propertyString = (PropertyString *)pString;
                                            propertyId = propertyString->GetPropertyRecord()->GetPropertyId();
                                        }
                                        else
                                        {
                                            const PropertyRecord* propertyRecord;
                                            objectContext->GetOrAddPropertyRecord(pString->GetSz(), pString->GetLength(), &propertyRecord);
                                            propertyId = propertyRecord->GetPropertyId();
                                        }
                                    }
                                    // MoveAndGetNext shouldn't return an internal property id
                                    Assert(!Js::IsInternalPropertyId(propertyId));

                                    uint32 indexVal;
                                    Var varValue;
                                    if (objectContext->IsNumericPropertyId(propertyId, &indexVal) && object->GetItem(object, indexVal, &varValue, objectContext))
                                    {
                                        InsertItem(propertyId, false /*isConst*/, false /*isUnscoped*/, varValue, &pMethodsGroupWalker, true /*shouldPinProperty*/);
                                    }
                                    else
                                    {
                                        InsertItem(originalObject, object, propertyId, false /*isConst*/, false /*isUnscoped*/, &pMethodsGroupWalker, true /*shouldPinProperty*/);
                                    }
                                }
                            }
                        }
                        catch (const JavascriptException& err)
                        {LOGMEIN("DiagObjectModel.cpp] 2442\n");
                            Var error = err.GetAndClear()->GetThrownObject(scriptContext);
                            if (error != nullptr && Js::JavascriptError::Is(error))
                            {LOGMEIN("DiagObjectModel.cpp] 2445\n");
                                Js::PropertyId propertyId = scriptContext->GetOrAddPropertyIdTracked(_u("{error}"));
                                InsertItem(propertyId, false /*isConst*/, false /*isUnscoped*/, error, &pMethodsGroupWalker);
                            }
                        }

                        if (typeId == TypeIds_Proxy)
                        {LOGMEIN("DiagObjectModel.cpp] 2452\n");
                            // Provide [Proxy] group object
                            EnsureFakeGroupObjectWalkerList();

                            JavascriptProxy* proxy = JavascriptProxy::FromVar(object);
                            RecyclableProxyObjectWalker* proxyWalker = Anew(arena, RecyclableProxyObjectWalker, scriptContext, proxy);
                            fakeGroupObjectWalkerList->Add(proxyWalker);
                        }
                        // If current object has internal proto object then provide [prototype] group object.
                        if (JavascriptOperators::GetTypeId(object->GetPrototype()) != TypeIds_Null)
                        {LOGMEIN("DiagObjectModel.cpp] 2462\n");
                            // Has [prototype] object.
                            EnsureFakeGroupObjectWalkerList();

                            RecyclableProtoObjectWalker *pProtoWalker = Anew(arena, RecyclableProtoObjectWalker, scriptContext, instance, (originalInstance == nullptr) ? instance : originalInstance);
                            fakeGroupObjectWalkerList->Add(pProtoWalker);
                        }
                    }
                    else
                    {
                        RecyclableObject* wrapperObject = nullptr;
                        if (JavascriptOperators::GetTypeId(object) == TypeIds_WithScopeObject)
                        {LOGMEIN("DiagObjectModel.cpp] 2474\n");
                            wrapperObject = object;
                            object = object->GetThisObjectOrUnWrap();
                        }

                        int count = object->GetPropertyCount();

                        for (int i = 0; i < count; i++)
                        {LOGMEIN("DiagObjectModel.cpp] 2482\n");
                            Js::PropertyId propertyId = object->GetPropertyId((PropertyIndex)i);
                            bool isUnscoped = false;
                            if (wrapperObject && JavascriptOperators::IsPropertyUnscopable(object, propertyId))
                            {LOGMEIN("DiagObjectModel.cpp] 2486\n");
                                isUnscoped = true;
                            }
                            if (propertyId != Js::Constants::NoProperty && !Js::IsInternalPropertyId(propertyId))
                            {
                                InsertItem(originalObject, object, propertyId, false /*isConst*/, isUnscoped, &pMethodsGroupWalker);
                            }
                        }

                        if (CONFIG_FLAG(EnumerateSpecialPropertiesInDebugger))
                        {LOGMEIN("DiagObjectModel.cpp] 2496\n");
                            count = object->GetSpecialPropertyCount();
                            PropertyId const * specialPropertyIds = object->GetSpecialPropertyIds();
                            for (int i = 0; i < count; i++)
                            {LOGMEIN("DiagObjectModel.cpp] 2500\n");
                                Js::PropertyId propertyId = specialPropertyIds[i];
                                bool isUnscoped = false;
                                if (wrapperObject && JavascriptOperators::IsPropertyUnscopable(object, propertyId))
                                {LOGMEIN("DiagObjectModel.cpp] 2504\n");
                                    isUnscoped = true;
                                }
                                if (propertyId != Js::Constants::NoProperty)
                                {LOGMEIN("DiagObjectModel.cpp] 2508\n");
                                    bool isConst = true;
                                    if (propertyId == PropertyIds::length && Js::JavascriptArray::Is(object))
                                    {LOGMEIN("DiagObjectModel.cpp] 2511\n");
                                        // For JavascriptArrays, we allow resetting the length special property.
                                        isConst = false;
                                    }

                                    auto containsPredicate = [&](Js::DebuggerPropertyDisplayInfo* info) {LOGMEIN("DiagObjectModel.cpp] 2516\n"); return info->propId == propertyId; };
                                    if (Js::BoundFunction::Is(object)
                                        && this->pMembersList->Any(containsPredicate))
                                    {LOGMEIN("DiagObjectModel.cpp] 2519\n");
                                        // Bound functions can already contain their special properties,
                                        // so we need to check for that (caller and arguments).  This occurs
                                        // when JavascriptFunction::EntryBind() is called.  Arguments can similarly
                                        // already display caller in compat mode 8.
                                        continue;
                                    }

                                    AssertMsg(!this->pMembersList->Any(containsPredicate), "Special property already on the object, no need to insert.");

                                    InsertItem(originalObject, object, propertyId, isConst, isUnscoped, &pMethodsGroupWalker);
                                }
                            }
                            if (Js::JavascriptFunction::Is(object))
                            {LOGMEIN("DiagObjectModel.cpp] 2533\n");
                                // We need to special-case RegExp constructor here because it has some special properties (above) and some
                                // special enumerable properties which should all show up in the debugger.
                                JavascriptRegExpConstructor* regExp = scriptContext->GetLibrary()->GetRegExpConstructor();

                                if (regExp == object)
                                {LOGMEIN("DiagObjectModel.cpp] 2539\n");
                                    bool isUnscoped = false;
                                    bool isConst = true;
                                    count = regExp->GetSpecialEnumerablePropertyCount();
                                    PropertyId const * specialEnumerablePropertyIds = regExp->GetSpecialEnumerablePropertyIds();

                                    for (int i = 0; i < count; i++)
                                    {LOGMEIN("DiagObjectModel.cpp] 2546\n");
                                        Js::PropertyId propertyId = specialEnumerablePropertyIds[i];

                                        InsertItem(originalObject, object, propertyId, isConst, isUnscoped, &pMethodsGroupWalker);
                                    }
                                }
                                else if (Js::JavascriptFunction::FromVar(object)->IsScriptFunction() || Js::JavascriptFunction::FromVar(object)->IsBoundFunction())
                                {
                                    // Adding special property length for the ScriptFunction, like it is done in JavascriptFunction::GetSpecialNonEnumerablePropertyName
                                    InsertItem(originalObject, object, PropertyIds::length, true/*not editable*/, false /*isUnscoped*/, &pMethodsGroupWalker);
                                }
                            }
                        }

                        // If current object has internal proto object then provide [prototype] group object.
                        if (JavascriptOperators::GetTypeId(object->GetPrototype()) != TypeIds_Null)
                        {LOGMEIN("DiagObjectModel.cpp] 2562\n");
                            // Has [prototype] object.
                            EnsureFakeGroupObjectWalkerList();

                            RecyclableProtoObjectWalker *pProtoWalker = Anew(arena, RecyclableProtoObjectWalker, scriptContext, instance, originalInstance);
                            fakeGroupObjectWalkerList->Add(pProtoWalker);
                        }
                    }

                    // If the object contains array indices.
                    if (typeId == TypeIds_Arguments)
                    {LOGMEIN("DiagObjectModel.cpp] 2573\n");
                        // Create ArgumentsArray walker for an arguments object

                        Js::ArgumentsObject * argObj = static_cast<Js::ArgumentsObject*>(instance);
                        Assert(argObj);

                        if (argObj->GetNumberOfArguments() > 0 || argObj->HasNonEmptyObjectArray())
                        {LOGMEIN("DiagObjectModel.cpp] 2580\n");
                            innerArrayObjectWalker = Anew(arena, RecyclableArgumentsArrayWalker, scriptContext, (Var)instance, originalInstance);
                        }
                    }
                    else if (typeId == TypeIds_Map)
                    {LOGMEIN("DiagObjectModel.cpp] 2585\n");
                        // Provide [Map] group object.
                        EnsureFakeGroupObjectWalkerList();

                        JavascriptMap* map = JavascriptMap::FromVar(object);
                        RecyclableMapObjectWalker *pMapWalker = Anew(arena, RecyclableMapObjectWalker, scriptContext, map);
                        fakeGroupObjectWalkerList->Add(pMapWalker);
                    }
                    else if (typeId == TypeIds_Set)
                    {LOGMEIN("DiagObjectModel.cpp] 2594\n");
                        // Provide [Set] group object.
                        EnsureFakeGroupObjectWalkerList();

                        JavascriptSet* set = JavascriptSet::FromVar(object);
                        RecyclableSetObjectWalker *pSetWalker = Anew(arena, RecyclableSetObjectWalker, scriptContext, set);
                        fakeGroupObjectWalkerList->Add(pSetWalker);
                    }
                    else if (typeId == TypeIds_WeakMap)
                    {LOGMEIN("DiagObjectModel.cpp] 2603\n");
                        // Provide [WeakMap] group object.
                        EnsureFakeGroupObjectWalkerList();

                        JavascriptWeakMap* weakMap = JavascriptWeakMap::FromVar(object);
                        RecyclableWeakMapObjectWalker *pWeakMapWalker = Anew(arena, RecyclableWeakMapObjectWalker, scriptContext, weakMap);
                        fakeGroupObjectWalkerList->Add(pWeakMapWalker);
                    }
                    else if (typeId == TypeIds_WeakSet)
                    {LOGMEIN("DiagObjectModel.cpp] 2612\n");
                        // Provide [WeakSet] group object.
                        EnsureFakeGroupObjectWalkerList();

                        JavascriptWeakSet* weakSet = JavascriptWeakSet::FromVar(object);
                        RecyclableWeakSetObjectWalker *pWeakSetWalker = Anew(arena, RecyclableWeakSetObjectWalker, scriptContext, weakSet);
                        fakeGroupObjectWalkerList->Add(pWeakSetWalker);
                    }
                    else if (typeId == TypeIds_Promise)
                    {LOGMEIN("DiagObjectModel.cpp] 2621\n");
                        // Provide [Promise] group object.
                        EnsureFakeGroupObjectWalkerList();

                        JavascriptPromise* promise = JavascriptPromise::FromVar(object);
                        RecyclablePromiseObjectWalker *pPromiseWalker = Anew(arena, RecyclablePromiseObjectWalker, scriptContext, promise);
                        fakeGroupObjectWalkerList->Add(pPromiseWalker);
                    }
                    else if (Js::DynamicType::Is(typeId))
                    {LOGMEIN("DiagObjectModel.cpp] 2630\n");
                        DynamicObject *const dynamicObject = Js::DynamicObject::FromVar(instance);
                        if (dynamicObject->HasNonEmptyObjectArray())
                        {LOGMEIN("DiagObjectModel.cpp] 2633\n");
                            ArrayObject* objectArray = dynamicObject->GetObjectArray();
                            if (Js::ES5Array::Is(objectArray))
                            {LOGMEIN("DiagObjectModel.cpp] 2636\n");
                                innerArrayObjectWalker = Anew(arena, RecyclableES5ArrayWalker, scriptContext, objectArray, originalInstance);
                            }
                            else if (Js::JavascriptArray::Is(objectArray))
                            {LOGMEIN("DiagObjectModel.cpp] 2640\n");
                                innerArrayObjectWalker = Anew(arena, RecyclableArrayWalker, scriptContext, objectArray, originalInstance);
                            }
                            else
                            {
                                innerArrayObjectWalker = Anew(arena, RecyclableTypedArrayWalker, scriptContext, objectArray, originalInstance);
                            }

                            innerArrayObjectWalker->SetOnlyWalkOwnProperties(true);
                        }
                    }
                }
            }
            // Sort the members of the methods group
            if (pMethodsGroupWalker)
            {LOGMEIN("DiagObjectModel.cpp] 2655\n");
                pMethodsGroupWalker->Sort();
            }

            // Sort current pMembersList.
            HostDebugContext* hostDebugContext = scriptContext->GetDebugContext()->GetHostDebugContext();
            if (hostDebugContext != nullptr)
            {LOGMEIN("DiagObjectModel.cpp] 2662\n");
                hostDebugContext->SortMembersList(pMembersList, scriptContext);
            }
        }

        uint32 childrenCount =
            pMembersList->Count()
          + (innerArrayObjectWalker ? innerArrayObjectWalker->GetChildrenCount() : 0)
          + (fakeGroupObjectWalkerList ? fakeGroupObjectWalkerList->Count() : 0);

        return childrenCount;
    }

    void RecyclableObjectWalker::InsertItem(
        Js::RecyclableObject *pOriginalObject,
        Js::RecyclableObject *pObject,
        PropertyId propertyId,
        bool isReadOnly,
        bool isUnscoped,
        Js::RecyclableMethodsGroupWalker **ppMethodsGroupWalker,
        bool shouldPinProperty /* = false*/)
    {LOGMEIN("DiagObjectModel.cpp] 2683\n");
        Assert(pOriginalObject);
        Assert(pObject);
        Assert(propertyId);
        Assert(ppMethodsGroupWalker);

        if (propertyId != PropertyIds::__proto__)
        {
            InsertItem(propertyId, isReadOnly, isUnscoped, RecyclableObjectWalker::GetObject(pOriginalObject, pObject, propertyId, scriptContext), ppMethodsGroupWalker, shouldPinProperty);
        }
        else // Since __proto__ defined as a Getter we should always evaluate it against object itself instead of walking prototype chain
        {
            InsertItem(propertyId, isReadOnly, isUnscoped, RecyclableObjectWalker::GetObject(pObject, pObject, propertyId, scriptContext), ppMethodsGroupWalker, shouldPinProperty);
        }
    }

    void RecyclableObjectWalker::InsertItem(
        PropertyId propertyId,
        bool isConst,
        bool isUnscoped,
        Var itemObj,
        Js:: RecyclableMethodsGroupWalker **ppMethodsGroupWalker,
        bool shouldPinProperty /* = false*/)
    {LOGMEIN("DiagObjectModel.cpp] 2706\n");
        Assert(propertyId);
        Assert(ppMethodsGroupWalker);

        if (itemObj == nullptr)
        {LOGMEIN("DiagObjectModel.cpp] 2711\n");
            itemObj = scriptContext->GetLibrary()->GetUndefined();
        }

        if (shouldPinProperty)
        {LOGMEIN("DiagObjectModel.cpp] 2716\n");
            const Js::PropertyRecord * propertyRecord = scriptContext->GetPropertyName(propertyId);
            if (propertyRecord)
            {LOGMEIN("DiagObjectModel.cpp] 2719\n");
                // Pin this record so that it will not go away till we are done with this break.
                scriptContext->GetDebugContext()->GetProbeContainer()->PinPropertyRecord(propertyRecord);
            }
        }

        ArenaAllocator *arena = GetArenaFromContext(scriptContext);

        if (JavascriptOperators::GetTypeId(itemObj) == TypeIds_Function)
        {LOGMEIN("DiagObjectModel.cpp] 2728\n");
            if (scriptContext->GetThreadContext()->GetDebugManager()->IsLocalsDisplayFlagsSet(Js::DebugManager::LocalsDisplayFlags::LocalsDisplayFlags_NoGroupMethods))
            {LOGMEIN("DiagObjectModel.cpp] 2730\n");
                DebuggerPropertyDisplayInfo *info = Anew(arena, DebuggerPropertyDisplayInfo, propertyId, itemObj, DebuggerPropertyDisplayInfoFlags_Const);
                pMembersList->Add(info);
            }
            else
            {
                EnsureFakeGroupObjectWalkerList();

                if (*ppMethodsGroupWalker == nullptr)
                {LOGMEIN("DiagObjectModel.cpp] 2739\n");
                    *ppMethodsGroupWalker = Anew(arena, RecyclableMethodsGroupWalker, scriptContext, instance);
                    fakeGroupObjectWalkerList->Add(*ppMethodsGroupWalker);
                }

                (*ppMethodsGroupWalker)->AddItem(propertyId, itemObj);
            }
        }
        else
        {
            DWORD flags = DebuggerPropertyDisplayInfoFlags_None;
            flags |= isConst ? DebuggerPropertyDisplayInfoFlags_Const : 0;
            flags |= isUnscoped ? DebuggerPropertyDisplayInfoFlags_Unscope : 0;

            DebuggerPropertyDisplayInfo *info = Anew(arena, DebuggerPropertyDisplayInfo, propertyId, itemObj, flags);

            pMembersList->Add(info);
        }
    }

    /*static*/
    Var RecyclableObjectWalker::GetObject(RecyclableObject* originalInstance, RecyclableObject* instance, PropertyId propertyId, ScriptContext* scriptContext)
    {LOGMEIN("DiagObjectModel.cpp] 2761\n");
        Assert(instance);
        Assert(!Js::IsInternalPropertyId(propertyId));

        Var obj = nullptr;
        try
        {LOGMEIN("DiagObjectModel.cpp] 2767\n");
            if (!RecyclableObjectDisplay::GetPropertyWithScriptEnter(originalInstance, instance, propertyId, &obj, scriptContext))
            {LOGMEIN("DiagObjectModel.cpp] 2769\n");
                return instance->GetScriptContext()->GetMissingPropertyResult();
            }
        }
        catch(const JavascriptException& err)
        {LOGMEIN("DiagObjectModel.cpp] 2774\n");
            Var error = err.GetAndClear()->GetThrownObject(instance->GetScriptContext());
            if (error != nullptr && Js::JavascriptError::Is(error))
            {LOGMEIN("DiagObjectModel.cpp] 2777\n");
                obj = error;
            }
        }

        return obj;
    }


    //--------------------------
    // RecyclableArrayAddress


    RecyclableArrayAddress::RecyclableArrayAddress(Var _parentArray, unsigned int _index)
        : parentArray(_parentArray),
          index(_index)
    {LOGMEIN("DiagObjectModel.cpp] 2793\n");
    }

    BOOL RecyclableArrayAddress::Set(Var updateObject)
    {LOGMEIN("DiagObjectModel.cpp] 2797\n");
        if (Js::JavascriptArray::Is(parentArray))
        {LOGMEIN("DiagObjectModel.cpp] 2799\n");
            Js::JavascriptArray* jsArray = Js::JavascriptArray::FromVar(parentArray);
            return jsArray->SetItem(index, updateObject, PropertyOperation_None);
        }
        return FALSE;
    }

    //--------------------------
    // RecyclableArrayDisplay


    RecyclableArrayDisplay::RecyclableArrayDisplay(ResolvedObject* resolvedObject)
        : RecyclableObjectDisplay(resolvedObject)
    {LOGMEIN("DiagObjectModel.cpp] 2812\n");
    }

    BOOL RecyclableArrayDisplay::HasChildrenInternal(Js::JavascriptArray* arrayObj)
    {LOGMEIN("DiagObjectModel.cpp] 2816\n");
        Assert(arrayObj);
        if (JavascriptOperators::GetTypeId(arrayObj->GetPrototype()) != TypeIds_Null)
        {LOGMEIN("DiagObjectModel.cpp] 2819\n");
            return TRUE;
        }

        uint32 index = arrayObj->GetNextIndex(Js::JavascriptArray::InvalidIndex);
        return index != Js::JavascriptArray::InvalidIndex && index < arrayObj->GetLength();
    }


    BOOL RecyclableArrayDisplay::HasChildren()
    {LOGMEIN("DiagObjectModel.cpp] 2829\n");
        if (Js::JavascriptArray::Is(instance))
        {LOGMEIN("DiagObjectModel.cpp] 2831\n");
            Js::JavascriptArray* arrayObj = Js::JavascriptArray::FromVar(instance);
            if (HasChildrenInternal(arrayObj))
            {LOGMEIN("DiagObjectModel.cpp] 2834\n");
                return TRUE;
            }
        }
        return RecyclableObjectDisplay::HasChildren();
    }

    WeakArenaReference<IDiagObjectModelWalkerBase>* RecyclableArrayDisplay::CreateWalker()
    {LOGMEIN("DiagObjectModel.cpp] 2842\n");
        return CreateAWalker<RecyclableArrayWalker>(scriptContext, instance, originalInstance);
    }


    //--------------------------
    // RecyclableArrayWalker


    uint32 RecyclableArrayWalker::GetItemCount(Js::JavascriptArray* arrayObj)
    {LOGMEIN("DiagObjectModel.cpp] 2852\n");
        if (pAbsoluteIndexList == nullptr)
        {LOGMEIN("DiagObjectModel.cpp] 2854\n");
            Assert(arrayObj);

            pAbsoluteIndexList = JsUtil::List<uint32, ArenaAllocator>::New(GetArenaFromContext(scriptContext));
            Assert(pAbsoluteIndexList);

            uint32 dataIndex = Js::JavascriptArray::InvalidIndex;
            uint32 descriptorIndex = Js::JavascriptArray::InvalidIndex;
            uint32 absIndex = Js::JavascriptArray::InvalidIndex;

            do
            {LOGMEIN("DiagObjectModel.cpp] 2865\n");
                if (absIndex == dataIndex)
                {LOGMEIN("DiagObjectModel.cpp] 2867\n");
                    dataIndex = arrayObj->GetNextIndex(dataIndex);
                }
                if (absIndex == descriptorIndex)
                {LOGMEIN("DiagObjectModel.cpp] 2871\n");
                    descriptorIndex = GetNextDescriptor(descriptorIndex);
                }

                absIndex = min(dataIndex, descriptorIndex);

                if (absIndex == Js::JavascriptArray::InvalidIndex || absIndex >= arrayObj->GetLength())
                {LOGMEIN("DiagObjectModel.cpp] 2878\n");
                    break;
                }

                pAbsoluteIndexList->Add(absIndex);

            } while (absIndex < arrayObj->GetLength());
        }

        return (uint32)pAbsoluteIndexList->Count();
    }

    BOOL RecyclableArrayWalker::FetchItemAtIndex(Js::JavascriptArray* arrayObj, uint32 index, Var * value)
    {LOGMEIN("DiagObjectModel.cpp] 2891\n");
        Assert(arrayObj);
        Assert(value);

        return arrayObj->DirectGetItemAt(index, value);
    }

    Var RecyclableArrayWalker::FetchItemAt(Js::JavascriptArray* arrayObj, uint32 index)
    {LOGMEIN("DiagObjectModel.cpp] 2899\n");
        Assert(arrayObj);
        return arrayObj->DirectGetItem(index);
    }

    LPCWSTR RecyclableArrayWalker::GetIndexName(uint32 index, StringBuilder<ArenaAllocator>* stringBuilder)
    {LOGMEIN("DiagObjectModel.cpp] 2905\n");
        stringBuilder->Append(_u('['));
        if (stringBuilder->AppendUint64(index) != 0)
        {LOGMEIN("DiagObjectModel.cpp] 2908\n");
            return _u("[.]");
        }
        stringBuilder->Append(_u(']'));
        return stringBuilder->Detach();
    }

    RecyclableArrayWalker::RecyclableArrayWalker(ScriptContext* scriptContext, Var instance, Var originalInstance)
        : indexedItemCount(0),
          pAbsoluteIndexList(nullptr),
          fOnlyOwnProperties(false),
          RecyclableObjectWalker(scriptContext,instance,originalInstance)
    {LOGMEIN("DiagObjectModel.cpp] 2920\n");
    }

    BOOL RecyclableArrayWalker::GetResolvedObject(Js::JavascriptArray* arrayObj, int index, ResolvedObject* pResolvedObject, uint32 * pabsIndex)
    {LOGMEIN("DiagObjectModel.cpp] 2924\n");
        Assert(arrayObj);
        Assert(pResolvedObject);
        Assert(pAbsoluteIndexList);
        Assert(pAbsoluteIndexList->Count() > index);

        // translate i'th Item to the correct array index and return
        uint32 absIndex = pAbsoluteIndexList->Item(index);
        pResolvedObject->obj = FetchItemAt(arrayObj, absIndex);
        pResolvedObject->scriptContext = scriptContext;
        pResolvedObject->typeId = JavascriptOperators::GetTypeId(pResolvedObject->obj);
        pResolvedObject->address = nullptr;

        StringBuilder<ArenaAllocator>* builder = GetBuilder();
        Assert(builder);
        builder->Reset();
        pResolvedObject->name = GetIndexName(absIndex, builder);
        if (pabsIndex)
        {LOGMEIN("DiagObjectModel.cpp] 2942\n");
            *pabsIndex = absIndex;
        }

        return TRUE;
    }

    BOOL RecyclableArrayWalker::Get(int i, ResolvedObject* pResolvedObject)
    {
        AssertMsg(pResolvedObject, "Bad usage of RecyclableArrayWalker::Get");

        if (Js::JavascriptArray::Is(instance) || Js::ES5Array::Is(instance))
        {LOGMEIN("DiagObjectModel.cpp] 2954\n");
            Js::JavascriptArray* arrayObj = GetArrayObject();

            int nonArrayElementCount = (!fOnlyOwnProperties ? RecyclableObjectWalker::GetChildrenCount() : 0);

            if (i < nonArrayElementCount)
            {LOGMEIN("DiagObjectModel.cpp] 2960\n");
                return RecyclableObjectWalker::Get(i, pResolvedObject);
            }
            else
            {
                i -= nonArrayElementCount;
                uint32 absIndex; // Absolute index
                GetResolvedObject(arrayObj, i, pResolvedObject, &absIndex);

                pResolvedObject->address = Anew(GetArenaFromContext(scriptContext),
                    RecyclableArrayAddress,
                    instance,
                    absIndex);

                return TRUE;
            }
        }
        return FALSE;
    }

    Js::JavascriptArray* RecyclableArrayWalker::GetArrayObject()
    {LOGMEIN("DiagObjectModel.cpp] 2981\n");
        Assert(Js::JavascriptArray::Is(instance) || Js::ES5Array::Is(instance));
        return  Js::ES5Array::Is(instance) ?
                    static_cast<Js::JavascriptArray *>(RecyclableObject::FromVar(instance)) :
                    Js::JavascriptArray::FromVar(instance);
    }

    uint32 RecyclableArrayWalker::GetChildrenCount()
    {LOGMEIN("DiagObjectModel.cpp] 2989\n");
        if (Js::JavascriptArray::Is(instance) || Js::ES5Array::Is(instance))
        {LOGMEIN("DiagObjectModel.cpp] 2991\n");
            uint32 count = (!fOnlyOwnProperties ? RecyclableObjectWalker::GetChildrenCount() : 0);

            Js::JavascriptArray* arrayObj = GetArrayObject();

            return GetItemCount(arrayObj) + count;
        }

        return 0;
    }

    StringBuilder<ArenaAllocator>* RecyclableArrayWalker::GetBuilder()
    {LOGMEIN("DiagObjectModel.cpp] 3003\n");
        return scriptContext->GetThreadContext()->GetDebugManager()->pCurrentInterpreterLocation->stringBuilder;
    }

    //--------------------------
    // RecyclableArgumentsArrayAddress

    RecyclableArgumentsArrayAddress::RecyclableArgumentsArrayAddress(Var _parentArray, unsigned int _index)
        : parentArray(_parentArray),
          index(_index)
    {LOGMEIN("DiagObjectModel.cpp] 3013\n");
    }

    BOOL RecyclableArgumentsArrayAddress::Set(Var updateObject)
    {LOGMEIN("DiagObjectModel.cpp] 3017\n");
        if (Js::ArgumentsObject::Is(parentArray))
        {LOGMEIN("DiagObjectModel.cpp] 3019\n");
            Js::ArgumentsObject* argObj = static_cast<Js::ArgumentsObject*>(parentArray);
            return argObj->SetItem(index, updateObject, PropertyOperation_None);
        }

        return FALSE;
    }


    //--------------------------
    // RecyclableArgumentsObjectDisplay

    RecyclableArgumentsObjectDisplay::RecyclableArgumentsObjectDisplay(ResolvedObject* resolvedObject, LocalsWalker *localsWalker)
        : RecyclableObjectDisplay(resolvedObject), pLocalsWalker(localsWalker)
    {LOGMEIN("DiagObjectModel.cpp] 3033\n");
    }

    BOOL RecyclableArgumentsObjectDisplay::HasChildren()
    {LOGMEIN("DiagObjectModel.cpp] 3037\n");
        // It must have children otherwise object itself was not created in first place.
        return TRUE;
    }

    WeakArenaReference<IDiagObjectModelWalkerBase>* RecyclableArgumentsObjectDisplay::CreateWalker()
    {LOGMEIN("DiagObjectModel.cpp] 3043\n");
        ReferencedArenaAdapter* pRefArena = scriptContext->GetThreadContext()->GetDebugManager()->GetDiagnosticArena();
        if (pRefArena)
        {LOGMEIN("DiagObjectModel.cpp] 3046\n");
            IDiagObjectModelWalkerBase* pOMWalker = Anew(pRefArena->Arena(), RecyclableArgumentsObjectWalker, scriptContext, instance, pLocalsWalker);
            return HeapNew(WeakArenaReference<IDiagObjectModelWalkerBase>,pRefArena, pOMWalker);
        }
        return nullptr;
    }

    //--------------------------
    // RecyclableArgumentsObjectWalker

    RecyclableArgumentsObjectWalker::RecyclableArgumentsObjectWalker(ScriptContext* pContext, Var _instance, LocalsWalker * localsWalker)
        : RecyclableObjectWalker(pContext, _instance), pLocalsWalker(localsWalker)
    {LOGMEIN("DiagObjectModel.cpp] 3058\n");
    }

    uint32 RecyclableArgumentsObjectWalker::GetChildrenCount()
    {LOGMEIN("DiagObjectModel.cpp] 3062\n");
        if (innerArrayObjectWalker == nullptr)
        {LOGMEIN("DiagObjectModel.cpp] 3064\n");
            uint32 count = RecyclableObjectWalker::GetChildrenCount();
            if (innerArrayObjectWalker != nullptr)
            {LOGMEIN("DiagObjectModel.cpp] 3067\n");
                RecyclableArgumentsArrayWalker *pWalker = static_cast<RecyclableArgumentsArrayWalker *> (innerArrayObjectWalker);
                pWalker->FetchFormalsAddress(pLocalsWalker);
            }
            return count;
        }

        return RecyclableObjectWalker::GetChildrenCount();
    }


    //--------------------------
    // RecyclableArgumentsArrayWalker

    RecyclableArgumentsArrayWalker::RecyclableArgumentsArrayWalker(ScriptContext* _scriptContext, Var _instance, Var _originalInstance)
        : RecyclableArrayWalker(_scriptContext, _instance, _originalInstance), pFormalsList(nullptr)
    {LOGMEIN("DiagObjectModel.cpp] 3083\n");
    }

    uint32 RecyclableArgumentsArrayWalker::GetChildrenCount()
    {LOGMEIN("DiagObjectModel.cpp] 3087\n");
        if (pMembersList == nullptr)
        {LOGMEIN("DiagObjectModel.cpp] 3089\n");
            Assert(Js::ArgumentsObject::Is(instance));
            Js::ArgumentsObject * argObj = static_cast<Js::ArgumentsObject*>(instance);

            pMembersList = JsUtil::List<DebuggerPropertyDisplayInfo *, ArenaAllocator>::New(GetArenaFromContext(scriptContext));
            Assert(pMembersList);

            uint32 totalCount = argObj->GetNumberOfArguments();
            Js::ArrayObject * objectArray = argObj->GetObjectArray();
            if (objectArray != nullptr && objectArray->GetLength() > totalCount)
            {LOGMEIN("DiagObjectModel.cpp] 3099\n");
                totalCount = objectArray->GetLength();
            }

            for (uint32 index = 0; index < totalCount; index++)
            {LOGMEIN("DiagObjectModel.cpp] 3104\n");
                Var itemObj;
                if (argObj->GetItem(argObj, index, &itemObj, scriptContext))
                {LOGMEIN("DiagObjectModel.cpp] 3107\n");
                    DebuggerPropertyDisplayInfo *info = Anew(GetArenaFromContext(scriptContext), DebuggerPropertyDisplayInfo, index, itemObj, DebuggerPropertyDisplayInfoFlags_None);
                    Assert(info);
                    pMembersList->Add(info);
                }
            }
       }

        return pMembersList ? pMembersList->Count() : 0;
    }

    void RecyclableArgumentsArrayWalker::FetchFormalsAddress(LocalsWalker * localsWalker)
    {LOGMEIN("DiagObjectModel.cpp] 3119\n");
        Assert(localsWalker);
        Assert(localsWalker->pFrame);
        Js::FunctionBody *pFBody = localsWalker->pFrame->GetJavascriptFunction()->GetFunctionBody();
        Assert(pFBody);

        PropertyIdOnRegSlotsContainer * container = pFBody->GetPropertyIdOnRegSlotsContainer();
        if (container &&  container->propertyIdsForFormalArgs)
        {LOGMEIN("DiagObjectModel.cpp] 3127\n");
            for (uint32 i = 0; i < container->propertyIdsForFormalArgs->count; i++)
            {LOGMEIN("DiagObjectModel.cpp] 3129\n");
                if (container->propertyIdsForFormalArgs->elements[i] != Js::Constants::NoRegister)
                {LOGMEIN("DiagObjectModel.cpp] 3131\n");
                    bool isConst = false;
                    IDiagObjectAddress * address = localsWalker->FindPropertyAddress(container->propertyIdsForFormalArgs->elements[i], false, isConst);
                    if (address)
                    {LOGMEIN("DiagObjectModel.cpp] 3135\n");
                        if (pFormalsList == nullptr)
                        {LOGMEIN("DiagObjectModel.cpp] 3137\n");
                            pFormalsList = JsUtil::List<IDiagObjectAddress *, ArenaAllocator>::New(GetArenaFromContext(scriptContext));
                        }

                        pFormalsList->Add(address);
                    }
                }
            }
        }
    }

    BOOL RecyclableArgumentsArrayWalker::Get(int i, ResolvedObject* pResolvedObject)
    {
        AssertMsg(pResolvedObject, "Bad usage of RecyclableArgumentsArrayWalker::Get");

        Assert(i >= 0);
        Assert(Js::ArgumentsObject::Is(instance));

        if (pMembersList && i < pMembersList->Count())
        {LOGMEIN("DiagObjectModel.cpp] 3156\n");
            Assert(pMembersList->Item(i) != nullptr);

            pResolvedObject->address = nullptr;
            if (pFormalsList && i < pFormalsList->Count())
            {LOGMEIN("DiagObjectModel.cpp] 3161\n");
                pResolvedObject->address = pFormalsList->Item(i);
                pResolvedObject->obj = pResolvedObject->address->GetValue(FALSE);
                if (pResolvedObject->obj == nullptr)
                {LOGMEIN("DiagObjectModel.cpp] 3165\n");
                    // Temp workaround till the arguments (In jit code) work is ready.
                    Assert(Js::Configuration::Global.EnableJitInDebugMode());
                    pResolvedObject->obj = pMembersList->Item(i)->aVar;
                }
                else if (pResolvedObject->obj != pMembersList->Item(i)->aVar)
                {LOGMEIN("DiagObjectModel.cpp] 3171\n");
                    // We set the formals value in the object itself, so that expression evaluation can reflect them correctly
                    Js::HeapArgumentsObject* argObj = static_cast<Js::HeapArgumentsObject*>(instance);
                    JavascriptOperators::SetItem(instance, argObj, (uint32)pMembersList->Item(i)->propId, pResolvedObject->obj, scriptContext, PropertyOperation_None);
                }
            }
            else
            {
                pResolvedObject->obj = pMembersList->Item(i)->aVar;
            }
            Assert(pResolvedObject->obj);

            pResolvedObject->scriptContext = scriptContext;
            pResolvedObject->typeId = JavascriptOperators::GetTypeId(pResolvedObject->obj);

            StringBuilder<ArenaAllocator>* builder = GetBuilder();
            Assert(builder);
            builder->Reset();
            pResolvedObject->name = GetIndexName(pMembersList->Item(i)->propId, builder);

            if (pResolvedObject->typeId != TypeIds_HostDispatch && pResolvedObject->address == nullptr)
            {LOGMEIN("DiagObjectModel.cpp] 3192\n");
                pResolvedObject->address = Anew(GetArenaFromContext(scriptContext),
                    RecyclableArgumentsArrayAddress,
                    instance,
                    pMembersList->Item(i)->propId);
            }
            return TRUE;
        }
        return FALSE;
    }



    //--------------------------
    // RecyclableTypedArrayAddress

    RecyclableTypedArrayAddress::RecyclableTypedArrayAddress(Var _parentArray, unsigned int _index)
        : RecyclableArrayAddress(_parentArray, _index)
    {LOGMEIN("DiagObjectModel.cpp] 3210\n");
    }

    BOOL RecyclableTypedArrayAddress::Set(Var updateObject)
    {LOGMEIN("DiagObjectModel.cpp] 3214\n");
        if (Js::TypedArrayBase::Is(parentArray))
        {LOGMEIN("DiagObjectModel.cpp] 3216\n");
            Js::TypedArrayBase* typedArrayObj = Js::TypedArrayBase::FromVar(parentArray);
            return typedArrayObj->SetItem(index, updateObject, PropertyOperation_None);
        }

        return FALSE;
    }


    //--------------------------
    // RecyclableTypedArrayDisplay

    RecyclableTypedArrayDisplay::RecyclableTypedArrayDisplay(ResolvedObject* resolvedObject)
        : RecyclableObjectDisplay(resolvedObject)
    {LOGMEIN("DiagObjectModel.cpp] 3230\n");
    }

    BOOL RecyclableTypedArrayDisplay::HasChildren()
    {LOGMEIN("DiagObjectModel.cpp] 3234\n");
        if (Js::TypedArrayBase::Is(instance))
        {LOGMEIN("DiagObjectModel.cpp] 3236\n");
            Js::TypedArrayBase* typedArrayObj = Js::TypedArrayBase::FromVar(instance);
            if (typedArrayObj->GetLength() > 0)
            {LOGMEIN("DiagObjectModel.cpp] 3239\n");
                return TRUE;
            }
        }
        return RecyclableObjectDisplay::HasChildren();
    }

    WeakArenaReference<IDiagObjectModelWalkerBase>* RecyclableTypedArrayDisplay::CreateWalker()
    {LOGMEIN("DiagObjectModel.cpp] 3247\n");
        return CreateAWalker<RecyclableTypedArrayWalker>(scriptContext, instance, originalInstance);
    }

    //--------------------------
    // RecyclableTypedArrayWalker

    RecyclableTypedArrayWalker::RecyclableTypedArrayWalker(ScriptContext* _scriptContext, Var _instance, Var _originalInstance)
        : RecyclableArrayWalker(_scriptContext, _instance, _originalInstance)
    {LOGMEIN("DiagObjectModel.cpp] 3256\n");
    }

    uint32 RecyclableTypedArrayWalker::GetChildrenCount()
    {LOGMEIN("DiagObjectModel.cpp] 3260\n");
        if (!indexedItemCount)
        {LOGMEIN("DiagObjectModel.cpp] 3262\n");
            Assert(Js::TypedArrayBase::Is(instance));

            Js::TypedArrayBase * typedArrayObj = Js::TypedArrayBase::FromVar(instance);

            indexedItemCount = typedArrayObj->GetLength() + (!fOnlyOwnProperties ? RecyclableObjectWalker::GetChildrenCount() : 0);
        }

        return indexedItemCount;
    }

    BOOL RecyclableTypedArrayWalker::Get(int i, ResolvedObject* pResolvedObject)
    {
        AssertMsg(pResolvedObject, "Bad usage of RecyclableTypedArrayWalker::Get");

        Assert(Js::TypedArrayBase::Is(instance));

        Js::TypedArrayBase * typedArrayObj = Js::TypedArrayBase::FromVar(instance);

        int nonArrayElementCount = (!fOnlyOwnProperties ? RecyclableObjectWalker::GetChildrenCount() : 0);

        if (i < nonArrayElementCount)
        {LOGMEIN("DiagObjectModel.cpp] 3284\n");
            return RecyclableObjectWalker::Get(i, pResolvedObject);
        }
        else
        {
            i -= nonArrayElementCount;
            pResolvedObject->scriptContext = scriptContext;
            pResolvedObject->obj = typedArrayObj->DirectGetItem(i);
            pResolvedObject->typeId = JavascriptOperators::GetTypeId(pResolvedObject->obj);

            StringBuilder<ArenaAllocator>* builder = GetBuilder();
            Assert(builder);
            builder->Reset();
            pResolvedObject->name = GetIndexName(i, builder);

            Assert(pResolvedObject->typeId != TypeIds_HostDispatch);

            pResolvedObject->address = Anew(GetArenaFromContext(scriptContext),
                RecyclableTypedArrayAddress,
                instance,
                i);
        }

        return TRUE;
    }


    //--------------------------
    // RecyclableES5ArrayAddress

    RecyclableES5ArrayAddress::RecyclableES5ArrayAddress(Var _parentArray, unsigned int _index)
        : RecyclableArrayAddress(_parentArray, _index)
    {LOGMEIN("DiagObjectModel.cpp] 3316\n");
    }

    BOOL RecyclableES5ArrayAddress::Set(Var updateObject)
    {LOGMEIN("DiagObjectModel.cpp] 3320\n");
        if (Js::ES5Array::Is(parentArray))
        {LOGMEIN("DiagObjectModel.cpp] 3322\n");
            Js::ES5Array* arrayObj = Js::ES5Array::FromVar(parentArray);
            return arrayObj->SetItem(index, updateObject, PropertyOperation_None);
        }

        return FALSE;
    }


    //--------------------------
    // RecyclableES5ArrayDisplay

    RecyclableES5ArrayDisplay::RecyclableES5ArrayDisplay(ResolvedObject* resolvedObject)
        : RecyclableArrayDisplay(resolvedObject)
    {LOGMEIN("DiagObjectModel.cpp] 3336\n");
    }

    BOOL RecyclableES5ArrayDisplay::HasChildren()
    {LOGMEIN("DiagObjectModel.cpp] 3340\n");
        if (Js::ES5Array::Is(instance))
        {LOGMEIN("DiagObjectModel.cpp] 3342\n");
            Js::JavascriptArray* arrayObj = static_cast<Js::JavascriptArray *>(RecyclableObject::FromVar(instance));
            if (HasChildrenInternal(arrayObj))
            {LOGMEIN("DiagObjectModel.cpp] 3345\n");
                return TRUE;
            }
        }
        return RecyclableObjectDisplay::HasChildren();
    }

    WeakArenaReference<IDiagObjectModelWalkerBase>* RecyclableES5ArrayDisplay::CreateWalker()
    {LOGMEIN("DiagObjectModel.cpp] 3353\n");
        return CreateAWalker<RecyclableES5ArrayWalker>(scriptContext, instance, originalInstance);
    }

    //--------------------------
    // RecyclableES5ArrayWalker

    RecyclableES5ArrayWalker::RecyclableES5ArrayWalker(ScriptContext* _scriptContext, Var _instance, Var _originalInstance)
        : RecyclableArrayWalker(_scriptContext, _instance, _originalInstance)
    {LOGMEIN("DiagObjectModel.cpp] 3362\n");
    }

    uint32 RecyclableES5ArrayWalker::GetNextDescriptor(uint32 currentDescriptor)
    {LOGMEIN("DiagObjectModel.cpp] 3366\n");
        Js::ES5Array *es5Array = static_cast<Js::ES5Array *>(RecyclableObject::FromVar(instance));
        IndexPropertyDescriptor* descriptor = nullptr;
        void * descriptorValidationToken = nullptr;
        return es5Array->GetNextDescriptor(currentDescriptor, &descriptor, &descriptorValidationToken);
    }


    BOOL RecyclableES5ArrayWalker::FetchItemAtIndex(Js::JavascriptArray* arrayObj, uint32 index, Var *value)
    {LOGMEIN("DiagObjectModel.cpp] 3375\n");
        Assert(arrayObj);
        Assert(value);

        return arrayObj->GetItem(arrayObj, index, value, scriptContext);
    }

    Var RecyclableES5ArrayWalker::FetchItemAt(Js::JavascriptArray* arrayObj, uint32 index)
    {LOGMEIN("DiagObjectModel.cpp] 3383\n");
        Assert(arrayObj);
        Var value = nullptr;
        if (FetchItemAtIndex(arrayObj, index, &value))
        {LOGMEIN("DiagObjectModel.cpp] 3387\n");
            return value;
        }
        return nullptr;
    }

    //--------------------------
    // RecyclableProtoObjectWalker

    RecyclableProtoObjectWalker::RecyclableProtoObjectWalker(ScriptContext* pContext, Var instance, Var originalInstance)
        : RecyclableObjectWalker(pContext, instance)
    {LOGMEIN("DiagObjectModel.cpp] 3398\n");
        this->originalInstance = originalInstance;
    }

    BOOL RecyclableProtoObjectWalker::GetGroupObject(ResolvedObject* pResolvedObject)
    {LOGMEIN("DiagObjectModel.cpp] 3403\n");
        Assert(pResolvedObject);

        DBGPROP_ATTRIB_FLAGS defaultAttributes = DBGPROP_ATTRIB_NO_ATTRIB;
        if (scriptContext->GetLibrary()->GetObjectPrototypeObject()->is__proto__Enabled())
        {LOGMEIN("DiagObjectModel.cpp] 3408\n");
            pResolvedObject->name           = _u("__proto__");
            pResolvedObject->propId         = PropertyIds::__proto__;
        }
        else
        {
            pResolvedObject->name           = _u("[prototype]");
            pResolvedObject->propId         = Constants::NoProperty; // This property will not be editable.
            defaultAttributes               = DBGPROP_ATTRIB_VALUE_IS_FAKE;
        }

        RecyclableObject *obj               = Js::RecyclableObject::FromVar(instance);

        Assert(obj->GetPrototype() != nullptr);
        //withscopeObjects prototype is null
        Assert(obj->GetPrototype()->GetTypeId() != TypeIds_Null || (obj->GetPrototype()->GetTypeId() == TypeIds_Null && obj->GetTypeId() == TypeIds_WithScopeObject));

        pResolvedObject->obj                = obj->GetPrototype();
        pResolvedObject->originalObj        = (originalInstance != nullptr) ? Js::RecyclableObject::FromVar(originalInstance) : pResolvedObject->obj;
        pResolvedObject->scriptContext      = scriptContext;
        pResolvedObject->typeId             = JavascriptOperators::GetTypeId(pResolvedObject->obj);

        ArenaAllocator * arena = GetArenaFromContext(scriptContext);
        pResolvedObject->objectDisplay      = pResolvedObject->CreateDisplay();
        pResolvedObject->objectDisplay->SetDefaultTypeAttribute(defaultAttributes);

        pResolvedObject->address = Anew(arena,
            RecyclableProtoObjectAddress,
            instance,
            PropertyIds::prototype,
            pResolvedObject->obj);

        return TRUE;
    }

    IDiagObjectAddress* RecyclableProtoObjectWalker::FindPropertyAddress(PropertyId propId, bool& isConst)
    {LOGMEIN("DiagObjectModel.cpp] 3444\n");
        ResolvedObject resolvedProto;
        GetGroupObject(&resolvedProto);

        struct AutoCleanup
        {
            WeakArenaReference<Js::IDiagObjectModelWalkerBase> * walkerRef;
            IDiagObjectModelWalkerBase * walker;

            AutoCleanup() : walkerRef(nullptr), walker(nullptr) {LOGMEIN("DiagObjectModel.cpp] 3453\n");};
            ~AutoCleanup()
            {LOGMEIN("DiagObjectModel.cpp] 3455\n");
                if (walker)
                {LOGMEIN("DiagObjectModel.cpp] 3457\n");
                    walkerRef->ReleaseStrongReference();
                }
                if (walkerRef)
                {LOGMEIN("DiagObjectModel.cpp] 3461\n");
                    HeapDelete(walkerRef);
                }
            }
        } autoCleanup;
        Assert(resolvedProto.objectDisplay);
        autoCleanup.walkerRef = resolvedProto.objectDisplay->CreateWalker();
        autoCleanup.walker = autoCleanup.walkerRef->GetStrongReference();
        return autoCleanup.walker ? autoCleanup.walker->FindPropertyAddress(propId, isConst) : nullptr;
    }

    //--------------------------
    // RecyclableProtoObjectAddress

    RecyclableProtoObjectAddress::RecyclableProtoObjectAddress(Var _parentObj, Js::PropertyId _propId, Js::Var _value)
        : RecyclableObjectAddress(_parentObj, _propId, _value, false /*isInDeadZone*/)
    {LOGMEIN("DiagObjectModel.cpp] 3477\n");
    }

    //--------------------------
    // RecyclableCollectionObjectWalker
    template <typename TData> const char16* RecyclableCollectionObjectWalker<TData>::Name() { static_assert(false, _u("Must use specialization")); }
    template <> const char16* RecyclableCollectionObjectWalker<JavascriptMap>::Name() {LOGMEIN("DiagObjectModel.cpp] 3483\n"); return _u("[Map]"); }
    template <> const char16* RecyclableCollectionObjectWalker<JavascriptSet>::Name() {LOGMEIN("DiagObjectModel.cpp] 3484\n"); return _u("[Set]"); }
    template <> const char16* RecyclableCollectionObjectWalker<JavascriptWeakMap>::Name() {LOGMEIN("DiagObjectModel.cpp] 3485\n"); return _u("[WeakMap]"); }
    template <> const char16* RecyclableCollectionObjectWalker<JavascriptWeakSet>::Name() {LOGMEIN("DiagObjectModel.cpp] 3486\n"); return _u("[WeakSet]"); }

    template <typename TData>
    BOOL RecyclableCollectionObjectWalker<TData>::GetGroupObject(ResolvedObject* pResolvedObject)
    {LOGMEIN("DiagObjectModel.cpp] 3490\n");
        pResolvedObject->name = Name();
        pResolvedObject->propId = Constants::NoProperty;
        pResolvedObject->obj = instance;
        pResolvedObject->scriptContext = scriptContext;
        pResolvedObject->typeId = JavascriptOperators::GetTypeId(pResolvedObject->obj);
        pResolvedObject->address = nullptr;

        typedef RecyclableCollectionObjectDisplay<TData> RecyclableDataObjectDisplay;
        pResolvedObject->objectDisplay = Anew(GetArenaFromContext(scriptContext), RecyclableDataObjectDisplay, scriptContext, pResolvedObject->name, this);

        return TRUE;
    }

    template <typename TData>
    BOOL RecyclableCollectionObjectWalker<TData>::Get(int i, ResolvedObject* pResolvedObject)
    {LOGMEIN("DiagObjectModel.cpp] 3506\n");
        auto builder = scriptContext->GetThreadContext()->GetDebugManager()->pCurrentInterpreterLocation->stringBuilder;
        builder->Reset();
        builder->AppendUint64(i);
        pResolvedObject->name = builder->Detach();
        pResolvedObject->propId = Constants::NoProperty;
        pResolvedObject->obj = instance;
        pResolvedObject->scriptContext = scriptContext;
        pResolvedObject->typeId = JavascriptOperators::GetTypeId(pResolvedObject->obj);
        pResolvedObject->address = nullptr;

        pResolvedObject->objectDisplay = CreateTDataDisplay(pResolvedObject, i);

        return TRUE;
    }

    template <typename TData>
    IDiagObjectModelDisplay* RecyclableCollectionObjectWalker<TData>::CreateTDataDisplay(ResolvedObject* resolvedObject, int i)
    {LOGMEIN("DiagObjectModel.cpp] 3524\n");
        Var key = propertyList->Item(i).key;
        Var value = propertyList->Item(i).value;
        return Anew(GetArenaFromContext(scriptContext), RecyclableKeyValueDisplay, resolvedObject->scriptContext, key, value, resolvedObject->name);
    }

    template <>
    IDiagObjectModelDisplay* RecyclableCollectionObjectWalker<JavascriptSet>::CreateTDataDisplay(ResolvedObject* resolvedObject, int i)
    {LOGMEIN("DiagObjectModel.cpp] 3532\n");
        resolvedObject->obj = propertyList->Item(i).value;
        IDiagObjectModelDisplay* display = resolvedObject->CreateDisplay();
        display->SetDefaultTypeAttribute(DBGPROP_ATTRIB_VALUE_READONLY | DBGPROP_ATTRIB_VALUE_IS_FAKE);
        return display;
    }

    template <>
    IDiagObjectModelDisplay* RecyclableCollectionObjectWalker<JavascriptWeakSet>::CreateTDataDisplay(ResolvedObject* resolvedObject, int i)
    {LOGMEIN("DiagObjectModel.cpp] 3541\n");
        resolvedObject->obj = propertyList->Item(i).value;
        IDiagObjectModelDisplay* display = resolvedObject->CreateDisplay();
        display->SetDefaultTypeAttribute(DBGPROP_ATTRIB_VALUE_READONLY | DBGPROP_ATTRIB_VALUE_IS_FAKE);
        return display;
    }

    template <typename TData>
    uint32 RecyclableCollectionObjectWalker<TData>::GetChildrenCount()
    {LOGMEIN("DiagObjectModel.cpp] 3550\n");
        TData* data = TData::FromVar(instance);
        if (data->Size() > 0 && propertyList == nullptr)
        {LOGMEIN("DiagObjectModel.cpp] 3553\n");
            propertyList = JsUtil::List<RecyclableCollectionObjectWalkerPropertyData<TData>, ArenaAllocator>::New(GetArenaFromContext(scriptContext));
            GetChildren();
        }

        return data->Size();
    }

    template <>
    void RecyclableCollectionObjectWalker<JavascriptMap>::GetChildren()
    {LOGMEIN("DiagObjectModel.cpp] 3563\n");
        JavascriptMap* data = JavascriptMap::FromVar(instance);
        auto iterator = data->GetIterator();
        while (iterator.Next())
        {LOGMEIN("DiagObjectModel.cpp] 3567\n");
            Var key = iterator.Current().Key();
            Var value = iterator.Current().Value();
            propertyList->Add(RecyclableCollectionObjectWalkerPropertyData<JavascriptMap>(key, value));
        }
    }

    template <>
    void RecyclableCollectionObjectWalker<JavascriptSet>::GetChildren()
    {LOGMEIN("DiagObjectModel.cpp] 3576\n");
        JavascriptSet* data = JavascriptSet::FromVar(instance);
        auto iterator = data->GetIterator();
        while (iterator.Next())
        {LOGMEIN("DiagObjectModel.cpp] 3580\n");
            Var value = iterator.Current();
            propertyList->Add(RecyclableCollectionObjectWalkerPropertyData<JavascriptSet>(value));
        }
    }

    template <>
    void RecyclableCollectionObjectWalker<JavascriptWeakMap>::GetChildren()
    {LOGMEIN("DiagObjectModel.cpp] 3588\n");
        JavascriptWeakMap* data = JavascriptWeakMap::FromVar(instance);
        data->Map([&](Var key, Var value)
        {
            propertyList->Add(RecyclableCollectionObjectWalkerPropertyData<JavascriptWeakMap>(key, value));
        });
    }

    template <>
    void RecyclableCollectionObjectWalker<JavascriptWeakSet>::GetChildren()
    {LOGMEIN("DiagObjectModel.cpp] 3598\n");
        JavascriptWeakSet* data = JavascriptWeakSet::FromVar(instance);
        data->Map([&](Var value)
        {
            propertyList->Add(RecyclableCollectionObjectWalkerPropertyData<JavascriptWeakSet>(value));
        });
    }

    //--------------------------
    // RecyclableCollectionObjectDisplay
    template <typename TData>
    LPCWSTR RecyclableCollectionObjectDisplay<TData>::Value(int radix)
    {LOGMEIN("DiagObjectModel.cpp] 3610\n");
        StringBuilder<ArenaAllocator>* builder = scriptContext->GetThreadContext()->GetDebugManager()->pCurrentInterpreterLocation->stringBuilder;
        builder->Reset();

        builder->AppendCppLiteral(_u("size = "));
        builder->AppendUint64(walker->GetChildrenCount());

        return builder->Detach();
    }

    template <typename TData>
    WeakArenaReference<IDiagObjectModelWalkerBase>* RecyclableCollectionObjectDisplay<TData>::CreateWalker()
    {LOGMEIN("DiagObjectModel.cpp] 3622\n");
        if (walker)
        {LOGMEIN("DiagObjectModel.cpp] 3624\n");
            ReferencedArenaAdapter* pRefArena = scriptContext->GetThreadContext()->GetDebugManager()->GetDiagnosticArena();
            if (pRefArena)
            {LOGMEIN("DiagObjectModel.cpp] 3627\n");
                return HeapNew(WeakArenaReference<IDiagObjectModelWalkerBase>, pRefArena, walker);
            }
        }
        return nullptr;
    }

    //--------------------------
    // RecyclableKeyValueDisplay
    WeakArenaReference<IDiagObjectModelWalkerBase>* RecyclableKeyValueDisplay::CreateWalker()
    {LOGMEIN("DiagObjectModel.cpp] 3637\n");
        ReferencedArenaAdapter* pRefArena = scriptContext->GetThreadContext()->GetDebugManager()->GetDiagnosticArena();
        if (pRefArena)
        {LOGMEIN("DiagObjectModel.cpp] 3640\n");
            IDiagObjectModelWalkerBase* pOMWalker = Anew(pRefArena->Arena(), RecyclableKeyValueWalker, scriptContext, key, value);
            return HeapNew(WeakArenaReference<IDiagObjectModelWalkerBase>, pRefArena, pOMWalker);
        }
        return nullptr;
    }

    LPCWSTR RecyclableKeyValueDisplay::Value(int radix)
    {LOGMEIN("DiagObjectModel.cpp] 3648\n");
        ResolvedObject ro;
        ro.scriptContext = scriptContext;

        ro.obj = key;
        RecyclableObjectDisplay keyDisplay(&ro);

        ro.obj = value;
        RecyclableObjectDisplay valueDisplay(&ro);

        // Note, RecyclableObjectDisplay::Value(int) uses the shared string builder
        // so we cannot call it while building our string below.  Call both before hand.
        const char16* keyValue = keyDisplay.Value(radix);
        const char16* valueValue = valueDisplay.Value(radix);

        StringBuilder<ArenaAllocator>* builder = scriptContext->GetThreadContext()->GetDebugManager()->pCurrentInterpreterLocation->stringBuilder;
        builder->Reset();

        builder->Append('[');
        builder->AppendSz(keyValue);
        builder->AppendCppLiteral(_u(", "));
        builder->AppendSz(valueValue);
        builder->Append(']');

        return builder->Detach();
    }

    //--------------------------
    // RecyclableKeyValueWalker
    BOOL RecyclableKeyValueWalker::Get(int i, ResolvedObject* pResolvedObject)
    {LOGMEIN("DiagObjectModel.cpp] 3678\n");
        if (i == 0)
        {LOGMEIN("DiagObjectModel.cpp] 3680\n");
            pResolvedObject->name = _u("key");
            pResolvedObject->obj = key;
        }
        else if (i == 1)
        {LOGMEIN("DiagObjectModel.cpp] 3685\n");
            pResolvedObject->name = _u("value");
            pResolvedObject->obj = value;
        }
        else
        {
            Assert(false);
            return FALSE;
        }

        pResolvedObject->propId = Constants::NoProperty;
        pResolvedObject->scriptContext = scriptContext;
        pResolvedObject->typeId = JavascriptOperators::GetTypeId(pResolvedObject->obj);
        pResolvedObject->objectDisplay = pResolvedObject->CreateDisplay();
        pResolvedObject->objectDisplay->SetDefaultTypeAttribute(DBGPROP_ATTRIB_VALUE_READONLY | DBGPROP_ATTRIB_VALUE_IS_FAKE);
        pResolvedObject->address = nullptr;

        return TRUE;
    }

    //--------------------------
    // RecyclableProxyObjectDisplay

    RecyclableProxyObjectDisplay::RecyclableProxyObjectDisplay(ResolvedObject* resolvedObject)
        : RecyclableObjectDisplay(resolvedObject)
    {LOGMEIN("DiagObjectModel.cpp] 3710\n");
    }

    WeakArenaReference<IDiagObjectModelWalkerBase>* RecyclableProxyObjectDisplay::CreateWalker()
    {LOGMEIN("DiagObjectModel.cpp] 3714\n");
        ReferencedArenaAdapter* pRefArena = scriptContext->GetThreadContext()->GetDebugManager()->GetDiagnosticArena();
        if (pRefArena)
        {LOGMEIN("DiagObjectModel.cpp] 3717\n");
            IDiagObjectModelWalkerBase* pOMWalker = Anew(pRefArena->Arena(), RecyclableProxyObjectWalker, scriptContext, instance);
            return HeapNew(WeakArenaReference<IDiagObjectModelWalkerBase>, pRefArena, pOMWalker);
        }
        return nullptr;
    }

    //--------------------------
    // RecyclableProxyObjectWalker

    RecyclableProxyObjectWalker::RecyclableProxyObjectWalker(ScriptContext* pContext, Var _instance)
        : RecyclableObjectWalker(pContext, _instance)
    {LOGMEIN("DiagObjectModel.cpp] 3729\n");
    }

    BOOL RecyclableProxyObjectWalker::GetGroupObject(ResolvedObject* pResolvedObject)
    {LOGMEIN("DiagObjectModel.cpp] 3733\n");
        pResolvedObject->name = _u("[Proxy]");
        pResolvedObject->propId = Constants::NoProperty;
        pResolvedObject->obj = instance;
        pResolvedObject->scriptContext = scriptContext;
        pResolvedObject->typeId = JavascriptOperators::GetTypeId(pResolvedObject->obj);
        pResolvedObject->address = nullptr;

        pResolvedObject->objectDisplay = Anew(GetArenaFromContext(scriptContext), RecyclableProxyObjectDisplay, pResolvedObject);
        pResolvedObject->objectDisplay->SetDefaultTypeAttribute(DBGPROP_ATTRIB_VALUE_READONLY | DBGPROP_ATTRIB_VALUE_IS_FAKE);
        return TRUE;
    }

    BOOL RecyclableProxyObjectWalker::Get(int i, ResolvedObject* pResolvedObject)
    {LOGMEIN("DiagObjectModel.cpp] 3747\n");
        JavascriptProxy* proxy = JavascriptProxy::FromVar(instance);
        if (proxy->GetTarget() == nullptr || proxy->GetHandler() == nullptr)
        {LOGMEIN("DiagObjectModel.cpp] 3750\n");
            return FALSE;
        }
        if (i == 0)
        {LOGMEIN("DiagObjectModel.cpp] 3754\n");
            pResolvedObject->name = _u("[target]");
            pResolvedObject->obj = proxy->GetTarget();
        }
        else if (i == 1)
        {LOGMEIN("DiagObjectModel.cpp] 3759\n");
            pResolvedObject->name = _u("[handler]");
            pResolvedObject->obj = proxy->GetHandler();
        }
        else
        {
            Assert(false);
            return FALSE;
        }

        pResolvedObject->propId = Constants::NoProperty;
        pResolvedObject->scriptContext = scriptContext;
        pResolvedObject->typeId = JavascriptOperators::GetTypeId(pResolvedObject->obj);
        pResolvedObject->objectDisplay = pResolvedObject->CreateDisplay();
        pResolvedObject->objectDisplay->SetDefaultTypeAttribute(DBGPROP_ATTRIB_VALUE_READONLY | DBGPROP_ATTRIB_VALUE_IS_FAKE);
        pResolvedObject->address = Anew(GetArenaFromContext(pResolvedObject->scriptContext),
            RecyclableObjectAddress,
            pResolvedObject->scriptContext->GetGlobalObject(),
            Js::PropertyIds::Proxy,
            pResolvedObject->obj,
            false /*isInDeadZone*/);

        return TRUE;
    }

    //--------------------------
    // RecyclablePromiseObjectDisplay

    RecyclablePromiseObjectDisplay::RecyclablePromiseObjectDisplay(ResolvedObject* resolvedObject)
        : RecyclableObjectDisplay(resolvedObject)
    {LOGMEIN("DiagObjectModel.cpp] 3789\n");
    }

    WeakArenaReference<IDiagObjectModelWalkerBase>* RecyclablePromiseObjectDisplay::CreateWalker()
    {LOGMEIN("DiagObjectModel.cpp] 3793\n");
        ReferencedArenaAdapter* pRefArena = scriptContext->GetThreadContext()->GetDebugManager()->GetDiagnosticArena();
        if (pRefArena)
        {LOGMEIN("DiagObjectModel.cpp] 3796\n");
            IDiagObjectModelWalkerBase* pOMWalker = Anew(pRefArena->Arena(), RecyclablePromiseObjectWalker, scriptContext, instance);
            return HeapNew(WeakArenaReference<IDiagObjectModelWalkerBase>, pRefArena, pOMWalker);
        }
        return nullptr;
    }

    //--------------------------
    // RecyclablePromiseObjectWalker

    RecyclablePromiseObjectWalker::RecyclablePromiseObjectWalker(ScriptContext* pContext, Var _instance)
        : RecyclableObjectWalker(pContext, _instance)
    {LOGMEIN("DiagObjectModel.cpp] 3808\n");
    }

    BOOL RecyclablePromiseObjectWalker::GetGroupObject(ResolvedObject* pResolvedObject)
    {LOGMEIN("DiagObjectModel.cpp] 3812\n");
        pResolvedObject->name = _u("[Promise]");
        pResolvedObject->propId = Constants::NoProperty;
        pResolvedObject->obj = instance;
        pResolvedObject->scriptContext = scriptContext;
        pResolvedObject->typeId = JavascriptOperators::GetTypeId(pResolvedObject->obj);
        pResolvedObject->address = nullptr;

        pResolvedObject->objectDisplay = Anew(GetArenaFromContext(scriptContext), RecyclablePromiseObjectDisplay, pResolvedObject);
        pResolvedObject->objectDisplay->SetDefaultTypeAttribute(DBGPROP_ATTRIB_VALUE_READONLY | DBGPROP_ATTRIB_VALUE_IS_FAKE);
        return TRUE;
    }

    BOOL RecyclablePromiseObjectWalker::Get(int i, ResolvedObject* pResolvedObject)
    {LOGMEIN("DiagObjectModel.cpp] 3826\n");
        JavascriptPromise* promise = JavascriptPromise::FromVar(instance);

        if (i == 0)
        {LOGMEIN("DiagObjectModel.cpp] 3830\n");
            pResolvedObject->name = _u("[status]");

            switch (promise->GetStatus())
            {LOGMEIN("DiagObjectModel.cpp] 3834\n");
            case JavascriptPromise::PromiseStatusCode_Undefined:
                pResolvedObject->obj = scriptContext->GetLibrary()->GetUndefinedDisplayString();
                break;
            case JavascriptPromise::PromiseStatusCode_Unresolved:
                pResolvedObject->obj = scriptContext->GetLibrary()->CreateStringFromCppLiteral(_u("pending"));
            case JavascriptPromise::PromiseStatusCode_HasResolution:
                pResolvedObject->obj = scriptContext->GetLibrary()->CreateStringFromCppLiteral(_u("resolved"));
                break;
            case JavascriptPromise::PromiseStatusCode_HasRejection:
                pResolvedObject->obj = scriptContext->GetLibrary()->CreateStringFromCppLiteral(_u("rejected"));
                break;
            default:
                AssertMsg(false, "New PromiseStatusCode not handled in debugger");
                pResolvedObject->obj = scriptContext->GetLibrary()->GetUndefinedDisplayString();
                break;
            }
        }
        else if (i == 1)
        {LOGMEIN("DiagObjectModel.cpp] 3853\n");
            pResolvedObject->name = _u("[value]");
            Var result = promise->GetResult();
            pResolvedObject->obj = result != nullptr ? result : scriptContext->GetLibrary()->GetUndefinedDisplayString();
        }
        else
        {
            Assert(false);
            return FALSE;
        }

        pResolvedObject->propId = Constants::NoProperty;
        pResolvedObject->scriptContext = scriptContext;
        pResolvedObject->typeId = JavascriptOperators::GetTypeId(pResolvedObject->obj);
        pResolvedObject->objectDisplay = pResolvedObject->CreateDisplay();
        pResolvedObject->objectDisplay->SetDefaultTypeAttribute(DBGPROP_ATTRIB_VALUE_READONLY | DBGPROP_ATTRIB_VALUE_IS_FAKE);
        pResolvedObject->address = nullptr;

        return TRUE;
    }

    // ---------------------------
    // RecyclableMethodsGroupWalker
    RecyclableMethodsGroupWalker::RecyclableMethodsGroupWalker(ScriptContext* scriptContext, Var instance)
        : RecyclableObjectWalker(scriptContext,instance)
    {LOGMEIN("DiagObjectModel.cpp] 3878\n");
    }

    void RecyclableMethodsGroupWalker::AddItem(Js::PropertyId propertyId, Var obj)
    {LOGMEIN("DiagObjectModel.cpp] 3882\n");
        if (pMembersList == nullptr)
        {LOGMEIN("DiagObjectModel.cpp] 3884\n");
            pMembersList = JsUtil::List<DebuggerPropertyDisplayInfo *, ArenaAllocator>::New(GetArenaFromContext(scriptContext));
        }

        Assert(pMembersList);

        DebuggerPropertyDisplayInfo *info = Anew(GetArenaFromContext(scriptContext), DebuggerPropertyDisplayInfo, propertyId, obj, DebuggerPropertyDisplayInfoFlags_Const);
        Assert(info);
        pMembersList->Add(info);
    }

    uint32 RecyclableMethodsGroupWalker::GetChildrenCount()
    {LOGMEIN("DiagObjectModel.cpp] 3896\n");
        return pMembersList ? pMembersList->Count() : 0;
    }

    BOOL RecyclableMethodsGroupWalker::Get(int i, ResolvedObject* pResolvedObject)
    {
        AssertMsg(pResolvedObject, "Bad usage of RecyclableMethodsGroupWalker::Get");

        return RecyclableObjectWalker::Get(i, pResolvedObject);
    }

    BOOL RecyclableMethodsGroupWalker::GetGroupObject(ResolvedObject* pResolvedObject)
    {LOGMEIN("DiagObjectModel.cpp] 3908\n");
        Assert(pResolvedObject);

        // This is fake [Methods] object.
        pResolvedObject->name           = _u("[Methods]");
        pResolvedObject->obj            = Js::RecyclableObject::FromVar(instance);
        pResolvedObject->scriptContext  = scriptContext;
        pResolvedObject->typeId         = JavascriptOperators::GetTypeId(pResolvedObject->obj);
        pResolvedObject->address        = nullptr; // Methods object will not be editable

        pResolvedObject->objectDisplay  = Anew(GetArenaFromContext(scriptContext), RecyclableMethodsGroupDisplay, this, pResolvedObject);

        return TRUE;
    }

    void RecyclableMethodsGroupWalker::Sort()
    {LOGMEIN("DiagObjectModel.cpp] 3924\n");
        HostDebugContext* hostDebugContext = this->scriptContext->GetDebugContext()->GetHostDebugContext();
        if (hostDebugContext != nullptr)
        {LOGMEIN("DiagObjectModel.cpp] 3927\n");
            hostDebugContext->SortMembersList(pMembersList, scriptContext);
        }
    }

    RecyclableMethodsGroupDisplay::RecyclableMethodsGroupDisplay(RecyclableMethodsGroupWalker *_methodGroupWalker, ResolvedObject* resolvedObject)
        : methodGroupWalker(_methodGroupWalker),
          RecyclableObjectDisplay(resolvedObject)
    {LOGMEIN("DiagObjectModel.cpp] 3935\n");
    }

    LPCWSTR RecyclableMethodsGroupDisplay::Type()
    {LOGMEIN("DiagObjectModel.cpp] 3939\n");
        return _u("");
    }

    LPCWSTR RecyclableMethodsGroupDisplay::Value(int radix)
    {LOGMEIN("DiagObjectModel.cpp] 3944\n");
        return _u("{...}");
    }

    BOOL RecyclableMethodsGroupDisplay::HasChildren()
    {LOGMEIN("DiagObjectModel.cpp] 3949\n");
        return methodGroupWalker ? TRUE : FALSE;
    }

    DBGPROP_ATTRIB_FLAGS RecyclableMethodsGroupDisplay::GetTypeAttribute()
    {LOGMEIN("DiagObjectModel.cpp] 3954\n");
        return DBGPROP_ATTRIB_VALUE_READONLY | DBGPROP_ATTRIB_VALUE_IS_FAKE | DBGPROP_ATTRIB_VALUE_IS_METHOD | DBGPROP_ATTRIB_VALUE_IS_EXPANDABLE;
    }

    WeakArenaReference<IDiagObjectModelWalkerBase>* RecyclableMethodsGroupDisplay::CreateWalker()
    {LOGMEIN("DiagObjectModel.cpp] 3959\n");
        if (methodGroupWalker)
        {LOGMEIN("DiagObjectModel.cpp] 3961\n");
            ReferencedArenaAdapter* pRefArena = scriptContext->GetThreadContext()->GetDebugManager()->GetDiagnosticArena();
            if (pRefArena)
            {LOGMEIN("DiagObjectModel.cpp] 3964\n");
                return HeapNew(WeakArenaReference<IDiagObjectModelWalkerBase>, pRefArena, methodGroupWalker);
            }
        }
        return nullptr;
    }


    ScopeVariablesGroupDisplay::ScopeVariablesGroupDisplay(VariableWalkerBase *walker, ResolvedObject* resolvedObject)
        : scopeGroupWalker(walker),
          RecyclableObjectDisplay(resolvedObject)
    {LOGMEIN("DiagObjectModel.cpp] 3975\n");
    }

    LPCWSTR ScopeVariablesGroupDisplay::Type()
    {LOGMEIN("DiagObjectModel.cpp] 3979\n");
        return _u("");
    }

    LPCWSTR ScopeVariablesGroupDisplay::Value(int radix)
    {LOGMEIN("DiagObjectModel.cpp] 3984\n");
        if (ActivationObject::Is(instance))
        {LOGMEIN("DiagObjectModel.cpp] 3986\n");
            // The scope is defined by the activation object.
            Js::RecyclableObject *object = Js::RecyclableObject::FromVar(instance);
            try
            {LOGMEIN("DiagObjectModel.cpp] 3990\n");
                // Trying to find out the JavascriptFunction from the scope.
                Var value = nullptr;
                if (object->GetTypeId() == TypeIds_ActivationObject && GetPropertyWithScriptEnter(object, object, PropertyIds::arguments, &value, scriptContext))
                {LOGMEIN("DiagObjectModel.cpp] 3994\n");
                    if (Js::RecyclableObject::Is(value))
                    {LOGMEIN("DiagObjectModel.cpp] 3996\n");
                        Js::RecyclableObject *argObject = Js::RecyclableObject::FromVar(value);
                        Var calleeFunc = nullptr;
                        if (GetPropertyWithScriptEnter(argObject, argObject, PropertyIds::callee, &calleeFunc, scriptContext) && Js::JavascriptFunction::Is(calleeFunc))
                        {LOGMEIN("DiagObjectModel.cpp] 4000\n");
                            Js::JavascriptFunction *calleeFunction = Js::JavascriptFunction::FromVar(calleeFunc);
                            Js::FunctionBody *pFuncBody = calleeFunction->GetFunctionBody();

                            if (pFuncBody)
                            {LOGMEIN("DiagObjectModel.cpp] 4005\n");
                                const char16* pDisplayName = pFuncBody->GetDisplayName();
                                if (pDisplayName)
                                {LOGMEIN("DiagObjectModel.cpp] 4008\n");
                                    StringBuilder<ArenaAllocator>* builder = GetStringBuilder();
                                    builder->Reset();
                                    builder->AppendSz(pDisplayName);
                                    return builder->Detach();
                                }
                            }
                        }
                    }
                }
            }
            catch(const JavascriptException& err)
            {LOGMEIN("DiagObjectModel.cpp] 4020\n");
                err.GetAndClear();  // discard exception object
            }

            return _u("");
        }
        else
        {
            // The scope is defined by a slot array object so grab the function body out to get the function name.
            ScopeSlots slotArray = ScopeSlots(reinterpret_cast<Var*>(instance));

            if(slotArray.IsFunctionScopeSlotArray())
            {LOGMEIN("DiagObjectModel.cpp] 4032\n");
                Js::FunctionBody *functionBody = slotArray.GetFunctionInfo()->GetFunctionBody();
                return functionBody->GetDisplayName();
            }
            else
            {
                // handling for block/catch scope
                return _u("");
            }
        }
    }

    BOOL ScopeVariablesGroupDisplay::HasChildren()
    {LOGMEIN("DiagObjectModel.cpp] 4045\n");
        return scopeGroupWalker ? TRUE : FALSE;
    }

    DBGPROP_ATTRIB_FLAGS ScopeVariablesGroupDisplay::GetTypeAttribute()
    {LOGMEIN("DiagObjectModel.cpp] 4050\n");
        return DBGPROP_ATTRIB_VALUE_READONLY | DBGPROP_ATTRIB_VALUE_IS_FAKE | DBGPROP_ATTRIB_VALUE_IS_EXPANDABLE;
    }

    WeakArenaReference<IDiagObjectModelWalkerBase>* ScopeVariablesGroupDisplay::CreateWalker()
    {LOGMEIN("DiagObjectModel.cpp] 4055\n");
        if (scopeGroupWalker)
        {LOGMEIN("DiagObjectModel.cpp] 4057\n");
            ReferencedArenaAdapter* pRefArena = scriptContext->GetThreadContext()->GetDebugManager()->GetDiagnosticArena();
            if (pRefArena)
            {LOGMEIN("DiagObjectModel.cpp] 4060\n");
                return HeapNew(WeakArenaReference<IDiagObjectModelWalkerBase>, pRefArena, scopeGroupWalker);
            }
        }
        return nullptr;
    }

    GlobalsScopeVariablesGroupDisplay::GlobalsScopeVariablesGroupDisplay(VariableWalkerBase *walker, ResolvedObject* resolvedObject)
        : globalsGroupWalker(walker),
          RecyclableObjectDisplay(resolvedObject)
    {LOGMEIN("DiagObjectModel.cpp] 4070\n");
    }

    LPCWSTR GlobalsScopeVariablesGroupDisplay::Type()
    {LOGMEIN("DiagObjectModel.cpp] 4074\n");
        return _u("");
    }

    LPCWSTR GlobalsScopeVariablesGroupDisplay::Value(int radix)
    {LOGMEIN("DiagObjectModel.cpp] 4079\n");
        return _u("");
    }

    BOOL GlobalsScopeVariablesGroupDisplay::HasChildren()
    {LOGMEIN("DiagObjectModel.cpp] 4084\n");
        return globalsGroupWalker ? globalsGroupWalker->GetChildrenCount() > 0 : FALSE;
    }

    DBGPROP_ATTRIB_FLAGS GlobalsScopeVariablesGroupDisplay::GetTypeAttribute()
    {LOGMEIN("DiagObjectModel.cpp] 4089\n");
        return DBGPROP_ATTRIB_VALUE_READONLY | DBGPROP_ATTRIB_VALUE_IS_FAKE | (HasChildren() ? DBGPROP_ATTRIB_VALUE_IS_EXPANDABLE : 0);
    }

    WeakArenaReference<IDiagObjectModelWalkerBase>* GlobalsScopeVariablesGroupDisplay::CreateWalker()
    {LOGMEIN("DiagObjectModel.cpp] 4094\n");
        if (globalsGroupWalker)
        {LOGMEIN("DiagObjectModel.cpp] 4096\n");
            ReferencedArenaAdapter* pRefArena = scriptContext->GetThreadContext()->GetDebugManager()->GetDiagnosticArena();
            if (pRefArena)
            {LOGMEIN("DiagObjectModel.cpp] 4099\n");
                return HeapNew(WeakArenaReference<IDiagObjectModelWalkerBase>, pRefArena, globalsGroupWalker);
            }
        }
        return nullptr;
    }
#ifdef ENABLE_MUTATION_BREAKPOINT
    PendingMutationBreakpointDisplay::PendingMutationBreakpointDisplay(ResolvedObject* resolvedObject, MutationType _mutationType)
        : RecyclableObjectDisplay(resolvedObject), mutationType(_mutationType)
    {LOGMEIN("DiagObjectModel.cpp] 4108\n");
        AssertMsg(_mutationType > MutationTypeNone && _mutationType < MutationTypeAll, "Invalid mutationType value passed to PendingMutationBreakpointDisplay");
    }

    WeakArenaReference<IDiagObjectModelWalkerBase>* PendingMutationBreakpointDisplay::CreateWalker()
    {LOGMEIN("DiagObjectModel.cpp] 4113\n");
        ReferencedArenaAdapter* pRefArena = scriptContext->GetThreadContext()->GetDebugManager()->GetDiagnosticArena();
        if (pRefArena)
        {LOGMEIN("DiagObjectModel.cpp] 4116\n");
            IDiagObjectModelWalkerBase* pOMWalker = Anew(pRefArena->Arena(), PendingMutationBreakpointWalker, scriptContext, instance, this->mutationType);
            return HeapNew(WeakArenaReference<IDiagObjectModelWalkerBase>, pRefArena, pOMWalker);
        }
        return nullptr;
    }

    uint32 PendingMutationBreakpointWalker::GetChildrenCount()
    {LOGMEIN("DiagObjectModel.cpp] 4124\n");
        switch (this->mutationType)
        {LOGMEIN("DiagObjectModel.cpp] 4126\n");
        case MutationTypeUpdate:
            return 3;
        case MutationTypeDelete:
        case MutationTypeAdd:
            return 2;
        default:
            AssertMsg(false, "Invalid mutationType");
            return 0;
        }
    }

    PendingMutationBreakpointWalker::PendingMutationBreakpointWalker(ScriptContext* pContext, Var _instance, MutationType mutationType)
        : RecyclableObjectWalker(pContext, _instance)
    {LOGMEIN("DiagObjectModel.cpp] 4140\n");
        this->mutationType = mutationType;
    }

    BOOL PendingMutationBreakpointWalker::Get(int i, ResolvedObject* pResolvedObject)
    {LOGMEIN("DiagObjectModel.cpp] 4145\n");
        Js::MutationBreakpoint *mutationBreakpoint = scriptContext->GetDebugContext()->GetProbeContainer()->GetDebugManager()->GetActiveMutationBreakpoint();
        Assert(mutationBreakpoint);
        if (mutationBreakpoint != nullptr)
        {LOGMEIN("DiagObjectModel.cpp] 4149\n");
            if (i == 0)
            {LOGMEIN("DiagObjectModel.cpp] 4151\n");
                // <Property Name> [Adding] : New Value
                // <Property Name> [Changing] : Old Value
                // <Property Name> [Deleting] : Old Value
                WCHAR * displayName = AnewArray(GetArenaFromContext(scriptContext), WCHAR, PENDING_MUTATION_VALUE_MAX_NAME);
                swprintf_s(displayName, PENDING_MUTATION_VALUE_MAX_NAME, _u("%s [%s]"), mutationBreakpoint->GetBreakPropertyName(), Js::MutationBreakpoint::GetBreakMutationTypeName(mutationType));
                pResolvedObject->name = displayName;
                if (mutationType == MutationTypeUpdate || mutationType == MutationTypeDelete)
                {LOGMEIN("DiagObjectModel.cpp] 4159\n");
                    // Old/Current value
                    PropertyId breakPId = mutationBreakpoint->GetBreakPropertyId();
                    pResolvedObject->propId = breakPId;
                    pResolvedObject->obj = JavascriptOperators::OP_GetProperty(mutationBreakpoint->GetMutationObjectVar(), breakPId, scriptContext);
                }
                else
                {
                    // New Value
                    pResolvedObject->obj = mutationBreakpoint->GetBreakNewValueVar();
                    pResolvedObject->propId = Constants::NoProperty;
                }
            }
            else if ((i == 1) && (mutationType == MutationTypeUpdate))
            {LOGMEIN("DiagObjectModel.cpp] 4173\n");
                pResolvedObject->name = _u("[New Value]");
                pResolvedObject->obj = mutationBreakpoint->GetBreakNewValueVar();
                pResolvedObject->propId = Constants::NoProperty;
            }
            else if (((i == 1) && (mutationType != MutationTypeUpdate)) || (i == 2))
            {LOGMEIN("DiagObjectModel.cpp] 4179\n");
                WCHAR * displayName = AnewArray(GetArenaFromContext(scriptContext), WCHAR, PENDING_MUTATION_VALUE_MAX_NAME);
                swprintf_s(displayName, PENDING_MUTATION_VALUE_MAX_NAME, _u("[Property container %s]"), mutationBreakpoint->GetParentPropertyName());
                pResolvedObject->name = displayName;
                pResolvedObject->obj = mutationBreakpoint->GetMutationObjectVar();
                pResolvedObject->propId = mutationBreakpoint->GetParentPropertyId();
            }
            else
            {
                Assert(false);
                return FALSE;
            }

            pResolvedObject->scriptContext = scriptContext;
            pResolvedObject->typeId = JavascriptOperators::GetTypeId(pResolvedObject->obj);
            pResolvedObject->objectDisplay = pResolvedObject->CreateDisplay();
            pResolvedObject->objectDisplay->SetDefaultTypeAttribute(DBGPROP_ATTRIB_VALUE_READONLY | DBGPROP_ATTRIB_VALUE_IS_FAKE);
            pResolvedObject->address = nullptr; // TODO: (SaAgarwa) Currently Pending mutation values are not editable, will do as part of another WI

            return TRUE;
        }
        return FALSE;
    }
#endif

    //--------------------------
    // RecyclableSimdObjectWalker

    template <typename simdType, uint elementCount>
    BOOL RecyclableSimdObjectWalker<simdType, elementCount>::Get(int i, ResolvedObject* pResolvedObject)
    {LOGMEIN("DiagObjectModel.cpp] 4209\n");
        Assert(elementCount == 4 || elementCount == 8 || elementCount == 16); // SIMD types such as int32x4, int8x16, int16x8
        Assert(i >= 0 && i <= elementCount);

        simdType* simd = simdType::FromVar(instance);
        SIMDValue value = simd->GetValue();

        WCHAR* indexName = AnewArray(GetArenaFromContext(scriptContext), WCHAR, SIMD_INDEX_VALUE_MAX);
        Assert(indexName);
        swprintf_s(indexName, SIMD_INDEX_VALUE_MAX, _u("[%d]"), i);
        pResolvedObject->name = indexName;

        TypeId simdTypeId = JavascriptOperators::GetTypeId(instance);

        switch (simdTypeId)
        {LOGMEIN("DiagObjectModel.cpp] 4224\n");
        case TypeIds_SIMDInt32x4:
            pResolvedObject->obj = JavascriptNumber::ToVarWithCheck(value.i32[i], scriptContext);
            break;
        case TypeIds_SIMDFloat32x4:
            pResolvedObject->obj = JavascriptNumber::ToVarWithCheck(value.f32[i], scriptContext);
            break;
        case TypeIds_SIMDInt8x16:
            pResolvedObject->obj = JavascriptNumber::ToVarWithCheck(value.i8[i], scriptContext);
            break;
        case TypeIds_SIMDInt16x8:
            pResolvedObject->obj = JavascriptNumber::ToVarWithCheck(value.i16[i], scriptContext);
            break;
        case TypeIds_SIMDBool32x4:
            pResolvedObject->obj = JavascriptBoolean::ToVar(value.i32[i], scriptContext);
            break;
        case TypeIds_SIMDBool8x16:
            pResolvedObject->obj = JavascriptBoolean::ToVar(value.i8[i], scriptContext);
            break;
        case TypeIds_SIMDBool16x8:
            pResolvedObject->obj = JavascriptBoolean::ToVar(value.i16[i], scriptContext);
            break;
        case TypeIds_SIMDUint32x4:
            pResolvedObject->obj = JavascriptNumber::ToVarWithCheck(value.u32[i], scriptContext);
            break;
        case TypeIds_SIMDUint8x16:
            pResolvedObject->obj = JavascriptNumber::ToVarWithCheck(value.u8[i], scriptContext);
            break;
        case TypeIds_SIMDUint16x8:
            pResolvedObject->obj = JavascriptNumber::ToVarWithCheck(value.u16[i], scriptContext);
            break;
        default:
            AssertMsg(false, "Unexpected SIMD typeId");
            return FALSE;
        }

        pResolvedObject->propId = Constants::NoProperty;
        pResolvedObject->scriptContext = scriptContext;
        pResolvedObject->typeId = simdTypeId;
        pResolvedObject->objectDisplay = pResolvedObject->CreateDisplay();
        pResolvedObject->objectDisplay->SetDefaultTypeAttribute(DBGPROP_ATTRIB_VALUE_READONLY | DBGPROP_ATTRIB_VALUE_IS_FAKE);
        pResolvedObject->address = nullptr;

        return TRUE;
    }

    //--------------------------
    // RecyclableSimdObjectDisplay

    template <typename simdType, typename simdWalker>
    LPCWSTR RecyclableSimdObjectDisplay<simdType, simdWalker>::Type()
    {LOGMEIN("DiagObjectModel.cpp] 4275\n");
        TypeId simdTypeId = JavascriptOperators::GetTypeId(instance);

        switch (simdTypeId)
        {LOGMEIN("DiagObjectModel.cpp] 4279\n");
        case TypeIds_SIMDInt32x4:
            return  _u("SIMD.Int32x4");
        case TypeIds_SIMDFloat32x4:
            return  _u("SIMD.Float32x4");
        case TypeIds_SIMDInt8x16:
            return  _u("SIMD.Int8x16");
        case TypeIds_SIMDInt16x8:
            return  _u("SIMD.Int16x8");
        case TypeIds_SIMDBool32x4:
            return  _u("SIMD.Bool32x4");
        case TypeIds_SIMDBool8x16:
            return  _u("SIMD.Bool8x16");
        case TypeIds_SIMDBool16x8:
            return  _u("SIMD.Bool16x8");
        case TypeIds_SIMDUint32x4:
            return  _u("SIMD.Uint32x4");
        case TypeIds_SIMDUint8x16:
            return  _u("SIMD.Uint8x16");
        case TypeIds_SIMDUint16x8:
            return  _u("SIMD.Uint16x8");
        default:
            AssertMsg(false, "Unexpected SIMD typeId");
            return nullptr;
        }
    }

    template <typename simdType, typename simdWalker>
    LPCWSTR RecyclableSimdObjectDisplay<simdType, simdWalker>::Value(int radix)
    {LOGMEIN("DiagObjectModel.cpp] 4308\n");
        StringBuilder<ArenaAllocator>* builder = GetStringBuilder();
        builder->Reset();

        simdType* simd = simdType::FromVar(instance);
        SIMDValue value = simd->GetValue();

        char16* stringBuffer = AnewArray(GetArenaFromContext(scriptContext), char16, SIMD_STRING_BUFFER_MAX);

        simdType::ToStringBuffer(value, stringBuffer, SIMD_STRING_BUFFER_MAX, scriptContext);

        builder->AppendSz(stringBuffer);

        return builder->Detach();
    }

    template <typename simdType, typename simdWalker>
    WeakArenaReference<IDiagObjectModelWalkerBase>* RecyclableSimdObjectDisplay<simdType, simdWalker>::CreateWalker()
    {LOGMEIN("DiagObjectModel.cpp] 4326\n");
        ReferencedArenaAdapter* pRefArena = scriptContext->GetThreadContext()->GetDebugManager()->GetDiagnosticArena();
        if (pRefArena)
        {LOGMEIN("DiagObjectModel.cpp] 4329\n");
            IDiagObjectModelWalkerBase* pOMWalker = Anew(pRefArena->Arena(), simdWalker, scriptContext, instance);
            return HeapNew(WeakArenaReference<IDiagObjectModelWalkerBase>, pRefArena, pOMWalker);
        }
        return nullptr;
    }
}
