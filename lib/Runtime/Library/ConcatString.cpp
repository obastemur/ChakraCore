//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"



namespace Js
{
    DEFINE_RECYCLER_TRACKER_PERF_COUNTER(ConcatString);

    // Note: see also: ConcatString.inl

    /////////////////////// ConcatStringBase //////////////////////////

    ConcatStringBase::ConcatStringBase(StaticType* stringType) : LiteralString(stringType)
    {LOGMEIN("ConcatString.cpp] 17\n");
    }

    // Copy the content of items into specified buffer.
    void ConcatStringBase::CopyImpl(_Out_writes_(m_charLength) char16 *const buffer,
            int itemCount, _In_reads_(itemCount) JavascriptString * const * items,
            StringCopyInfoStack &nestedStringTreeCopyInfos, const byte recursionDepth)
    {LOGMEIN("ConcatString.cpp] 24\n");

        Assert(!IsFinalized());
        Assert(buffer);

        CharCount copiedCharLength = 0;
        for(int i = 0; i < itemCount; ++i)
        {LOGMEIN("ConcatString.cpp] 31\n");
            JavascriptString *const s = items[i];
            if(!s)
            {LOGMEIN("ConcatString.cpp] 34\n");
                continue;
            }

            if (s->IsFinalized())
            {LOGMEIN("ConcatString.cpp] 39\n");
                // If we have the buffer already, just copy it
                const CharCount copyCharLength = s->GetLength();
                AnalysisAssert(copiedCharLength + copyCharLength <= this->GetLength());
                CopyHelper(&buffer[copiedCharLength], s->GetString(), copyCharLength);
                copiedCharLength += copyCharLength;
                continue;
            }

            if(i == itemCount - 1)
            {LOGMEIN("ConcatString.cpp] 49\n");
                JavascriptString * const * newItems;
                int newItemCount = s->GetRandomAccessItemsFromConcatString(newItems);
                if (newItemCount != -1)
                {LOGMEIN("ConcatString.cpp] 53\n");
                    // Optimize for right-weighted ConcatString tree (the append case). Even though appending to a ConcatString will
                    // transition into a CompoundString fairly quickly, strings created by doing just a few appends are very common.
                    items = newItems;
                    itemCount = newItemCount;
                    i = -1;
                    continue;
                }
            }

            const CharCount copyCharLength = s->GetLength();
            AnalysisAssert(copyCharLength <= GetLength() - copiedCharLength);

            if(recursionDepth == MaxCopyRecursionDepth && s->IsTree())
            {LOGMEIN("ConcatString.cpp] 67\n");
                // Don't copy nested string trees yet, as that involves a recursive call, and the recursion can become
                // excessive. Just collect the nested string trees and the buffer location where they should be copied, and
                // the caller can deal with those after returning.
                nestedStringTreeCopyInfos.Push(StringCopyInfo(s, &buffer[copiedCharLength]));
            }
            else
            {
                Assert(recursionDepth <= MaxCopyRecursionDepth);
                s->Copy(&buffer[copiedCharLength], nestedStringTreeCopyInfos, recursionDepth + 1);
            }
            copiedCharLength += copyCharLength;
        }

        Assert(copiedCharLength == GetLength());
    }

    bool ConcatStringBase::IsTree() const
    {LOGMEIN("ConcatString.cpp] 85\n");
        Assert(!IsFinalized());

        return true;
    }

    /////////////////////// ConcatString //////////////////////////

    ConcatString::ConcatString(JavascriptString* a, JavascriptString* b) :
        ConcatStringN<2>(a->GetLibrary()->GetStringTypeStatic(), false)
    {LOGMEIN("ConcatString.cpp] 95\n");
        Assert(a);
        Assert(b);

        a = CompoundString::GetImmutableOrScriptUnreferencedString(a);
        b = CompoundString::GetImmutableOrScriptUnreferencedString(b);

        m_slots[0] = a;
        m_slots[1] = b;

        this->SetLength(a->GetLength() + b->GetLength()); // does not include null character
    }

    ConcatString* ConcatString::New(JavascriptString* left, JavascriptString* right)
    {LOGMEIN("ConcatString.cpp] 109\n");
        Assert(left);

#ifdef PROFILE_STRINGS
       StringProfiler::RecordConcatenation( left->GetScriptContext(), left->GetLength(), right->GetLength(), ConcatType_ConcatTree);
#endif
        Recycler* recycler = left->GetScriptContext()->GetRecycler();
        return RecyclerNew(recycler, ConcatString, left, right);
    }

    /////////////////////// ConcatStringBuilder //////////////////////////

    // MAX number of slots in one chunk. Until we fit into this, we realloc, otherwise create new chunk.
    // The VS2013 linker treats this as a redefinition of an already
    // defined constant and complains. So skip the declaration if we're compiling
    // with VS2013 or below.
#if !defined(_MSC_VER) || _MSC_VER >= 1900
    const int ConcatStringBuilder::c_maxChunkSlotCount;
#endif

    ConcatStringBuilder::ConcatStringBuilder(ScriptContext* scriptContext, int initialSlotCount) :
        ConcatStringBase(scriptContext->GetLibrary()->GetStringTypeStatic()),
        m_count(0), m_prevChunk(nullptr)
    {LOGMEIN("ConcatString.cpp] 132\n");
        Assert(scriptContext);

        // Note: m_slotCount is a valid scenario -- when you don't know how many will be there.
        this->AllocateSlots(initialSlotCount);
        this->SetLength(0); // does not include null character
    }

    ConcatStringBuilder::ConcatStringBuilder(const ConcatStringBuilder& other):
        ConcatStringBase(other.GetScriptContext()->GetLibrary()->GetStringTypeStatic())
    {LOGMEIN("ConcatString.cpp] 142\n");
        m_slots = other.m_slots;
        m_count = other.m_count;
        m_slotCount = other.m_slotCount;
        m_prevChunk = other.m_prevChunk;
        this->SetLength(other.GetLength());
        // TODO: should we copy the JavascriptString buffer and if so, how do we pass over the ownership?
    }

    ConcatStringBuilder* ConcatStringBuilder::New(ScriptContext* scriptContext, int initialSlotCount)
    {LOGMEIN("ConcatString.cpp] 152\n");
        Assert(scriptContext);

        return RecyclerNew(scriptContext->GetRecycler(), ConcatStringBuilder, scriptContext, initialSlotCount);
    }

    const char16 * ConcatStringBuilder::GetSz()
    {LOGMEIN("ConcatString.cpp] 159\n");
        const char16 * sz = GetSzImpl<ConcatStringBuilder>();

        // Allow a/b to be garbage collected if no more refs.
        ConcatStringBuilder* current = this;
        while (current != NULL)
        {LOGMEIN("ConcatString.cpp] 165\n");
            ClearArray(current->m_slots, current->m_count);
            current = current->m_prevChunk;
        }

        return sz;
    }

    // Append/concat a new string to us.
    // The idea is that we will grow/realloc current slot if new size fits into MAX chunk size (c_maxChunkSlotCount).
    // Otherwise we will create a new chunk.
    void ConcatStringBuilder::Append(JavascriptString* str)
    {LOGMEIN("ConcatString.cpp] 177\n");
        // Note: we are quite lucky here because we always add 1 (no more) string to us.

        Assert(str);
        charcount_t len = this->GetLength(); // This is len of all chunks.

        if (m_count == m_slotCount)
        {LOGMEIN("ConcatString.cpp] 184\n");
            // Out of free slots, current chunk is full, need to grow.
            int oldItemCount = this->GetItemCount();
            int newItemCount = oldItemCount > 0 ?
                oldItemCount > 1 ? oldItemCount + oldItemCount / 2 : 2 :
                1;
            Assert(newItemCount > oldItemCount);
            int growDelta = newItemCount - oldItemCount; // # of items to grow by.
            int newSlotCount = m_slotCount + growDelta;
            if (newSlotCount <= c_maxChunkSlotCount)
            {LOGMEIN("ConcatString.cpp] 194\n");
                // While we fit into MAX chunk size, realloc/grow current chunk.
                Field(JavascriptString*)* newSlots = RecyclerNewArray(
                    this->GetScriptContext()->GetRecycler(), Field(JavascriptString*), newSlotCount);
                CopyArray(newSlots, newSlotCount, m_slots, m_slotCount);
                m_slots = newSlots;
                m_slotCount = newSlotCount;
            }
            else
            {
                // Create new chunk with MAX size, swap new instance's data with this's data.
                // We never create more than one chunk at a time.
                ConcatStringBuilder* newChunk = RecyclerNew(this->GetScriptContext()->GetRecycler(), ConcatStringBuilder, *this); // Create a copy.
                m_prevChunk = newChunk;
                m_count = 0;
                AllocateSlots(this->c_maxChunkSlotCount);
                Assert(m_slots);
            }
        }

        str = CompoundString::GetImmutableOrScriptUnreferencedString(str);

        m_slots[m_count++] = str;

        len += str->GetLength();
        this->SetLength(len);
    }

    // Allocate slots, set m_slots and m_slotCount.
    // Note: the amount of slots allocated can be less than the requestedSlotCount parameter.
    void ConcatStringBuilder::AllocateSlots(int requestedSlotCount)
    {LOGMEIN("ConcatString.cpp] 225\n");
        if (requestedSlotCount > 0)
        {LOGMEIN("ConcatString.cpp] 227\n");
            m_slotCount = min(requestedSlotCount, this->c_maxChunkSlotCount);
            m_slots = RecyclerNewArray(this->GetScriptContext()->GetRecycler(), Field(JavascriptString*), m_slotCount);
        }
        else
        {
            m_slotCount = 0;
            m_slots = nullptr;
        }
    }

    // Returns the number of JavascriptString* items accumulated so far in all chunks.
    int ConcatStringBuilder::GetItemCount() const
    {LOGMEIN("ConcatString.cpp] 240\n");
        int count = 0;
        const ConcatStringBuilder* current = this;
        while (current != NULL)
        {LOGMEIN("ConcatString.cpp] 244\n");
            count += current->m_count;
            current = current->m_prevChunk;
        }
        return count;
    }

    ConcatStringBuilder* ConcatStringBuilder::GetHead() const
    {LOGMEIN("ConcatString.cpp] 252\n");
        ConcatStringBuilder* current = const_cast<ConcatStringBuilder*>(this);
        ConcatStringBuilder* head;
        do
        {LOGMEIN("ConcatString.cpp] 256\n");
            head = current;
            current = current->m_prevChunk;
        } while (current != NULL);
        return head;
    }

    void ConcatStringBuilder::CopyVirtual(
        _Out_writes_(m_charLength) char16 *const buffer,
        StringCopyInfoStack &nestedStringTreeCopyInfos,
        const byte recursionDepth)
    {LOGMEIN("ConcatString.cpp] 267\n");
        Assert(!this->IsFinalized());
        Assert(buffer);

        CharCount remainingCharLengthToCopy = GetLength();
        for(const ConcatStringBuilder *current = this; current; current = current->m_prevChunk)
        {LOGMEIN("ConcatString.cpp] 273\n");
            for(int i = current->m_count - 1; i >= 0; --i)
            {LOGMEIN("ConcatString.cpp] 275\n");
                JavascriptString *const s = current->m_slots[i];
                if(!s)
                {LOGMEIN("ConcatString.cpp] 278\n");
                    continue;
                }

                const CharCount copyCharLength = s->GetLength();
                Assert(remainingCharLengthToCopy >= copyCharLength);
                remainingCharLengthToCopy -= copyCharLength;
                if(recursionDepth == MaxCopyRecursionDepth && s->IsTree())
                {LOGMEIN("ConcatString.cpp] 286\n");
                    // Don't copy nested string trees yet, as that involves a recursive call, and the recursion can become
                    // excessive. Just collect the nested string trees and the buffer location where they should be copied, and
                    // the caller can deal with those after returning.
                    nestedStringTreeCopyInfos.Push(StringCopyInfo(s, &buffer[remainingCharLengthToCopy]));
                }
                else
                {
                    Assert(recursionDepth <= MaxCopyRecursionDepth);
                    s->Copy(&buffer[remainingCharLengthToCopy], nestedStringTreeCopyInfos, recursionDepth + 1);
                }
            }
        }
    }

    /////////////////////// ConcatStringMulti //////////////////////////
    ConcatStringMulti::ConcatStringMulti(uint slotCount, JavascriptString * a1, JavascriptString * a2, StaticType* stringTypeStatic) :
        ConcatStringBase(stringTypeStatic), slotCount(slotCount)
    {LOGMEIN("ConcatString.cpp] 304\n");
#if DBG
        ClearArray(m_slots, slotCount);
#endif
        m_slots[0] = CompoundString::GetImmutableOrScriptUnreferencedString(a1);
        m_slots[1] = CompoundString::GetImmutableOrScriptUnreferencedString(a2);

        this->SetLength(a1->GetLength() + a2->GetLength());
    }

    size_t
    ConcatStringMulti::GetAllocSize(uint slotCount)
    {LOGMEIN("ConcatString.cpp] 316\n");
        return sizeof(ConcatStringMulti) + (sizeof(JavascriptString *) * slotCount);
    }

    ConcatStringMulti*
    ConcatStringMulti::New(uint slotCount, JavascriptString * a1, JavascriptString * a2, ScriptContext * scriptContext)
    {LOGMEIN("ConcatString.cpp] 322\n");
        return RecyclerNewPlus(scriptContext->GetRecycler(),
            sizeof(JavascriptString *) * slotCount, ConcatStringMulti, slotCount, a1, a2,
            scriptContext->GetLibrary()->GetStringTypeStatic());
    }

    bool
    ConcatStringMulti::Is(Var var)
    {LOGMEIN("ConcatString.cpp] 330\n");
        return VirtualTableInfo<ConcatStringMulti>::HasVirtualTable(var);
    }

    ConcatStringMulti *
    ConcatStringMulti::FromVar(Var var)
    {LOGMEIN("ConcatString.cpp] 336\n");
        Assert(ConcatStringMulti::Is(var));
        return static_cast<ConcatStringMulti *>(var);
    }

    const char16 *
    ConcatStringMulti::GetSz()
    {LOGMEIN("ConcatString.cpp] 343\n");
        Assert(IsFilled());
        const char16 * sz = GetSzImpl<ConcatStringMulti>();

        // Allow slots to be garbage collected if no more refs.
        ClearArray(m_slots, slotCount);

        return sz;
    }

    void
    ConcatStringMulti::SetItem(_In_range_(0, slotCount - 1) uint index, JavascriptString* value)
    {LOGMEIN("ConcatString.cpp] 355\n");
        Assert(index < slotCount);
        Assert(m_slots[index] == nullptr);
        value = CompoundString::GetImmutableOrScriptUnreferencedString(value);
        this->SetLength(this->GetLength() + value->GetLength());
        m_slots[index] = value;
    }

#if DBG
    bool
    ConcatStringMulti::IsFilled() const
    {LOGMEIN("ConcatString.cpp] 366\n");
        for (uint i = slotCount; i > 0; i--)
        {LOGMEIN("ConcatString.cpp] 368\n");
            if (m_slots[i - 1] == nullptr) {LOGMEIN("ConcatString.cpp] 369\n"); return false; }
        }
        return true;
    }
#endif
} // namespace Js.
