//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#pragma once

struct BailoutConstantValue {
public:
    void InitIntConstValue(int32 value) {LOGMEIN("InlineeFrameInfo.h] 9\n"); this->type = TyInt32; this->u.intConst.value = (IntConstType)value; };
    void InitIntConstValue(IntConstType value, IRType type) {LOGMEIN("InlineeFrameInfo.h] 10\n");
        Assert(IRType_IsSignedInt(type));
        this->type = type; this->u.intConst.value = value;
    };
    void InitVarConstValue(Js::Var value);
    void InitFloatConstValue(FloatConstType value) {LOGMEIN("InlineeFrameInfo.h] 15\n"); this->type = TyFloat64; this->u.floatConst.value = value; }
    bool IsEqual(const BailoutConstantValue & bailoutConstValue);
public:
    IRType type;
    union
    {
        struct sIntConst
        {
            IntConstType value;
        } intConst;
        struct sVarConst
        {
            Js::Var value;
        } varConst;
        struct sFloatConst
        {
            FloatConstType value;
        } floatConst;
    } u;
    Js::Var ToVar(Func* func) const;
};

enum InlineeFrameInfoValueType
{
    InlineeFrameInfoValueType_None,
    InlineeFrameInfoValueType_Sym,
    InlineeFrameInfoValueType_Const
};

struct InlineFrameInfoValue
{
    InlineeFrameInfoValueType type;
    union
    {
        StackSym* sym;
        BailoutConstantValue constValue;
    };

    bool IsConst() {LOGMEIN("InlineeFrameInfo.h] 53\n"); return this->type == InlineeFrameInfoValueType_Const; }

    InlineFrameInfoValue() : type(InlineeFrameInfoValueType_None), sym(nullptr) {LOGMEIN("InlineeFrameInfo.h] 55\n");}
    InlineFrameInfoValue(StackSym* sym) : type(InlineeFrameInfoValueType_Sym), sym(sym) {LOGMEIN("InlineeFrameInfo.h] 56\n");}
    InlineFrameInfoValue(BailoutConstantValue value) : type(InlineeFrameInfoValueType_Const), constValue(value) {LOGMEIN("InlineeFrameInfo.h] 57\n");}

};
struct InlineeFrameInfo;

struct InlineeFrameRecord
{
    int functionOffset;
    int inlineDepth;
    uint inlineeStartOffset;
    int* argOffsets;
    Js::Var * constants;
    InlineeFrameRecord* parent;

    uint argCount;
    BVUnit floatArgs;
    BVUnit losslessInt32Args;

    template<class Fnc>
    void MapOffsets(Fnc callback)
    {LOGMEIN("InlineeFrameInfo.h] 77\n");
        callback(functionOffset);

        for (uint i = 0; i < argCount; i++)
        {LOGMEIN("InlineeFrameInfo.h] 81\n");
            callback(argOffsets[i]);
        }
    }

#if DBG_DUMP
    uint constantCount;
    Js::FunctionBody* functionBody;
    InlineeFrameInfo* frameInfo;
#endif

    // Fields are zero initialized any way
    InlineeFrameRecord(uint argCount, Js::FunctionBody* functionBody, InlineeFrameInfo* frameInfo) : argCount(argCount)
#if DBG_DUMP
        , functionBody(functionBody)
        , frameInfo(frameInfo)
#endif
    {}

    static InlineeFrameRecord* New(NativeCodeData::Allocator* alloc, uint argCount, uint constantCount, intptr_t functionBodyAddr, InlineeFrameInfo* frameInfo)
    {LOGMEIN("InlineeFrameInfo.h] 101\n");
        InlineeFrameRecord* record = NativeCodeDataNewZ(alloc, InlineeFrameRecord, argCount, (Js::FunctionBody*)functionBodyAddr, frameInfo);
        record->argOffsets = (int*)NativeCodeDataNewArrayNoFixup(alloc, IntType<DataDesc_InlineeFrameRecord_ArgOffsets>, argCount);
        record->constants = (Js::Var*)NativeCodeDataNewArrayNoFixup(alloc, VarType<DataDesc_InlineeFrameRecord_Constants>, constantCount);
        DebugOnly(record->constantCount = constantCount);
        return record;
    }

    void PopulateParent(Func* func);
    void RestoreFrames(Js::FunctionBody* functionBody, InlinedFrameLayout* outerMostInlinee, Js::JavascriptCallStackLayout* callstack);
    void Finalize(Func* inlinee, uint currentOffset);
#if DBG_DUMP
    void Dump() const;
    void DumpOffset(int offset) const;
#endif

    void Fixup(NativeCodeData::DataChunk* chunkList)
    {
        FixupNativeDataPointer(argOffsets, chunkList);
        FixupNativeDataPointer(constants, chunkList);
        FixupNativeDataPointer(parent, chunkList);
    }

private:
    void Restore(Js::FunctionBody* functionBody, InlinedFrameLayout *outerMostFrame, Js::JavascriptCallStackLayout * layout) const;
    Js::Var Restore(int offset, bool isFloat64, bool isInt32, Js::JavascriptCallStackLayout * layout, Js::FunctionBody* functionBody) const;
    InlineeFrameRecord* Reverse();
};

struct NativeOffsetInlineeFramePair
{
    uint32 offset;
    InlineeFrameRecord* record;
};

struct NativeOffsetInlineeFrameRecordOffset
{
    uint32 offset;
    uint32 recordOffset;
    static uint32 InvalidRecordOffset;
};

struct InlineeFrameInfo
{
    typedef JsUtil::List<InlineFrameInfoValue, JitArenaAllocator, /*isLeaf*/ false> ArgList;
    InlineFrameInfoValue function;
    ArgList* arguments;
    InlineeFrameRecord* record;
    BVSparse<JitArenaAllocator>* floatSyms;
    BVSparse<JitArenaAllocator>* intSyms;

    BVSparse<JitArenaAllocator>* simd128F4Syms;
    BVSparse<JitArenaAllocator>* simd128I4Syms;
    BVSparse<JitArenaAllocator>* simd128I8Syms;
    BVSparse<JitArenaAllocator>* simd128I16Syms;
    BVSparse<JitArenaAllocator>* simd128U4Syms;
    BVSparse<JitArenaAllocator>* simd128U8Syms;
    BVSparse<JitArenaAllocator>* simd128U16Syms;
    bool isRecorded;

    static InlineeFrameInfo* New(JitArenaAllocator* alloc)
    {LOGMEIN("InlineeFrameInfo.h] 162\n");
        InlineeFrameInfo* frameInfo = JitAnewStructZ(alloc, InlineeFrameInfo);
        frameInfo->arguments = JitAnew(alloc, ArgList, alloc);
        return frameInfo;
    }

    template<class Fn>
    void IterateSyms(Fn callback, bool inReverse = false)
    {LOGMEIN("InlineeFrameInfo.h] 170\n");
        auto iterator = [=](uint index, InlineFrameInfoValue& value)
        {LOGMEIN("InlineeFrameInfo.h] 172\n");
            if (value.type == InlineeFrameInfoValueType_Sym)
            {LOGMEIN("InlineeFrameInfo.h] 174\n");
                callback(value.sym);
            }
            Assert(value.type != InlineeFrameInfoValueType_None);
        };

        if (inReverse && function.type == InlineeFrameInfoValueType_Sym)
        {LOGMEIN("InlineeFrameInfo.h] 181\n");
            callback(function.sym);
        }

        if (inReverse)
        {LOGMEIN("InlineeFrameInfo.h] 186\n");
            arguments->ReverseMap(iterator);
        }
        else
        {
            arguments->Map(iterator);
        }
        Assert(function.type != InlineeFrameInfoValueType_None);
        if (!inReverse && function.type == InlineeFrameInfoValueType_Sym)
        {LOGMEIN("InlineeFrameInfo.h] 195\n");
            callback(function.sym);
        }
    }
    void AllocateRecord(Func* func, intptr_t functionBodyAddr);

#if DBG_DUMP
    void Dump() const;
#endif

};


