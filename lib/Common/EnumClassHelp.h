//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

// This macro defines some global operators, and hence must be used at global scope
#define ENUM_CLASS_HELPERS(TEnum, TUnderlying) \
    inline TEnum operator +(const TEnum e, const TUnderlying n) \
    {TRACE_IT(22461); \
        return static_cast<TEnum>(static_cast<TUnderlying>(e) + n); \
    } \
    \
    inline TEnum operator +(const TUnderlying n, const TEnum e) \
    {TRACE_IT(22462); \
        return static_cast<TEnum>(n + static_cast<TUnderlying>(e)); \
    } \
    \
    inline TEnum operator +(const TEnum e0, const TEnum e1) \
    {TRACE_IT(22463); \
        return static_cast<TUnderlying>(e0) + e1; \
    } \
    \
    inline TEnum &operator +=(TEnum &e, const TUnderlying n) \
    {TRACE_IT(22464); \
        return e = e + n; \
    } \
    \
    inline TEnum &operator ++(TEnum &e) \
    {TRACE_IT(22465); \
        return e += 1; \
    } \
    \
    inline TEnum operator ++(TEnum &e, const int) \
    {TRACE_IT(22466); \
        const TEnum old = e; \
        ++e; \
        return old; \
    } \
    \
    inline TEnum operator -(const TEnum e, const TUnderlying n) \
    {TRACE_IT(22467); \
        return static_cast<TEnum>(static_cast<TUnderlying>(e) - n); \
    } \
    \
    inline TEnum operator -(const TUnderlying n, const TEnum e) \
    {TRACE_IT(22468); \
        return static_cast<TEnum>(n - static_cast<TUnderlying>(e)); \
    } \
    \
    inline TEnum operator -(const TEnum e0, const TEnum e1) \
    {TRACE_IT(22469); \
        return static_cast<TUnderlying>(e0) - e1; \
    } \
    \
    inline TEnum &operator -=(TEnum &e, const TUnderlying n) \
    {TRACE_IT(22470); \
        return e = e - n; \
    } \
    \
    inline TEnum &operator --(TEnum &e) \
    {TRACE_IT(22471); \
        return e -= 1; \
    } \
    \
    inline TEnum operator --(TEnum &e, const int) \
    {TRACE_IT(22472); \
        const TEnum old = e; \
        --e; \
        return old; \
    } \
    \
    inline TEnum operator &(const TEnum e0, const TEnum e1) \
    {TRACE_IT(22473); \
        return static_cast<TEnum>(static_cast<TUnderlying>(e0) & static_cast<TUnderlying>(e1)); \
    } \
    \
    inline TEnum &operator &=(TEnum &e0, const TEnum e1) \
    {TRACE_IT(22474); \
        return e0 = e0 & e1; \
    } \
    \
    inline TEnum operator ^(const TEnum e0, const TEnum e1) \
    {TRACE_IT(22475); \
        return static_cast<TEnum>(static_cast<TUnderlying>(e0) ^ static_cast<TUnderlying>(e1)); \
    } \
    \
    inline TEnum &operator ^=(TEnum &e0, const TEnum e1) \
    {TRACE_IT(22476); \
        return e0 = e0 ^ e1; \
    } \
    \
    inline TEnum operator |(const TEnum e0, const TEnum e1) \
    {TRACE_IT(22477); \
        return static_cast<TEnum>(static_cast<TUnderlying>(e0) | static_cast<TUnderlying>(e1)); \
    } \
    \
    inline TEnum &operator |=(TEnum &e0, const TEnum e1) \
    {TRACE_IT(22478); \
        return e0 = e0 | e1; \
    } \
    \
    inline TEnum operator <<(const TEnum e, const TUnderlying n) \
    {TRACE_IT(22479); \
        return static_cast<TEnum>(static_cast<TUnderlying>(e) << n); \
    } \
    \
    inline TEnum &operator <<=(TEnum &e, const TUnderlying n) \
    {TRACE_IT(22480); \
        return e = e << n; \
    } \
    \
    inline TEnum operator >>(const TEnum e, const TUnderlying n) \
    {TRACE_IT(22481); \
        return static_cast<TEnum>(static_cast<TUnderlying>(e) >> n); \
    } \
    \
    inline TEnum &operator >>=(TEnum &e, const TUnderlying n) \
    {TRACE_IT(22482); \
        return e = e >> n; \
    } \
    \
    inline TEnum operator ~(const TEnum e) \
    {TRACE_IT(22483); \
        return static_cast<TEnum>(~static_cast<TUnderlying>(e)); \
    } \
    \
    inline TEnum operator +(const TEnum e) \
    {TRACE_IT(22484); \
        return e; \
    } \
    \
    inline TEnum operator -(const TEnum e) \
    {TRACE_IT(22485); \
        return static_cast<TEnum>(static_cast<TUnderlying>(0) - static_cast<TUnderlying>(e)); \
    } \
    \
    inline bool operator !(const TEnum e) \
    {TRACE_IT(22486); \
        return !static_cast<TUnderlying>(e); \
    } \

// For private enum classes defined inside other classes, this macro can be used inside the class to declare friends
#define ENUM_CLASS_HELPER_FRIENDS(TEnum, TUnderlying) \
    friend TEnum operator +(const TEnum e, const TUnderlying n); \
    friend TEnum operator +(const TUnderlying n, const TEnum e); \
    friend TEnum operator +(const TEnum e0, const TEnum e1); \
    friend TEnum &operator +=(TEnum &e, const TUnderlying n); \
    friend TEnum &operator ++(TEnum &e); \
    friend TEnum operator ++(TEnum &e, const int); \
    friend TEnum operator -(const TEnum e, const TUnderlying n); \
    friend TEnum operator -(const TUnderlying n, const TEnum e); \
    friend TEnum operator -(const TEnum e0, const TEnum e1); \
    friend TEnum &operator -=(TEnum &e, const TUnderlying n); \
    friend TEnum &operator --(TEnum &e); \
    friend TEnum operator --(TEnum &e, const int); \
    friend TEnum operator &(const TEnum e0, const TEnum e1); \
    friend TEnum &operator &=(TEnum &e0, const TEnum e1); \
    friend TEnum operator ^(const TEnum e0, const TEnum e1); \
    friend TEnum &operator ^=(TEnum &e0, const TEnum e1); \
    friend TEnum operator |(const TEnum e0, const TEnum e1); \
    friend TEnum &operator |=(TEnum &e0, const TEnum e1); \
    friend TEnum operator <<(const TEnum e, const TUnderlying n); \
    friend TEnum &operator <<=(TEnum &e, const TUnderlying n); \
    friend TEnum operator >>(const TEnum e, const TUnderlying n); \
    friend TEnum &operator >>=(TEnum &e, const TUnderlying n); \
    friend TEnum operator ~(const TEnum e); \
    friend TEnum operator +(const TEnum e); \
    friend TEnum operator -(const TEnum e); \
    friend bool operator !(const TEnum e);
