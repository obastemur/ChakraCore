//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#define NativeCodeDataNew(alloc, T, ...) AllocatorNew(NativeCodeData::AllocatorT<T>, alloc, T, __VA_ARGS__)
#define NativeCodeDataNewZ(alloc, T, ...) AllocatorNewZ(NativeCodeData::AllocatorT<T>, alloc, T, __VA_ARGS__)
#define NativeCodeDataNewArray(alloc, T, count) AllocatorNewArray(NativeCodeData::AllocatorT<NativeCodeData::Array<T>>, alloc, T, count)
#define NativeCodeDataNewArrayZ(alloc, T, count) AllocatorNewArrayZ(NativeCodeData::AllocatorT<NativeCodeData::Array<T>>, alloc, T, count)
#define NativeCodeDataNewPlusZ(size, alloc, T, ...) AllocatorNewPlusZ(NativeCodeData::AllocatorT<T>, alloc, size, T, __VA_ARGS__)

#define NativeCodeDataNewNoFixup(alloc, T, ...) AllocatorNew(NativeCodeData::AllocatorNoFixup<T>, alloc, T, __VA_ARGS__)
#define NativeCodeDataNewZNoFixup(alloc, T, ...) AllocatorNewZ(NativeCodeData::AllocatorNoFixup<T>, alloc, T, __VA_ARGS__)
#define NativeCodeDataNewArrayNoFixup(alloc, T, count) AllocatorNewArray(NativeCodeData::AllocatorNoFixup<NativeCodeData::Array<T>>, alloc, T, count)
#define NativeCodeDataNewArrayZNoFixup(alloc, T, count) AllocatorNewArrayZ(NativeCodeData::AllocatorNoFixup<NativeCodeData::Array<T>>, alloc, T, count)
#define NativeCodeDataNewPlusZNoFixup(size, alloc, T, ...) AllocatorNewPlusZ(NativeCodeData::AllocatorNoFixup<T>, alloc, size, T, __VA_ARGS__)

#define FixupNativeDataPointer(field, chunkList) NativeCodeData::AddFixupEntry(this->field, &this->field, this, chunkList)

class NativeCodeData
{

public:

    struct DataChunk
    {
        unsigned int len;
        unsigned int allocIndex;
        unsigned int offset; // offset to the aggregated buffer
#if DBG
        const char* dataType;
#endif

        // todo: use union?
        void(*fixupFunc)(void* _this, NativeCodeData::DataChunk*);
        NativeDataFixupEntry *fixupList;

        DataChunk * next;
        char data[0];
    };

    struct DataChunkNoFixup
    {
        DataChunkNoFixup * next;
        char data[0];
    };

    static DataChunk* GetDataChunk(void* data)
    {TRACE_IT(13762);
        Assert(JITManager::GetJITManager()->IsJITServer());
        return (NativeCodeData::DataChunk*)((char*)data - offsetof(NativeCodeData::DataChunk, data));
    }

    static char16* GetDataDescription(void* data, JitArenaAllocator * alloc);

    static unsigned int GetDataTotalOffset(void* data)
    {TRACE_IT(13763);
        Assert(JITManager::GetJITManager()->IsJITServer());
        return GetDataChunk(data)->offset;
    }

    NativeCodeData(DataChunk * chunkList);
    union
    {
        DataChunk * chunkList;
        DataChunkNoFixup * noFixupChunkList;
    };

#ifdef PERF_COUNTERS
    size_t size;
#endif
public:

    static void VerifyExistFixupEntry(void* targetAddr, void* addrToFixup, void* startAddress);
    static void AddFixupEntry(void* targetAddr, void* addrToFixup, void* startAddress, DataChunk * chunkList);
    static void AddFixupEntry(void* targetAddr, void* targetStartAddr, void* addrToFixup, void* startAddress, DataChunk * chunkList);
    static void AddFixupEntryForPointerArray(void* startAddress, DataChunk * chunkList);
    template<class DataChunkT>
    static void DeleteChunkList(DataChunkT * chunkList);
public:
    class Allocator
    {
    public:
        static const bool FakeZeroLengthArray = false;

        Allocator();
        ~Allocator();

        char * Alloc(DECLSPEC_GUARD_OVERFLOW size_t requestedBytes);
        char * AllocZero(DECLSPEC_GUARD_OVERFLOW size_t requestedBytes);
        char * AllocLeaf(DECLSPEC_GUARD_OVERFLOW size_t requestedBytes);

        NativeCodeData * Finalize();
        void Free(void * buffer, size_t byteSize);

        union
        {
            DataChunk * chunkList;
            DataChunkNoFixup* noFixupChunkList;
        };
        DataChunk * lastChunkList; // used to maintain the allocation order in the list
        unsigned int totalSize;
        unsigned int allocCount;

#ifdef TRACK_ALLOC
        // Doesn't support tracking information, dummy implementation
        Allocator * TrackAllocInfo(TrackAllocData const& data) {TRACE_IT(13764); return this; }
        void ClearTrackAllocInfo(TrackAllocData* data = NULL) {TRACE_IT(13765);}
#endif
    protected:
        bool isOOPJIT;
    private:
#if DBG
        bool finalized;
#endif
#ifdef PERF_COUNTERS
        size_t size;
#endif
    };

    template<typename T>
    class Array
    {
    public:
        void Fixup(NativeCodeData::DataChunk* chunkList)
        {TRACE_IT(13766);
            int count = NativeCodeData::GetDataChunk(this)->len / sizeof(T);
            while (count-- > 0)
            {TRACE_IT(13767);
                (((T*)this) + count)->Fixup(chunkList);
            }
        }
    };

    template<typename T>
    class AllocatorNoFixup : public Allocator
    {
    public:
        char * Alloc(size_t requestedBytes)
        {TRACE_IT(13768);
            char* dataBlock = __super::Alloc(requestedBytes);
#if DBG
            if (JITManager::GetJITManager()->IsJITServer())
            {TRACE_IT(13769);
                DataChunk* chunk = NativeCodeData::GetDataChunk(dataBlock);
                chunk->dataType = typeid(T).name();
                if (PHASE_TRACE1(Js::NativeCodeDataPhase))
                {TRACE_IT(13770);
                    Output::Print(_u("NativeCodeData AllocNoFix: chunk: %p, data: %p, index: %d, len: %x, totalOffset: %x, type: %S\n"),
                        chunk, (void*)dataBlock, chunk->allocIndex, chunk->len, chunk->offset, chunk->dataType);
                }
            }
#endif

            return dataBlock;
        }
        char * AllocZero(size_t requestedBytes)
        {TRACE_IT(13771);
            char* dataBlock = __super::AllocZero(requestedBytes);

#if DBG
            if (JITManager::GetJITManager()->IsJITServer())
            {TRACE_IT(13772);
                DataChunk* chunk = NativeCodeData::GetDataChunk(dataBlock);
                chunk->dataType = typeid(T).name();
                if (PHASE_TRACE1(Js::NativeCodeDataPhase))
                {TRACE_IT(13773);
                    Output::Print(_u("NativeCodeData AllocNoFix: chunk: %p, data: %p, index: %d, len: %x, totalOffset: %x, type: %S\n"),
                        chunk, (void*)dataBlock, chunk->allocIndex, chunk->len, chunk->offset, chunk->dataType);
                }
            }
#endif

            return dataBlock;
        }
        char * AllocLeaf(size_t requestedBytes)
        {TRACE_IT(13774);
            return Alloc(requestedBytes);
        }
    };

    template<typename T>
    class AllocatorT : public Allocator
    {
        char* AddFixup(char* dataBlock)
        {TRACE_IT(13775);
            if (isOOPJIT)
            {TRACE_IT(13776);
                DataChunk* chunk = NativeCodeData::GetDataChunk(dataBlock);
                chunk->fixupFunc = &Fixup;
#if DBG
                chunk->dataType = typeid(T).name();
                if (PHASE_TRACE1(Js::NativeCodeDataPhase))
                {TRACE_IT(13777);
                    Output::Print(_u("NativeCodeData Alloc: chunk: %p, data: %p, index: %d, len: %x, totalOffset: %x, type: %S\n"),
                        chunk, (void*)dataBlock, chunk->allocIndex, chunk->len, chunk->offset, chunk->dataType);
                }
#endif
            }
            return dataBlock;
        }

    public:
        char * Alloc(size_t requestedBytes)
        {TRACE_IT(13778);
            return AddFixup(__super::Alloc(requestedBytes));
        }
        char * AllocZero(size_t requestedBytes)
        {TRACE_IT(13779);
            return AddFixup(__super::AllocZero(requestedBytes));
        }

        static void Fixup(void* pThis, NativeCodeData::DataChunk* chunkList)
        {TRACE_IT(13780);
            ((T*)pThis)->Fixup(chunkList);
        }
    };

    ~NativeCodeData();
};

enum DataDesc
{
    DataDesc_None,
    DataDesc_InlineeFrameRecord_ArgOffsets,
    DataDesc_InlineeFrameRecord_Constants,
    DataDesc_BailoutInfo_CotalOutParamCount,
    DataDesc_ArgOutOffsetInfo_StartCallOutParamCounts,
    DataDesc_ArgOutOffsetInfo_StartCallArgRestoreAdjustCounts,
    DataDesc_LowererMD_LoadFloatValue_Float,
    DataDesc_LowererMD_LoadFloatValue_Double,
    DataDesc_LowererMD_EmitLoadFloatCommon_Double,
    DataDesc_LowererMD_Simd128LoadConst,
};

template<DataDesc desc = DataDesc_None>
struct IntType
{
    int data;
};

template<DataDesc desc = DataDesc_None>
struct UIntType
{
    uint data;
};

template<DataDesc desc = DataDesc_None>
struct FloatType
{
    FloatType(float val) :data(val) {TRACE_IT(13781);}
    float data;
};

template<DataDesc desc = DataDesc_None>
struct DoubleType
{
    DoubleType() {TRACE_IT(13782);}
    DoubleType(double val) :data(val) {TRACE_IT(13783);}
    double data;
};

template<DataDesc desc = DataDesc_None>
struct SIMDType
{
    SIMDType() {TRACE_IT(13784);}
    SIMDType(AsmJsSIMDValue val) :data(val) {TRACE_IT(13785);}
    AsmJsSIMDValue data;
};

template<DataDesc desc = DataDesc_None>
struct VarType
{
    Js::Var data;
    void Fixup(NativeCodeData::DataChunk* chunkList)
    {
        AssertMsg(false, "Please specialize Fixup method for this Var type or use no-fixup allocator");
    }
};

template<>
inline void VarType<DataDesc_InlineeFrameRecord_Constants>::Fixup(NativeCodeData::DataChunk* chunkList)
{
    AssertMsg(false, "InlineeFrameRecord::constants contains Var from main process, should not fixup");
}

struct GlobalBailOutRecordDataTable;
template<>
inline void NativeCodeData::Array<GlobalBailOutRecordDataTable *>::Fixup(NativeCodeData::DataChunk* chunkList)
{TRACE_IT(13786);
    NativeCodeData::AddFixupEntryForPointerArray(this, chunkList);
}
