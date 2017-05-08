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
    {TRACE_IT(62648);
    }

    bool StringProfiler::IsOnWrongThread() const
    {TRACE_IT(62649);
        return GetCurrentThreadContextId() != this->mainThreadId;
    }

    void StringProfiler::RecordNewString( const char16* sz, uint length )
    {TRACE_IT(62650);
        if( IsOnWrongThread() )
        {TRACE_IT(62651);
            ::InterlockedIncrement(&discardedWrongThread);
            return;
        }
        RequiredEncoding encoding = ASCII7bit;
        if(sz)
        {TRACE_IT(62652);
            encoding = GetRequiredEncoding(sz, length);
        }

        StringMetrics metrics = {};

        if( stringLengthMetrics.TryGetValue(length, &metrics) )
        {TRACE_IT(62653);
            metrics.Accumulate(encoding);
            stringLengthMetrics.Item(length,metrics);
        }
        else
        {TRACE_IT(62654);
            metrics.Accumulate(encoding);
            stringLengthMetrics.Add(length,metrics);
        }

        if(sz)
        {TRACE_IT(62655);
            uint embeddedNULs = CountEmbeddedNULs(sz, length);
            if( embeddedNULs != 0 )
            {TRACE_IT(62656);

                this->embeddedNULChars += embeddedNULs;
                this->embeddedNULStrings++;
            }
        }
    }

    /*static*/ StringProfiler::RequiredEncoding StringProfiler::GetRequiredEncoding( const char16* sz, uint length )
    {TRACE_IT(62657);
        RequiredEncoding encoding = ASCII7bit;

        for( uint i = 0; i != length; ++i )
        {TRACE_IT(62658);
            unsigned short ch = static_cast< unsigned short >(sz[i]);
            if( ch >= 0x100 )
            {TRACE_IT(62659);
                encoding = Unicode16bit;
                break; // no need to look further
            }
            else if( ch >= 0x80 )
            {TRACE_IT(62660);
                encoding = ASCII8bit;
            }
        }

        return encoding;
    }

    /*static*/ uint StringProfiler::CountEmbeddedNULs( const char16* sz, uint length )
    {TRACE_IT(62661);
        uint result = 0;
        for( uint i = 0; i != length; ++i )
        {TRACE_IT(62662);
            if( sz[i] == _u('\0') ) ++result;
        }

        return result;
    }

    StringProfiler::HistogramIndex::HistogramIndex( ArenaAllocator* allocator, uint size )
    {TRACE_IT(62663);
        this->index = AnewArray(allocator, UintUintPair, size);
        this->count = 0;
    }

    void StringProfiler::HistogramIndex::Add( uint len, uint freq )
    {TRACE_IT(62664);
        index[count].first = len;
        index[count].second = freq;
        count++;
    }

    uint StringProfiler::HistogramIndex::Get( uint i ) const
    {TRACE_IT(62665);
        Assert( i < count );
        return index[i].first;
    }

    uint StringProfiler::HistogramIndex::Count() const
    {TRACE_IT(62666);
        return count;
    }

    /*static*/ int StringProfiler::HistogramIndex::CompareDescending( const void* lhs, const void* rhs )
    {TRACE_IT(62667);
        // Compare on frequency (second)
        const UintUintPair* lhsPair = static_cast< const UintUintPair* >(lhs);
        const UintUintPair* rhsPair = static_cast< const UintUintPair* >(rhs);
        if( lhsPair->second < rhsPair->second ) return 1;
        if( lhsPair->second == rhsPair->second ) return 0;
        return -1;
    }

    void StringProfiler::HistogramIndex::SortDescending()
    {TRACE_IT(62668);
        qsort(this->index, this->count, sizeof(UintUintPair), CompareDescending);
    }

    /*static*/ void StringProfiler::PrintOne(
        unsigned int len,
        StringMetrics metrics,
        uint totalCount
        )
    {TRACE_IT(62669);
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
    {TRACE_IT(62670);
        if( val >= k_MaxConcatLength )
        {TRACE_IT(62671);
            Output::Print(_u(" Large"), k_MaxConcatLength);
        }
        else
        {TRACE_IT(62672);
            Output::Print(_u("%6u"), val);
        }
    }

    /*static*/ void StringProfiler::PrintOneConcat( UintUintPair const& key, const ConcatMetrics& metrics)
    {TRACE_IT(62673);
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
    {TRACE_IT(62674);
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
        {TRACE_IT(62675);
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
        {TRACE_IT(62676);
            Output::Print(_u("WARNING: %u strings were not counted because they were allocated on a background thread\n"),discardedWrongThread);
        }
        Output::Print(_u("\n"));
        Output::Print(_u("Max string length is %u chars\n"), maxLength);
        Output::Print(_u("%u empty strings (Literals or BufferString) were requested\n"), emptyStrings);
        Output::Print(_u("%u single char strings (Literals or BufferString) were requested\n"), singleCharStrings);
        if( this->embeddedNULStrings == 0 )
        {TRACE_IT(62677);
            Output::Print(_u("No embedded NULs were detected\n"));
        }
        else
        {TRACE_IT(62678);
            Output::Print(_u("Embedded NULs: %u NULs in %u strings\n"), this->embeddedNULChars, this->embeddedNULStrings);
        }
        Output::Print(_u("\n"));

        if(stringConcatMetrics.Count() == 0)
        {TRACE_IT(62679);
            Output::Print(_u("No string concatenations were performed\n"));
        }
        else
        {TRACE_IT(62680);
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
    {TRACE_IT(62681);
        if( IsOnWrongThread() )
        {TRACE_IT(62682);
            return;
        }

        lenLeft = min( lenLeft, k_MaxConcatLength );
        lenRight = min( lenRight, k_MaxConcatLength );

        UintUintPair key = { lenLeft, lenRight };
        ConcatMetrics* metrics;
        if(!stringConcatMetrics.TryGetReference(key, &metrics))
        {TRACE_IT(62683);
            stringConcatMetrics.Add(key, ConcatMetrics(type));
        }
        else
        {TRACE_IT(62684);
            metrics->Accumulate(type);
        }
    }

    /*static*/ void StringProfiler::RecordNewString( ScriptContext* scriptContext, const char16* sz, uint length )
    {TRACE_IT(62685);
        StringProfiler* stringProfiler = scriptContext->GetStringProfiler();
        if( stringProfiler )
        {TRACE_IT(62686);
            stringProfiler->RecordNewString(sz, length);
        }
    }

    /*static*/ void StringProfiler::RecordConcatenation( ScriptContext* scriptContext, uint lenLeft, uint lenRight, ConcatType type)
    {TRACE_IT(62687);
        StringProfiler* stringProfiler = scriptContext->GetStringProfiler();
        if( stringProfiler )
        {TRACE_IT(62688);
            stringProfiler->RecordConcatenation(lenLeft, lenRight, type);
        }
    }

    /*static*/ void StringProfiler::RecordEmptyStringRequest( ScriptContext* scriptContext )
    {TRACE_IT(62689);
        StringProfiler* stringProfiler = scriptContext->GetStringProfiler();
        if( stringProfiler )
        {TRACE_IT(62690);
            ::InterlockedIncrement( &stringProfiler->emptyStrings );
        }
    }

    /*static*/ void StringProfiler::RecordSingleCharStringRequest( ScriptContext* scriptContext )
    {TRACE_IT(62691);
        StringProfiler* stringProfiler = scriptContext->GetStringProfiler();
        if( stringProfiler )
        {TRACE_IT(62692);
            ::InterlockedIncrement( &stringProfiler->singleCharStrings );
        }
    }


} // namespace Js

#endif
