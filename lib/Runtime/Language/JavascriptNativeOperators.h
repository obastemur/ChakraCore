//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
namespace Js
{
#if ENABLE_NATIVE_CODEGEN
    template <typename T>
    class BranchDictionaryWrapper
    {
    public:
        class DictAllocator :public NativeCodeData::Allocator
        {
        public:
            char * Alloc(size_t requestedBytes)
            {TRACE_IT(49927);
                char* dataBlock = __super::Alloc(requestedBytes);
#if DBG
                if (JITManager::GetJITManager()->IsJITServer())
                {TRACE_IT(49928);
                    NativeCodeData::DataChunk* chunk = NativeCodeData::GetDataChunk(dataBlock);
                    chunk->dataType = "BranchDictionary::Bucket";
                    if (PHASE_TRACE1(Js::NativeCodeDataPhase))
                    {TRACE_IT(49929);
                        Output::Print(_u("NativeCodeData BranchDictionary::Bucket: chunk: %p, data: %p, index: %d, len: %x, totalOffset: %x, type: %S\n"),
                            chunk, (void*)dataBlock, chunk->allocIndex, chunk->len, chunk->offset, chunk->dataType);
                    }
                }
#endif
                return dataBlock;
            }

            char * AllocZero(size_t requestedBytes)
            {TRACE_IT(49930);
                char* dataBlock = __super::AllocZero(requestedBytes);
#if DBG
                if (JITManager::GetJITManager()->IsJITServer())
                {TRACE_IT(49931);
                    NativeCodeData::DataChunk* chunk = NativeCodeData::GetDataChunk(dataBlock);
                    chunk->dataType = "BranchDictionary::Entries";
                    if (PHASE_TRACE1(Js::NativeCodeDataPhase))
                    {TRACE_IT(49932);
                        Output::Print(_u("NativeCodeData BranchDictionary::Entries: chunk: %p, data: %p, index: %d, len: %x, totalOffset: %x, type: %S\n"),
                            chunk, (void*)dataBlock, chunk->allocIndex, chunk->len, chunk->offset, chunk->dataType);
                    }
                }
#endif
                return dataBlock;
            }
        };

        template <class TKey, class TValue>
        class SimpleDictionaryEntryWithFixUp : public JsUtil::SimpleDictionaryEntry<TKey, TValue>
        {
        public:
            void FixupWithRemoteKey(void* remoteKey)
            {TRACE_IT(49933);
                this->key = (TKey)remoteKey;
            }
        };

        typedef JsUtil::BaseDictionary<T, void*, DictAllocator, PowerOf2SizePolicy, DefaultComparer, SimpleDictionaryEntryWithFixUp> BranchBaseDictionary;

        class BranchDictionary :public BranchBaseDictionary
        {
        public:
            BranchDictionary(DictAllocator* allocator, uint dictionarySize)
                : BranchBaseDictionary(allocator, dictionarySize)
            {TRACE_IT(49934);
            }
            void Fixup(NativeCodeData::DataChunk* chunkList, void** remoteKeys)
            {TRACE_IT(49935);
                for (int i = 0; i < this->Count(); i++)
                {TRACE_IT(49936);
                    this->entries[i].FixupWithRemoteKey(remoteKeys[i]);
                }
                FixupNativeDataPointer(buckets, chunkList);
                FixupNativeDataPointer(entries, chunkList);
            }
        };

        BranchDictionaryWrapper(NativeCodeData::Allocator * allocator, uint dictionarySize, ArenaAllocator* remoteKeyAlloc) :
            defaultTarget(nullptr), dictionary((DictAllocator*)allocator, dictionarySize)
        {TRACE_IT(49937);
            if (remoteKeyAlloc)
            {TRACE_IT(49938);
                remoteKeys = AnewArrayZ(remoteKeyAlloc, void*, dictionarySize);
            }
            else
            {TRACE_IT(49939);
                Assert(!JITManager::GetJITManager()->IsJITServer());
                remoteKeys = nullptr;
            }
        }

        BranchDictionary dictionary;
        void* defaultTarget;
        void** remoteKeys;

        static BranchDictionaryWrapper* New(NativeCodeData::Allocator * allocator, uint dictionarySize, ArenaAllocator* remoteKeyAlloc)
        {TRACE_IT(49940);
            return NativeCodeDataNew(allocator, BranchDictionaryWrapper, allocator, dictionarySize, remoteKeyAlloc);
        }

        void AddEntry(uint32 offset, T key, void* remoteVar)
        {TRACE_IT(49941);
            int index = dictionary.AddNew(key, (void**)offset);
            if (JITManager::GetJITManager()->IsJITServer())
            {TRACE_IT(49942);
                Assert(remoteKeys);
                remoteKeys[index] = remoteVar;
            }
        }

        void Fixup(NativeCodeData::DataChunk* chunkList)
        {TRACE_IT(49943);
            if (JITManager::GetJITManager()->IsJITServer())
            {TRACE_IT(49944);
                dictionary.Fixup(chunkList, remoteKeys);
            }
        }
    };

    class JavascriptNativeOperators
    {
    public:
        static void * Op_SwitchStringLookUp(JavascriptString* str, Js::BranchDictionaryWrapper<Js::JavascriptString*>* stringDictionary, uintptr_t funcStart, uintptr_t funcEnd);
    };
#endif
};
