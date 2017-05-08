//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Wasm
{
const uint64 specialDivLeftValue = (uint64)1 << 63;

template<>
inline int64 WasmMath::Rem( int64 aLeft, int64 aRight )
{TRACE_IT(64887);
    return (aLeft == specialDivLeftValue && aRight == -1) ? 0 : aLeft % aRight;
}

template<>
inline uint64 WasmMath::Rem( uint64 aLeft, uint64 aRight )
{TRACE_IT(64888);
    return (aLeft == specialDivLeftValue && aRight == -1) ? specialDivLeftValue : aLeft % aRight;
}

template<typename T> 
inline T WasmMath::Shl( T aLeft, T aRight )
{TRACE_IT(64889);
    return aLeft << (aRight & (sizeof(T)*8-1));
}
template<typename T> 
inline T WasmMath::Shr( T aLeft, T aRight )
{TRACE_IT(64890);
    return aLeft >> (aRight & (sizeof(T)*8-1));
}

template<typename T> 
inline T WasmMath::ShrU( T aLeft, T aRight )
{TRACE_IT(64891);
    return aLeft >> (aRight & (sizeof(T)*8-1));
}
template<> 
inline int WasmMath::Ctz(int value)
{TRACE_IT(64892);
    DWORD index;
    if (_BitScanForward(&index, value))
    {TRACE_IT(64893);
        return index;
    }
    return 32;
}

template<> 
inline int64 WasmMath::Ctz(int64 value)
{TRACE_IT(64894);
    DWORD index;
#if TARGET_64
    if (_BitScanForward64(&index, value))
    {TRACE_IT(64895);
        return index;
    }
#else
    if (_BitScanForward(&index, (int32)value))
    {TRACE_IT(64896);
        return index;
    }
    if (_BitScanForward(&index, (int32)(value >> 32)))
    {TRACE_IT(64897);
        return index + 32;
    }
#endif
    return 64;
}

template<> 
inline int64 WasmMath::Clz(int64 value)
{TRACE_IT(64898);
    DWORD index;
#if TARGET_64
    if (_BitScanReverse64(&index, value))
    {TRACE_IT(64899);
        return 63 - index;
    }
#else
    if (_BitScanReverse(&index, (int32)(value >> 32)))
    {TRACE_IT(64900);
        return 31 - index;
    }
    if (_BitScanReverse(&index, (int32)value))
    {TRACE_IT(64901);
        return 63 - index;
    }
#endif
    return 64;
}

template<> 
inline int WasmMath::PopCnt(int value)
{TRACE_IT(64902);
    return ::Math::PopCnt32(value);
}

template<> 
inline int64 WasmMath::PopCnt(int64 value)
{TRACE_IT(64903);
    uint64 v = (uint64)value;
    // https://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel
    v = v - ((v >> 1) & 0x5555555555555555LL);
    v = (v & 0x3333333333333333LL) + ((v >> 2) & 0x3333333333333333LL);
    v = (v + (v >> 4)) & 0x0f0f0f0f0f0f0f0f;
    v = (uint64)(v * 0x0101010101010101LL) >> (sizeof(uint64) - 1) * CHAR_BIT;
    return (int64)v;
}


template<typename T>
inline int WasmMath::Eqz(T value)
{TRACE_IT(64904);
    return value == 0;
}

template<>
inline double WasmMath::Copysign(double aLeft, double aRight)
{TRACE_IT(64905);
    return _copysign(aLeft, aRight);
}

template<>
inline float WasmMath::Copysign(float aLeft, float aRight)
{TRACE_IT(64906);
    uint32 res = ((*(uint32*)(&aLeft) & 0x7fffffffu) | (*(uint32*)(&aRight) & 0x80000000u));
    return *(float*)(&res);
}

template <typename T> bool WasmMath::LessThan(T aLeft, T aRight)
{TRACE_IT(64907);
    return aLeft < aRight;
}

template <typename T> bool WasmMath::LessOrEqual(T aLeft, T aRight)
{TRACE_IT(64908);
    return aLeft <= aRight;
}

template <typename STYPE,
    typename UTYPE,
    UTYPE MAX,
    UTYPE NEG_ZERO,
    UTYPE NEG_ONE,
    WasmMath::CmpPtr<UTYPE> CMP1,
    WasmMath::CmpPtr<UTYPE> CMP2>
bool WasmMath::isInRange(STYPE srcVal)
{TRACE_IT(64909);
    Assert(sizeof(STYPE) == sizeof(UTYPE));
    UTYPE val = *reinterpret_cast<UTYPE*> (&srcVal);
    return (CMP1(val, MAX)) || (val >= NEG_ZERO && CMP2(val, NEG_ONE));
}

template <typename STYPE> bool  WasmMath::isNaN(STYPE src)
{TRACE_IT(64910);
    return src != src;
}

template<typename T>
inline T WasmMath::Trunc(T value)
{TRACE_IT(64911);
    if (value == 0.0)
    {TRACE_IT(64912);
        return value;
    }
    else
    {TRACE_IT(64913);
        T result;
        if (value < 0.0)
        {TRACE_IT(64914);
            result = ceil(value);
        }
        else
        {TRACE_IT(64915);
            result = floor(value);
        }
        // TODO: Propagating NaN sign for now awaiting consensus on semantics
        return result;
    }
}

template<typename T>
inline T WasmMath::Nearest(T value)
{TRACE_IT(64916);
    if (value == 0.0)
    {TRACE_IT(64917);
        return value;
    }
    else
    {TRACE_IT(64918);
        T result;
        T u = ceil(value);
        T d = floor(value);
        T um = fabs(value - u);
        T dm = fabs(value - d);
        if (um < dm || (um == dm && floor(u / 2) == u / 2))
        {TRACE_IT(64919);
            result = u;
        }
        else
        {TRACE_IT(64920);
            result = d;
        }
        // TODO: Propagating NaN sign for now awaiting consensus on semantics
        return result;
    }
}

template<>
inline int WasmMath::Rol(int aLeft, int aRight)
{TRACE_IT(64921);
    return _rotl(aLeft, aRight);
}

template<>
inline int64 WasmMath::Rol(int64 aLeft, int64 aRight)
{TRACE_IT(64922);
    return _rotl64(aLeft, (int)aRight);
}

template<>
inline int WasmMath::Ror(int aLeft, int aRight)
{TRACE_IT(64923);
    return _rotr(aLeft, aRight);
}

template<>
inline int64 WasmMath::Ror(int64 aLeft, int64 aRight)
{TRACE_IT(64924);
    return _rotr64(aLeft, (int)aRight);
}

}
