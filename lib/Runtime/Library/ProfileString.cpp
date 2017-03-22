//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

#ifdef PROFILE_STRINGS

namespace Js
{
    // The VS2013 linker treats this as a redefinition of an already
    // defined constant and complains. So skip the declaration if we're compiling
    // with VS2013 or below.
#if !defined(_MSC_VER) || _MSC_VER >= 1900
    const uint StringProfiler::k_MaxConcatLength;
#endif

    StringProfiler::StringProfiler(PageAllocator * pageAllocator)
      : allocator(_u("StringProfiler"), pageAllocator, Throw::OutOfMemory ),
        mainThreadId(GetCurrentThreadContextId() ),
        discardedWrongThread(0),
        stringLengthMetrics(&allocator),
        embeddedNULChars(0),
        embeddedNULStrings(0),
        emptyStrings(0),
        singleCharStrings(0),
        stringConcatMetrics(&allocator, 43)
    {LOGMEIN("ProfileString.cpp] 27\n");
    }

    bool StringProfiler::IsOnWrongThread() const
    {LOGMEIN("ProfileString.cpp] 31\n");
        return GetCurrentThreadContextId() != this->mainThreadId;
    }

    void StringProfiler::RecordNewString( const char16* sz, uint length )
    {LOGMEIN("ProfileString.cpp] 36\n");
        if( IsOnWrongThread() )
        {LOGMEIN("ProfileString.cpp] 38\n");
            ::InterlockedIncrement(&discardedWrongThread);
            return;
        }
        RequiredEncoding encoding = ASCII7bit;
        if(sz)
        {LOGMEIN("ProfileString.cpp] 44\n");
            encoding = GetRequiredEncoding(sz, length);
        }

        StringMetrics metrics = {};

        if( stringLengthMetrics.TryGetValue(length, &metrics) )
        {LOGMEIN("ProfileString.cpp] 51\n");
            metrics.Accumulate(encoding);
            stringLengthMetrics.Item(length,metrics);
        }
        else
        {
            metrics.Accumulate(encoding);
            stringLengthMetrics.Add(length,metrics);
        }

        if(sz)
        {LOGMEIN("ProfileString.cpp] 62\n");
            uint embeddedNULs = CountEmbeddedNULs(sz, length);
            if( embeddedNULs != 0 )
            {LOGMEIN("ProfileString.cpp] 65\n");

                this->embeddedNULChars += embeddedNULs;
                this->embeddedNULStrings++;
            }
        }
    }

    /*static*/ StringProfiler::RequiredEncoding StringProfiler::GetRequiredEncoding( const char16* sz, uint length )
    {LOGMEIN("ProfileString.cpp] 74\n");
        RequiredEncoding encoding = ASCII7bit;

        for( uint i = 0; i != length; ++i )
        {LOGMEIN("ProfileString.cpp] 78\n");
            unsigned short ch = static_cast< unsigned short >(sz[i]);
            if( ch >= 0x100 )
            {LOGMEIN("ProfileString.cpp] 81\n");
                encoding = Unicode16bit;
                break; // no need to look further
            }
            else if( ch >= 0x80 )
            {LOGMEIN("ProfileString.cpp] 86\n");
                encoding = ASCII8bit;
            }
        }

        return encoding;
    }

    /*static*/ uint StringProfiler::CountEmbeddedNULs( const char16* sz, uint length )
    {LOGMEIN("ProfileString.cpp] 95\n");
        uint result = 0;
        for( uint i = 0; i != length; ++i )
        {LOGMEIN("ProfileString.cpp] 98\n");
            if( sz[i] == _u('\0') ) ++result;
        }

        return result;
    }

    StringProfiler::HistogramIndex::HistogramIndex( ArenaAllocator* allocator, uint size )
    {LOGMEIN("ProfileString.cpp] 106\n");
        this->index = AnewArray(allocator, UintUintPair, size);
        this->count = 0;
    }

    void StringProfiler::HistogramIndex::Add( uint len, uint freq )
    {LOGMEIN("ProfileString.cpp] 112\n");
        index[count].first = len;
        index[count].second = freq;
        count++;
    }

    uint StringProfiler::HistogramIndex::Get( uint i ) const
    {LOGMEIN("ProfileString.cpp] 119\n");
        Assert( i < count );
        return index[i].first;
    }

    uint StringProfiler::HistogramIndex::Count() const
    {LOGMEIN("ProfileString.cpp] 125\n");
        return count;
    }

    /*static*/ int StringProfiler::HistogramIndex::CompareDescending( const void* lhs, const void* rhs )
    {LOGMEIN("ProfileString.cpp] 130\n");
        // Compare on frequency (second)
        const UintUintPair* lhsPair = static_cast< const UintUintPair* >(lhs);
        const UintUintPair* rhsPair = static_cast< const UintUintPair* >(rhs);
        if( lhsPair->second < rhsPair->second ) return 1;
        if( lhsPair->second == rhsPair->second ) return 0;
        return -1;
    }

    void StringProfiler::HistogramIndex::SortDescending()
    {LOGMEIN("ProfileString.cpp] 140\n");
        qsort(this->index, this->count, sizeof(UintUintPair), CompareDescending);
    }

    /*static*/ void StringProfiler::PrintOne(
        unsigned int len,
        StringMetrics metrics,
        uint totalCount
        )
    {LOGMEIN("ProfileString.cpp] 149\n");
        Output::Print(_u("%10u %10u %10u %10u %10u (%.1f%%)\n"),
                len,
                metrics.count7BitASCII,
                metrics.count8BitASCII,
                metrics.countUnicode,
                metrics.Total(),
                100.0*(double)metrics.Total()/(double)totalCount
                );
    }

    /*static*/ void StringProfiler::PrintUintOrLarge( uint val )
    {LOGMEIN("ProfileString.cpp] 161\n");
        if( val >= k_MaxConcatLength )
        {LOGMEIN("ProfileString.cpp] 163\n");
            Output::Print(_u(" Large"), k_MaxConcatLength);
        }
        else
        {
            Output::Print(_u("%6u"), val);
        }
    }

    /*static*/ void StringProfiler::PrintOneConcat( UintUintPair const& key, const ConcatMetrics& metrics)
    {LOGMEIN("ProfileString.cpp] 173\n");
        PrintUintOrLarge(key.first);
        Output::Print(_u(" "));
        PrintUintOrLarge(key.second);
        Output::Print(_u(" %6u"), metrics.compoundStringCount);
        Output::Print(_u(" %6u"), metrics.concatTreeCount);
        Output::Print(_u(" %6u"), metrics.bufferStringBuilderCount);
        Output::Print(_u(" %6u"), metrics.unknownCount);
        Output::Print(_u(" %6u\n"), metrics.Total());
    }

    void StringProfiler::PrintAll()
    {LOGMEIN("ProfileString.cpp] 185\n");
        Output::Print(_u("=============================================================\n"));
        Output::Print(_u("String Statistics\n"));
        Output::Print(_u("-------------------------------------------------------------\n"));
        Output::Print(_u("    Length 7bit ASCII 8bit ASCII    Unicode      Total %%Total\n"));
        Output::Print(_u(" --------- ---------- ---------- ---------- ---------- ------\n"));

        // Build an index for printing the histogram in descending order
        HistogramIndex index(&allocator, stringLengthMetrics.Count());
        uint totalStringCount = 0;
        stringLengthMetrics.Map([this, &index, &totalStringCount](unsigned int len, StringMetrics metrics)
        {
            uint lengthTotal = metrics.Total();
            index.Add(len, lengthTotal);
            totalStringCount += lengthTotal;
        });
        index.SortDescending();

        StringMetrics cumulative = {};
        uint maxLength = 0;

        for(uint i = 0; i != index.Count(); ++i )
        {LOGMEIN("ProfileString.cpp] 207\n");
            uint length = index.Get(i);

            // UintHashMap::Lookup doesn't work with value-types (it returns NULL
            // on error), so use TryGetValue instead.
            StringMetrics metrics;
            if( stringLengthMetrics.TryGetValue(length, &metrics) )
            {
                PrintOne( length, metrics, totalStringCount );

                cumulative.Accumulate(metrics);
                maxLength = max( maxLength, length );
            }
        }

        Output::Print(_u("-------------------------------------------------------------\n"));
        Output::Print(_u("    Totals %10u %10u %10u %10u (100%%)\n"),
            cumulative.count7BitASCII,
            cumulative.count8BitASCII,
            cumulative.countUnicode,
            cumulative.Total() );

        if(discardedWrongThread>0)
        {LOGMEIN("ProfileString.cpp] 230\n");
            Output::Print(_u("WARNING: %u strings were not counted because they were allocated on a background thread\n"),discardedWrongThread);
        }
        Output::Print(_u("\n"));
        Output::Print(_u("Max string length is %u chars\n"), maxLength);
        Output::Print(_u("%u empty strings (Literals or BufferString) were requested\n"), emptyStrings);
        Output::Print(_u("%u single char strings (Literals or BufferString) were requested\n"), singleCharStrings);
        if( this->embeddedNULStrings == 0 )
        {LOGMEIN("ProfileString.cpp] 238\n");
            Output::Print(_u("No embedded NULs were detected\n"));
        }
        else
        {
            Output::Print(_u("Embedded NULs: %u NULs in %u strings\n"), this->embeddedNULChars, this->embeddedNULStrings);
        }
        Output::Print(_u("\n"));

        if(stringConcatMetrics.Count() == 0)
        {LOGMEIN("ProfileString.cpp] 248\n");
            Output::Print(_u("No string concatenations were performed\n"));
        }
        else
        {
            Output::Print(_u("String concatenations (Strings %u chars or longer are treated as \"Large\")\n"), k_MaxConcatLength);
            Output::Print(_u("   LHS +  RHS  SB    Concat   Buf  Other  Total\n"));
            Output::Print(_u("------ ------ ------ ------ ------ ------ ------\n"));

            uint totalConcatenations = 0;
            uint totalConcatTree = 0;
            uint totalBufString = 0;
            uint totalCompoundString = 0;
            uint totalOther = 0;
            stringConcatMetrics.Map([&](UintUintPair const& key, const ConcatMetrics& metrics)
            {
                PrintOneConcat(key, metrics);
                totalConcatenations += metrics.Total();
                totalConcatTree += metrics.concatTreeCount;
                totalBufString += metrics.bufferStringBuilderCount;
                totalCompoundString += metrics.compoundStringCount;
                totalOther += metrics.unknownCount;
            }
            );
            Output::Print(_u("-------------------------------------------------------\n"));
            Output::Print(_u("Total %6u %6u %6u %6u %6u\n"), totalConcatenations, totalCompoundString, totalConcatTree, totalBufString, totalOther);
        }

        Output::Flush();
    }

    void StringProfiler::RecordConcatenation( uint lenLeft, uint lenRight, ConcatType type )
    {LOGMEIN("ProfileString.cpp] 280\n");
        if( IsOnWrongThread() )
        {LOGMEIN("ProfileString.cpp] 282\n");
            return;
        }

        lenLeft = min( lenLeft, k_MaxConcatLength );
        lenRight = min( lenRight, k_MaxConcatLength );

        UintUintPair key = { lenLeft, lenRight };
        ConcatMetrics* metrics;
        if(!stringConcatMetrics.TryGetReference(key, &metrics))
        {LOGMEIN("ProfileString.cpp] 292\n");
            stringConcatMetrics.Add(key, ConcatMetrics(type));
        }
        else
        {
            metrics->Accumulate(type);
        }
    }

    /*static*/ void StringProfiler::RecordNewString( ScriptContext* scriptContext, const char16* sz, uint length )
    {LOGMEIN("ProfileString.cpp] 302\n");
        StringProfiler* stringProfiler = scriptContext->GetStringProfiler();
        if( stringProfiler )
        {LOGMEIN("ProfileString.cpp] 305\n");
            stringProfiler->RecordNewString(sz, length);
        }
    }

    /*static*/ void StringProfiler::RecordConcatenation( ScriptContext* scriptContext, uint lenLeft, uint lenRight, ConcatType type)
    {LOGMEIN("ProfileString.cpp] 311\n");
        StringProfiler* stringProfiler = scriptContext->GetStringProfiler();
        if( stringProfiler )
        {LOGMEIN("ProfileString.cpp] 314\n");
            stringProfiler->RecordConcatenation(lenLeft, lenRight, type);
        }
    }

    /*static*/ void StringProfiler::RecordEmptyStringRequest( ScriptContext* scriptContext )
    {LOGMEIN("ProfileString.cpp] 320\n");
        StringProfiler* stringProfiler = scriptContext->GetStringProfiler();
        if( stringProfiler )
        {LOGMEIN("ProfileString.cpp] 323\n");
            ::InterlockedIncrement( &stringProfiler->emptyStrings );
        }
    }

    /*static*/ void StringProfiler::RecordSingleCharStringRequest( ScriptContext* scriptContext )
    {LOGMEIN("ProfileString.cpp] 329\n");
        StringProfiler* stringProfiler = scriptContext->GetStringProfiler();
        if( stringProfiler )
        {LOGMEIN("ProfileString.cpp] 332\n");
            ::InterlockedIncrement( &stringProfiler->singleCharStrings );
        }
    }


} // namespace Js

#endif
