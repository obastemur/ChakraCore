//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

enum TypeFlagMask : uint8
{
    TypeFlagMask_None                                                              = 0x00,
    TypeFlagMask_AreThisAndPrototypesEnsuredToHaveOnlyWritableDataProperties       = 0x01,
    TypeFlagMask_IsFalsy                                                           = 0x02,
    TypeFlagMask_HasSpecialPrototype                                               = 0x04,
    TypeFlagMask_External                                                          = 0x08,
    TypeFlagMask_SkipsPrototype                                                    = 0x10,
    TypeFlagMask_CanHaveInterceptors                                               = 0x20,
    TypeFlagMask_JsrtExternal                                                      = 0x40,
    TypeFlagMask_HasBeenCached                                                     = 0x80
};
ENUM_CLASS_HELPERS(TypeFlagMask, uint8);

namespace Js
{
    class TypePropertyCache;
    class Type
    {
        friend class DynamicObject;
        friend class GlobalObject;
        friend class ScriptEngineBase;

    protected:
        Field(TypeId) typeId;
        Field(TypeFlagMask) flags;

        Field(JavascriptLibrary*) javascriptLibrary;

        Field(RecyclableObject*) prototype;
        FieldNoBarrier(JavascriptMethod) entryPoint;
    private:
        Field(TypePropertyCache *) propertyCache;
    protected:
        Type(Type * type);
        Type(ScriptContext* scriptContext, TypeId typeId, RecyclableObject* prototype, JavascriptMethod entryPoint);

    public:
        static DWORD GetJavascriptLibraryOffset() {LOGMEIN("Type.h] 44\n"); return offsetof(Type, javascriptLibrary); }
        inline TypeId GetTypeId() const {LOGMEIN("Type.h] 45\n"); return typeId; }
        void SetTypeId(TypeId typeId) {LOGMEIN("Type.h] 46\n"); this->typeId = typeId; }
        RecyclableObject* GetPrototype() const {LOGMEIN("Type.h] 47\n"); return prototype; }
        JavascriptMethod GetEntryPoint() const {LOGMEIN("Type.h] 48\n"); return entryPoint; }
        JavascriptLibrary* GetLibrary() const {LOGMEIN("Type.h] 49\n"); return javascriptLibrary; }
        ScriptContext * GetScriptContext() const;
        Recycler * GetRecycler() const;
        TypePropertyCache *GetPropertyCache();
        TypePropertyCache *CreatePropertyCache();
        BOOL HasSpecialPrototype() const {LOGMEIN("Type.h] 54\n"); return (flags & TypeFlagMask_HasSpecialPrototype) == TypeFlagMask_HasSpecialPrototype; }

        // This function has a different meaning from RecyclableObject::HasOnlyWritableDataProperties. If this function returns
        // true, then it's implied that RecyclableObject::HasOnlyWritableDataProperties would return true for an object of this
        // type and all of its prototypes. However, if this function returns false, it does not imply the converse.
        BOOL AreThisAndPrototypesEnsuredToHaveOnlyWritableDataProperties() const;
        void SetAreThisAndPrototypesEnsuredToHaveOnlyWritableDataProperties(const bool truth);

        inline BOOL IsExternal() const {LOGMEIN("Type.h] 62\n"); return (this->flags & TypeFlagMask_External) != 0; }
        inline BOOL IsJsrtExternal() const {LOGMEIN("Type.h] 63\n"); return (this->flags & TypeFlagMask_JsrtExternal) != 0; }
        inline BOOL SkipsPrototype() const {LOGMEIN("Type.h] 64\n"); return (this->flags & TypeFlagMask_SkipsPrototype) != 0 ; }
        inline BOOL CanHaveInterceptors() const {LOGMEIN("Type.h] 65\n"); return (this->flags & TypeFlagMask_CanHaveInterceptors) != 0; }
        inline BOOL IsFalsy() const {LOGMEIN("Type.h] 66\n"); return flags & TypeFlagMask_IsFalsy; }
        inline BOOL HasBeenCached() const {LOGMEIN("Type.h] 67\n"); return flags & TypeFlagMask_HasBeenCached; }
        inline void SetHasBeenCached()
        {LOGMEIN("Type.h] 69\n");
            // Once set, this flag should never be reset.
            flags |= TypeFlagMask_HasBeenCached;
        };

        void SetIsFalsy(const bool truth);
        void SetHasSpecialPrototype(const bool hasSpecialPrototype);

        // This is for static lib verification use only.
        static DWORD GetTypeIdFieldOffset() {LOGMEIN("Type.h] 78\n"); return offsetof(Type, typeId); }
        static size_t OffsetOfWritablePropertiesFlag()
        {LOGMEIN("Type.h] 80\n");
            return offsetof(Type, flags);
        }

        static uint32 GetOffsetOfTypeId();
        static uint32 GetOffsetOfFlags();
        static uint32 GetOffsetOfEntryPoint();
        static uint32 GetOffsetOfPrototype();

        static InternalString UndefinedTypeNameString;
        static InternalString ObjectTypeNameString;
        static InternalString BooleanTypeNameString;
        static InternalString NumberTypeNameString;
        static InternalString StringTypeNameString;
        static InternalString FunctionTypeNameString;

#if defined(PROFILE_RECYCLER_ALLOC) && defined(RECYCLER_DUMP_OBJECT_GRAPH)
        static bool DumpObjectFunction(type_info const * typeinfo, bool isArray, void * objectAddress);
#endif

#if ENABLE_TTD
        void ExtractSnapType(TTD::NSSnapType::SnapType* sType, TTD::NSSnapType::SnapHandler* optHandler, TTD::SlabAllocator& alloc) const;
#endif
    };
};
