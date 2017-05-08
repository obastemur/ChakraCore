//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once


struct PrimePolicy
{
    inline static uint GetBucket(hash_t hashCode, int size)
    {TRACE_IT(22182);
        uint targetBucket = hashCode % size;
        return targetBucket;
    }

    inline static uint GetSize(uint capacity)
    {TRACE_IT(22183);
        return GetPrime(capacity);
    }

private:
    static bool IsPrime(uint candidate);
    static uint GetPrime(uint min);
};

struct PowerOf2Policy
{
    inline static uint GetBucket(hash_t hashCode, int size)
    {TRACE_IT(22184);
        AssertMsg(Math::IsPow2(size), "Size is not a power of 2.");
        uint targetBucket = hashCode & (size-1);
        return targetBucket;
    }

    /// Returns a size that is power of 2 and
    /// greater than specified capacity.
    inline static uint GetSize(size_t minCapacity_t)
    {TRACE_IT(22185);
        AssertMsg(minCapacity_t <= MAXINT32, "the next higher power of 2  must fit in uint32");
        uint minCapacity = static_cast<uint>(minCapacity_t);

        if(minCapacity <= 0)
        {TRACE_IT(22186);
            return 4;
        }

        if (Math::IsPow2(minCapacity))
        {TRACE_IT(22187);
            return minCapacity;
        }
        else
        {TRACE_IT(22188);
            return 1 << (Math::Log2(minCapacity) + 1);
        }
    }
};

#ifndef JD_PRIVATE
template <class SizePolicy, uint averageChainLength = 2, uint growthRateNumerator = 2, uint growthRateDenominator = 1, uint minBucket = 4>
struct DictionarySizePolicy
{
    CompileAssert(growthRateNumerator > growthRateDenominator);
    CompileAssert(growthRateDenominator != 0);
    inline static uint GetBucket(hash_t hashCode, uint bucketCount)
    {TRACE_IT(22189);
        return SizePolicy::GetBucket(hashCode, bucketCount);
    }
    inline static uint GetNextSize(uint minCapacity)
    {TRACE_IT(22190);
        uint nextSize = minCapacity * growthRateNumerator / growthRateDenominator;
        return (growthRateDenominator != 1 && nextSize <= minCapacity)? minCapacity + 1 : nextSize;
    }
    inline static uint GetBucketSize(uint size)
    {TRACE_IT(22191);
        if (minBucket * averageChainLength >= size)
        {TRACE_IT(22192);
            return SizePolicy::GetSize(minBucket);
        }
        return SizePolicy::GetSize((size + (averageChainLength - 1)) / averageChainLength);
    }
};

typedef DictionarySizePolicy<PrimePolicy> PrimeSizePolicy;
typedef DictionarySizePolicy<PowerOf2Policy> PowerOf2SizePolicy;
#endif
