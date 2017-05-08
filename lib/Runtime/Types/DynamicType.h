//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    class DynamicType : public Type
    {
#if DBG
        friend class JavascriptFunction;
#endif
        friend class DynamicObject;
        friend class DynamicTypeHandler;
        friend class CrossSite;
        friend class TypePath;
        friend class PathTypeHandlerBase;
        friend class SimplePathTypeHandler;
        friend class PathTypeHandler;
        friend class ES5ArrayType;
        friend class JavascriptOperators;

        template <typename TPropertyIndex, typename TMapKey, bool IsNotExtensibleSupported>
        friend class SimpleDictionaryTypeHandlerBase;

    private:
        Field(DynamicTypeHandler *) typeHandler;
        Field(bool) isLocked;
        Field(bool) isShared;
        Field(bool) hasNoEnumerableProperties;
#if DBG
        Field(bool) isCachedForChangePrototype;
#endif

    protected:
        DynamicType(DynamicType * type) : Type(type), typeHandler(type->typeHandler), isLocked(false), isShared(false) {TRACE_IT(66100);}
        DynamicType(DynamicType * type, DynamicTypeHandler *typeHandler, bool isLocked, bool isShared);
        DynamicType(ScriptContext* scriptContext, TypeId typeId, RecyclableObject* prototype, JavascriptMethod entryPoint, DynamicTypeHandler * typeHandler, bool isLocked, bool isShared);

    public:
        DynamicTypeHandler * GetTypeHandler() const {TRACE_IT(66101); return typeHandler; }

        void SetPrototype(RecyclableObject* newPrototype) {TRACE_IT(66102); this->prototype = newPrototype; }
        bool GetIsLocked() const {TRACE_IT(66103); return this->isLocked; }
        bool GetIsShared() const {TRACE_IT(66104); return this->isShared; }
#if DBG
        bool GetIsCachedForChangePrototype() const {TRACE_IT(66105); return this->isCachedForChangePrototype; }
        void SetIsCachedForChangePrototype() {TRACE_IT(66106); this->isCachedForChangePrototype = true; }
#endif
        void SetEntryPoint(JavascriptMethod method) {TRACE_IT(66107); entryPoint = method; }

        BOOL AllPropertiesAreEnumerable() {TRACE_IT(66108); return typeHandler->AllPropertiesAreEnumerable(); }

        bool LockType()
        {TRACE_IT(66109);
            if (GetIsLocked())
            {TRACE_IT(66110);
                Assert(this->GetTypeHandler()->IsLockable());
                return true;
            }
            if (this->GetTypeHandler()->IsLockable())
            {TRACE_IT(66111);
                this->GetTypeHandler()->LockTypeHandler();
                this->isLocked = true;
                return true;
            }
            return false;
        }

        bool ShareType()
        {TRACE_IT(66112);
            if (this->GetIsShared())
            {TRACE_IT(66113);
                Assert(this->GetTypeHandler()->IsSharable());
                return true;
            }
            if (this->GetTypeHandler()->IsSharable())
            {TRACE_IT(66114);
                LockType();
                this->GetTypeHandler()->ShareTypeHandler(this->GetScriptContext());
                this->isShared = true;
                return true;
            }
            return false;
        }

        bool GetHasNoEnumerableProperties() const {TRACE_IT(66115); return hasNoEnumerableProperties; }
        bool SetHasNoEnumerableProperties(bool value);
        bool PrepareForTypeSnapshotEnumeration();

        static bool Is(TypeId typeId);
        static DynamicType * New(ScriptContext* scriptContext, TypeId typeId, RecyclableObject* prototype, JavascriptMethod entryPoint, DynamicTypeHandler * typeHandler, bool isLocked = false, bool isShared = false);

        static uint32 GetOffsetOfTypeHandler() {TRACE_IT(66116); return offsetof(DynamicType, typeHandler); }
        static uint32 GetOffsetOfIsShared() {TRACE_IT(66117); return offsetof(DynamicType, isShared); }
        static uint32 GetOffsetOfHasNoEnumerableProperties() {TRACE_IT(66118); return offsetof(DynamicType, hasNoEnumerableProperties); }
    private:
        void SetIsLocked() {TRACE_IT(66119); Assert(this->GetTypeHandler()->GetIsLocked()); this->isLocked = true; }
        void SetIsShared() {TRACE_IT(66120); Assert(this->GetIsLocked() && this->GetTypeHandler()->GetIsShared()); this->isShared = true; }
        void SetIsLockedAndShared() {TRACE_IT(66121); SetIsLocked(); SetIsShared(); }

    };
};
