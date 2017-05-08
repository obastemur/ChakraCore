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
    {TRACE_IT(54529);
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
    {TRACE_IT(54530);
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
    {TRACE_IT(54531);
        // This only does a shallow copy. The metadata is copied, and a reference to the other block is included in this copy
        // for access to the other block's buffer.
        Assert(usedCharLength <= other.charCapacity);
    }

    CompoundString::Block *CompoundString::Block::New(
        const uint size,
        const Block *const previous,
        Recycler *const recycler)
    {TRACE_IT(54532);
        Assert(HeapInfo::IsAlignedSize(size));
        Assert(recycler);

        return RecyclerNewPlus(recycler, size - sizeof(Block), Block, CharCapacityFromSize(size), previous);
    }

    CompoundString::Block *CompoundString::Block::New(
        const void *const buffer,
        const CharCount usedCharLength,
        const bool reserveMoreSpace,
        Recycler *const recycler)
    {TRACE_IT(54533);
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
    {TRACE_IT(54534);
        Assert(recycler);

        return RecyclerNew(recycler, Block, *this, usedCharLength);
    }

    CharCount CompoundString::Block::CharCapacityFromSize(const uint size)
    {TRACE_IT(54535);
        Assert(size >= sizeof(Block));

        return (size - sizeof(Block)) / sizeof(char16);
    }

    uint CompoundString::Block::SizeFromCharCapacity(const CharCount charCapacity)
    {TRACE_IT(54536);
        Assert(IsValidCharCount(charCapacity));
        return UInt32Math::Add(sizeof(Block), charCapacity * sizeof(char16));
    }

    #endif

    inline CharCount CompoundString::Block::PointerAlign(const CharCount charLength)
    {TRACE_IT(54537);
        const CharCount alignedCharLength = ::Math::Align(charLength, static_cast<CharCount>(sizeof(void *) / sizeof(char16)));
        Assert(alignedCharLength >= charLength);
        return alignedCharLength;
    }

    inline const char16 *CompoundString::Block::Chars(const void *const buffer)
    {TRACE_IT(54538);
        return static_cast<const char16 *>(buffer);
    }

    #ifndef IsJsDiag

    char16 *CompoundString::Block::Chars(void *const buffer)
    {TRACE_IT(54539);
        return static_cast<char16 *>(buffer);
    }

    const Field(void*) *CompoundString::Block::Pointers(const void *const buffer)
    {TRACE_IT(54540);
        return (const Field(void*)*)(buffer);
    }

    Field(void*) *CompoundString::Block::Pointers(void *const buffer)
    {TRACE_IT(54541);
        return static_cast<Field(void*)*>(buffer);
    }

    CharCount CompoundString::Block::PointerCapacityFromCharCapacity(const CharCount charCapacity)
    {TRACE_IT(54542);
        return charCapacity / (sizeof(void *) / sizeof(char16));
    }

    CharCount CompoundString::Block::CharCapacityFromPointerCapacity(const CharCount pointerCapacity)
    {TRACE_IT(54543);
        return pointerCapacity * (sizeof(void *) / sizeof(char16));
    }

    #endif

    // ChakraDiag includes CompoundString.cpp as a header file so this method needs to be marked as inline
    // to handle that case
    JS_DIAG_INLINE CharCount CompoundString::Block::PointerLengthFromCharLength(const CharCount charLength)
    {TRACE_IT(54544);
        return PointerAlign(charLength) / (sizeof(void *) / sizeof(char16));
    }

    #ifndef IsJsDiag

    CharCount CompoundString::Block::CharLengthFromPointerLength(const CharCount pointerLength)
    {TRACE_IT(54545);
        return pointerLength * (sizeof(void *) / sizeof(char16));
    }

    uint CompoundString::Block::SizeFromUsedCharLength(const CharCount usedCharLength)
    {TRACE_IT(54546);
        const size_t usedSize = SizeFromCharCapacity(usedCharLength);
        const size_t alignedUsedSize = HeapInfo::GetAlignedSizeNoCheck(usedSize);
        if (alignedUsedSize != (uint)alignedUsedSize)
        {TRACE_IT(54547);
            Js::Throw::OutOfMemory();
        }
        return (uint)alignedUsedSize;
    }

    bool CompoundString::Block::ShouldAppendChars(
        const CharCount appendCharLength,
        const uint additionalSizeForPointerAppend)
    {TRACE_IT(54548);
        // Append characters instead of pointers when it would save space. Add some buffer as well, as flattening becomes more
        // expensive after the switch to pointer mode.
        //
        // 'additionalSizeForPointerAppend' should be provided when appending a pointer also involves creating a string object
        // or some other additional space (such as LiteralString, in which case this parameter should be sizeof(LiteralString)),
        // as that additional size also needs to be taken into account.
        return appendCharLength <= (sizeof(void *) * 2 + additionalSizeForPointerAppend) / sizeof(char16);
    }

    const void *CompoundString::Block::Buffer() const
    {TRACE_IT(54549);
        return bufferOwner + 1;
    }

    void *CompoundString::Block::Buffer()
    {TRACE_IT(54550);
        return bufferOwner + 1;
    }

    const CompoundString::Block *CompoundString::Block::Previous() const
    {TRACE_IT(54551);
        return previous;
    }

    const char16 *CompoundString::Block::Chars() const
    {TRACE_IT(54552);
        return Chars(Buffer());
    }

    char16 *CompoundString::Block::Chars()
    {TRACE_IT(54553);
        return Chars(Buffer());
    }

    CharCount CompoundString::Block::CharLength() const
    {TRACE_IT(54554);
        return charLength;
    }

    void CompoundString::Block::SetCharLength(const CharCount charLength)
    {TRACE_IT(54555);
        Assert(charLength <= CharCapacity());

        this->charLength = charLength;
    }

    CharCount CompoundString::Block::CharCapacity() const
    {TRACE_IT(54556);
        return charCapacity;
    }

    const Field(void*) *CompoundString::Block::Pointers() const
    {TRACE_IT(54557);
        return Pointers(Buffer());
    }

    Field(void*) *CompoundString::Block::Pointers()
    {TRACE_IT(54558);
        return Pointers(Buffer());
    }

    CharCount CompoundString::Block::PointerLength() const
    {TRACE_IT(54559);
        return PointerLengthFromCharLength(CharLength());
    }

    CharCount CompoundString::Block::PointerCapacity() const
    {TRACE_IT(54560);
        return PointerCapacityFromCharCapacity(CharCapacity());
    }

    uint CompoundString::Block::GrowSize(const uint size)
    {TRACE_IT(54561);
        Assert(size >= sizeof(Block));
        Assert(HeapInfo::IsAlignedSize(size));

        const uint newSize = size << 1;
        Assert(newSize > size);
        return newSize;
    }

    uint CompoundString::Block::GrowSizeForChaining(const uint size)
    {TRACE_IT(54562);
        const uint newSize = GrowSize(size);
        return min(MaxChainedBlockSize, newSize);
    }

    CompoundString::Block *CompoundString::Block::Chain(Recycler *const recycler)
    {TRACE_IT(54563);
        return New(GrowSizeForChaining(SizeFromUsedCharLength(CharLength())), this, recycler);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    #endif
    #pragma endregion

    #pragma region CompoundString::BlockInfo
    #ifndef IsJsDiag
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    CompoundString::BlockInfo::BlockInfo() : buffer(nullptr), charLength(0), charCapacity(0)
    {TRACE_IT(54564);
    }

    CompoundString::BlockInfo::BlockInfo(Block *const block)
    {TRACE_IT(54565);
        CopyFrom(block);
    }

    char16 *CompoundString::BlockInfo::Chars() const
    {TRACE_IT(54566);
        return Block::Chars(buffer);
    }

    CharCount CompoundString::BlockInfo::CharLength() const
    {TRACE_IT(54567);
        return charLength;
    }

    void CompoundString::BlockInfo::SetCharLength(const CharCount charLength)
    {TRACE_IT(54568);
        Assert(charLength <= CharCapacity());

        this->charLength = charLength;
    }

    CharCount CompoundString::BlockInfo::CharCapacity() const
    {TRACE_IT(54569);
        return charCapacity;
    }

    Field(void*) *CompoundString::BlockInfo::Pointers() const
    {TRACE_IT(54570);
        return Block::Pointers(buffer);
    }

    CharCount CompoundString::BlockInfo::PointerLength() const
    {TRACE_IT(54571);
        return Block::PointerLengthFromCharLength(CharLength());
    }

    void CompoundString::BlockInfo::SetPointerLength(const CharCount pointerLength)
    {TRACE_IT(54572);
        Assert(pointerLength <= PointerCapacity());

        charLength = Block::CharLengthFromPointerLength(pointerLength);
    }

    CharCount CompoundString::BlockInfo::PointerCapacity() const
    {TRACE_IT(54573);
        return Block::PointerCapacityFromCharCapacity(CharCapacity());
    }

    CharCount CompoundString::BlockInfo::AlignCharCapacityForAllocation(const CharCount charCapacity)
    {TRACE_IT(54574);
        const CharCount alignedCharCapacity =
            ::Math::AlignOverflowCheck(
                charCapacity == 0 ? static_cast<CharCount>(1) : charCapacity,
                static_cast<CharCount>(HeapConstants::ObjectGranularity / sizeof(char16)));
        Assert(alignedCharCapacity != 0);
        return alignedCharCapacity;
    }

    CharCount CompoundString::BlockInfo::GrowCharCapacity(const CharCount charCapacity)
    {TRACE_IT(54575);
        Assert(charCapacity != 0);
        Assert(AlignCharCapacityForAllocation(charCapacity) == charCapacity);

        const CharCount newCharCapacity = UInt32Math::Mul<2>(charCapacity);
        Assert(newCharCapacity > charCapacity);
        return newCharCapacity;
    }

    bool CompoundString::BlockInfo::ShouldAllocateBuffer(const CharCount charCapacity)
    {TRACE_IT(54576);
        Assert(charCapacity != 0);
        Assert(AlignCharCapacityForAllocation(charCapacity) == charCapacity);

        return charCapacity < Block::ChainSizeThreshold / sizeof(char16);
    }

    void CompoundString::BlockInfo::AllocateBuffer(const CharCount charCapacity, Recycler *const recycler)
    {TRACE_IT(54577);
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
    {TRACE_IT(54578);
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
    {TRACE_IT(54579);
        Assert(recycler);

        const CharCount newCharCapacity = GrowCharCapacity(AlignCharCapacityForAllocation(CharLength()));
        if(ShouldAllocateBuffer(newCharCapacity))
        {TRACE_IT(54580);
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
    {TRACE_IT(54581);
        buffer = block->Buffer();
        charLength = block->CharLength();
        charCapacity = block->CharCapacity();
    }

    void CompoundString::BlockInfo::CopyTo(Block *const block)
    {TRACE_IT(54582);
        Assert(block->Buffer() == buffer);
        Assert(block->CharLength() <= charLength);
        Assert(block->CharCapacity() == charCapacity);

        block->SetCharLength(charLength);
    }

    void CompoundString::BlockInfo::Unreference()
    {TRACE_IT(54583);
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
    {TRACE_IT(54584);
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
    {TRACE_IT(54585);
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
    {TRACE_IT(54586);
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
    {TRACE_IT(54587);
        Assert(!other.IsFinalized());

        SetLength(other.GetLength());

        if(forAppending)
        {TRACE_IT(54588);
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
    {TRACE_IT(54589);
        const CharCount alignedInitialCharCapacity = BlockInfo::AlignCharCapacityForAllocation(initialCharCapacity);
        if(BlockInfo::ShouldAllocateBuffer(alignedInitialCharCapacity))
            return NewWithBufferCharCapacity(alignedInitialCharCapacity, library);
        return NewWithBlockSize(Block::SizeFromUsedCharLength(initialCharCapacity), library);
    }

    CompoundString *CompoundString::NewWithPointerCapacity(
        const CharCount initialPointerCapacity,
        JavascriptLibrary *const library)
    {TRACE_IT(54590);
        return NewWithCharCapacity(Block::CharCapacityFromPointerCapacity(initialPointerCapacity), library);
    }

    CompoundString *CompoundString::NewWithBufferCharCapacity(const CharCount initialCharCapacity, JavascriptLibrary *const library)
    {TRACE_IT(54591);
        Assert(library);

        return RecyclerNew(library->GetRecycler(), CompoundString, initialCharCapacity, library);
    }

    CompoundString *CompoundString::NewWithBlockSize(const CharCount initialBlockSize, JavascriptLibrary *const library)
    {TRACE_IT(54592);
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
    {TRACE_IT(54593);
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
    {TRACE_IT(54594);
        return RecyclerNew(GetLibrary()->GetRecycler(), CompoundString, *this, forAppending);
    }

    CompoundString * CompoundString::JitClone(CompoundString * cs)
    {TRACE_IT(54595);
        Assert(Is(cs));
        return cs->Clone(false);
    }

    CompoundString * CompoundString::JitCloneForAppending(CompoundString * cs)
    {TRACE_IT(54596);
        Assert(Is(cs));
        return cs->Clone(true);
    }

    bool CompoundString::Is(RecyclableObject *const object)
    {TRACE_IT(54597);
        return VirtualTableInfo<CompoundString>::HasVirtualTable(object);
    }

    bool CompoundString::Is(const Var var)
    {TRACE_IT(54598);
        return RecyclableObject::Is(var) && Is(RecyclableObject::FromVar(var));
    }

    CompoundString *CompoundString::FromVar(RecyclableObject *const object)
    {TRACE_IT(54599);
        Assert(Is(object));

        CompoundString *const cs = static_cast<CompoundString *>(object);
        Assert(!cs->IsFinalized());
        return cs;
    }

    CompoundString *CompoundString::FromVar(const Var var)
    {TRACE_IT(54600);
        return FromVar(RecyclableObject::FromVar(var));
    }

    JavascriptString *CompoundString::GetImmutableOrScriptUnreferencedString(JavascriptString *const s)
    {TRACE_IT(54601);
        Assert(s);

        // The provided string may be referenced by script code. A script-unreferenced version of the string is being requested,
        // likely because the provided string will be referenced directly in a concatenation operation (by ConcatString or
        // another CompoundString, for instance). If the provided string is a CompoundString, it must not be mutated by script
        // code after the concatenation operation. In that case, clone the string to ensure that it is not referenced by script
        // code. If the clone is never handed back to script code, it effectively behaves as an immutable string.
        return Is(s) ? FromVar(s)->Clone(false) : s;
    }

    bool CompoundString::ShouldAppendChars(const CharCount appendCharLength)
    {TRACE_IT(54602);
        return Block::ShouldAppendChars(appendCharLength);
    }

    bool CompoundString::HasOnlyDirectChars() const
    {TRACE_IT(54603);
        return directCharLength == static_cast<CharCount>(-1);
    }

    void CompoundString::SwitchToPointerMode()
    {TRACE_IT(54604);
        Assert(HasOnlyDirectChars());

        directCharLength = GetLength();

        if(PHASE_TRACE_StringConcat)
        {TRACE_IT(54605);
            Output::Print(_u("CompoundString::SwitchToPointerMode()\n"));
            Output::Flush();
        }
    }

    bool CompoundString::OwnsLastBlock() const
    {TRACE_IT(54606);
        return ownsLastBlock;
    }

    const char16 *CompoundString::GetAppendStringBuffer(JavascriptString *const s) const
    {TRACE_IT(54607);
        Assert(s);

        // A compound string cannot flatten itself while appending itself to itself since flattening would make the append
        // illegal. Clone the string being appended if necessary, before flattening.
        return s == this ? FromVar(s)->Clone(false)->GetSz() : s->GetString();
    }

    char16 *CompoundString::LastBlockChars() const
    {TRACE_IT(54608);
        return lastBlockInfo.Chars();
    }

    CharCount CompoundString::LastBlockCharLength() const
    {TRACE_IT(54609);
        return lastBlockInfo.CharLength();
    }

    void CompoundString::SetLastBlockCharLength(const CharCount charLength)
    {TRACE_IT(54610);
        lastBlockInfo.SetCharLength(charLength);
    }

    CharCount CompoundString::LastBlockCharCapacity() const
    {TRACE_IT(54611);
        return lastBlockInfo.CharCapacity();
    }

    Field(void*) *CompoundString::LastBlockPointers() const
    {TRACE_IT(54612);
        return lastBlockInfo.Pointers();
    }

    CharCount CompoundString::LastBlockPointerLength() const
    {TRACE_IT(54613);
        return lastBlockInfo.PointerLength();
    }

    void CompoundString::SetLastBlockPointerLength(const CharCount pointerLength)
    {TRACE_IT(54614);
        lastBlockInfo.SetPointerLength(pointerLength);
    }

    CharCount CompoundString::LastBlockPointerCapacity() const
    {TRACE_IT(54615);
        return lastBlockInfo.PointerCapacity();
    }

    void CompoundString::PackSubstringInfo(
        const CharCount startIndex,
        const CharCount length,
        void * *const packedSubstringInfoRef,
        void * *const packedSubstringInfo2Ref)
    {TRACE_IT(54616);
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
        {TRACE_IT(54617);
            *packedSubstringInfoRef =
                reinterpret_cast<void *>(
                    (static_cast<uintptr_t>(startIndex) << 16) +
                    (static_cast<uintptr_t>(length) << 1) +
                    1);
            *packedSubstringInfo2Ref = nullptr;
        }
        else
        {TRACE_IT(54618);
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
    {TRACE_IT(54619);
        Assert(pointer);

        return reinterpret_cast<uintptr_t>(pointer) & 1;
    }

    inline void CompoundString::UnpackSubstringInfo(
        void *const pointer,
        void *const pointer2,
        CharCount *const startIndexRef,
        CharCount *const lengthRef)
    {TRACE_IT(54620);
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
        {TRACE_IT(54621);
            *startIndexRef = static_cast<CharCount>(packedSubstringInfo >> 16);
            *lengthRef = static_cast<CharCount>(static_cast<uint16>(packedSubstringInfo) >> 1);
        }
        else
        {TRACE_IT(54622);
            *startIndexRef = static_cast<CharCount>(packedSubstringInfo >> 1);
            const uintptr_t packedSubstringInfo2 = reinterpret_cast<uintptr_t>(pointer2);
            Assert(packedSubstringInfo2 & 1);
            *lengthRef = static_cast<CharCount>(packedSubstringInfo2 >> 1);
        }
    #endif
    }

    #ifndef IsJsDiag

    void CompoundString::AppendSlow(const char16 c)
    {TRACE_IT(54623);
        Grow();
        const bool appended =
            HasOnlyDirectChars()
                ? TryAppendGeneric(c, this)
                : TryAppendGeneric(GetLibrary()->GetCharStringCache().GetStringForChar(c), 1, this);
        Assert(appended);
    }

    void CompoundString::AppendSlow(JavascriptString *const s)
    {TRACE_IT(54624);
        Grow();
        const bool appended = TryAppendGeneric(s, s->GetLength(), this);
        Assert(appended);
    }

    void CompoundString::AppendSlow(
        __in_xcount(appendCharLength) const char16 *const s,
        const CharCount appendCharLength)
    {TRACE_IT(54625);
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
        {TRACE_IT(54626);
            const CharCount blockCharLength = LastBlockCharLength();
            const CharCount copyCharLength =
                min(LastBlockCharCapacity() - blockCharLength, appendCharLength - copiedCharLength);
            CopyHelper(&LastBlockChars()[blockCharLength], &s[copiedCharLength], copyCharLength);
            SetLastBlockCharLength(blockCharLength + copyCharLength);
            copiedCharLength += copyCharLength;
            if(copiedCharLength >= appendCharLength)
                break;
            try
            {TRACE_IT(54627);
                Grow();
            }
            catch(...)
            {TRACE_IT(54628);
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
    {TRACE_IT(54629);
        Grow();
        const bool appended = TryAppendGeneric(s, packedSubstringInfo, packedSubstringInfo2, appendCharLength, this);
        Assert(appended);
    }

    void CompoundString::PrepareForAppend()
    {TRACE_IT(54630);
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
    {TRACE_IT(54631);
        size_t len = wcslen(s);
        // We limit the length of the string to MaxCharCount,
        // so just OOM if we are appending a string that exceed this limit already
        if (!IsValidCharCount(len))
        {TRACE_IT(54632);
            JavascriptExceptionOperators::ThrowOutOfMemory(this->GetScriptContext());
        }
        AppendChars(s, (CharCount)len);
    }

    void CompoundString::Grow()
    {TRACE_IT(54633);
        Assert(!IsFinalized());
        Assert(OwnsLastBlock());

        Block *const lastBlock = this->lastBlock;
        if(!lastBlock)
        {TRACE_IT(54634);
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
    {TRACE_IT(54635);
        Assert(!IsFinalized());
        Assert(!OwnsLastBlock());

        // Another string object owns the last block's buffer. The buffer must be copied, or another block must be chained.

        Block *const lastBlock = this->lastBlock;
        if(!lastBlock)
        {TRACE_IT(54636);
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
    {TRACE_IT(54637);
        lastBlockInfo.Unreference();
        directCharLength = 0;
        ownsLastBlock = false;
        lastBlock = nullptr;
    }

    const char16 *CompoundString::GetSz()
    {TRACE_IT(54638);
        Assert(!IsFinalized());

        const CharCount totalCharLength = GetLength();
        switch(totalCharLength)
        {
            case 0:
            {TRACE_IT(54639);
                Unreference();
                const char16 *const buffer = _u("");
                SetBuffer(buffer);
                VirtualTableInfo<LiteralString>::SetVirtualTable(this);
                return buffer;
            }

            case 1:
            {TRACE_IT(54640);
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
        {TRACE_IT(54641);
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
    {TRACE_IT(54642);
        Assert(!IsFinalized());
        Assert(buffer);

        const CharCount totalCharLength = GetLength();
        switch(totalCharLength)
        {
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
        {TRACE_IT(54643);
            while(pointerIndex == 0)
            {TRACE_IT(54644);
                Assert(block);
                block = block->Previous();
                Assert(block);
                blockPointers = (Field(void*) const *)block->Pointers();
                pointerIndex = block->PointerLength();
            }

            void *const pointer = blockPointers[--pointerIndex];
            if(IsPackedInfo(pointer))
            {TRACE_IT(54645);
                Assert(pointerIndex != 0);
                void *pointer2 = blockPointers[--pointerIndex];
                JavascriptString *s;
    #if defined(_M_X64_OR_ARM64)
                Assert(!IsPackedInfo(pointer2));
    #else
                if(IsPackedInfo(pointer2))
                {TRACE_IT(54646);
                    Assert(pointerIndex != 0);
                    s = JavascriptString::FromVar(blockPointers[--pointerIndex]);
                }
                else
    #endif
                {TRACE_IT(54647);
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
            {TRACE_IT(54648);
                JavascriptString *const s = JavascriptString::FromVar(pointer);
                const CharCount copyCharLength = s->GetLength();

                Assert(remainingCharLengthToCopy >= copyCharLength);
                remainingCharLengthToCopy -= copyCharLength;
                if(recursionDepth == MaxCopyRecursionDepth && s->IsTree())
                {TRACE_IT(54649);
                    // Don't copy nested string trees yet, as that involves a recursive call, and the recursion can become
                    // excessive. Just collect the nested string trees and the buffer location where they should be copied, and
                    // the caller can deal with those after returning.
                    nestedStringTreeCopyInfos.Push(StringCopyInfo(s, &buffer[remainingCharLengthToCopy]));
                }
                else
                {TRACE_IT(54650);
                    Assert(recursionDepth <= MaxCopyRecursionDepth);
                    s->Copy(&buffer[remainingCharLengthToCopy], nestedStringTreeCopyInfos, recursionDepth + 1);
                }
            }
        }

        Assert(remainingCharLengthToCopy == directCharLength);
        if(remainingCharLengthToCopy != 0)
        {TRACE_IT(54651);
            // Determine the number of direct chars in the current block
            CharCount blockCharLength;
            if(pointerIndex == 0)
            {TRACE_IT(54652);
                // The string switched to pointer mode at the beginning of the current block, or the string never switched to
                // pointer mode and the last block is empty. In either case, direct chars span to the end of the previous block.
                Assert(block);
                block = block->Previous();
                Assert(block);
                blockCharLength = block->CharLength();
            }
            else if(hasOnlyDirectChars)
            {TRACE_IT(54653);
                // The string never switched to pointer mode, so the current block's char length is where direct chars end
                blockCharLength = block == lastBlock ? LastBlockCharLength() : block->CharLength();
            }
            else
            {TRACE_IT(54654);
                // The string switched to pointer mode somewhere in the middle of the current block. To determine where direct
                // chars end in this block, all previous blocks are scanned and their char lengths discounted.
                blockCharLength = remainingCharLengthToCopy;
                if(block)
                {TRACE_IT(54655);
                    for(const Block *previousBlock = block->Previous();
                        previousBlock;
                        previousBlock = previousBlock->Previous())
                    {TRACE_IT(54656);
                        Assert(blockCharLength >= previousBlock->CharLength());
                        blockCharLength -= previousBlock->CharLength();
                    }
                }
                Assert(Block::PointerLengthFromCharLength(blockCharLength) == pointerIndex);
            }

            // Copy direct chars
            const char16 *blockChars = block == lastBlock ? LastBlockChars() : block->Chars();
            while(true)
            {TRACE_IT(54657);
                if(blockCharLength != 0)
                {TRACE_IT(54658);
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
        {TRACE_IT(54659);
            while(true)
            {TRACE_IT(54660);
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
    {TRACE_IT(54661);
        Assert(!IsFinalized());

        return !HasOnlyDirectChars();
    }

    DEFINE_RECYCLER_TRACKER_PERF_COUNTER(CompoundString);

    CompileAssert(static_cast<CharCount>(-1) > static_cast<CharCount>(0)); // CharCount is assumed to be unsigned

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    #endif
    #pragma endregion
}
