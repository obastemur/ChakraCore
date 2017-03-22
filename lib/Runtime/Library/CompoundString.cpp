//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
// JScriptDiag does not link with Runtime.lib and does not include .cpp files, so this file will be included as a header
#include "RuntimeLibraryPch.h"


namespace Js
{
    #pragma region CompoundString::Block
    #ifndef IsJsDiag
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    const uint CompoundString::Block::MaxChainedBlockSize = HeapConstants::MaxSmallObjectSize; // TODO: LargeAlloc seems to be significantly slower, hence this threshold
    const uint CompoundString::Block::ChainSizeThreshold = MaxChainedBlockSize / 2;
    // TODO: Once the above LargeAlloc issue is fixed, experiment with forcing resizing as long as the string has only direct chars

    CompoundString::Block::Block(const CharCount charCapacity, const Block *const previous)
        : bufferOwner(this), charLength(0), charCapacity(charCapacity), previous(previous)
    {LOGMEIN("CompoundString.cpp] 20\n");
        Assert(HeapInfo::IsAlignedSize(ChainSizeThreshold));
        Assert(ChainSizeThreshold <= MaxChainedBlockSize);
        Assert(HeapInfo::IsAlignedSize(MaxChainedBlockSize));
        Assert((MaxChainedBlockSize << 1) > MaxChainedBlockSize);

        Assert(charCapacity != 0);
        Assert(GrowSize(SizeFromCharCapacity(charCapacity)) != 0);
    }

    CompoundString::Block::Block(
        const void *const buffer,
        const CharCount charLength,
        const CharCount charCapacity)
        : bufferOwner(this), charLength(charLength), charCapacity(charCapacity), previous(nullptr)
    {LOGMEIN("CompoundString.cpp] 35\n");
        Assert(buffer);
        Assert(charLength <= charCapacity);

        ArrayWriteBarrierVerifyBits(Block::Pointers(Chars()), Block::PointerLengthFromCharLength(charLength));
        js_wmemcpy_s(Chars(), charLength, Chars(buffer), charLength);
        // SWB: buffer may contain chars or pointers. Trigger write barrier for the whole buffer.
        ArrayWriteBarrier(Pointers(), PointerLengthFromCharLength(charLength));
    }

    CompoundString::Block::Block(const Block &other, const CharCount usedCharLength)
        : bufferOwner(other.bufferOwner),
        charLength(usedCharLength),
        charCapacity(other.charCapacity),
        previous(other.previous)
    {LOGMEIN("CompoundString.cpp] 50\n");
        // This only does a shallow copy. The metadata is copied, and a reference to the other block is included in this copy
        // for access to the other block's buffer.
        Assert(usedCharLength <= other.charCapacity);
    }

    CompoundString::Block *CompoundString::Block::New(
        const uint size,
        const Block *const previous,
        Recycler *const recycler)
    {LOGMEIN("CompoundString.cpp] 60\n");
        Assert(HeapInfo::IsAlignedSize(size));
        Assert(recycler);

        return RecyclerNewPlus(recycler, size - sizeof(Block), Block, CharCapacityFromSize(size), previous);
    }

    CompoundString::Block *CompoundString::Block::New(
        const void *const buffer,
        const CharCount usedCharLength,
        const bool reserveMoreSpace,
        Recycler *const recycler)
    {LOGMEIN("CompoundString.cpp] 72\n");
        Assert(buffer);
        Assert(recycler);

        uint size = SizeFromUsedCharLength(usedCharLength);
        if(reserveMoreSpace)
            size = GrowSize(size);
        return RecyclerNewPlus(recycler, size - sizeof(Block), Block, buffer, usedCharLength, CharCapacityFromSize(size));
    }

    CompoundString::Block *CompoundString::Block::Clone(
        const CharCount usedCharLength,
        Recycler *const recycler) const
    {LOGMEIN("CompoundString.cpp] 85\n");
        Assert(recycler);

        return RecyclerNew(recycler, Block, *this, usedCharLength);
    }

    CharCount CompoundString::Block::CharCapacityFromSize(const uint size)
    {LOGMEIN("CompoundString.cpp] 92\n");
        Assert(size >= sizeof(Block));

        return (size - sizeof(Block)) / sizeof(char16);
    }

    uint CompoundString::Block::SizeFromCharCapacity(const CharCount charCapacity)
    {LOGMEIN("CompoundString.cpp] 99\n");
        Assert(IsValidCharCount(charCapacity));
        return UInt32Math::Add(sizeof(Block), charCapacity * sizeof(char16));
    }

    #endif

    inline CharCount CompoundString::Block::PointerAlign(const CharCount charLength)
    {LOGMEIN("CompoundString.cpp] 107\n");
        const CharCount alignedCharLength = ::Math::Align(charLength, static_cast<CharCount>(sizeof(void *) / sizeof(char16)));
        Assert(alignedCharLength >= charLength);
        return alignedCharLength;
    }

    inline const char16 *CompoundString::Block::Chars(const void *const buffer)
    {LOGMEIN("CompoundString.cpp] 114\n");
        return static_cast<const char16 *>(buffer);
    }

    #ifndef IsJsDiag

    char16 *CompoundString::Block::Chars(void *const buffer)
    {LOGMEIN("CompoundString.cpp] 121\n");
        return static_cast<char16 *>(buffer);
    }

    const Field(void*) *CompoundString::Block::Pointers(const void *const buffer)
    {LOGMEIN("CompoundString.cpp] 126\n");
        return (const Field(void*)*)(buffer);
    }

    Field(void*) *CompoundString::Block::Pointers(void *const buffer)
    {LOGMEIN("CompoundString.cpp] 131\n");
        return static_cast<Field(void*)*>(buffer);
    }

    CharCount CompoundString::Block::PointerCapacityFromCharCapacity(const CharCount charCapacity)
    {LOGMEIN("CompoundString.cpp] 136\n");
        return charCapacity / (sizeof(void *) / sizeof(char16));
    }

    CharCount CompoundString::Block::CharCapacityFromPointerCapacity(const CharCount pointerCapacity)
    {LOGMEIN("CompoundString.cpp] 141\n");
        return pointerCapacity * (sizeof(void *) / sizeof(char16));
    }

    #endif

    // ChakraDiag includes CompoundString.cpp as a header file so this method needs to be marked as inline
    // to handle that case
    JS_DIAG_INLINE CharCount CompoundString::Block::PointerLengthFromCharLength(const CharCount charLength)
    {LOGMEIN("CompoundString.cpp] 150\n");
        return PointerAlign(charLength) / (sizeof(void *) / sizeof(char16));
    }

    #ifndef IsJsDiag

    CharCount CompoundString::Block::CharLengthFromPointerLength(const CharCount pointerLength)
    {LOGMEIN("CompoundString.cpp] 157\n");
        return pointerLength * (sizeof(void *) / sizeof(char16));
    }

    uint CompoundString::Block::SizeFromUsedCharLength(const CharCount usedCharLength)
    {LOGMEIN("CompoundString.cpp] 162\n");
        const size_t usedSize = SizeFromCharCapacity(usedCharLength);
        const size_t alignedUsedSize = HeapInfo::GetAlignedSizeNoCheck(usedSize);
        if (alignedUsedSize != (uint)alignedUsedSize)
        {LOGMEIN("CompoundString.cpp] 166\n");
            Js::Throw::OutOfMemory();
        }
        return (uint)alignedUsedSize;
    }

    bool CompoundString::Block::ShouldAppendChars(
        const CharCount appendCharLength,
        const uint additionalSizeForPointerAppend)
    {LOGMEIN("CompoundString.cpp] 175\n");
        // Append characters instead of pointers when it would save space. Add some buffer as well, as flattening becomes more
        // expensive after the switch to pointer mode.
        //
        // 'additionalSizeForPointerAppend' should be provided when appending a pointer also involves creating a string object
        // or some other additional space (such as LiteralString, in which case this parameter should be sizeof(LiteralString)),
        // as that additional size also needs to be taken into account.
        return appendCharLength <= (sizeof(void *) * 2 + additionalSizeForPointerAppend) / sizeof(char16);
    }

    const void *CompoundString::Block::Buffer() const
    {LOGMEIN("CompoundString.cpp] 186\n");
        return bufferOwner + 1;
    }

    void *CompoundString::Block::Buffer()
    {LOGMEIN("CompoundString.cpp] 191\n");
        return bufferOwner + 1;
    }

    const CompoundString::Block *CompoundString::Block::Previous() const
    {LOGMEIN("CompoundString.cpp] 196\n");
        return previous;
    }

    const char16 *CompoundString::Block::Chars() const
    {LOGMEIN("CompoundString.cpp] 201\n");
        return Chars(Buffer());
    }

    char16 *CompoundString::Block::Chars()
    {LOGMEIN("CompoundString.cpp] 206\n");
        return Chars(Buffer());
    }

    CharCount CompoundString::Block::CharLength() const
    {LOGMEIN("CompoundString.cpp] 211\n");
        return charLength;
    }

    void CompoundString::Block::SetCharLength(const CharCount charLength)
    {LOGMEIN("CompoundString.cpp] 216\n");
        Assert(charLength <= CharCapacity());

        this->charLength = charLength;
    }

    CharCount CompoundString::Block::CharCapacity() const
    {LOGMEIN("CompoundString.cpp] 223\n");
        return charCapacity;
    }

    const Field(void*) *CompoundString::Block::Pointers() const
    {LOGMEIN("CompoundString.cpp] 228\n");
        return Pointers(Buffer());
    }

    Field(void*) *CompoundString::Block::Pointers()
    {LOGMEIN("CompoundString.cpp] 233\n");
        return Pointers(Buffer());
    }

    CharCount CompoundString::Block::PointerLength() const
    {LOGMEIN("CompoundString.cpp] 238\n");
        return PointerLengthFromCharLength(CharLength());
    }

    CharCount CompoundString::Block::PointerCapacity() const
    {LOGMEIN("CompoundString.cpp] 243\n");
        return PointerCapacityFromCharCapacity(CharCapacity());
    }

    uint CompoundString::Block::GrowSize(const uint size)
    {LOGMEIN("CompoundString.cpp] 248\n");
        Assert(size >= sizeof(Block));
        Assert(HeapInfo::IsAlignedSize(size));

        const uint newSize = size << 1;
        Assert(newSize > size);
        return newSize;
    }

    uint CompoundString::Block::GrowSizeForChaining(const uint size)
    {LOGMEIN("CompoundString.cpp] 258\n");
        const uint newSize = GrowSize(size);
        return min(MaxChainedBlockSize, newSize);
    }

    CompoundString::Block *CompoundString::Block::Chain(Recycler *const recycler)
    {LOGMEIN("CompoundString.cpp] 264\n");
        return New(GrowSizeForChaining(SizeFromUsedCharLength(CharLength())), this, recycler);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    #endif
    #pragma endregion

    #pragma region CompoundString::BlockInfo
    #ifndef IsJsDiag
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    CompoundString::BlockInfo::BlockInfo() : buffer(nullptr), charLength(0), charCapacity(0)
    {LOGMEIN("CompoundString.cpp] 277\n");
    }

    CompoundString::BlockInfo::BlockInfo(Block *const block)
    {LOGMEIN("CompoundString.cpp] 281\n");
        CopyFrom(block);
    }

    char16 *CompoundString::BlockInfo::Chars() const
    {LOGMEIN("CompoundString.cpp] 286\n");
        return Block::Chars(buffer);
    }

    CharCount CompoundString::BlockInfo::CharLength() const
    {LOGMEIN("CompoundString.cpp] 291\n");
        return charLength;
    }

    void CompoundString::BlockInfo::SetCharLength(const CharCount charLength)
    {LOGMEIN("CompoundString.cpp] 296\n");
        Assert(charLength <= CharCapacity());

        this->charLength = charLength;
    }

    CharCount CompoundString::BlockInfo::CharCapacity() const
    {LOGMEIN("CompoundString.cpp] 303\n");
        return charCapacity;
    }

    Field(void*) *CompoundString::BlockInfo::Pointers() const
    {LOGMEIN("CompoundString.cpp] 308\n");
        return Block::Pointers(buffer);
    }

    CharCount CompoundString::BlockInfo::PointerLength() const
    {LOGMEIN("CompoundString.cpp] 313\n");
        return Block::PointerLengthFromCharLength(CharLength());
    }

    void CompoundString::BlockInfo::SetPointerLength(const CharCount pointerLength)
    {LOGMEIN("CompoundString.cpp] 318\n");
        Assert(pointerLength <= PointerCapacity());

        charLength = Block::CharLengthFromPointerLength(pointerLength);
    }

    CharCount CompoundString::BlockInfo::PointerCapacity() const
    {LOGMEIN("CompoundString.cpp] 325\n");
        return Block::PointerCapacityFromCharCapacity(CharCapacity());
    }

    CharCount CompoundString::BlockInfo::AlignCharCapacityForAllocation(const CharCount charCapacity)
    {LOGMEIN("CompoundString.cpp] 330\n");
        const CharCount alignedCharCapacity =
            ::Math::AlignOverflowCheck(
                charCapacity == 0 ? static_cast<CharCount>(1) : charCapacity,
                static_cast<CharCount>(HeapConstants::ObjectGranularity / sizeof(char16)));
        Assert(alignedCharCapacity != 0);
        return alignedCharCapacity;
    }

    CharCount CompoundString::BlockInfo::GrowCharCapacity(const CharCount charCapacity)
    {LOGMEIN("CompoundString.cpp] 340\n");
        Assert(charCapacity != 0);
        Assert(AlignCharCapacityForAllocation(charCapacity) == charCapacity);

        const CharCount newCharCapacity = UInt32Math::Mul<2>(charCapacity);
        Assert(newCharCapacity > charCapacity);
        return newCharCapacity;
    }

    bool CompoundString::BlockInfo::ShouldAllocateBuffer(const CharCount charCapacity)
    {LOGMEIN("CompoundString.cpp] 350\n");
        Assert(charCapacity != 0);
        Assert(AlignCharCapacityForAllocation(charCapacity) == charCapacity);

        return charCapacity < Block::ChainSizeThreshold / sizeof(char16);
    }

    void CompoundString::BlockInfo::AllocateBuffer(const CharCount charCapacity, Recycler *const recycler)
    {LOGMEIN("CompoundString.cpp] 358\n");
        Assert(!buffer);
        Assert(CharLength() == 0);
        Assert(CharCapacity() == 0);
        Assert(ShouldAllocateBuffer(charCapacity));
        Assert(recycler);

        buffer = RecyclerNewArray(recycler, char16, charCapacity);
        this->charCapacity = charCapacity;
    }

    CompoundString::Block *CompoundString::BlockInfo::CopyBuffer(
        const void *const buffer,
        const CharCount usedCharLength,
        const bool reserveMoreSpace,
        Recycler *const recycler)
    {LOGMEIN("CompoundString.cpp] 374\n");
        Assert(buffer);
        Assert(recycler);

        CharCount charCapacity = AlignCharCapacityForAllocation(usedCharLength);
        if(reserveMoreSpace)
            charCapacity = GrowCharCapacity(charCapacity);
        if(ShouldAllocateBuffer(charCapacity))
        {
            AllocateBuffer(charCapacity, recycler);
            charLength = usedCharLength;
            
            ArrayWriteBarrierVerifyBits(Block::Pointers(Chars()), Block::PointerLengthFromCharLength(charCapacity));
            js_wmemcpy_s(Chars(), charCapacity, (const char16*)(buffer), usedCharLength);
            // SWB: buffer may contain chars or pointers. Trigger write barrier for the whole buffer.
            ArrayWriteBarrier(Pointers(), PointerLength());
            return nullptr;
        }

        Block *const block = Block::New(buffer, usedCharLength, reserveMoreSpace, recycler);
        CopyFrom(block);
        return block;
    }

    CompoundString::Block *CompoundString::BlockInfo::Resize(Recycler *const recycler)
    {LOGMEIN("CompoundString.cpp] 399\n");
        Assert(recycler);

        const CharCount newCharCapacity = GrowCharCapacity(AlignCharCapacityForAllocation(CharLength()));
        if(ShouldAllocateBuffer(newCharCapacity))
        {LOGMEIN("CompoundString.cpp] 404\n");
            void *const newBuffer = RecyclerNewArray(recycler, char16, newCharCapacity);
            charCapacity = newCharCapacity;
            const CharCount charLength = CharLength();

            ArrayWriteBarrierVerifyBits(Block::Pointers(newBuffer), Block::PointerLengthFromCharLength(charCapacity));
            js_wmemcpy_s((char16*)newBuffer, charCapacity, (char16*)PointerValue(buffer), charLength);
            buffer = newBuffer;
            // SWB: buffer may contain chars or pointers. Trigger write barrier for the whole buffer.
            ArrayWriteBarrier(Pointers(), PointerLength());
            return nullptr;
        }

        Block *const block = Block::New(buffer, CharLength(), true, recycler);
        CopyFrom(block);
        return block;
    }

    void CompoundString::BlockInfo::CopyFrom(Block *const block)
    {LOGMEIN("CompoundString.cpp] 423\n");
        buffer = block->Buffer();
        charLength = block->CharLength();
        charCapacity = block->CharCapacity();
    }

    void CompoundString::BlockInfo::CopyTo(Block *const block)
    {LOGMEIN("CompoundString.cpp] 430\n");
        Assert(block->Buffer() == buffer);
        Assert(block->CharLength() <= charLength);
        Assert(block->CharCapacity() == charCapacity);

        block->SetCharLength(charLength);
    }

    void CompoundString::BlockInfo::Unreference()
    {LOGMEIN("CompoundString.cpp] 439\n");
        buffer = nullptr;
        charLength = 0;
        charCapacity = 0;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    #endif
    #pragma endregion

    #pragma region CompoundString
    #ifndef IsJsDiag
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    CompoundString::CompoundString(const CharCount initialCharCapacity, JavascriptLibrary *const library)
        : LiteralString(library->GetStringTypeStatic()),
        directCharLength(static_cast<CharCount>(-1)),
        ownsLastBlock(true),
        lastBlock(nullptr)
    {LOGMEIN("CompoundString.cpp] 458\n");
        Assert(library);

        lastBlockInfo.AllocateBuffer(initialCharCapacity, library->GetRecycler());
    }

    CompoundString::CompoundString(
        const CharCount initialBlockSize,
        const bool allocateBlock,
        JavascriptLibrary *const library)
        : LiteralString(library->GetStringTypeStatic()),
        directCharLength(static_cast<CharCount>(-1)),
        ownsLastBlock(true)
    {LOGMEIN("CompoundString.cpp] 471\n");
        Assert(allocateBlock);
        Assert(library);

        Block *const block = Block::New(initialBlockSize, nullptr, library->GetRecycler());
        lastBlockInfo.CopyFrom(block);
        lastBlock = block;
    }

    CompoundString::CompoundString(
        const CharCount stringLength,
        const CharCount directCharLength,
        const void *const buffer,
        const CharCount usedCharLength,
        const bool reserveMoreSpace,
        JavascriptLibrary *const library)
        : LiteralString(library->GetStringTypeStatic()),
        directCharLength(directCharLength),
        ownsLastBlock(true)
    {LOGMEIN("CompoundString.cpp] 490\n");
        Assert(directCharLength == static_cast<CharCount>(-1) || directCharLength <= stringLength);
        Assert(buffer);
        Assert(library);

        SetLength(stringLength);
        lastBlock = lastBlockInfo.CopyBuffer(buffer, usedCharLength, reserveMoreSpace, library->GetRecycler());
    }

    CompoundString::CompoundString(CompoundString &other, const bool forAppending)
        : LiteralString(other.GetLibrary()->GetStringTypeStatic()),
        lastBlockInfo(other.lastBlockInfo),
        directCharLength(other.directCharLength),
        lastBlock(other.lastBlock)
    {LOGMEIN("CompoundString.cpp] 504\n");
        Assert(!other.IsFinalized());

        SetLength(other.GetLength());

        if(forAppending)
        {LOGMEIN("CompoundString.cpp] 510\n");
            // This compound string will be used for appending, so take ownership of the last block. Appends are fast for a
            // compound string that owns the last block.
            const bool ownsLastBlock = other.ownsLastBlock;
            other.ownsLastBlock = false;
            this->ownsLastBlock = ownsLastBlock;
            if(ownsLastBlock)
                return;
            TakeOwnershipOfLastBlock();
            return;
        }

        ownsLastBlock = false;
    }

    CompoundString *CompoundString::NewWithCharCapacity(
        const CharCount initialCharCapacity,
        JavascriptLibrary *const library)
    {LOGMEIN("CompoundString.cpp] 528\n");
        const CharCount alignedInitialCharCapacity = BlockInfo::AlignCharCapacityForAllocation(initialCharCapacity);
        if(BlockInfo::ShouldAllocateBuffer(alignedInitialCharCapacity))
            return NewWithBufferCharCapacity(alignedInitialCharCapacity, library);
        return NewWithBlockSize(Block::SizeFromUsedCharLength(initialCharCapacity), library);
    }

    CompoundString *CompoundString::NewWithPointerCapacity(
        const CharCount initialPointerCapacity,
        JavascriptLibrary *const library)
    {LOGMEIN("CompoundString.cpp] 538\n");
        return NewWithCharCapacity(Block::CharCapacityFromPointerCapacity(initialPointerCapacity), library);
    }

    CompoundString *CompoundString::NewWithBufferCharCapacity(const CharCount initialCharCapacity, JavascriptLibrary *const library)
    {LOGMEIN("CompoundString.cpp] 543\n");
        Assert(library);

        return RecyclerNew(library->GetRecycler(), CompoundString, initialCharCapacity, library);
    }

    CompoundString *CompoundString::NewWithBlockSize(const CharCount initialBlockSize, JavascriptLibrary *const library)
    {LOGMEIN("CompoundString.cpp] 550\n");
        Assert(library);

        return RecyclerNew(library->GetRecycler(), CompoundString, initialBlockSize, true, library);
    }

    CompoundString *CompoundString::New(
        const CharCount stringLength,
        const CharCount directCharLength,
        const void *const buffer,
        const CharCount usedCharLength,
        const bool reserveMoreSpace,
        JavascriptLibrary *const library)
    {LOGMEIN("CompoundString.cpp] 563\n");
        Assert(library);

        return
            RecyclerNew(
                library->GetRecycler(),
                CompoundString,
                stringLength,
                directCharLength,
                buffer,
                usedCharLength,
                reserveMoreSpace,
                library);
    }

    CompoundString *CompoundString::Clone(const bool forAppending)
    {LOGMEIN("CompoundString.cpp] 579\n");
        return RecyclerNew(GetLibrary()->GetRecycler(), CompoundString, *this, forAppending);
    }

    CompoundString * CompoundString::JitClone(CompoundString * cs)
    {LOGMEIN("CompoundString.cpp] 584\n");
        Assert(Is(cs));
        return cs->Clone(false);
    }

    CompoundString * CompoundString::JitCloneForAppending(CompoundString * cs)
    {LOGMEIN("CompoundString.cpp] 590\n");
        Assert(Is(cs));
        return cs->Clone(true);
    }

    bool CompoundString::Is(RecyclableObject *const object)
    {LOGMEIN("CompoundString.cpp] 596\n");
        return VirtualTableInfo<CompoundString>::HasVirtualTable(object);
    }

    bool CompoundString::Is(const Var var)
    {LOGMEIN("CompoundString.cpp] 601\n");
        return RecyclableObject::Is(var) && Is(RecyclableObject::FromVar(var));
    }

    CompoundString *CompoundString::FromVar(RecyclableObject *const object)
    {LOGMEIN("CompoundString.cpp] 606\n");
        Assert(Is(object));

        CompoundString *const cs = static_cast<CompoundString *>(object);
        Assert(!cs->IsFinalized());
        return cs;
    }

    CompoundString *CompoundString::FromVar(const Var var)
    {LOGMEIN("CompoundString.cpp] 615\n");
        return FromVar(RecyclableObject::FromVar(var));
    }

    JavascriptString *CompoundString::GetImmutableOrScriptUnreferencedString(JavascriptString *const s)
    {LOGMEIN("CompoundString.cpp] 620\n");
        Assert(s);

        // The provided string may be referenced by script code. A script-unreferenced version of the string is being requested,
        // likely because the provided string will be referenced directly in a concatenation operation (by ConcatString or
        // another CompoundString, for instance). If the provided string is a CompoundString, it must not be mutated by script
        // code after the concatenation operation. In that case, clone the string to ensure that it is not referenced by script
        // code. If the clone is never handed back to script code, it effectively behaves as an immutable string.
        return Is(s) ? FromVar(s)->Clone(false) : s;
    }

    bool CompoundString::ShouldAppendChars(const CharCount appendCharLength)
    {LOGMEIN("CompoundString.cpp] 632\n");
        return Block::ShouldAppendChars(appendCharLength);
    }

    bool CompoundString::HasOnlyDirectChars() const
    {LOGMEIN("CompoundString.cpp] 637\n");
        return directCharLength == static_cast<CharCount>(-1);
    }

    void CompoundString::SwitchToPointerMode()
    {LOGMEIN("CompoundString.cpp] 642\n");
        Assert(HasOnlyDirectChars());

        directCharLength = GetLength();

        if(PHASE_TRACE_StringConcat)
        {LOGMEIN("CompoundString.cpp] 648\n");
            Output::Print(_u("CompoundString::SwitchToPointerMode()\n"));
            Output::Flush();
        }
    }

    bool CompoundString::OwnsLastBlock() const
    {LOGMEIN("CompoundString.cpp] 655\n");
        return ownsLastBlock;
    }

    const char16 *CompoundString::GetAppendStringBuffer(JavascriptString *const s) const
    {LOGMEIN("CompoundString.cpp] 660\n");
        Assert(s);

        // A compound string cannot flatten itself while appending itself to itself since flattening would make the append
        // illegal. Clone the string being appended if necessary, before flattening.
        return s == this ? FromVar(s)->Clone(false)->GetSz() : s->GetString();
    }

    char16 *CompoundString::LastBlockChars() const
    {LOGMEIN("CompoundString.cpp] 669\n");
        return lastBlockInfo.Chars();
    }

    CharCount CompoundString::LastBlockCharLength() const
    {LOGMEIN("CompoundString.cpp] 674\n");
        return lastBlockInfo.CharLength();
    }

    void CompoundString::SetLastBlockCharLength(const CharCount charLength)
    {LOGMEIN("CompoundString.cpp] 679\n");
        lastBlockInfo.SetCharLength(charLength);
    }

    CharCount CompoundString::LastBlockCharCapacity() const
    {LOGMEIN("CompoundString.cpp] 684\n");
        return lastBlockInfo.CharCapacity();
    }

    Field(void*) *CompoundString::LastBlockPointers() const
    {LOGMEIN("CompoundString.cpp] 689\n");
        return lastBlockInfo.Pointers();
    }

    CharCount CompoundString::LastBlockPointerLength() const
    {LOGMEIN("CompoundString.cpp] 694\n");
        return lastBlockInfo.PointerLength();
    }

    void CompoundString::SetLastBlockPointerLength(const CharCount pointerLength)
    {LOGMEIN("CompoundString.cpp] 699\n");
        lastBlockInfo.SetPointerLength(pointerLength);
    }

    CharCount CompoundString::LastBlockPointerCapacity() const
    {LOGMEIN("CompoundString.cpp] 704\n");
        return lastBlockInfo.PointerCapacity();
    }

    void CompoundString::PackSubstringInfo(
        const CharCount startIndex,
        const CharCount length,
        void * *const packedSubstringInfoRef,
        void * *const packedSubstringInfo2Ref)
    {LOGMEIN("CompoundString.cpp] 713\n");
        Assert(static_cast<int32>(startIndex) >= 0);
        Assert(static_cast<int32>(length) >= 0);
        Assert(packedSubstringInfoRef);
        Assert(packedSubstringInfo2Ref);

    #if defined(_M_X64_OR_ARM64)
        // On 64-bit architectures, two nonnegative 32-bit ints fit completely in a tagged pointer
        *packedSubstringInfoRef =
            reinterpret_cast<void *>(
                (static_cast<uintptr_t>(startIndex) << 32) +
                (static_cast<uintptr_t>(length) << 1) +
                1);
        *packedSubstringInfo2Ref = nullptr;
    #else
        CompileAssert(sizeof(void *) == sizeof(int32));

        // On 32-bit architectures, it will be attempted to fit both pieces of into one pointer by using 16 bits for the
        // start index, 15 for the length, and 1 for the tag. If it does not fit, an additional pointer will be used.
        if(startIndex <= static_cast<CharCount>(0xffff) && length <= static_cast<CharCount>(0x7fff))
        {LOGMEIN("CompoundString.cpp] 733\n");
            *packedSubstringInfoRef =
                reinterpret_cast<void *>(
                    (static_cast<uintptr_t>(startIndex) << 16) +
                    (static_cast<uintptr_t>(length) << 1) +
                    1);
            *packedSubstringInfo2Ref = nullptr;
        }
        else
        {
            *packedSubstringInfoRef = reinterpret_cast<void *>((static_cast<uintptr_t>(startIndex) << 1) + 1);
            *packedSubstringInfo2Ref = reinterpret_cast<void *>((static_cast<uintptr_t>(length) << 1) + 1);
        }
    #endif

    #if DBG
        CharCount unpackedStartIndex, unpackedLength;
        UnpackSubstringInfo(*packedSubstringInfoRef, *packedSubstringInfo2Ref, &unpackedStartIndex, &unpackedLength);
        Assert(unpackedStartIndex == startIndex);
        Assert(unpackedLength == length);
    #endif
    }

    #endif

    inline bool CompoundString::IsPackedInfo(void *const pointer)
    {LOGMEIN("CompoundString.cpp] 759\n");
        Assert(pointer);

        return reinterpret_cast<uintptr_t>(pointer) & 1;
    }

    inline void CompoundString::UnpackSubstringInfo(
        void *const pointer,
        void *const pointer2,
        CharCount *const startIndexRef,
        CharCount *const lengthRef)
    {LOGMEIN("CompoundString.cpp] 770\n");
        Assert(pointer);
        Assert(startIndexRef);
        Assert(lengthRef);

        const uintptr_t packedSubstringInfo = reinterpret_cast<uintptr_t>(pointer);
        Assert(packedSubstringInfo & 1);

    #if defined(_M_X64_OR_ARM64)
        // On 64-bit architectures, two nonnegative 32-bit ints fit completely in a tagged pointer
        Assert(!pointer2);
        *startIndexRef = static_cast<CharCount>(packedSubstringInfo >> 32);
        *lengthRef = static_cast<CharCount>(static_cast<uint32>(packedSubstringInfo) >> 1);
    #else
        CompileAssert(sizeof(void *) == sizeof(int32));

        // On 32-bit architectures, it will be attempted to fit both pieces of into one pointer by using 16 bits for the
        // start index, 15 for the length, and 1 for the tag. If it does not fit, an additional pointer will be used.
        if(!pointer2)
        {LOGMEIN("CompoundString.cpp] 789\n");
            *startIndexRef = static_cast<CharCount>(packedSubstringInfo >> 16);
            *lengthRef = static_cast<CharCount>(static_cast<uint16>(packedSubstringInfo) >> 1);
        }
        else
        {
            *startIndexRef = static_cast<CharCount>(packedSubstringInfo >> 1);
            const uintptr_t packedSubstringInfo2 = reinterpret_cast<uintptr_t>(pointer2);
            Assert(packedSubstringInfo2 & 1);
            *lengthRef = static_cast<CharCount>(packedSubstringInfo2 >> 1);
        }
    #endif
    }

    #ifndef IsJsDiag

    void CompoundString::AppendSlow(const char16 c)
    {LOGMEIN("CompoundString.cpp] 806\n");
        Grow();
        const bool appended =
            HasOnlyDirectChars()
                ? TryAppendGeneric(c, this)
                : TryAppendGeneric(GetLibrary()->GetCharStringCache().GetStringForChar(c), 1, this);
        Assert(appended);
    }

    void CompoundString::AppendSlow(JavascriptString *const s)
    {LOGMEIN("CompoundString.cpp] 816\n");
        Grow();
        const bool appended = TryAppendGeneric(s, s->GetLength(), this);
        Assert(appended);
    }

    void CompoundString::AppendSlow(
        __in_xcount(appendCharLength) const char16 *const s,
        const CharCount appendCharLength)
    {LOGMEIN("CompoundString.cpp] 825\n");
        Assert(!IsFinalized());
        Assert(OwnsLastBlock());
        Assert(HasOnlyDirectChars());
        Assert(s);

        // In case of exception, save enough state to revert back to the current state
        const BlockInfo savedLastBlockInfo(lastBlockInfo);
        Block *const savedLastBlock = lastBlock;
        const CharCount savedStringLength = GetLength();

        SetLength(savedStringLength + appendCharLength);

        CharCount copiedCharLength = 0;
        while(true)
        {LOGMEIN("CompoundString.cpp] 840\n");
            const CharCount blockCharLength = LastBlockCharLength();
            const CharCount copyCharLength =
                min(LastBlockCharCapacity() - blockCharLength, appendCharLength - copiedCharLength);
            CopyHelper(&LastBlockChars()[blockCharLength], &s[copiedCharLength], copyCharLength);
            SetLastBlockCharLength(blockCharLength + copyCharLength);
            copiedCharLength += copyCharLength;
            if(copiedCharLength >= appendCharLength)
                break;
            try
            {LOGMEIN("CompoundString.cpp] 850\n");
                Grow();
            }
            catch(...)
            {LOGMEIN("CompoundString.cpp] 854\n");
                lastBlockInfo = savedLastBlockInfo;
                if(savedLastBlock)
                    savedLastBlock->SetCharLength(savedLastBlockInfo.CharLength());
                lastBlock = savedLastBlock;
                SetLength(savedStringLength);
                throw;
            }
        }

        Assert(copiedCharLength == appendCharLength);
    }

    void CompoundString::AppendSlow(
        JavascriptString *const s,
        void *const packedSubstringInfo,
        void *const packedSubstringInfo2,
        const CharCount appendCharLength)
    {LOGMEIN("CompoundString.cpp] 872\n");
        Grow();
        const bool appended = TryAppendGeneric(s, packedSubstringInfo, packedSubstringInfo2, appendCharLength, this);
        Assert(appended);
    }

    void CompoundString::PrepareForAppend()
    {LOGMEIN("CompoundString.cpp] 879\n");
        Assert(!IsFinalized());

        if(OwnsLastBlock())
            return;
        TakeOwnershipOfLastBlock();
    }

    void CompoundString::Append(const char16 c)
    {
        AppendGeneric(c, this, false);
    }

    void CompoundString::AppendChars(const char16 c)
    {
        AppendGeneric(c, this, true);
    }

    void CompoundString::Append(JavascriptString *const s)
    {
        AppendGeneric(s, this, false);
    }

    void CompoundString::AppendChars(JavascriptString *const s)
    {
        AppendGeneric(s, this, true);
    }

    void CompoundString::Append(
        JavascriptString *const s,
        const CharCount startIndex,
        const CharCount appendCharLength)
    {
        AppendGeneric(s, startIndex, appendCharLength, this, false);
    }

    void CompoundString::AppendChars(
        JavascriptString *const s,
        const CharCount startIndex,
        const CharCount appendCharLength)
    {
        AppendGeneric(s, startIndex, appendCharLength, this, true);
    }

    void CompoundString::Append(
        __in_xcount(appendCharLength) const char16 *const s,
        const CharCount appendCharLength)
    {
        AppendGeneric(s, appendCharLength, this, false);
    }

    void CompoundString::AppendChars(
        __in_xcount(appendCharLength) const char16 *const s,
        const CharCount appendCharLength)
    {
        AppendGeneric(s, appendCharLength, this, true);
    }

    void CompoundString::AppendCharsSz(__in_z const char16 *const s)
    {LOGMEIN("CompoundString.cpp] 938\n");
        size_t len = wcslen(s);
        // We limit the length of the string to MaxCharCount,
        // so just OOM if we are appending a string that exceed this limit already
        if (!IsValidCharCount(len))
        {LOGMEIN("CompoundString.cpp] 943\n");
            JavascriptExceptionOperators::ThrowOutOfMemory(this->GetScriptContext());
        }
        AppendChars(s, (CharCount)len);
    }

    void CompoundString::Grow()
    {LOGMEIN("CompoundString.cpp] 950\n");
        Assert(!IsFinalized());
        Assert(OwnsLastBlock());

        Block *const lastBlock = this->lastBlock;
        if(!lastBlock)
        {LOGMEIN("CompoundString.cpp] 956\n");
            // There is no last block. Only the buffer was allocated, and is held in 'lastBlockInfo'. In that case it is always
            // within the threshold to resize. Resize the buffer or resize it into a new block depending on its size.
            this->lastBlock = lastBlockInfo.Resize(GetLibrary()->GetRecycler());
            return;
        }

        lastBlockInfo.CopyTo(lastBlock);
        Block *const newLastBlock = lastBlock->Chain(GetLibrary()->GetRecycler());
        lastBlockInfo.CopyFrom(newLastBlock);
        this->lastBlock = newLastBlock;
    }

    void CompoundString::TakeOwnershipOfLastBlock()
    {LOGMEIN("CompoundString.cpp] 970\n");
        Assert(!IsFinalized());
        Assert(!OwnsLastBlock());

        // Another string object owns the last block's buffer. The buffer must be copied, or another block must be chained.

        Block *const lastBlock = this->lastBlock;
        if(!lastBlock)
        {LOGMEIN("CompoundString.cpp] 978\n");
            // There is no last block. Only the buffer was allocated, and is held in 'lastBlockInfo'. In that case it is always
            // within the threshold to resize. Resize the buffer or resize it into a new block depending on its size.
            this->lastBlock = lastBlockInfo.Resize(GetLibrary()->GetRecycler());
            ownsLastBlock = true;
            return;
        }

        // The last block is already in a chain, or is over the threshold to resize. Shallow-clone the last block (clone
        // just its metadata, while still pointing to the original buffer), and chain it to a new last block.
        Recycler *const recycler = GetLibrary()->GetRecycler();
        Block *const newLastBlock = lastBlock->Clone(LastBlockCharLength(), recycler)->Chain(recycler);
        lastBlockInfo.CopyFrom(newLastBlock);
        ownsLastBlock = true;
        this->lastBlock = newLastBlock;
    }

    void CompoundString::Unreference()
    {LOGMEIN("CompoundString.cpp] 996\n");
        lastBlockInfo.Unreference();
        directCharLength = 0;
        ownsLastBlock = false;
        lastBlock = nullptr;
    }

    const char16 *CompoundString::GetSz()
    {LOGMEIN("CompoundString.cpp] 1004\n");
        Assert(!IsFinalized());

        const CharCount totalCharLength = GetLength();
        switch(totalCharLength)
        {LOGMEIN("CompoundString.cpp] 1009\n");
            case 0:
            {LOGMEIN("CompoundString.cpp] 1011\n");
                Unreference();
                const char16 *const buffer = _u("");
                SetBuffer(buffer);
                VirtualTableInfo<LiteralString>::SetVirtualTable(this);
                return buffer;
            }

            case 1:
            {LOGMEIN("CompoundString.cpp] 1020\n");
                Assert(HasOnlyDirectChars());
                Assert(LastBlockCharLength() == 1);

                const char16 *const buffer = GetLibrary()->GetCharStringCache().GetStringForChar(LastBlockChars()[0])->UnsafeGetBuffer();
                Unreference();
                SetBuffer(buffer);
                VirtualTableInfo<LiteralString>::SetVirtualTable(this);
                return buffer;
            }
        }

        if(OwnsLastBlock() && HasOnlyDirectChars() && !lastBlock && TryAppendGeneric(_u('\0'), this)) // GetSz() requires null termination
        {LOGMEIN("CompoundString.cpp] 1033\n");
            // There is no last block. Only the buffer was allocated, and is held in 'lastBlockInfo'. Since this string owns the
            // last block, has only direct chars, and the buffer was allocated directly (buffer pointer is not an internal
            // pointer), there is no need to copy the buffer.
            SetLength(totalCharLength); // terminating null should not count towards the string length
            const char16 *const buffer = LastBlockChars();
            Unreference();
            SetBuffer(buffer);
            VirtualTableInfo<LiteralString>::SetVirtualTable(this);
            return buffer;
        }

        char16 *const buffer = RecyclerNewArrayLeaf(GetScriptContext()->GetRecycler(), char16, SafeSzSize(totalCharLength));
        buffer[totalCharLength] = _u('\0'); // GetSz() requires null termination
        Copy<CompoundString>(buffer, totalCharLength);
        Assert(buffer[totalCharLength] == _u('\0'));
        Unreference();
        SetBuffer(buffer);
        VirtualTableInfo<LiteralString>::SetVirtualTable(this);
        return buffer;
    }

    void CompoundString::CopyVirtual(
        _Out_writes_(m_charLength) char16 *const buffer,
        StringCopyInfoStack &nestedStringTreeCopyInfos,
        const byte recursionDepth)
    {LOGMEIN("CompoundString.cpp] 1059\n");
        Assert(!IsFinalized());
        Assert(buffer);

        const CharCount totalCharLength = GetLength();
        switch(totalCharLength)
        {LOGMEIN("CompoundString.cpp] 1065\n");
            case 0:
                return;

            case 1:
                Assert(HasOnlyDirectChars());
                Assert(LastBlockCharLength() == 1);

                buffer[0] = LastBlockChars()[0];
                return;
        }

        // Copy buffers from string pointers
        const bool hasOnlyDirectChars = HasOnlyDirectChars();
        const CharCount directCharLength = hasOnlyDirectChars ? totalCharLength : this->directCharLength;
        CharCount remainingCharLengthToCopy = totalCharLength;
        const Block *const lastBlock = this->lastBlock;
        const Block *block = lastBlock;
        Field(void*) const *blockPointers = LastBlockPointers();
        CharCount pointerIndex = LastBlockPointerLength();
        while(remainingCharLengthToCopy > directCharLength)
        {LOGMEIN("CompoundString.cpp] 1086\n");
            while(pointerIndex == 0)
            {LOGMEIN("CompoundString.cpp] 1088\n");
                Assert(block);
                block = block->Previous();
                Assert(block);
                blockPointers = (Field(void*) const *)block->Pointers();
                pointerIndex = block->PointerLength();
            }

            void *const pointer = blockPointers[--pointerIndex];
            if(IsPackedInfo(pointer))
            {LOGMEIN("CompoundString.cpp] 1098\n");
                Assert(pointerIndex != 0);
                void *pointer2 = blockPointers[--pointerIndex];
                JavascriptString *s;
    #if defined(_M_X64_OR_ARM64)
                Assert(!IsPackedInfo(pointer2));
    #else
                if(IsPackedInfo(pointer2))
                {LOGMEIN("CompoundString.cpp] 1106\n");
                    Assert(pointerIndex != 0);
                    s = JavascriptString::FromVar(blockPointers[--pointerIndex]);
                }
                else
    #endif
                {
                    s = JavascriptString::FromVar(pointer2);
                    pointer2 = nullptr;
                }

                CharCount startIndex, copyCharLength;
                UnpackSubstringInfo(pointer, pointer2, &startIndex, &copyCharLength);
                Assert(startIndex <= s->GetLength());
                Assert(copyCharLength <= s->GetLength() - startIndex);

                Assert(remainingCharLengthToCopy >= copyCharLength);
                remainingCharLengthToCopy -= copyCharLength;
                CopyHelper(&buffer[remainingCharLengthToCopy], &s->GetString()[startIndex], copyCharLength);
            }
            else
            {
                JavascriptString *const s = JavascriptString::FromVar(pointer);
                const CharCount copyCharLength = s->GetLength();

                Assert(remainingCharLengthToCopy >= copyCharLength);
                remainingCharLengthToCopy -= copyCharLength;
                if(recursionDepth == MaxCopyRecursionDepth && s->IsTree())
                {LOGMEIN("CompoundString.cpp] 1134\n");
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

        Assert(remainingCharLengthToCopy == directCharLength);
        if(remainingCharLengthToCopy != 0)
        {LOGMEIN("CompoundString.cpp] 1150\n");
            // Determine the number of direct chars in the current block
            CharCount blockCharLength;
            if(pointerIndex == 0)
            {LOGMEIN("CompoundString.cpp] 1154\n");
                // The string switched to pointer mode at the beginning of the current block, or the string never switched to
                // pointer mode and the last block is empty. In either case, direct chars span to the end of the previous block.
                Assert(block);
                block = block->Previous();
                Assert(block);
                blockCharLength = block->CharLength();
            }
            else if(hasOnlyDirectChars)
            {LOGMEIN("CompoundString.cpp] 1163\n");
                // The string never switched to pointer mode, so the current block's char length is where direct chars end
                blockCharLength = block == lastBlock ? LastBlockCharLength() : block->CharLength();
            }
            else
            {
                // The string switched to pointer mode somewhere in the middle of the current block. To determine where direct
                // chars end in this block, all previous blocks are scanned and their char lengths discounted.
                blockCharLength = remainingCharLengthToCopy;
                if(block)
                {LOGMEIN("CompoundString.cpp] 1173\n");
                    for(const Block *previousBlock = block->Previous();
                        previousBlock;
                        previousBlock = previousBlock->Previous())
                    {LOGMEIN("CompoundString.cpp] 1177\n");
                        Assert(blockCharLength >= previousBlock->CharLength());
                        blockCharLength -= previousBlock->CharLength();
                    }
                }
                Assert(Block::PointerLengthFromCharLength(blockCharLength) == pointerIndex);
            }

            // Copy direct chars
            const char16 *blockChars = block == lastBlock ? LastBlockChars() : block->Chars();
            while(true)
            {LOGMEIN("CompoundString.cpp] 1188\n");
                if(blockCharLength != 0)
                {LOGMEIN("CompoundString.cpp] 1190\n");
                    Assert(remainingCharLengthToCopy >= blockCharLength);
                    remainingCharLengthToCopy -= blockCharLength;
                    // SWB: this is copying "direct chars" and there should be no pointers here. No write barrier needed.
                    js_wmemcpy_s(&buffer[remainingCharLengthToCopy], blockCharLength, blockChars, blockCharLength);
                    if(remainingCharLengthToCopy == 0)
                        break;
                }

                Assert(block);
                block = block->Previous();
                Assert(block);
                blockChars = block->Chars();
                blockCharLength = block->CharLength();
            }
        }

    #if DBG
        // Verify that all nonempty blocks have been visited
        if(block)
        {LOGMEIN("CompoundString.cpp] 1210\n");
            while(true)
            {LOGMEIN("CompoundString.cpp] 1212\n");
                block = block->Previous();
                if(!block)
                    break;
                Assert(block->CharLength() == 0);
            }
        }
    #endif

        Assert(remainingCharLengthToCopy == 0);
    }

    bool CompoundString::IsTree() const
    {LOGMEIN("CompoundString.cpp] 1225\n");
        Assert(!IsFinalized());

        return !HasOnlyDirectChars();
    }

    DEFINE_RECYCLER_TRACKER_PERF_COUNTER(CompoundString);

    CompileAssert(static_cast<CharCount>(-1) > static_cast<CharCount>(0)); // CharCount is assumed to be unsigned

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    #endif
    #pragma endregion
}
