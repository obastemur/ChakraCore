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
        DynamicType(DynamicType * type) : Type(type), typeHandler(type->typeHandler), isLocked(false), isShared(false) {LOGMEIN("DynamicType.h] 36\n");}
        DynamicType(DynamicType * type, DynamicTypeHandler *typeHandler, bool isLocked, bool isShared);
        DynamicType(ScriptContext* scriptContext, TypeId typeId, RecyclableObject* prototype, JavascriptMethod entryPoint, DynamicTypeHandler * typeHandler, bool isLocked, bool isShared);

    public:
        DynamicTypeHandler * GetTypeHandler() const {LOGMEIN("DynamicType.h] 41\n"); return typeHandler; }

        void SetPrototype(RecyclableObject* newPrototype) {LOGMEIN("DynamicType.h] 43\n"); this->prototype = newPrototype; }
        bool GetIsLocked() const {LOGMEIN("DynamicType.h] 44\n"); return this->isLocked; }
        bool GetIsShared() const {LOGMEIN("DynamicType.h] 45\n"); return this->isShared; }
#if DBG
        bool GetIsCachedForChangePrototype() const {LOGMEIN("DynamicType.h] 47\n"); return this->isCachedForChangePrototype; }
        void SetIsCachedForChangePrototype() {LOGMEIN("DynamicType.h] 48\n"); this->isCachedForChangePrototype = true; }
#endif
        void SetEntryPoint(JavascriptMethod method) {LOGMEIN("DynamicType.h] 50\n"); entryPoint = method; }

        BOOL AllPropertiesAreEnumerable() {LOGMEIN("DynamicType.h] 52\n"); return typeHandler->AllPropertiesAreEnumerable(); }

        bool LockType()
        {LOGMEIN("DynamicType.h] 55\n");
            if (GetIsLocked())
            {LOGMEIN("DynamicType.h] 57\n");
                Assert(this->GetTypeHandler()->IsLockable());
                return true;
            }
            if (this->GetTypeHandler()->IsLockable())
            {LOGMEIN("DynamicType.h] 62\n");
                this->GetTypeHandler()->LockTypeHandler();
                this->isLocked = true;
                return true;
            }
            return false;
        }

        bool ShareType()
        {LOGMEIN("DynamicType.h] 71\n");
            if (this->GetIsShared())
            {LOGMEIN("DynamicType.h] 73\n");
                Assert(this->GetTypeHandler()->IsSharable());
                return true;
            }
            if (this->GetTypeHandler()->IsSharable())
            {LOGMEIN("DynamicType.h] 78\n");
                LockType();
                this->GetTypeHandler()->ShareTypeHandler(this->GetScriptContext());
                this->isShared = true;
                return true;
            }
            return false;
        }

        bool GetHasNoEnumerableProperties() const {LOGMEIN("DynamicType.h] 87\n"); return hasNoEnumerableProperties; }
        bool SetHasNoEnumerableProperties(bool value);
        bool PrepareForTypeSnapshotEnumeration();

        static bool Is(TypeId typeId);
        static DynamicType * New(ScriptContext* scriptContext, TypeId typeId, RecyclableObject* prototype, JavascriptMethod entryPoint, DynamicTypeHandler * typeHandler, bool isLocked = false, bool isShared = false);

        static uint32 GetOffsetOfTypeHandler() {LOGMEIN("DynamicType.h] 94\n"); return offsetof(DynamicType, typeHandler); }
        static uint32 GetOffsetOfIsShared() {LOGMEIN("DynamicType.h] 95\n"); return offsetof(DynamicType, isShared); }
        static uint32 GetOffsetOfHasNoEnumerableProperties() {LOGMEIN("DynamicType.h] 96\n"); return offsetof(DynamicType, hasNoEnumerableProperties); }
    private:
        void SetIsLocked() {LOGMEIN("DynamicType.h] 98\n"); Assert(this->GetTypeHandler()->GetIsLocked()); this->isLocked = true; }
        void SetIsShared() {LOGMEIN("DynamicType.h] 99\n"); Assert(this->GetIsLocked() && this->GetTypeHandler()->GetIsShared()); this->isShared = true; }
        void SetIsLockedAndShared() {LOGMEIN("DynamicType.h] 100\n"); SetIsLocked(); SetIsShared(); }

    };
};
