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
            {LOGMEIN("JavascriptNativeOperators.h] 15\n");
                char* dataBlock = __super::Alloc(requestedBytes);
#if DBG
                if (JITManager::GetJITManager()->IsJITServer())
                {LOGMEIN("JavascriptNativeOperators.h] 19\n");
                    NativeCodeData::DataChunk* chunk = NativeCodeData::GetDataChunk(dataBlock);
                    chunk->dataType = "BranchDictionary::Bucket";
                    if (PHASE_TRACE1(Js::NativeCodeDataPhase))
                    {LOGMEIN("JavascriptNativeOperators.h] 23\n");
                        Output::Print(_u("NativeCodeData BranchDictionary::Bucket: chunk: %p, data: %p, index: %d, len: %x, totalOffset: %x, type: %S\n"),
                            chunk, (void*)dataBlock, chunk->allocIndex, chunk->len, chunk->offset, chunk->dataType);
                    }
                }
#endif
                return dataBlock;
            }

            char * AllocZero(size_t requestedBytes)
            {LOGMEIN("JavascriptNativeOperators.h] 33\n");
                char* dataBlock = __super::AllocZero(requestedBytes);
#if DBG
                if (JITManager::GetJITManager()->IsJITServer())
                {LOGMEIN("JavascriptNativeOperators.h] 37\n");
                    NativeCodeData::DataChunk* chunk = NativeCodeData::GetDataChunk(dataBlock);
                    chunk->dataType = "BranchDictionary::Entries";
                    if (PHASE_TRACE1(Js::NativeCodeDataPhase))
                    {LOGMEIN("JavascriptNativeOperators.h] 41\n");
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
            {LOGMEIN("JavascriptNativeOperators.h] 56\n");
                this->key = (TKey)remoteKey;
            }
        };

        typedef JsUtil::BaseDictionary<T, void*, DictAllocator, PowerOf2SizePolicy, DefaultComparer, SimpleDictionaryEntryWithFixUp> BranchBaseDictionary;

        class BranchDictionary :public BranchBaseDictionary
        {
        public:
            BranchDictionary(DictAllocator* allocator, uint dictionarySize)
                : BranchBaseDictionary(allocator, dictionarySize)
            {LOGMEIN("JavascriptNativeOperators.h] 68\n");
            }
            void Fixup(NativeCodeData::DataChunk* chunkList, void** remoteKeys)
            {LOGMEIN("JavascriptNativeOperators.h] 71\n");
                for (int i = 0; i < this->Count(); i++)
                {LOGMEIN("JavascriptNativeOperators.h] 73\n");
                    this->entries[i].FixupWithRemoteKey(remoteKeys[i]);
                }
                FixupNativeDataPointer(buckets, chunkList);
                FixupNativeDataPointer(entries, chunkList);
            }
        };

        BranchDictionaryWrapper(NativeCodeData::Allocator * allocator, uint dictionarySize, ArenaAllocator* remoteKeyAlloc) :
            defaultTarget(nullptr), dictionary((DictAllocator*)allocator, dictionarySize)
        {LOGMEIN("JavascriptNativeOperators.h] 83\n");
            if (remoteKeyAlloc)
            {LOGMEIN("JavascriptNativeOperators.h] 85\n");
                remoteKeys = AnewArrayZ(remoteKeyAlloc, void*, dictionarySize);
            }
            else
            {
                Assert(!JITManager::GetJITManager()->IsJITServer());
                remoteKeys = nullptr;
            }
        }

        BranchDictionary dictionary;
        void* defaultTarget;
        void** remoteKeys;

        static BranchDictionaryWrapper* New(NativeCodeData::Allocator * allocator, uint dictionarySize, ArenaAllocator* remoteKeyAlloc)
        {LOGMEIN("JavascriptNativeOperators.h] 100\n");
            return NativeCodeDataNew(allocator, BranchDictionaryWrapper, allocator, dictionarySize, remoteKeyAlloc);
        }

        void AddEntry(uint32 offset, T key, void* remoteVar)
        {LOGMEIN("JavascriptNativeOperators.h] 105\n");
            int index = dictionary.AddNew(key, (void**)offset);
            if (JITManager::GetJITManager()->IsJITServer())
            {LOGMEIN("JavascriptNativeOperators.h] 108\n");
                Assert(remoteKeys);
                remoteKeys[index] = remoteVar;
            }
        }

        void Fixup(NativeCodeData::DataChunk* chunkList)
        {LOGMEIN("JavascriptNativeOperators.h] 115\n");
            if (JITManager::GetJITManager()->IsJITServer())
            {LOGMEIN("JavascriptNativeOperators.h] 117\n");
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
