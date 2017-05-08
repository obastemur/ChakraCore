//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#pragma once

class Value;

namespace IR {

class IntConstOpnd;
class Int64ConstOpnd;
class FloatConstOpnd;
class Simd128ConstOpnd;
class HelperCallOpnd;
class SymOpnd;
class PropertySymOpnd;
class RegOpnd;
class ArrayRegOpnd;
class AddrOpnd;
class IndirOpnd;
class LabelOpnd;
class MemRefOpnd;
class RegBVOpnd;

enum OpndKind : BYTE {
    OpndKindInvalid,
    OpndKindIntConst,
    OpndKindInt64Const,
    OpndKindFloatConst,
    OpndKindSimd128Const,
    OpndKindHelperCall,
    OpndKindSym,
    OpndKindReg,
    OpndKindAddr,
    OpndKindIndir,
    OpndKindLabel,
    OpndKindMemRef,
    OpndKindRegBV
};

enum AddrOpndKind : BYTE {
    // The following address kinds are safe for relocatable JIT and regular
    // JIT
    AddrOpndKindConstantAddress,
    AddrOpndKindConstantVar, // a constant var value (null or tagged int)
    // NOTE: None of the following address kinds should be generated directly
    // or you WILL break relocatable JIT code. Each kind has a helper that
    // will generate correct code for relocatable code & non-relocatable code.
    // The only exception is places where it is KNOWN that we will never
    // generate the code in relocatable JIT.

    // use LoadScriptContextOpnd
    AddrOpndKindDynamicScriptContext,
    // use LoadVTableValueOpnd
    AddrOpndKindDynamicVtable,
    // use LoadLibraryValueOpnd
    AddrOpndKindDynamicCharStringCache,
    // use appropriate helper
    AddrOpndKindDynamicMisc,
    // no profiling in dynamic JIT
    AddrOpndKindDynamicFunctionBody,
    AddrOpndKindDynamicFunctionInfo,
    // use LoadRuntimeInlineCacheOpnd for runtime caches,
    // in relocatable JIT polymorphic inline caches aren't generated and can
    // be referenced directly (for now)
    AddrOpndKindDynamicInlineCache,
    // no bailouts in dynamic JIT
    AddrOpndKindDynamicBailOutRecord,
    // use appropriate helper
    AddrOpndKindDynamicVar,
    AddrOpndKindDynamicType,
    AddrOpndKindDynamicTypeHandler,
    AddrOpndKindDynamicFrameDisplay,
    AddrOpndKindDynamicGuardValueRef,
    AddrOpndKindDynamicArrayCallSiteInfo,
    AddrOpndKindDynamicFunctionBodyWeakRef,
    AddrOpndKindDynamicObjectTypeRef,
    AddrOpndKindDynamicTypeCheckGuard,
    AddrOpndKindDynamicRecyclerAllocatorEndAddressRef,
    AddrOpndKindDynamicRecyclerAllocatorFreeListRef,
    AddrOpndKindDynamicBailOutKindRef,
    AddrOpndKindDynamicAuxSlotArrayRef,
    AddrOpndKindDynamicPropertySlotRef,
    AddrOpndKindDynamicFunctionEnvironmentRef,
    AddrOpndKindDynamicIsInstInlineCacheFunctionRef,
    AddrOpndKindDynamicIsInstInlineCacheTypeRef,
    AddrOpndKindDynamicIsInstInlineCacheResultRef,
    AddrOpndKindSz,
    AddrOpndKindDynamicFloatRef,
    AddrOpndKindDynamicDoubleRef,
    AddrOpndKindDynamicNativeCodeDataRef,
    AddrOpndKindDynamicAuxBufferRef,
    AddrOpndKindForInCache,
    AddrOpndKindForInCacheType,
    AddrOpndKindForInCacheData,
    AddrOpndKindWriteBarrierCardTable,
};

///---------------------------------------------------------------------------
///
/// class Opnd
///
///     IntConstOpnd        ; int values
///     FLoatConstOpnd      ; float values
///     HelperCallOpnd      ; lib helper address (more convenient dumps than AddrOpnd)
///     SymOpnd             ; stack symbol operand (not enregistered)
///     RegOpnd             ; register operand
///     AddrOpnd            ; address or var operand (includes TaggedInt's)
///     IndirOpnd           ; indirections operand (also used for JS array references)
///     LabelOpnd           ; label operand
///     MemRefOpnd          ; direct memory reference at a given memory address.
///     RegBVOpnd           ; unsigned int bit field used to denote a bit field vector. Example: Registers to push in STM.
///
///---------------------------------------------------------------------------

class Opnd
{
protected:
    Opnd() :
        m_inUse(false),
        m_isDead(false),
        m_isValueTypeFixed(false),
        canStoreTemp(false),
        isDiagHelperCallOpnd(false),
        isPropertySymOpnd(false)
    {TRACE_IT(14637);
#if DBG
        isFakeDst = false;
        isDeleted = false;
#endif
        m_kind = (OpndKind)0;
    }

    Opnd(const Opnd& oldOpnd) :
        m_type(oldOpnd.m_type),
        m_isDead(false),
        m_inUse(false),
        m_isValueTypeFixed(false),
        canStoreTemp(oldOpnd.canStoreTemp)
    {TRACE_IT(14638);
#if DBG
        isFakeDst = false;
        isDeleted = false;
#endif
        m_kind = oldOpnd.m_kind;

        // We will set isDeleted bit on a freed Opnd, this should not overlap with the next field of BVSparseNode
        // because BVSparseNode* are used to maintain freelist of memory of BVSparseNode size
#if DBG
        typedef BVSparseNode<JitArenaAllocator> BVSparseNode;
        CompileAssert(
            offsetof(Opnd, isDeleted) > offsetof(BVSparseNode, next) + sizeof(BVSparseNode*) ||
            offsetof(Opnd, isDeleted) < offsetof(BVSparseNode, next) + sizeof(BVSparseNode*));
#endif
    }
public:
    bool                IsConstOpnd() const;
    bool                IsImmediateOpnd() const;
    bool                IsMemoryOpnd() const;
    bool                IsIntConstOpnd() const;
    IntConstOpnd *      AsIntConstOpnd();
    bool                IsInt64ConstOpnd() const;
    Int64ConstOpnd *    AsInt64ConstOpnd();
    bool                IsFloatConstOpnd() const;
    FloatConstOpnd *    AsFloatConstOpnd();
    bool                IsSimd128ConstOpnd() const;
    Simd128ConstOpnd *  AsSimd128ConstOpnd();
    bool                IsHelperCallOpnd() const;
    HelperCallOpnd *    AsHelperCallOpnd();
    bool                IsSymOpnd() const;
    SymOpnd *           AsSymOpnd();
    PropertySymOpnd *   AsPropertySymOpnd();
    bool                IsRegOpnd() const;
    const RegOpnd *     AsRegOpnd() const;
    RegOpnd *           AsRegOpnd();
    bool                IsAddrOpnd() const;
    AddrOpnd *          AsAddrOpnd();
    bool                IsIndirOpnd() const;
    IndirOpnd *         AsIndirOpnd();
    bool                IsLabelOpnd() const;
    LabelOpnd *         AsLabelOpnd();
    bool                IsMemRefOpnd() const;
    MemRefOpnd *        AsMemRefOpnd();
    bool                IsRegBVOpnd() const;
    RegBVOpnd *         AsRegBVOpnd();

    OpndKind            GetKind() const;
    Opnd *              Copy(Func *func);
    Opnd *              CloneDef(Func *func);
    Opnd *              CloneUse(Func *func);
    StackSym *          GetStackSym() const;
    Sym *               GetSym() const;
    Opnd *              UseWithNewType(IRType type, Func * func);

    bool                IsEqual(Opnd *opnd);
    void                Free(Func * func);
    bool                IsInUse() const {TRACE_IT(14639); return m_inUse; }
    Opnd *              Use(Func * func);
    void                UnUse();
    IRType              GetType() const {TRACE_IT(14640); return this->m_type; }
    void                SetType(IRType type) {TRACE_IT(14641); this->m_type = type; }
    bool                IsSigned() const {TRACE_IT(14642); return IRType_IsSignedInt(this->m_type); }
    bool                IsUnsigned() const {TRACE_IT(14643); return IRType_IsUnsignedInt(this->m_type); }
    int                 GetSize() const {TRACE_IT(14644); return TySize[this->m_type]; }
    bool                IsInt64() const {TRACE_IT(14645); return IRType_IsInt64(this->m_type); }
    bool                IsInt32() const {TRACE_IT(14646); return this->m_type == TyInt32; }
    bool                IsUInt32() const {TRACE_IT(14647); return this->m_type == TyUint32; }
    bool                IsFloat32() const {TRACE_IT(14648); return this->m_type == TyFloat32; }
    bool                IsFloat64() const {TRACE_IT(14649); return this->m_type == TyFloat64; }
    bool                IsFloat() const {TRACE_IT(14650); return this->IsFloat32() || this->IsFloat64(); }
    bool                IsSimd128() const {TRACE_IT(14651); return IRType_IsSimd128(this->m_type);  }
    bool                IsSimd128F4()  const {TRACE_IT(14652); return this->m_type == TySimd128F4;  }
    bool                IsSimd128I4()  const {TRACE_IT(14653); return this->m_type == TySimd128I4;  }
    bool                IsSimd128I8()  const {TRACE_IT(14654); return this->m_type == TySimd128I8;  }
    bool                IsSimd128I16() const {TRACE_IT(14655); return this->m_type == TySimd128I16; }
    bool                IsSimd128U4()  const {TRACE_IT(14656); return this->m_type == TySimd128U4;  }
    bool                IsSimd128U8()  const {TRACE_IT(14657); return this->m_type == TySimd128U8;  }
    bool                IsSimd128U16() const {TRACE_IT(14658); return this->m_type == TySimd128U16; }
    bool                IsSimd128B4()  const {TRACE_IT(14659); return this->m_type == TySimd128B4;  }
    bool                IsSimd128B8()  const {TRACE_IT(14660); return this->m_type == TySimd128B8;  }
    bool                IsSimd128B16() const {TRACE_IT(14661); return this->m_type == TySimd128B16; }
    bool                IsSimd128D2()  const {TRACE_IT(14662); return this->m_type == TySimd128D2;  }
    bool                IsVar() const {TRACE_IT(14663); return this->m_type == TyVar; }
    bool                IsTaggedInt() const;
    bool                IsTaggedValue() const;
    bool                IsNotNumber() const;
    bool                IsNotInt() const;
    bool                IsNotTaggedValue() const;
    bool                IsWriteBarrierTriggerableValue();
    void                SetIsDead(const bool isDead = true)   {TRACE_IT(14664); this->m_isDead = isDead; }
    bool                GetIsDead()   {TRACE_IT(14665); return this->m_isDead; }
    int64               GetImmediateValue(Func * func);
#if TARGET_32 && !defined(_M_IX86)
    // Helper for 32bits systems without int64 const operand support
    int32               GetImmediateValueAsInt32(Func * func);
#endif
    BailoutConstantValue GetConstValue();
    bool                GetIsJITOptimizedReg() const {TRACE_IT(14666); return m_isJITOptimizedReg; }
    void                SetIsJITOptimizedReg(bool value) {TRACE_IT(14667); Assert(!value || !this->IsIndirOpnd()); m_isJITOptimizedReg = value; }

    ValueType           GetValueType() const {TRACE_IT(14668); return m_valueType; }
    void                SetValueType(const ValueType valueType);
    ValueType           FindProfiledValueType();
    bool                IsScopeObjOpnd(Func * func);
#if DBG_DUMP || defined(ENABLE_IR_VIEWER)
    virtual void        DummyFunction() {TRACE_IT(14669);} // Note needed for the VS debugger to disambiguate the different classes.
    void                DumpValueType();
    static void         DumpValueType(const ValueType valueType);
#endif

    bool                IsValueTypeFixed() const {TRACE_IT(14670); return m_isValueTypeFixed; }
    void                SetValueTypeFixed() {TRACE_IT(14671); m_isValueTypeFixed = true; }
    IR::RegOpnd *       FindRegUse(IR::RegOpnd *regOpnd);
    bool                IsArgumentsObject();

    static IntConstOpnd *CreateUint32Opnd(const uint i, Func *const func);
    static IntConstOpnd *CreateProfileIdOpnd(const Js::ProfileId profileId, Func *const func);
    static IntConstOpnd *CreateInlineCacheIndexOpnd(const Js::InlineCacheIndex inlineCacheIndex, Func *const func);
    static RegOpnd *CreateFramePointerOpnd(Func *const func);
public:
#if DBG_DUMP || defined(ENABLE_IR_VIEWER)
    static void         DumpAddress(void *address, bool printToConsole, bool skipMaskedAddress);
    static void         DumpFunctionInfo(_Outptr_result_buffer_(*count) char16 ** buffer, size_t * count, Js::FunctionInfo * info, bool printToConsole, _In_opt_z_ char16 const * type = nullptr);
    void                Dump(IRDumpFlags flags, Func *func);
    void                DumpOpndKindAddr(bool AsmDumpMode, Func *func);
    void                DumpOpndKindMemRef(bool AsmDumpMode, Func *func);
    static void         WriteToBuffer(_Outptr_result_buffer_(*count) char16 **buffer, size_t *count, const char16 *fmt, ...);
    void                GetAddrDescription(__out_ecount(count) char16 *const description, const size_t count, bool AsmDumpMode,
                            bool printToConsole, Func *func);
    static void         GetAddrDescription(__out_ecount(count) char16 *const description, const size_t count,
                            void * address, IR::AddrOpndKind addressKind, bool AsmDumpMode, bool printToConsole, Func *func, bool skipMaskedAddress = false);
    void                Dump();
#endif

    bool                CanStoreTemp() const {TRACE_IT(14672); return canStoreTemp; }
    void                SetCanStoreTemp() {TRACE_IT(14673); Assert(this->IsSymOpnd() || this->IsIndirOpnd()); canStoreTemp = true; }
protected:
    ValueType           m_valueType;
    IRType              m_type;

    // If true, it was deemed that the value type is definite (not likely) and shouldn't be changed. This is used for NewScArray
    // and the store-element instructions that follow it.
    bool                m_isValueTypeFixed:1;

    bool                m_inUse:1;
    bool                m_isDead:1;
    // This def/use of a byte code sym is not in the original byte code, don't count them in the bailout
    bool                m_isJITOptimizedReg:1;

    // For SymOpnd, this bit applies to the object pointer stack sym
    // For IndirOpnd, this bit applies to the base operand

    // If this opnd is a dst, that means that the object pointer is a stack object,
    // and we can store temp object/number on it
    // If the opnd is a src, that means that the object pointer may be a stack object
    // so the load may be a temp object/number and we need to track its use
    bool                canStoreTemp : 1;

    bool                isDiagHelperCallOpnd : 1;
    bool                isPropertySymOpnd : 1;
public:
#if DBG
    bool                isFakeDst : 1;
#endif
    OpndKind            m_kind;

#ifdef DBG
public:
    bool                isDeleted;
#endif
};

///---------------------------------------------------------------------------
///
/// class IntConstOpnd
///
///---------------------------------------------------------------------------

class IntConstOpnd sealed : public Opnd
{
public:
    static IntConstOpnd *   New(IntConstType value, IRType type, Func *func, bool dontEncode = false);
#if DBG_DUMP || defined(ENABLE_IR_VIEWER)
    static IntConstOpnd *   New(IntConstType value, IRType type, const char16 * name, Func *func, bool dontEncode = false);
#endif
    static IR::Opnd*        NewFromType(int64 value, IRType type, Func* func);

public:
    //Note: type OpndKindIntConst
    IntConstOpnd *          CopyInternal(Func *func);
    bool                    IsEqualInternal(Opnd *opnd);
    void                    FreeInternal(Func * func) ;
public:
    bool                    m_dontEncode;       // Setting this to true turns off XOR encoding for this constant.  Only set this on
                                                // constants not controllable by the user.

    IntConstType GetValue()
    {TRACE_IT(14674);
        return m_value;
    }

    void IncrValue(IntConstType by)
    {TRACE_IT(14675);
        SetValue(m_value + by);
    }

    void DecrValue(IntConstType by)
    {TRACE_IT(14676);
        SetValue(m_value - by);
    }

    void SetValue(IntConstType value);
    int32 AsInt32();
    uint32 AsUint32();

#if DBG_DUMP || defined(ENABLE_IR_VIEWER)
    IntConstType            decodedValue;  // FIXME (t-doilij) set ENABLE_IR_VIEWER blocks where this is set
    char16 const *         name;  // FIXME (t-doilij) set ENABLE_IR_VIEWER blocks where this is set
#endif

private:
    IntConstType            m_value;
};

///---------------------------------------------------------------------------
///
/// class Int64ConstOpnd
///
///---------------------------------------------------------------------------
class Int64ConstOpnd sealed : public Opnd
{
public:
    static Int64ConstOpnd* New(int64 value, IRType type, Func *func);

public:
    //Note: type OpndKindIntConst
    Int64ConstOpnd* CopyInternal(Func *func);
    bool IsEqualInternal(Opnd *opnd);
    void FreeInternal(Func * func) ;
public:
    int64 GetValue();

private:
    int64            m_value;
};

///---------------------------------------------------------------------------
///
/// class FloatConstOpnd
///
///---------------------------------------------------------------------------

class FloatConstOpnd: public Opnd
{
public:
    static FloatConstOpnd * New(FloatConstType value, IRType type, Func *func);
    static FloatConstOpnd * New(Js::Var floatVar, IRType type, Func *func, Js::Var varLocal = nullptr);

public:
    //Note: type OpndKindFloatConst
    FloatConstOpnd         *CopyInternal(Func *func);
    bool                    IsEqualInternal(Opnd *opnd);
    void                    FreeInternal(Func * func);
    AddrOpnd               *GetAddrOpnd(Func *func, bool dontEncode = false);
public:
    FloatConstType          m_value;
protected:
#if !FLOATVAR
    Js::Var                 m_number;
    Js::JavascriptNumber    *m_numberCopy;
#endif
};


class Simd128ConstOpnd sealed : public Opnd
{

public:
    static Simd128ConstOpnd * New(AsmJsSIMDValue value, IRType type, Func *func);

public:

    Simd128ConstOpnd *      CopyInternal(Func *func);
    bool                    IsEqualInternal(Opnd *opnd);
    void                    FreeInternal(Func * func);

public:
    AsmJsSIMDValue          m_value;
};

///---------------------------------------------------------------------------
///
/// class HelperCallOpnd
///
///---------------------------------------------------------------------------

class HelperCallOpnd: public Opnd
{
public:
    static HelperCallOpnd * New(JnHelperMethod fnHelper, Func *func);

protected:
    void Init(JnHelperMethod fnHelper);

public:
    //Note type : OpndKindHelperCall
    HelperCallOpnd         *CopyInternal(Func *func);
    bool                    IsEqualInternal(Opnd *opnd);
    void                    FreeInternal(Func * func);
    bool                    IsDiagHelperCallOpnd() const
    {TRACE_IT(14677);
        Assert(this->DbgIsDiagHelperCallOpnd() == isDiagHelperCallOpnd);
        return isDiagHelperCallOpnd;
    }
public:
    JnHelperMethod m_fnHelper;

#if DBG
private:
    virtual bool DbgIsDiagHelperCallOpnd() const {TRACE_IT(14678); return false; }
#endif
};

///---------------------------------------------------------------------------
///
/// class DiagHelperCallOpnd
/// Used in debug mode (Fast F12) for wrapping original helper method with try-catch wrapper.
///
///---------------------------------------------------------------------------

class DiagHelperCallOpnd: public HelperCallOpnd
{
public:
    static DiagHelperCallOpnd * New(JnHelperMethod fnHelper, Func *func, int argCount);
public:
    DiagHelperCallOpnd     *CopyInternalSub(Func *func);
    bool                    IsEqualInternalSub(Opnd *opnd);
public:
    int                     m_argCount;

#if DBG
private:
    virtual bool DbgIsDiagHelperCallOpnd() const override { return true; }
#endif
};


///---------------------------------------------------------------------------
///
/// class SymOpnd
///
///---------------------------------------------------------------------------

class SymOpnd: public Opnd
{
public:
    static SymOpnd *        New(Sym *sym, IRType type, Func *func);
    static SymOpnd *        New(Sym *sym, uint32 offset, IRType type, Func *func);

public:
    // Note type: OpndKindSym
    SymOpnd *               CopyInternal(Func *func);
    SymOpnd *               CloneDefInternal(Func *func);
    SymOpnd *               CloneUseInternal(Func *func);
    StackSym *              GetStackSymInternal() const;
    bool                    IsEqualInternal(Opnd *opnd);
    void                    FreeInternal(Func * func);
    bool                    IsPropertySymOpnd() const
    {TRACE_IT(14679);
        Assert(this->DbgIsPropertySymOpnd() == this->isPropertySymOpnd);
        return isPropertySymOpnd;
    }
public:
    Sym *                   m_sym;
    uint32                  m_offset;

private:
#if DBG
    virtual bool            DbgIsPropertySymOpnd() const {TRACE_IT(14680); return false; }
#endif

private:
    ValueType propertyOwnerValueType;

public:
    ValueType GetPropertyOwnerValueType() const
    {TRACE_IT(14681);
        return propertyOwnerValueType;
    }

    void SetPropertyOwnerValueType(const ValueType valueType)
    {TRACE_IT(14682);
        propertyOwnerValueType = valueType;
    }

    RegOpnd *CreatePropertyOwnerOpnd(Func *const func) const;
};

class PropertySymOpnd sealed : public SymOpnd
{
protected:
    PropertySymOpnd() : SymOpnd() {TRACE_IT(14683);}
    PropertySymOpnd(SymOpnd* symOpnd) : SymOpnd(*symOpnd) {TRACE_IT(14684);}

public:
    static PropertySymOpnd * New(PropertySym *propertySym, uint inlineCacheIndex, IRType type, Func *func);

public:
    PropertySymOpnd * CopyCommon(Func *func);
    PropertySymOpnd * CopyWithoutFlowSensitiveInfo(Func *func);
    PropertySymOpnd * CopyInternalSub(Func *func);
    PropertySymOpnd * CloneDefInternalSub(Func *func);
    PropertySymOpnd * CloneUseInternalSub(Func *func);
    void              Init(uint inlineCacheIndex, Func *func);

private:
    static PropertySymOpnd * New(PropertySym *propertySym, IRType type, Func *func);
    void Init(uint inlineCacheIndex, intptr_t runtimeInlineCache, JITTimePolymorphicInlineCache * runtimePolymorphicInlineCache, JITObjTypeSpecFldInfo* objTypeSpecFldInfo, byte polyCacheUtil);
#if DBG
    virtual bool      DbgIsPropertySymOpnd() const override { return true; }
#endif
public:
    Js::InlineCacheIndex m_inlineCacheIndex;
    intptr_t m_runtimeInlineCache;
    JITTimePolymorphicInlineCache* m_runtimePolymorphicInlineCache;
private:
    JITObjTypeSpecFldInfo* objTypeSpecFldInfo;
public:
    JITTypeHolder finalType;
    JITTypeHolder monoGuardType;
    BVSparse<JitArenaAllocator>* guardedPropOps;
    BVSparse<JitArenaAllocator>* writeGuards;
    byte m_polyCacheUtil;

private:
    bool usesAuxSlot : 1;
    Js::PropertyIndex slotIndex;
    uint16 checkedTypeSetIndex;

public:
    union
    {
        struct
        {
            bool isTypeCheckOnly: 1;
            // Note that even usesFixedValue cannot live on ObjTypeSpecFldInfo, because we may share a cache between
            // e.g. Object.prototype and new Object(), and only the latter actually uses the fixed value, even though both have it.
            bool usesFixedValue: 1;

            union
            {
                struct
                {
                    bool isTypeCheckSeqCandidate: 1;
                    bool typeAvailable: 1;
                    bool typeDead: 1;
                    bool typeChecked: 1;
                    bool initialTypeChecked: 1;
                    bool typeMismatch: 1;
                    bool writeGuardChecked: 1;
                };
                uint8 typeCheckSeqFlags;
            };
        };
        uint16 objTypeSpecFlags;
    };

public:
    StackSym * GetObjectSym() const {TRACE_IT(14685); return this->m_sym->AsPropertySym()->m_stackSym; };
    bool HasObjectTypeSym() const {TRACE_IT(14686); return this->m_sym->AsPropertySym()->HasObjectTypeSym(); };
    StackSym * GetObjectTypeSym() const {TRACE_IT(14687); return this->m_sym->AsPropertySym()->GetObjectTypeSym(); };
    PropertySym* GetPropertySym() const {TRACE_IT(14688); return this->m_sym->AsPropertySym(); }

    void TryDisableRuntimePolymorphicCache()
    {TRACE_IT(14689);
        if (this->m_runtimePolymorphicInlineCache && (this->m_polyCacheUtil < PolymorphicInlineCacheUtilizationThreshold))
        {TRACE_IT(14690);
            this->m_runtimePolymorphicInlineCache = nullptr;
        }
    }

    bool HasObjTypeSpecFldInfo() const
    {TRACE_IT(14691);
        return this->objTypeSpecFldInfo != nullptr;
    }

    void SetObjTypeSpecFldInfo(JITObjTypeSpecFldInfo *const objTypeSpecFldInfo)
    {TRACE_IT(14692);
        this->objTypeSpecFldInfo = objTypeSpecFldInfo;

        // The following information may change in a flow-based manner, and an ObjTypeSpecFldInfo is shared among several
        // PropertySymOpnds, so copy the information to the opnd
        if(!objTypeSpecFldInfo)
        {TRACE_IT(14693);
            usesAuxSlot = false;
            slotIndex = 0;
            return;
        }
        usesAuxSlot = objTypeSpecFldInfo->UsesAuxSlot();
        slotIndex = objTypeSpecFldInfo->GetSlotIndex();
    }

    void TryResetObjTypeSpecFldInfo()
    {TRACE_IT(14694);
        if (this->ShouldResetObjTypeSpecFldInfo())
        {TRACE_IT(14695);
            SetObjTypeSpecFldInfo(nullptr);
        }
    }

    bool ShouldResetObjTypeSpecFldInfo()
    {TRACE_IT(14696);
        // If an objTypeSpecFldInfo was created just for the purpose of polymorphic inlining but didn't get used for the same (for some reason or the other), and the polymorphic cache it was created from, wasn't equivalent,
        // we should null out this info on the propertySymOpnd so that assumptions downstream around equivalent object type spec still hold.
        if (HasObjTypeSpecFldInfo() && IsPoly() && (DoesntHaveEquivalence() || !IsLoadedFromProto()))
        {TRACE_IT(14697);
            return true;
        }
        return false;
    }

    JITObjTypeSpecFldInfo* GetObjTypeSpecInfo() const
    {TRACE_IT(14698);
        return this->objTypeSpecFldInfo;
    }

    uint GetObjTypeSpecFldId() const
    {TRACE_IT(14699);
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetObjTypeSpecFldId();
    }

    bool IsMono() const
    {TRACE_IT(14700);
        return HasObjTypeSpecFldInfo() && this->objTypeSpecFldInfo->IsMono();
    }

    bool IsPoly() const
    {TRACE_IT(14701);
        return HasObjTypeSpecFldInfo() && this->objTypeSpecFldInfo->IsPoly();
    }

    bool HasEquivalentTypeSet() const
    {TRACE_IT(14702);
        return HasObjTypeSpecFldInfo() && this->objTypeSpecFldInfo->HasEquivalentTypeSet();
    }

    bool DoesntHaveEquivalence() const
    {TRACE_IT(14703);
        return HasObjTypeSpecFldInfo() && this->objTypeSpecFldInfo->DoesntHaveEquivalence();
    }

    bool UsesAuxSlot() const
    {TRACE_IT(14704);
        return usesAuxSlot && HasObjTypeSpecFldInfo();
    }

    void SetUsesAuxSlot(bool value)
    {TRACE_IT(14705);
        Assert(HasObjTypeSpecFldInfo());
        usesAuxSlot = value;
    }

    bool IsLoadedFromProto() const
    {TRACE_IT(14706);
        return HasObjTypeSpecFldInfo() && this->objTypeSpecFldInfo->IsLoadedFromProto();
    }

    bool UsesAccessor() const
    {TRACE_IT(14707);
        return HasObjTypeSpecFldInfo() && this->objTypeSpecFldInfo->UsesAccessor();
    }

    bool HasFixedValue() const
    {TRACE_IT(14708);
        return HasObjTypeSpecFldInfo() && this->objTypeSpecFldInfo->HasFixedValue();
    }

    bool UsesFixedValue() const
    {TRACE_IT(14709);
        return this->usesFixedValue;
    }

    void SetUsesFixedValue(bool value)
    {TRACE_IT(14710);
        this->usesFixedValue = value;
    }

    bool MustDoMonoCheck() const
    {TRACE_IT(14711);
        return this->monoGuardType != nullptr;
    }

    JITTypeHolder GetMonoGuardType() const
    {TRACE_IT(14712);
        return this->monoGuardType;
    }

    void SetMonoGuardType(JITTypeHolder type)
    {TRACE_IT(14713);
        this->monoGuardType = type;
    }

    bool NeedsMonoCheck() const
    {TRACE_IT(14714);
        Assert(HasObjTypeSpecFldInfo());
        return this->IsBeingAdded() || (this->HasFixedValue() && !this->IsLoadedFromProto());
    }

    bool IsBeingStored() const
    {TRACE_IT(14715);
        return HasObjTypeSpecFldInfo() && this->objTypeSpecFldInfo->IsBeingStored();
    }

    void SetIsBeingStored(bool value)
    {TRACE_IT(14716);
        Assert(HasObjTypeSpecFldInfo());
        this->objTypeSpecFldInfo->SetIsBeingStored(value);
    }

    bool IsBeingAdded() const
    {TRACE_IT(14717);
        return HasObjTypeSpecFldInfo() && this->objTypeSpecFldInfo->IsBeingAdded();
    }

    bool IsRootObjectNonConfigurableField() const
    {TRACE_IT(14718);
        return HasObjTypeSpecFldInfo() && this->objTypeSpecFldInfo->IsRootObjectNonConfigurableField();
    }

    bool IsRootObjectNonConfigurableFieldLoad() const
    {TRACE_IT(14719);
        return HasObjTypeSpecFldInfo() && this->objTypeSpecFldInfo->IsRootObjectNonConfigurableFieldLoad();
    }

    uint16 GetSlotIndex() const
    {TRACE_IT(14720);
        Assert(HasObjTypeSpecFldInfo());
        return slotIndex;
    }

    void SetSlotIndex(uint16 index)
    {TRACE_IT(14721);
        Assert(HasObjTypeSpecFldInfo());
        slotIndex = index;
    }

    uint16 GetCheckedTypeSetIndex() const
    {TRACE_IT(14722);
        Assert(HasEquivalentTypeSet());
        return checkedTypeSetIndex;
    }

    void SetCheckedTypeSetIndex(uint16 index)
    {TRACE_IT(14723);
        Assert(HasEquivalentTypeSet());
        checkedTypeSetIndex = index;
    }

    Js::PropertyId GetPropertyId() const
    {TRACE_IT(14724);
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetPropertyId();
    }

    intptr_t GetProtoObject() const
    {TRACE_IT(14725);
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetProtoObject();
    }

    JITTimeFixedField * GetFixedFunction() const
    {TRACE_IT(14726);
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetFixedFieldIfAvailableAsFixedFunction();
    }

    JITTimeFixedField * GetFixedFunction(uint i) const
    {TRACE_IT(14727);
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetFixedFieldIfAvailableAsFixedFunction(i);
    }

    intptr_t GetFieldValueAsFixedData() const
    {TRACE_IT(14728);
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetFieldValueAsFixedDataIfAvailable();
    }

    intptr_t GetFieldValue(uint i) const
    {TRACE_IT(14729);
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetFieldValue(i);
    }

    JITTimeFixedField * GetFixedFieldInfoArray()
    {TRACE_IT(14730);
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetFixedFieldInfoArray();
    }

    uint16 GetFixedFieldCount()
    {TRACE_IT(14731);
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetFixedFieldCount();
    }

    JITTimeConstructorCache * GetCtorCache() const
    {TRACE_IT(14732);
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetCtorCache();
    }

    intptr_t GetPropertyGuardValueAddr() const
    {TRACE_IT(14733);
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetPropertyGuardValueAddr();
    }

    bool IsTypeCheckSeqCandidate() const
    {TRACE_IT(14734);
        Assert(IsObjTypeSpecCandidate() || !this->isTypeCheckSeqCandidate);
        return this->isTypeCheckSeqCandidate;
    }

    void SetTypeCheckSeqCandidate(bool value)
    {TRACE_IT(14735);
        Assert(IsObjTypeSpecCandidate() || !value);
        this->isTypeCheckSeqCandidate = value;
    }

    bool IsTypeCheckOnly() const
    {TRACE_IT(14736);
        return this->isTypeCheckOnly;
    }

    void SetTypeCheckOnly(bool value)
    {TRACE_IT(14737);
        this->isTypeCheckOnly = value;
    }

    bool IsTypeAvailable() const
    {TRACE_IT(14738);
        return this->typeAvailable;
    }

    void SetTypeAvailable(bool value)
    {TRACE_IT(14739);
        Assert(IsTypeCheckSeqCandidate());
        this->typeAvailable = value;
    }

    bool IsTypeDead() const
    {TRACE_IT(14740);
        return this->typeDead;
    }

    void SetTypeDead(bool value)
    {TRACE_IT(14741);
        Assert(IsTypeCheckSeqCandidate());
        this->typeDead = value;
    }

    void SetTypeDeadIfTypeCheckSeqCandidate(bool value)
    {TRACE_IT(14742);
        if (IsTypeCheckSeqCandidate())
        {TRACE_IT(14743);
            this->typeDead = value;
        }
    }

    bool IsTypeChecked() const
    {TRACE_IT(14744);
        return this->typeChecked;
    }

    void SetTypeChecked(bool value)
    {TRACE_IT(14745);
        Assert(IsTypeCheckSeqCandidate());
        this->typeChecked = value;
    }

    bool IsInitialTypeChecked() const
    {TRACE_IT(14746);
        return this->initialTypeChecked;
    }

    void SetInitialTypeChecked(bool value)
    {TRACE_IT(14747);
        Assert(IsTypeCheckSeqCandidate());
        this->initialTypeChecked = value;
    }

    bool HasTypeMismatch() const
    {TRACE_IT(14748);
        return this->typeMismatch;
    }

    void SetTypeMismatch(bool value)
    {TRACE_IT(14749);
        Assert(IsTypeCheckSeqCandidate());
        this->typeMismatch = value;
    }

    bool IsWriteGuardChecked() const
    {TRACE_IT(14750);
        return this->writeGuardChecked;
    }

    void SetWriteGuardChecked(bool value)
    {TRACE_IT(14751);
        Assert(IsTypeCheckSeqCandidate());
        this->writeGuardChecked = value;
    }

    uint16 GetObjTypeSpecFlags() const
    {TRACE_IT(14752);
        return this->objTypeSpecFlags;
    }

    void ClearObjTypeSpecFlags()
    {TRACE_IT(14753);
        this->objTypeSpecFlags = 0;
    }

    uint16 GetTypeCheckSeqFlags() const
    {TRACE_IT(14754);
        return this->typeCheckSeqFlags;
    }

    void ClearTypeCheckSeqFlags()
    {TRACE_IT(14755);
        this->typeCheckSeqFlags = 0;
    }

    bool MayNeedTypeCheckProtection() const
    {TRACE_IT(14756);
        return IsObjTypeSpecCandidate() && (IsTypeCheckSeqCandidate() || UsesFixedValue());
    }

    bool MayNeedWriteGuardProtection() const
    {TRACE_IT(14757);
        return IsLoadedFromProto() || UsesFixedValue();
    }

    bool IsTypeCheckProtected() const
    {TRACE_IT(14758);
        return IsTypeCheckSeqCandidate() && IsTypeChecked();
    }

    bool NeedsPrimaryTypeCheck() const
    {TRACE_IT(14759);
        // Only indicate that we need a primary type check, i.e. the type isn't yet available but will be needed downstream.
        // Type checks and bailouts may still be needed in other places (e.g. loads from proto, fixed field checks, or
        // property adds), if a primary type check cannot protect them.
        Assert(MayNeedTypeCheckProtection());
        Assert(TypeCheckSeqBitsSetOnlyIfCandidate());
        return IsTypeCheckSeqCandidate() && !IsTypeDead() && !IsTypeChecked() && !HasTypeMismatch();
    }

    bool NeedsLocalTypeCheck() const
    {TRACE_IT(14760);
        Assert(MayNeedTypeCheckProtection());
        Assert(TypeCheckSeqBitsSetOnlyIfCandidate());
        // Indicate whether this operation needs a type check for its own sake, since the type is dead and no downstream
        // operations require the type to be checked.
        return !PHASE_OFF1(Js::ObjTypeSpecIsolatedFldOpsPhase) &&
            IsTypeCheckSeqCandidate() && IsTypeDead() && !IsTypeCheckOnly() && !IsTypeChecked() && !HasTypeMismatch();
    }

    bool NeedsWriteGuardTypeCheck() const
    {TRACE_IT(14761);
        Assert(MayNeedTypeCheckProtection());
        Assert(TypeCheckSeqBitsSetOnlyIfCandidate());
        // Type has been checked but property might have been written to since then.
        return !IsTypeCheckOnly() && !NeedsPrimaryTypeCheck() && IsTypeChecked() && !IsWriteGuardChecked();
    }

    bool NeedsLoadFromProtoTypeCheck() const
    {TRACE_IT(14762);
        Assert(MayNeedTypeCheckProtection());
        Assert(TypeCheckSeqBitsSetOnlyIfCandidate());
        // Proto cache, where type has been checked but property might have been written to since then.
        return !IsTypeCheckOnly() && !NeedsPrimaryTypeCheck() && IsLoadedFromProto() && NeedsWriteGuardTypeCheck();
    }

    bool NeedsAddPropertyTypeCheck() const
    {TRACE_IT(14763);
        Assert(MayNeedTypeCheckProtection());
        Assert(TypeCheckSeqBitsSetOnlyIfCandidate());
        // A property cannot become read-only without an explicit or implicit call (at least Object.defineProperty is needed), so if this
        // operation is protected by a primary type check upstream, there is no need for an additional local type check.
        return false;
    }

    bool NeedsCheckFixedFieldTypeCheck() const
    {TRACE_IT(14764);
        Assert(MayNeedTypeCheckProtection());
        Assert(TypeCheckSeqBitsSetOnlyIfCandidate());
        return !IsTypeCheckOnly() && !NeedsPrimaryTypeCheck() && UsesFixedValue() && (!IsTypeChecked() || NeedsWriteGuardTypeCheck());
    }

    bool NeedsTypeCheck() const
    {TRACE_IT(14765);
        return NeedsPrimaryTypeCheck() || NeedsLocalTypeCheck() ||
            NeedsLoadFromProtoTypeCheck() || NeedsAddPropertyTypeCheck() || NeedsCheckFixedFieldTypeCheck();
    }

    bool NeedsTypeCheckAndBailOut() const
    {TRACE_IT(14766);
        return NeedsPrimaryTypeCheck() || (PHASE_ON1(Js::ObjTypeSpecIsolatedFldOpsWithBailOutPhase) && NeedsLocalTypeCheck()) || NeedsCheckFixedFieldTypeCheck();
    }

    // Is the instruction involving this operand optimized with a direct slot load or store? In other words, is it guarded
    // by a type check, either as part of the type check sequence, or explicitly on this instruction.
    bool IsObjTypeSpecOptimized() const
    {TRACE_IT(14767);
        return MayNeedTypeCheckProtection() && (NeedsTypeCheckAndBailOut() || IsTypeCheckProtected());
    }

    // May the instruction involving this operand result in an implicit call?  Note, that because in dead store pass we
    // may choose to remove a type check and fall back on a check against a live cache, instructions that have a primary
    // type check may still end up with implicit call bailout.  However, if we are type check protected we will never
    // fall back on live cache.  Similarly, for fixed method checks.
    bool MayHaveImplicitCall() const
    {TRACE_IT(14768);
        return !IsRootObjectNonConfigurableFieldLoad() && !UsesFixedValue() && (!IsTypeCheckSeqCandidate() || !IsTypeCheckProtected());
    }

    // Is the instruction involving this operand part of a type check sequence? This is different from IsObjTypeSpecOptimized
    // in that an instruction such as CheckFixedFld may require a type check even if it is not part of a type check
    // sequence. In this case IsObjTypeSpecOptimized() == true, but IsTypeCheckSeqParticipant() == false.
    bool IsTypeCheckSeqParticipant() const
    {TRACE_IT(14769);
        Assert(IsTypeCheckSeqCandidate());
        return NeedsPrimaryTypeCheck() || IsTypeCheckProtected();
    }

    bool HasFinalType() const;

    JITTypeHolder GetFinalType() const
    {TRACE_IT(14770);
        return this->finalType;
    }

    void SetFinalType(JITTypeHolder type)
    {TRACE_IT(14771);
        Assert(type != nullptr);
        this->finalType = type;
    }

    void ClearFinalType()
    {TRACE_IT(14772);
        this->finalType = JITTypeHolder(nullptr);
    }

    BVSparse<JitArenaAllocator>* GetGuardedPropOps()
    {TRACE_IT(14773);
        return this->guardedPropOps;
    }

    void EnsureGuardedPropOps(JitArenaAllocator* allocator)
    {TRACE_IT(14774);
        if (this->guardedPropOps == nullptr)
        {TRACE_IT(14775);
            this->guardedPropOps = JitAnew(allocator, BVSparse<JitArenaAllocator>, allocator);
        }
    }

    void SetGuardedPropOp(uint propOpId)
    {TRACE_IT(14776);
        Assert(this->guardedPropOps != nullptr);
        this->guardedPropOps->Set(propOpId);
    }

    void AddGuardedPropOps(const BVSparse<JitArenaAllocator>* propOps)
    {TRACE_IT(14777);
        Assert(this->guardedPropOps != nullptr);
        this->guardedPropOps->Or(propOps);
    }

    BVSparse<JitArenaAllocator>* GetWriteGuards()
    {TRACE_IT(14778);
        return this->writeGuards;
    }

    void SetWriteGuards(BVSparse<JitArenaAllocator>* value)
    {TRACE_IT(14779);
        Assert(this->writeGuards == nullptr);
        this->writeGuards = value;
    }

    void ClearWriteGuards()
    {TRACE_IT(14780);
        this->writeGuards = nullptr;
    }

#if DBG
    bool TypeCheckSeqBitsSetOnlyIfCandidate() const
    {TRACE_IT(14781);
        return IsTypeCheckSeqCandidate() || (!IsTypeAvailable() && !IsTypeChecked() && !IsWriteGuardChecked() && !IsTypeDead());
    }
#endif

    bool IsObjTypeSpecCandidate() const
    {TRACE_IT(14782);
        return HasObjTypeSpecFldInfo();
    }

    bool IsMonoObjTypeSpecCandidate() const
    {TRACE_IT(14783);
        return HasObjTypeSpecFldInfo() && this->objTypeSpecFldInfo->IsMonoObjTypeSpecCandidate();
    }

    bool IsPolyObjTypeSpecCandidate() const
    {TRACE_IT(14784);
        return HasObjTypeSpecFldInfo() && this->objTypeSpecFldInfo->IsPolyObjTypeSpecCandidate();
    }

    Js::TypeId GetTypeId() const
    {TRACE_IT(14785);
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetTypeId();
    }

    Js::TypeId GetTypeId(uint i) const
    {TRACE_IT(14786);
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetTypeId(i);
    }

    JITTypeHolder GetType() const
    {TRACE_IT(14787);
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetType();
    }

    JITTypeHolder GetType(uint i) const
    {TRACE_IT(14788);
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetType(i);
    }

    bool HasInitialType() const
    {TRACE_IT(14789);
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->HasInitialType();
    }

    JITTypeHolder GetInitialType() const
    {TRACE_IT(14790);
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetInitialType();
    }

    Js::EquivalentTypeSet * GetEquivalentTypeSet() const
    {TRACE_IT(14791);
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetEquivalentTypeSet();
    }

    JITTypeHolder GetFirstEquivalentType() const
    {TRACE_IT(14792);
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetFirstEquivalentType();
    }

    bool IsObjectHeaderInlined() const;
    void UpdateSlotForFinalType();
    bool ChangesObjectLayout() const;

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    const char16* GetCacheLayoutString() const
    {TRACE_IT(14793);
        return HasObjTypeSpecFldInfo() ? this->objTypeSpecFldInfo->GetCacheLayoutString() : _u("empty");
    }
#endif

};

///---------------------------------------------------------------------------
///
/// class RegOpnd
///
///---------------------------------------------------------------------------

class RegOpnd : public Opnd
{
protected:
    RegOpnd(StackSym *sym, RegNum reg, IRType type);
    RegOpnd(const RegOpnd &other, StackSym * sym);
private:
    void                    Initialize(StackSym *sym, RegNum reg, IRType type);

public:
    static RegOpnd *        New(IRType type, Func *func);
    static RegOpnd *        New(StackSym *sym, IRType type, Func *func);
    static RegOpnd *        New(StackSym *sym, RegNum reg, IRType type, Func *func);

public:
    bool                    IsArrayRegOpnd() const
    {TRACE_IT(14794);
        Assert(m_isArrayRegOpnd == DbgIsArrayRegOpnd());
        Assert(!m_isArrayRegOpnd || m_valueType.IsAnyOptimizedArray());
        return m_isArrayRegOpnd;
    }

    ArrayRegOpnd *          AsArrayRegOpnd();

    RegNum                  GetReg() const;
    void                    SetReg(RegNum reg);
    //Note type: OpndKindReg
    RegOpnd *               CopyInternal(Func *func);
    RegOpnd *               CloneDefInternal(Func *func);
    RegOpnd *               CloneUseInternal(Func *func);
    StackSym *              GetStackSymInternal() const;
    static StackSym *       TryGetStackSym(Opnd *const opnd);
    bool                    IsEqualInternal(Opnd *opnd);
    void                    FreeInternal(Func * func);
    bool                    IsSameReg(Opnd *opnd);
    bool                    IsSameRegUntyped(Opnd *opnd);

#if DBG
    void FreezeSymValue() {TRACE_IT(14795); m_symValueFrozen = true; }
    bool IsSymValueFrozen() const {TRACE_IT(14796); return m_symValueFrozen; }

    virtual bool DbgIsArrayRegOpnd() const {TRACE_IT(14797); return false; }
#endif

private:
    RegOpnd *               CopyInternal(StackSym * sym, Func * func);

public:
    StackSym *              m_sym;
    bool                    m_isTempLastUse:1;
    bool                    m_isCallArg:1;
    bool                    m_dontDeadStore: 1;
    bool                    m_fgPeepTmp: 1;
    bool                    m_wasNegativeZeroPreventedByBailout : 1;
    bool                    m_isArrayRegOpnd : 1;
#if DBG
private:
    bool                    m_symValueFrozen : 1; // if true, prevents this operand from being used as the destination operand in an instruction
#endif

private:
    RegNum                  m_reg;

    PREVENT_COPY(RegOpnd);
};

///---------------------------------------------------------------------------
///
/// class ArrayRegOpnd
///
///---------------------------------------------------------------------------

class ArrayRegOpnd sealed : public RegOpnd
{
private:
    StackSym *headSegmentSym;
    StackSym *headSegmentLengthSym;
    StackSym *lengthSym;
    const bool eliminatedLowerBoundCheck, eliminatedUpperBoundCheck;

protected:
    ArrayRegOpnd(StackSym *const arraySym, const ValueType valueType, StackSym *const headSegmentSym, StackSym *const headSegmentLengthSym, StackSym *const lengthSym, const bool eliminatedLowerBoundCheck, const bool eliminatedUpperBoundCheck);
    ArrayRegOpnd(const RegOpnd &other, StackSym *const arraySym, const ValueType valueType, StackSym *const headSegmentSym, StackSym *const headSegmentLengthSym, StackSym *const lengthSym, const bool eliminatedLowerBoundCheck, const bool eliminatedUpperBoundCheck);

public:
    static ArrayRegOpnd *New(StackSym *const arraySym, const ValueType valueType, StackSym *const headSegmentSym, StackSym *const headSegmentLengthSym, StackSym *const lengthSym, const bool eliminatedLowerBoundCheck, const bool eliminatedUpperBoundCheck, Func *const func);
    static ArrayRegOpnd *New(const RegOpnd *const other, const ValueType valueType, StackSym *const headSegmentSym, StackSym *const headSegmentLengthSym, StackSym *const lengthSym, const bool eliminatedLowerBoundCheck, const bool eliminatedUpperBoundCheck, Func *const func);

public:
#if DBG
    virtual bool DbgIsArrayRegOpnd() const {TRACE_IT(14798); return true; }
#endif
    StackSym *HeadSegmentSym() const
    {TRACE_IT(14799);
        return headSegmentSym;
    }

    void RemoveHeadSegmentSym()
    {TRACE_IT(14800);
        headSegmentSym = nullptr;
    }

    StackSym *HeadSegmentLengthSym() const
    {TRACE_IT(14801);
        return headSegmentLengthSym;
    }

    void RemoveHeadSegmentLengthSym()
    {TRACE_IT(14802);
        headSegmentLengthSym = nullptr;
    }

    StackSym *LengthSym() const
    {TRACE_IT(14803);
        // For typed arrays, the head segment length is the same as the array length
        Assert(!(m_valueType.IsLikelyTypedArray() && !m_valueType.IsOptimizedTypedArray()));
        return m_valueType.IsLikelyTypedArray() ? HeadSegmentLengthSym() : lengthSym;
    }

    void RemoveLengthSym()
    {TRACE_IT(14804);
        Assert(m_valueType.IsArray());
        lengthSym = nullptr;
    }

    bool EliminatedLowerBoundCheck() const
    {TRACE_IT(14805);
        return eliminatedLowerBoundCheck;
    }

    bool EliminatedUpperBoundCheck() const
    {TRACE_IT(14806);
        return eliminatedUpperBoundCheck;
    }

public:
    RegOpnd *CopyAsRegOpnd(Func *func);
    ArrayRegOpnd * CopyInternalSub(Func *func);
    ArrayRegOpnd *CloneDefInternalSub(Func *func);
    ArrayRegOpnd *CloneUseInternalSub(Func *func);
private:
    ArrayRegOpnd *Clone(StackSym *const arraySym, StackSym *const headSegmentSym, StackSym *const headSegmentLengthSym, StackSym *const lengthSym, Func *const func) const;

public:
    void FreeInternalSub(Func *func);

    // IsEqual is not overridden because this opnd still primarily represents the array sym. Equality comparisons using IsEqual
    // are used to determine whether opnds should be swapped, etc. and the extra information in this class should not affect
    // that behavior.
    // virtual bool IsEqual(Opnd *opnd) override;

    PREVENT_COPY(ArrayRegOpnd);
};

///---------------------------------------------------------------------------
///
/// class AddrOpnd
///
///---------------------------------------------------------------------------

class AddrOpnd sealed : public Opnd
{
public:
    static AddrOpnd *       New(intptr_t address, AddrOpndKind addrOpndKind, Func *func, bool dontEncode = false, Js::Var varLocal = nullptr);
    static AddrOpnd *       New(Js::Var address, AddrOpndKind addrOpndKind, Func *func, bool dontEncode = false, Js::Var varLocal = nullptr);
    static AddrOpnd *       NewFromNumber(double value, Func *func, bool dontEncode = false);
    static AddrOpnd *       NewFromNumber(int32 value, Func *func, bool dontEncode = false);
    static AddrOpnd *       NewFromNumber(int64 value, Func *func, bool dontEncode = false);
    static AddrOpnd *       NewFromNumberVar(double value, Func *func, bool dontEncode = false);
    static AddrOpnd *       NewNull(Func * func);
public:
    //Note type: OpndKindAddr
    AddrOpnd *              CopyInternal(Func *func);
    bool                    IsEqualInternal(Opnd *opnd);
    void                    FreeInternal(Func * func);

    bool                    IsDynamic() const {TRACE_IT(14807); return addrOpndKind > AddrOpndKindConstantVar; }
    bool                    IsVar() const {TRACE_IT(14808); return addrOpndKind == AddrOpndKindDynamicVar || addrOpndKind == AddrOpndKindConstantVar; }
    void                    SetEncodedValue(Js::Var address, AddrOpndKind addrOpndKind);
    AddrOpndKind            GetAddrOpndKind() const {TRACE_IT(14809); return addrOpndKind; }
    void                    SetAddress(Js::Var address, AddrOpndKind addrOpndKind);
public:

    // TODO: OOP JIT, make this union more transparent
    //union {
        void *                  m_metadata;
        Js::Var                 m_localAddress;
    //};
    Js::Var                 m_address;
    bool                    m_dontEncode: 1;
    bool                    m_isFunction: 1;
private:
    AddrOpndKind            addrOpndKind;
public:
#if DBG_DUMP || defined(ENABLE_IR_VIEWER)
    Js::Var                 decodedValue;  // FIXME (t-doilij) set ENABLE_IR_VIEWER blocks where this is set
#endif
#if DBG_DUMP || defined(ENABLE_IR_VIEWER)
    bool                    wasVar;
#endif
};

///---------------------------------------------------------------------------
///
/// class IndirOpnd
///
///---------------------------------------------------------------------------

class IndirOpnd: public Opnd
{
public:
    static IndirOpnd *      New(RegOpnd * baseOpnd, RegOpnd * indexOpnd, IRType type, Func *func);
    static IndirOpnd *      New(RegOpnd * baseOpnd, RegOpnd * indexOpnd, byte scale, IRType type, Func *func);
    static IndirOpnd *      New(RegOpnd * baseOpnd, int32 offset, IRType type, Func *func, bool dontEncode = false);
#if DBG_DUMP || defined(ENABLE_IR_VIEWER)
    static IndirOpnd *      New(RegOpnd * baseOpnd, int32 offset, IRType type, const char16 *desc, Func *func, bool dontEncode = false);
#endif

public:
    IndirOpnd() : Opnd(), m_baseOpnd(nullptr), m_indexOpnd(nullptr), m_offset(0), m_scale(0), m_func(nullptr), m_dontEncode(false)
#if DBG_DUMP || defined(ENABLE_IR_VIEWER)
        , m_desc(nullptr)
#endif
#if DBG_DUMP
        , m_addrKind((IR::AddrOpndKind)-1)
#endif
    {
    }
    ~IndirOpnd();

    // Note type: OpndKindIndir
    IndirOpnd *             CopyInternal(Func *func);
    IndirOpnd *             CloneDefInternal(Func *func);
    IndirOpnd *             CloneUseInternal(Func *func);
    bool                    IsEqualInternal(Opnd *opnd);
    void                    FreeInternal(Func * func);

    RegOpnd *               GetBaseOpnd() const;
    void                    SetBaseOpnd(RegOpnd *baseOpnd);
    RegOpnd *               UnlinkBaseOpnd();
    void                    ReplaceBaseOpnd(RegOpnd *newBase);
    RegOpnd *               GetIndexOpnd();
    void                    SetIndexOpnd(RegOpnd *indexOpnd);
    RegOpnd *               GetIndexOpnd() const;
    RegOpnd *               UnlinkIndexOpnd();
    void                    ReplaceIndexOpnd(RegOpnd *newIndex);
    int32                   GetOffset() const;
    void                    SetOffset(int32 offset, bool dontEncode = false);
    byte                    GetScale() const;
    void                    SetScale(byte scale);
    bool                    TryGetIntConstIndexValue(bool trySym, IntConstType *pValue, bool *pIsNotInt);
#if DBG_DUMP || defined(ENABLE_IR_VIEWER)
    const char16 *         GetDescription();
    IR::AddrOpndKind        GetAddrKind() const;
    bool                    HasAddrKind() const;
    void *                  GetOriginalAddress() const;
#endif
    bool                    m_dontEncode;

#if DBG_DUMP
    void                    SetAddrKind(IR::AddrOpndKind kind, void * originalAddress);
#endif
private:
    RegOpnd *               m_baseOpnd;
    RegOpnd *               m_indexOpnd;
    int32                   m_offset;
    byte                    m_scale;
    Func *                  m_func;  // We need the allocator to copy the base and index...

#if DBG_DUMP || defined(ENABLE_IR_VIEWER)
    const char16 *         m_desc;
#endif
#if DBG_DUMP
    IR::AddrOpndKind        m_addrKind;  // if m_addrKind != -1, than this used to be MemRefOpnd which has the address hoisted;
    void *                  m_originalAddress;
#endif

};

///---------------------------------------------------------------------------
///
/// class MemRefOpnd - represents a reference to a fixed memory location
///
///---------------------------------------------------------------------------

class MemRefOpnd : public Opnd
{
public:
    static MemRefOpnd *     New(void * pMemLoc, IRType, Func * func, AddrOpndKind addrOpndKind = AddrOpndKindDynamicMisc);
    static MemRefOpnd *     New(intptr_t pMemLoc, IRType, Func * func, AddrOpndKind addrOpndKind = AddrOpndKindDynamicMisc);

public:
    // Note type: OpndKindMemRef
    MemRefOpnd *            CopyInternal(Func * func);
    bool                    IsEqualInternal(Opnd *opnd);
    void                    FreeInternal(Func * func);

    intptr_t                  GetMemLoc() const;
    void                    SetMemLoc(intptr_t pMemLoc);

    IR::AddrOpndKind        GetAddrKind() const;

private:
    intptr_t                  m_memLoc;
#if DBG_DUMP
    AddrOpndKind            m_addrKind;
#endif
};

//
// class LabelOpnd - represents a reference to a local code address
//

class LabelOpnd : public Opnd
{
public:
    static LabelOpnd *      New(LabelInstr * labelInstr, Func * func);

public:
    //Note type: OpndKindLabel
    LabelOpnd *             CopyInternal(Func * func);
    bool                    IsEqualInternal(Opnd * opnd);
    void                    FreeInternal(Func * func);

    LabelInstr *            GetLabel() const;
    void                    SetLabel(LabelInstr * labelInstr);

private:
    LabelInstr *            m_label;
};

///---------------------------------------------------------------------------
///
/// class Bit Field vector
///
///---------------------------------------------------------------------------

class RegBVOpnd: public Opnd
{
public:
    static RegBVOpnd *      New(BVUnit32 value, IRType type, Func *func);

public:
    //Note: type: OpndKindRegBV
    RegBVOpnd *             CopyInternal(Func *func);
    bool                    IsEqualInternal(Opnd *opnd);
    void                    FreeInternal(Func * func);
    BVUnit32                GetValue() const;
public:
    BVUnit32                m_value;
};

class AutoReuseOpnd
{
private:
    Opnd *opnd;
    Func *func;
    bool autoDelete;
    bool wasInUse;

public:
    AutoReuseOpnd() : opnd(nullptr), wasInUse(true)
    {TRACE_IT(14810);
    }

    AutoReuseOpnd(Opnd *const opnd, Func *const func, const bool autoDelete = true) : opnd(nullptr)
    {
        Initialize(opnd, func, autoDelete);
    }

    void Initialize(Opnd *const opnd, Func *const func, const bool autoDelete = true)
    {TRACE_IT(14811);
        Assert(!this->opnd);
        Assert(func);

        if(!opnd)
        {TRACE_IT(14812);
            // Simulate the default constructor
            wasInUse = true;
            return;
        }

        this->opnd = opnd;
        wasInUse = opnd->IsInUse();
        if(wasInUse)
        {TRACE_IT(14813);
            return;
        }
        this->func = func;
        this->autoDelete = autoDelete;

        // Create a fake use of the opnd to enable opnd reuse during lowering. One issue is that when an unused opnd is first
        // used in an instruction and the instruction is legalized, the opnd may be replaced by legalization and the original
        // opnd would be freed. By creating a fake use, it forces the opnd to be copied when used by the instruction, so the
        // original opnd can continue to be reused for other instructions. Typically, any opnds used during lowering in more
        // than one instruction can use this class to enable opnd reuse.
        opnd->Use(func);
    }

    ~AutoReuseOpnd()
    {TRACE_IT(14814);
        if(wasInUse)
        {TRACE_IT(14815);
            return;
        }
        if(autoDelete)
        {TRACE_IT(14816);
            opnd->Free(func);
        }
        else
        {TRACE_IT(14817);
            opnd->UnUse();
        }
    }

    PREVENT_COPY(AutoReuseOpnd)
};

} // namespace IR
