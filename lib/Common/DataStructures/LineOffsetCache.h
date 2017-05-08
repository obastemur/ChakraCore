//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace JsUtil
{
    template <typename TAllocator>
    class LineOffsetCache
    {
    public:
        // Stores line offset information for a line in the source info.  The index of this
        // item in the LineOffsetCacheList determines which line it is.
        struct LineOffsetCacheItem
        {
            // The character offset of where the line begins starting from the start of the source info.
            charcount_t characterOffset;

            // The byte offset of where the line begins starting from the start of the source info's character buffer (UTF8).
            charcount_t byteOffset;
        };
    private:
        typedef List<LineOffsetCacheItem, TAllocator, true /*isLeaf*/> LineOffsetCacheList;
        typedef ReadOnlyList<LineOffsetCacheItem> LineOffsetCacheReadOnlyList;

    public:

        static int FindLineForCharacterOffset(
            _In_z_ LPCUTF8 sourceStartCharacter,
            _In_z_ LPCUTF8 sourceEndCharacter,
            charcount_t &inOutLineCharOffset,
            charcount_t &inOutByteOffset,
            charcount_t characterOffset)
        {TRACE_IT(21760);
            int lastLine = 0;

            while (FindNextLine(sourceStartCharacter, sourceEndCharacter, inOutLineCharOffset, inOutByteOffset, characterOffset))
            {TRACE_IT(21761);
                lastLine++;
            }

            return lastLine;
        }

        LineOffsetCache(TAllocator* allocator,
            _In_z_ LPCUTF8 sourceStartCharacter,
            _In_z_ LPCUTF8 sourceEndCharacter,
            charcount_t startingCharacterOffset = 0,
            charcount_t startingByteOffset = 0) :
            allocator(allocator),
            isCacheBuilt(false)
        {
            AssertMsg(allocator, "An allocator must be supplied to the cache for allocation of items.");
            LineOffsetCacheList *list = AllocatorNew(TAllocator, allocator, LineOffsetCacheList, allocator);
            this->lineOffsetCacheList = list;
            this->BuildCache(list, sourceStartCharacter, sourceEndCharacter, startingCharacterOffset, startingByteOffset);
        }

        LineOffsetCache(TAllocator *allocator,
            _In_reads_(numberOfLines) const LineOffsetCacheItem *lines,
            __in int numberOfLines) :
            allocator(allocator),
            isCacheBuilt(false)
        {TRACE_IT(21762);
            this->lineOffsetCacheList = LineOffsetCacheReadOnlyList::New(allocator, (LineOffsetCacheItem *)lines, numberOfLines);
        }

        ~LineOffsetCache()
        {TRACE_IT(21763);
            if (this->lineOffsetCacheList != nullptr)
            {TRACE_IT(21764);
                this->lineOffsetCacheList->Delete();
            }
        }

        // outLineCharOffset - The character offset of the start of the line returned
        int GetLineForCharacterOffset(charcount_t characterOffset, charcount_t *outLineCharOffset, charcount_t *outByteOffset)
        {TRACE_IT(21765);
            Assert(this->lineOffsetCacheList->Count() > 0);

            // The list is sorted, so binary search to find the line info.
            int closestIndex = -1;
            int minRange = INT_MAX;

            this->lineOffsetCacheList->BinarySearch([&](const LineOffsetCacheItem& item, int index)
            {
                int offsetRange = characterOffset - item.characterOffset;
                if (offsetRange >= 0)
                {TRACE_IT(21766);
                    if (offsetRange < minRange)
                    {TRACE_IT(21767);
                        // There are potentially many lines with starting offsets greater than the one we're searching
                        // for.  As a result, we should track which index we've encountered so far that is the closest
                        // to the offset we're looking for without going under.  This will find the line that contains
                        // the offset.
                        closestIndex = index;
                        minRange = offsetRange;
                    }

                    // Search lower to see if we can find a closer index.
                    return -1;
                }
                else
                {TRACE_IT(21768);
                    // Search higher to get into a range that is greater than the offset.
                    return 1;
                }

                // Note that we purposely don't return 0 (==) here.  We want the search to end in failure (-1) because
                // we're searching for the closest element, not necessarily an exact element offset.  Exact offsets
                // are possible when the offset we're searching for is the first character of the line, but that will
                // be handled by the if statement above.
            });

            if (closestIndex >= 0)
            {TRACE_IT(21769);
                LineOffsetCacheItem lastItem = this->lineOffsetCacheList->Item(closestIndex);

                if (outLineCharOffset != nullptr)
                {TRACE_IT(21770);
                    *outLineCharOffset = lastItem.characterOffset;
                }

                if (outByteOffset != nullptr)
                {TRACE_IT(21771);
                    *outByteOffset = lastItem.byteOffset;
                }
            }

            return closestIndex;
        }

        charcount_t GetCharacterOffsetForLine(charcount_t line, charcount_t *outByteOffset) const
        {TRACE_IT(21772);
            AssertMsg(line < this->GetLineCount(), "Invalid line value passed in.");

            LineOffsetCacheItem item = this->lineOffsetCacheList->Item(line);

            if (outByteOffset != nullptr)
            {TRACE_IT(21773);
                *outByteOffset = item.byteOffset;
            }

            return item.characterOffset;
        }

        uint32 GetLineCount() const
        {TRACE_IT(21774);
            AssertMsg(this->lineOffsetCacheList != nullptr, "The list was either not set from the ByteCode or not created.");
            return this->lineOffsetCacheList->Count();
        }

        const LineOffsetCacheItem* GetItems()
        {TRACE_IT(21775);
            return this->lineOffsetCacheList->GetBuffer();
        }

    private:

        static bool FindNextLine(_In_z_ LPCUTF8 &currentSourcePosition, _In_z_ LPCUTF8 sourceEndCharacter, charcount_t &inOutCharacterOffset, charcount_t &inOutByteOffset, charcount_t maxCharacterOffset = UINT32_MAX)
        {TRACE_IT(21776);
            charcount_t currentCharacterOffset = inOutCharacterOffset;
            charcount_t currentByteOffset = inOutByteOffset;
            utf8::DecodeOptions options = utf8::doAllowThreeByteSurrogates;

            while (currentSourcePosition < sourceEndCharacter)
            {TRACE_IT(21777);
                LPCUTF8 previousCharacter = currentSourcePosition;

                // Decode from UTF8 to wide char.  Note that Decode will advance the current character by 1 at least.
                char16 decodedCharacter = utf8::Decode(currentSourcePosition, sourceEndCharacter, options);

                bool wasLineEncountered = false;
                switch (decodedCharacter)
                {
                case _u('\r'):
                    // Check if the next character is a '\n'.  If so, consume that character as well
                    // (consider as one line).
                    if (*currentSourcePosition == '\n')
                    {TRACE_IT(21778);
                        ++currentSourcePosition;
                        ++currentCharacterOffset;
                    }

                    // Intentional fall-through.
                case _u('\n'):
                case 0x2028:
                case 0x2029:
                    // Found a new line.
                    wasLineEncountered = true;
                    break;
                }

                // Move to the next character offset.
                ++currentCharacterOffset;

                // Count the current byte offset we're at in the UTF-8 buffer.
                // The character size can be > 1 for unicode characters.
                currentByteOffset += static_cast<int>(currentSourcePosition - previousCharacter);

                if (wasLineEncountered)
                {TRACE_IT(21779);
                    inOutCharacterOffset = currentCharacterOffset;
                    inOutByteOffset = currentByteOffset;
                    return true;
                }
                else if (currentCharacterOffset >= maxCharacterOffset)
                {TRACE_IT(21780);
                    return false;
                }
            }

            return false;
        }

        // Builds the cache of line offsets from the passed in source.
        void BuildCache(_In_ LineOffsetCacheList *list, _In_z_ LPCUTF8 sourceStartCharacter,
            _In_z_ LPCUTF8 sourceEndCharacter,
            charcount_t startingCharacterOffset = 0,
            charcount_t startingByteOffset = 0)
        {
            AssertMsg(sourceStartCharacter, "The source start character passed in is null.");
            AssertMsg(sourceEndCharacter, "The source end character passed in is null.");
            AssertMsg(sourceStartCharacter <= sourceEndCharacter, "The source start character should not be beyond the source end character.");
            AssertMsg(!this->isCacheBuilt, "The cache is already built.");

            // Add the first line in the cache list.
            this->AddLine(list, startingCharacterOffset, startingByteOffset);

            while (FindNextLine(sourceStartCharacter, sourceEndCharacter, startingCharacterOffset, startingByteOffset))
            {TRACE_IT(21781);
                this->AddLine(list, startingCharacterOffset, startingByteOffset);
            }

            isCacheBuilt = true;
        }

        // Tracks a new line offset in the cache.
        void AddLine(_In_ LineOffsetCacheList *list, int characterOffset, int byteOffset)
        {TRACE_IT(21782);
            AssertMsg(characterOffset >= 0, "The character offset is invalid.");
            AssertMsg(byteOffset >= 0, "The byte offset is invalid.");

            LineOffsetCacheItem item;
            item.characterOffset = characterOffset;
            item.byteOffset = byteOffset;

            list->Add(item);

#if DBG
            if (list->Count() > 1)
            {TRACE_IT(21783);
                // Ensure that the list remains sorted during insertion.
                LineOffsetCacheItem previousItem = list->Item(list->Count() - 2);
                AssertMsg(item.characterOffset > previousItem.characterOffset, "The character offsets must be inserted in increasing order per line.");
                AssertMsg(item.byteOffset > previousItem.byteOffset, "The byte offsets must be inserted in increasing order per line.");
            }
#endif // DBG
        }

    private:
        FieldNoBarrier(TAllocator*) allocator;

        // Line offset cache list used for quickly finding line/column offsets.
        Field(LineOffsetCacheReadOnlyList*) lineOffsetCacheList;
        Field(bool) isCacheBuilt;
    };
}
