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
    {LOGMEIN("Opnd.h] 127\n");
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
    {LOGMEIN("Opnd.h] 141\n");
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
    bool                IsInUse() const {LOGMEIN("Opnd.h] 198\n"); return m_inUse; }
    Opnd *              Use(Func * func);
    void                UnUse();
    IRType              GetType() const {LOGMEIN("Opnd.h] 201\n"); return this->m_type; }
    void                SetType(IRType type) {LOGMEIN("Opnd.h] 202\n"); this->m_type = type; }
    bool                IsSigned() const {LOGMEIN("Opnd.h] 203\n"); return IRType_IsSignedInt(this->m_type); }
    bool                IsUnsigned() const {LOGMEIN("Opnd.h] 204\n"); return IRType_IsUnsignedInt(this->m_type); }
    int                 GetSize() const {LOGMEIN("Opnd.h] 205\n"); return TySize[this->m_type]; }
    bool                IsInt64() const {LOGMEIN("Opnd.h] 206\n"); return IRType_IsInt64(this->m_type); }
    bool                IsInt32() const {LOGMEIN("Opnd.h] 207\n"); return this->m_type == TyInt32; }
    bool                IsUInt32() const {LOGMEIN("Opnd.h] 208\n"); return this->m_type == TyUint32; }
    bool                IsFloat32() const {LOGMEIN("Opnd.h] 209\n"); return this->m_type == TyFloat32; }
    bool                IsFloat64() const {LOGMEIN("Opnd.h] 210\n"); return this->m_type == TyFloat64; }
    bool                IsFloat() const {LOGMEIN("Opnd.h] 211\n"); return this->IsFloat32() || this->IsFloat64(); }
    bool                IsSimd128() const {LOGMEIN("Opnd.h] 212\n"); return IRType_IsSimd128(this->m_type);  }
    bool                IsSimd128F4()  const {LOGMEIN("Opnd.h] 213\n"); return this->m_type == TySimd128F4;  }
    bool                IsSimd128I4()  const {LOGMEIN("Opnd.h] 214\n"); return this->m_type == TySimd128I4;  }
    bool                IsSimd128I8()  const {LOGMEIN("Opnd.h] 215\n"); return this->m_type == TySimd128I8;  }
    bool                IsSimd128I16() const {LOGMEIN("Opnd.h] 216\n"); return this->m_type == TySimd128I16; }
    bool                IsSimd128U4()  const {LOGMEIN("Opnd.h] 217\n"); return this->m_type == TySimd128U4;  }
    bool                IsSimd128U8()  const {LOGMEIN("Opnd.h] 218\n"); return this->m_type == TySimd128U8;  }
    bool                IsSimd128U16() const {LOGMEIN("Opnd.h] 219\n"); return this->m_type == TySimd128U16; }
    bool                IsSimd128B4()  const {LOGMEIN("Opnd.h] 220\n"); return this->m_type == TySimd128B4;  }
    bool                IsSimd128B8()  const {LOGMEIN("Opnd.h] 221\n"); return this->m_type == TySimd128B8;  }
    bool                IsSimd128B16() const {LOGMEIN("Opnd.h] 222\n"); return this->m_type == TySimd128B16; }
    bool                IsSimd128D2()  const {LOGMEIN("Opnd.h] 223\n"); return this->m_type == TySimd128D2;  }
    bool                IsVar() const {LOGMEIN("Opnd.h] 224\n"); return this->m_type == TyVar; }
    bool                IsTaggedInt() const;
    bool                IsTaggedValue() const;
    bool                IsNotNumber() const;
    bool                IsNotInt() const;
    bool                IsNotTaggedValue() const;
    bool                IsWriteBarrierTriggerableValue();
    void                SetIsDead(const bool isDead = true)   {LOGMEIN("Opnd.h] 231\n"); this->m_isDead = isDead; }
    bool                GetIsDead()   {LOGMEIN("Opnd.h] 232\n"); return this->m_isDead; }
    int64               GetImmediateValue(Func * func);
#if TARGET_32 && !defined(_M_IX86)
    // Helper for 32bits systems without int64 const operand support
    int32               GetImmediateValueAsInt32(Func * func);
#endif
    BailoutConstantValue GetConstValue();
    bool                GetIsJITOptimizedReg() const {LOGMEIN("Opnd.h] 239\n"); return m_isJITOptimizedReg; }
    void                SetIsJITOptimizedReg(bool value) {LOGMEIN("Opnd.h] 240\n"); Assert(!value || !this->IsIndirOpnd()); m_isJITOptimizedReg = value; }

    ValueType           GetValueType() const {LOGMEIN("Opnd.h] 242\n"); return m_valueType; }
    void                SetValueType(const ValueType valueType);
    ValueType           FindProfiledValueType();
    bool                IsScopeObjOpnd(Func * func);
#if DBG_DUMP || defined(ENABLE_IR_VIEWER)
    virtual void        DummyFunction() {LOGMEIN("Opnd.h] 247\n");} // Note needed for the VS debugger to disambiguate the different classes.
    void                DumpValueType();
    static void         DumpValueType(const ValueType valueType);
#endif

    bool                IsValueTypeFixed() const {LOGMEIN("Opnd.h] 252\n"); return m_isValueTypeFixed; }
    void                SetValueTypeFixed() {LOGMEIN("Opnd.h] 253\n"); m_isValueTypeFixed = true; }
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

    bool                CanStoreTemp() const {LOGMEIN("Opnd.h] 276\n"); return canStoreTemp; }
    void                SetCanStoreTemp() {LOGMEIN("Opnd.h] 277\n"); Assert(this->IsSymOpnd() || this->IsIndirOpnd()); canStoreTemp = true; }
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
    {LOGMEIN("Opnd.h] 339\n");
        return m_value;
    }

    void IncrValue(IntConstType by)
    {LOGMEIN("Opnd.h] 344\n");
        SetValue(m_value + by);
    }

    void DecrValue(IntConstType by)
    {LOGMEIN("Opnd.h] 349\n");
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
    {LOGMEIN("Opnd.h] 452\n");
        Assert(this->DbgIsDiagHelperCallOpnd() == isDiagHelperCallOpnd);
        return isDiagHelperCallOpnd;
    }
public:
    JnHelperMethod m_fnHelper;

#if DBG
private:
    virtual bool DbgIsDiagHelperCallOpnd() const {LOGMEIN("Opnd.h] 461\n"); return false; }
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
    {LOGMEIN("Opnd.h] 510\n");
        Assert(this->DbgIsPropertySymOpnd() == this->isPropertySymOpnd);
        return isPropertySymOpnd;
    }
public:
    Sym *                   m_sym;
    uint32                  m_offset;

private:
#if DBG
    virtual bool            DbgIsPropertySymOpnd() const {LOGMEIN("Opnd.h] 520\n"); return false; }
#endif

private:
    ValueType propertyOwnerValueType;

public:
    ValueType GetPropertyOwnerValueType() const
    {LOGMEIN("Opnd.h] 528\n");
        return propertyOwnerValueType;
    }

    void SetPropertyOwnerValueType(const ValueType valueType)
    {LOGMEIN("Opnd.h] 533\n");
        propertyOwnerValueType = valueType;
    }

    RegOpnd *CreatePropertyOwnerOpnd(Func *const func) const;
};

class PropertySymOpnd sealed : public SymOpnd
{
protected:
    PropertySymOpnd() : SymOpnd() {LOGMEIN("Opnd.h] 543\n");}
    PropertySymOpnd(SymOpnd* symOpnd) : SymOpnd(*symOpnd) {LOGMEIN("Opnd.h] 544\n");}

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
    StackSym * GetObjectSym() const {LOGMEIN("Opnd.h] 610\n"); return this->m_sym->AsPropertySym()->m_stackSym; };
    bool HasObjectTypeSym() const {LOGMEIN("Opnd.h] 611\n"); return this->m_sym->AsPropertySym()->HasObjectTypeSym(); };
    StackSym * GetObjectTypeSym() const {LOGMEIN("Opnd.h] 612\n"); return this->m_sym->AsPropertySym()->GetObjectTypeSym(); };
    PropertySym* GetPropertySym() const {LOGMEIN("Opnd.h] 613\n"); return this->m_sym->AsPropertySym(); }

    void TryDisableRuntimePolymorphicCache()
    {LOGMEIN("Opnd.h] 616\n");
        if (this->m_runtimePolymorphicInlineCache && (this->m_polyCacheUtil < PolymorphicInlineCacheUtilizationThreshold))
        {LOGMEIN("Opnd.h] 618\n");
            this->m_runtimePolymorphicInlineCache = nullptr;
        }
    }

    bool HasObjTypeSpecFldInfo() const
    {LOGMEIN("Opnd.h] 624\n");
        return this->objTypeSpecFldInfo != nullptr;
    }

    void SetObjTypeSpecFldInfo(JITObjTypeSpecFldInfo *const objTypeSpecFldInfo)
    {LOGMEIN("Opnd.h] 629\n");
        this->objTypeSpecFldInfo = objTypeSpecFldInfo;

        // The following information may change in a flow-based manner, and an ObjTypeSpecFldInfo is shared among several
        // PropertySymOpnds, so copy the information to the opnd
        if(!objTypeSpecFldInfo)
        {LOGMEIN("Opnd.h] 635\n");
            usesAuxSlot = false;
            slotIndex = 0;
            return;
        }
        usesAuxSlot = objTypeSpecFldInfo->UsesAuxSlot();
        slotIndex = objTypeSpecFldInfo->GetSlotIndex();
    }

    void TryResetObjTypeSpecFldInfo()
    {LOGMEIN("Opnd.h] 645\n");
        if (this->ShouldResetObjTypeSpecFldInfo())
        {LOGMEIN("Opnd.h] 647\n");
            SetObjTypeSpecFldInfo(nullptr);
        }
    }

    bool ShouldResetObjTypeSpecFldInfo()
    {LOGMEIN("Opnd.h] 653\n");
        // If an objTypeSpecFldInfo was created just for the purpose of polymorphic inlining but didn't get used for the same (for some reason or the other), and the polymorphic cache it was created from, wasn't equivalent,
        // we should null out this info on the propertySymOpnd so that assumptions downstream around equivalent object type spec still hold.
        if (HasObjTypeSpecFldInfo() && IsPoly() && (DoesntHaveEquivalence() || !IsLoadedFromProto()))
        {LOGMEIN("Opnd.h] 657\n");
            return true;
        }
        return false;
    }

    JITObjTypeSpecFldInfo* GetObjTypeSpecInfo() const
    {LOGMEIN("Opnd.h] 664\n");
        return this->objTypeSpecFldInfo;
    }

    uint GetObjTypeSpecFldId() const
    {LOGMEIN("Opnd.h] 669\n");
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetObjTypeSpecFldId();
    }

    bool IsMono() const
    {LOGMEIN("Opnd.h] 675\n");
        return HasObjTypeSpecFldInfo() && this->objTypeSpecFldInfo->IsMono();
    }

    bool IsPoly() const
    {LOGMEIN("Opnd.h] 680\n");
        return HasObjTypeSpecFldInfo() && this->objTypeSpecFldInfo->IsPoly();
    }

    bool HasEquivalentTypeSet() const
    {LOGMEIN("Opnd.h] 685\n");
        return HasObjTypeSpecFldInfo() && this->objTypeSpecFldInfo->HasEquivalentTypeSet();
    }

    bool DoesntHaveEquivalence() const
    {LOGMEIN("Opnd.h] 690\n");
        return HasObjTypeSpecFldInfo() && this->objTypeSpecFldInfo->DoesntHaveEquivalence();
    }

    bool UsesAuxSlot() const
    {LOGMEIN("Opnd.h] 695\n");
        return usesAuxSlot && HasObjTypeSpecFldInfo();
    }

    void SetUsesAuxSlot(bool value)
    {LOGMEIN("Opnd.h] 700\n");
        Assert(HasObjTypeSpecFldInfo());
        usesAuxSlot = value;
    }

    bool IsLoadedFromProto() const
    {LOGMEIN("Opnd.h] 706\n");
        return HasObjTypeSpecFldInfo() && this->objTypeSpecFldInfo->IsLoadedFromProto();
    }

    bool UsesAccessor() const
    {LOGMEIN("Opnd.h] 711\n");
        return HasObjTypeSpecFldInfo() && this->objTypeSpecFldInfo->UsesAccessor();
    }

    bool HasFixedValue() const
    {LOGMEIN("Opnd.h] 716\n");
        return HasObjTypeSpecFldInfo() && this->objTypeSpecFldInfo->HasFixedValue();
    }

    bool UsesFixedValue() const
    {LOGMEIN("Opnd.h] 721\n");
        return this->usesFixedValue;
    }

    void SetUsesFixedValue(bool value)
    {LOGMEIN("Opnd.h] 726\n");
        this->usesFixedValue = value;
    }

    bool MustDoMonoCheck() const
    {LOGMEIN("Opnd.h] 731\n");
        return this->monoGuardType != nullptr;
    }

    JITTypeHolder GetMonoGuardType() const
    {LOGMEIN("Opnd.h] 736\n");
        return this->monoGuardType;
    }

    void SetMonoGuardType(JITTypeHolder type)
    {LOGMEIN("Opnd.h] 741\n");
        this->monoGuardType = type;
    }

    bool NeedsMonoCheck() const
    {LOGMEIN("Opnd.h] 746\n");
        Assert(HasObjTypeSpecFldInfo());
        return this->IsBeingAdded() || (this->HasFixedValue() && !this->IsLoadedFromProto());
    }

    bool IsBeingStored() const
    {LOGMEIN("Opnd.h] 752\n");
        return HasObjTypeSpecFldInfo() && this->objTypeSpecFldInfo->IsBeingStored();
    }

    void SetIsBeingStored(bool value)
    {LOGMEIN("Opnd.h] 757\n");
        Assert(HasObjTypeSpecFldInfo());
        this->objTypeSpecFldInfo->SetIsBeingStored(value);
    }

    bool IsBeingAdded() const
    {LOGMEIN("Opnd.h] 763\n");
        return HasObjTypeSpecFldInfo() && this->objTypeSpecFldInfo->IsBeingAdded();
    }

    bool IsRootObjectNonConfigurableField() const
    {LOGMEIN("Opnd.h] 768\n");
        return HasObjTypeSpecFldInfo() && this->objTypeSpecFldInfo->IsRootObjectNonConfigurableField();
    }

    bool IsRootObjectNonConfigurableFieldLoad() const
    {LOGMEIN("Opnd.h] 773\n");
        return HasObjTypeSpecFldInfo() && this->objTypeSpecFldInfo->IsRootObjectNonConfigurableFieldLoad();
    }

    uint16 GetSlotIndex() const
    {LOGMEIN("Opnd.h] 778\n");
        Assert(HasObjTypeSpecFldInfo());
        return slotIndex;
    }

    void SetSlotIndex(uint16 index)
    {LOGMEIN("Opnd.h] 784\n");
        Assert(HasObjTypeSpecFldInfo());
        slotIndex = index;
    }

    uint16 GetCheckedTypeSetIndex() const
    {LOGMEIN("Opnd.h] 790\n");
        Assert(HasEquivalentTypeSet());
        return checkedTypeSetIndex;
    }

    void SetCheckedTypeSetIndex(uint16 index)
    {LOGMEIN("Opnd.h] 796\n");
        Assert(HasEquivalentTypeSet());
        checkedTypeSetIndex = index;
    }

    Js::PropertyId GetPropertyId() const
    {LOGMEIN("Opnd.h] 802\n");
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetPropertyId();
    }

    intptr_t GetProtoObject() const
    {LOGMEIN("Opnd.h] 808\n");
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetProtoObject();
    }

    JITTimeFixedField * GetFixedFunction() const
    {LOGMEIN("Opnd.h] 814\n");
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetFixedFieldIfAvailableAsFixedFunction();
    }

    JITTimeFixedField * GetFixedFunction(uint i) const
    {LOGMEIN("Opnd.h] 820\n");
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetFixedFieldIfAvailableAsFixedFunction(i);
    }

    intptr_t GetFieldValueAsFixedData() const
    {LOGMEIN("Opnd.h] 826\n");
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetFieldValueAsFixedDataIfAvailable();
    }

    intptr_t GetFieldValue(uint i) const
    {LOGMEIN("Opnd.h] 832\n");
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetFieldValue(i);
    }

    JITTimeFixedField * GetFixedFieldInfoArray()
    {LOGMEIN("Opnd.h] 838\n");
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetFixedFieldInfoArray();
    }

    uint16 GetFixedFieldCount()
    {LOGMEIN("Opnd.h] 844\n");
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetFixedFieldCount();
    }

    JITTimeConstructorCache * GetCtorCache() const
    {LOGMEIN("Opnd.h] 850\n");
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetCtorCache();
    }

    intptr_t GetPropertyGuardValueAddr() const
    {LOGMEIN("Opnd.h] 856\n");
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetPropertyGuardValueAddr();
    }

    bool IsTypeCheckSeqCandidate() const
    {LOGMEIN("Opnd.h] 862\n");
        Assert(IsObjTypeSpecCandidate() || !this->isTypeCheckSeqCandidate);
        return this->isTypeCheckSeqCandidate;
    }

    void SetTypeCheckSeqCandidate(bool value)
    {LOGMEIN("Opnd.h] 868\n");
        Assert(IsObjTypeSpecCandidate() || !value);
        this->isTypeCheckSeqCandidate = value;
    }

    bool IsTypeCheckOnly() const
    {LOGMEIN("Opnd.h] 874\n");
        return this->isTypeCheckOnly;
    }

    void SetTypeCheckOnly(bool value)
    {LOGMEIN("Opnd.h] 879\n");
        this->isTypeCheckOnly = value;
    }

    bool IsTypeAvailable() const
    {LOGMEIN("Opnd.h] 884\n");
        return this->typeAvailable;
    }

    void SetTypeAvailable(bool value)
    {LOGMEIN("Opnd.h] 889\n");
        Assert(IsTypeCheckSeqCandidate());
        this->typeAvailable = value;
    }

    bool IsTypeDead() const
    {LOGMEIN("Opnd.h] 895\n");
        return this->typeDead;
    }

    void SetTypeDead(bool value)
    {LOGMEIN("Opnd.h] 900\n");
        Assert(IsTypeCheckSeqCandidate());
        this->typeDead = value;
    }

    void SetTypeDeadIfTypeCheckSeqCandidate(bool value)
    {LOGMEIN("Opnd.h] 906\n");
        if (IsTypeCheckSeqCandidate())
        {LOGMEIN("Opnd.h] 908\n");
            this->typeDead = value;
        }
    }

    bool IsTypeChecked() const
    {LOGMEIN("Opnd.h] 914\n");
        return this->typeChecked;
    }

    void SetTypeChecked(bool value)
    {LOGMEIN("Opnd.h] 919\n");
        Assert(IsTypeCheckSeqCandidate());
        this->typeChecked = value;
    }

    bool IsInitialTypeChecked() const
    {LOGMEIN("Opnd.h] 925\n");
        return this->initialTypeChecked;
    }

    void SetInitialTypeChecked(bool value)
    {LOGMEIN("Opnd.h] 930\n");
        Assert(IsTypeCheckSeqCandidate());
        this->initialTypeChecked = value;
    }

    bool HasTypeMismatch() const
    {LOGMEIN("Opnd.h] 936\n");
        return this->typeMismatch;
    }

    void SetTypeMismatch(bool value)
    {LOGMEIN("Opnd.h] 941\n");
        Assert(IsTypeCheckSeqCandidate());
        this->typeMismatch = value;
    }

    bool IsWriteGuardChecked() const
    {LOGMEIN("Opnd.h] 947\n");
        return this->writeGuardChecked;
    }

    void SetWriteGuardChecked(bool value)
    {LOGMEIN("Opnd.h] 952\n");
        Assert(IsTypeCheckSeqCandidate());
        this->writeGuardChecked = value;
    }

    uint16 GetObjTypeSpecFlags() const
    {LOGMEIN("Opnd.h] 958\n");
        return this->objTypeSpecFlags;
    }

    void ClearObjTypeSpecFlags()
    {LOGMEIN("Opnd.h] 963\n");
        this->objTypeSpecFlags = 0;
    }

    uint16 GetTypeCheckSeqFlags() const
    {LOGMEIN("Opnd.h] 968\n");
        return this->typeCheckSeqFlags;
    }

    void ClearTypeCheckSeqFlags()
    {LOGMEIN("Opnd.h] 973\n");
        this->typeCheckSeqFlags = 0;
    }

    bool MayNeedTypeCheckProtection() const
    {LOGMEIN("Opnd.h] 978\n");
        return IsObjTypeSpecCandidate() && (IsTypeCheckSeqCandidate() || UsesFixedValue());
    }

    bool MayNeedWriteGuardProtection() const
    {LOGMEIN("Opnd.h] 983\n");
        return IsLoadedFromProto() || UsesFixedValue();
    }

    bool IsTypeCheckProtected() const
    {LOGMEIN("Opnd.h] 988\n");
        return IsTypeCheckSeqCandidate() && IsTypeChecked();
    }

    bool NeedsPrimaryTypeCheck() const
    {LOGMEIN("Opnd.h] 993\n");
        // Only indicate that we need a primary type check, i.e. the type isn't yet available but will be needed downstream.
        // Type checks and bailouts may still be needed in other places (e.g. loads from proto, fixed field checks, or
        // property adds), if a primary type check cannot protect them.
        Assert(MayNeedTypeCheckProtection());
        Assert(TypeCheckSeqBitsSetOnlyIfCandidate());
        return IsTypeCheckSeqCandidate() && !IsTypeDead() && !IsTypeChecked() && !HasTypeMismatch();
    }

    bool NeedsLocalTypeCheck() const
    {LOGMEIN("Opnd.h] 1003\n");
        Assert(MayNeedTypeCheckProtection());
        Assert(TypeCheckSeqBitsSetOnlyIfCandidate());
        // Indicate whether this operation needs a type check for its own sake, since the type is dead and no downstream
        // operations require the type to be checked.
        return !PHASE_OFF1(Js::ObjTypeSpecIsolatedFldOpsPhase) &&
            IsTypeCheckSeqCandidate() && IsTypeDead() && !IsTypeCheckOnly() && !IsTypeChecked() && !HasTypeMismatch();
    }

    bool NeedsWriteGuardTypeCheck() const
    {LOGMEIN("Opnd.h] 1013\n");
        Assert(MayNeedTypeCheckProtection());
        Assert(TypeCheckSeqBitsSetOnlyIfCandidate());
        // Type has been checked but property might have been written to since then.
        return !IsTypeCheckOnly() && !NeedsPrimaryTypeCheck() && IsTypeChecked() && !IsWriteGuardChecked();
    }

    bool NeedsLoadFromProtoTypeCheck() const
    {LOGMEIN("Opnd.h] 1021\n");
        Assert(MayNeedTypeCheckProtection());
        Assert(TypeCheckSeqBitsSetOnlyIfCandidate());
        // Proto cache, where type has been checked but property might have been written to since then.
        return !IsTypeCheckOnly() && !NeedsPrimaryTypeCheck() && IsLoadedFromProto() && NeedsWriteGuardTypeCheck();
    }

    bool NeedsAddPropertyTypeCheck() const
    {LOGMEIN("Opnd.h] 1029\n");
        Assert(MayNeedTypeCheckProtection());
        Assert(TypeCheckSeqBitsSetOnlyIfCandidate());
        // A property cannot become read-only without an explicit or implicit call (at least Object.defineProperty is needed), so if this
        // operation is protected by a primary type check upstream, there is no need for an additional local type check.
        return false;
    }

    bool NeedsCheckFixedFieldTypeCheck() const
    {LOGMEIN("Opnd.h] 1038\n");
        Assert(MayNeedTypeCheckProtection());
        Assert(TypeCheckSeqBitsSetOnlyIfCandidate());
        return !IsTypeCheckOnly() && !NeedsPrimaryTypeCheck() && UsesFixedValue() && (!IsTypeChecked() || NeedsWriteGuardTypeCheck());
    }

    bool NeedsTypeCheck() const
    {LOGMEIN("Opnd.h] 1045\n");
        return NeedsPrimaryTypeCheck() || NeedsLocalTypeCheck() ||
            NeedsLoadFromProtoTypeCheck() || NeedsAddPropertyTypeCheck() || NeedsCheckFixedFieldTypeCheck();
    }

    bool NeedsTypeCheckAndBailOut() const
    {LOGMEIN("Opnd.h] 1051\n");
        return NeedsPrimaryTypeCheck() || (PHASE_ON1(Js::ObjTypeSpecIsolatedFldOpsWithBailOutPhase) && NeedsLocalTypeCheck()) || NeedsCheckFixedFieldTypeCheck();
    }

    // Is the instruction involving this operand optimized with a direct slot load or store? In other words, is it guarded
    // by a type check, either as part of the type check sequence, or explicitly on this instruction.
    bool IsObjTypeSpecOptimized() const
    {LOGMEIN("Opnd.h] 1058\n");
        return MayNeedTypeCheckProtection() && (NeedsTypeCheckAndBailOut() || IsTypeCheckProtected());
    }

    // May the instruction involving this operand result in an implicit call?  Note, that because in dead store pass we
    // may choose to remove a type check and fall back on a check against a live cache, instructions that have a primary
    // type check may still end up with implicit call bailout.  However, if we are type check protected we will never
    // fall back on live cache.  Similarly, for fixed method checks.
    bool MayHaveImplicitCall() const
    {LOGMEIN("Opnd.h] 1067\n");
        return !IsRootObjectNonConfigurableFieldLoad() && !UsesFixedValue() && (!IsTypeCheckSeqCandidate() || !IsTypeCheckProtected());
    }

    // Is the instruction involving this operand part of a type check sequence? This is different from IsObjTypeSpecOptimized
    // in that an instruction such as CheckFixedFld may require a type check even if it is not part of a type check
    // sequence. In this case IsObjTypeSpecOptimized() == true, but IsTypeCheckSeqParticipant() == false.
    bool IsTypeCheckSeqParticipant() const
    {LOGMEIN("Opnd.h] 1075\n");
        Assert(IsTypeCheckSeqCandidate());
        return NeedsPrimaryTypeCheck() || IsTypeCheckProtected();
    }

    bool HasFinalType() const;

    JITTypeHolder GetFinalType() const
    {LOGMEIN("Opnd.h] 1083\n");
        return this->finalType;
    }

    void SetFinalType(JITTypeHolder type)
    {LOGMEIN("Opnd.h] 1088\n");
        Assert(type != nullptr);
        this->finalType = type;
    }

    void ClearFinalType()
    {LOGMEIN("Opnd.h] 1094\n");
        this->finalType = JITTypeHolder(nullptr);
    }

    BVSparse<JitArenaAllocator>* GetGuardedPropOps()
    {LOGMEIN("Opnd.h] 1099\n");
        return this->guardedPropOps;
    }

    void EnsureGuardedPropOps(JitArenaAllocator* allocator)
    {LOGMEIN("Opnd.h] 1104\n");
        if (this->guardedPropOps == nullptr)
        {LOGMEIN("Opnd.h] 1106\n");
            this->guardedPropOps = JitAnew(allocator, BVSparse<JitArenaAllocator>, allocator);
        }
    }

    void SetGuardedPropOp(uint propOpId)
    {LOGMEIN("Opnd.h] 1112\n");
        Assert(this->guardedPropOps != nullptr);
        this->guardedPropOps->Set(propOpId);
    }

    void AddGuardedPropOps(const BVSparse<JitArenaAllocator>* propOps)
    {LOGMEIN("Opnd.h] 1118\n");
        Assert(this->guardedPropOps != nullptr);
        this->guardedPropOps->Or(propOps);
    }

    BVSparse<JitArenaAllocator>* GetWriteGuards()
    {LOGMEIN("Opnd.h] 1124\n");
        return this->writeGuards;
    }

    void SetWriteGuards(BVSparse<JitArenaAllocator>* value)
    {LOGMEIN("Opnd.h] 1129\n");
        Assert(this->writeGuards == nullptr);
        this->writeGuards = value;
    }

    void ClearWriteGuards()
    {LOGMEIN("Opnd.h] 1135\n");
        this->writeGuards = nullptr;
    }

#if DBG
    bool TypeCheckSeqBitsSetOnlyIfCandidate() const
    {LOGMEIN("Opnd.h] 1141\n");
        return IsTypeCheckSeqCandidate() || (!IsTypeAvailable() && !IsTypeChecked() && !IsWriteGuardChecked() && !IsTypeDead());
    }
#endif

    bool IsObjTypeSpecCandidate() const
    {LOGMEIN("Opnd.h] 1147\n");
        return HasObjTypeSpecFldInfo();
    }

    bool IsMonoObjTypeSpecCandidate() const
    {LOGMEIN("Opnd.h] 1152\n");
        return HasObjTypeSpecFldInfo() && this->objTypeSpecFldInfo->IsMonoObjTypeSpecCandidate();
    }

    bool IsPolyObjTypeSpecCandidate() const
    {LOGMEIN("Opnd.h] 1157\n");
        return HasObjTypeSpecFldInfo() && this->objTypeSpecFldInfo->IsPolyObjTypeSpecCandidate();
    }

    Js::TypeId GetTypeId() const
    {LOGMEIN("Opnd.h] 1162\n");
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetTypeId();
    }

    Js::TypeId GetTypeId(uint i) const
    {LOGMEIN("Opnd.h] 1168\n");
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetTypeId(i);
    }

    JITTypeHolder GetType() const
    {LOGMEIN("Opnd.h] 1174\n");
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetType();
    }

    JITTypeHolder GetType(uint i) const
    {LOGMEIN("Opnd.h] 1180\n");
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetType(i);
    }

    bool HasInitialType() const
    {LOGMEIN("Opnd.h] 1186\n");
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->HasInitialType();
    }

    JITTypeHolder GetInitialType() const
    {LOGMEIN("Opnd.h] 1192\n");
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetInitialType();
    }

    Js::EquivalentTypeSet * GetEquivalentTypeSet() const
    {LOGMEIN("Opnd.h] 1198\n");
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetEquivalentTypeSet();
    }

    JITTypeHolder GetFirstEquivalentType() const
    {LOGMEIN("Opnd.h] 1204\n");
        Assert(HasObjTypeSpecFldInfo());
        return this->objTypeSpecFldInfo->GetFirstEquivalentType();
    }

    bool IsObjectHeaderInlined() const;
    void UpdateSlotForFinalType();
    bool ChangesObjectLayout() const;

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    const char16* GetCacheLayoutString() const
    {LOGMEIN("Opnd.h] 1215\n");
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
    {LOGMEIN("Opnd.h] 1243\n");
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
    void FreezeSymValue() {LOGMEIN("Opnd.h] 1265\n"); m_symValueFrozen = true; }
    bool IsSymValueFrozen() const {LOGMEIN("Opnd.h] 1266\n"); return m_symValueFrozen; }

    virtual bool DbgIsArrayRegOpnd() const {LOGMEIN("Opnd.h] 1268\n"); return false; }
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
    virtual bool DbgIsArrayRegOpnd() const {LOGMEIN("Opnd.h] 1317\n"); return true; }
#endif
    StackSym *HeadSegmentSym() const
    {LOGMEIN("Opnd.h] 1320\n");
        return headSegmentSym;
    }

    void RemoveHeadSegmentSym()
    {LOGMEIN("Opnd.h] 1325\n");
        headSegmentSym = nullptr;
    }

    StackSym *HeadSegmentLengthSym() const
    {LOGMEIN("Opnd.h] 1330\n");
        return headSegmentLengthSym;
    }

    void RemoveHeadSegmentLengthSym()
    {LOGMEIN("Opnd.h] 1335\n");
        headSegmentLengthSym = nullptr;
    }

    StackSym *LengthSym() const
    {LOGMEIN("Opnd.h] 1340\n");
        // For typed arrays, the head segment length is the same as the array length
        Assert(!(m_valueType.IsLikelyTypedArray() && !m_valueType.IsOptimizedTypedArray()));
        return m_valueType.IsLikelyTypedArray() ? HeadSegmentLengthSym() : lengthSym;
    }

    void RemoveLengthSym()
    {LOGMEIN("Opnd.h] 1347\n");
        Assert(m_valueType.IsArray());
        lengthSym = nullptr;
    }

    bool EliminatedLowerBoundCheck() const
    {LOGMEIN("Opnd.h] 1353\n");
        return eliminatedLowerBoundCheck;
    }

    bool EliminatedUpperBoundCheck() const
    {LOGMEIN("Opnd.h] 1358\n");
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

    bool                    IsDynamic() const {LOGMEIN("Opnd.h] 1403\n"); return addrOpndKind > AddrOpndKindConstantVar; }
    bool                    IsVar() const {LOGMEIN("Opnd.h] 1404\n"); return addrOpndKind == AddrOpndKindDynamicVar || addrOpndKind == AddrOpndKindConstantVar; }
    void                    SetEncodedValue(Js::Var address, AddrOpndKind addrOpndKind);
    AddrOpndKind            GetAddrOpndKind() const {LOGMEIN("Opnd.h] 1406\n"); return addrOpndKind; }
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
    {LOGMEIN("Opnd.h] 1589\n");
    }

    AutoReuseOpnd(Opnd *const opnd, Func *const func, const bool autoDelete = true) : opnd(nullptr)
    {
        Initialize(opnd, func, autoDelete);
    }

    void Initialize(Opnd *const opnd, Func *const func, const bool autoDelete = true)
    {LOGMEIN("Opnd.h] 1598\n");
        Assert(!this->opnd);
        Assert(func);

        if(!opnd)
        {LOGMEIN("Opnd.h] 1603\n");
            // Simulate the default constructor
            wasInUse = true;
            return;
        }

        this->opnd = opnd;
        wasInUse = opnd->IsInUse();
        if(wasInUse)
        {LOGMEIN("Opnd.h] 1612\n");
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
    {LOGMEIN("Opnd.h] 1627\n");
        if(wasInUse)
        {LOGMEIN("Opnd.h] 1629\n");
            return;
        }
        if(autoDelete)
        {LOGMEIN("Opnd.h] 1633\n");
            opnd->Free(func);
        }
        else
        {
            opnd->UnUse();
        }
    }

    PREVENT_COPY(AutoReuseOpnd)
};

} // namespace IR
