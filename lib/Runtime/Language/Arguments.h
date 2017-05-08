//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

// To extract variadic args array after known args list:
//      argx, callInfo, ...
// NOTE: The last known arg name is hard-coded to "callInfo".
#ifdef _WIN32
#define DECLARE_ARGS_VARARRAY(va, ...)                              \
    va_list _vl;                                                    \
    va_start(_vl, callInfo);                                        \
    Js::Var* va = (Js::Var*)_vl
#else
// We use a custom calling convention to invoke JavascriptMethod based on
// System ABI. At entry of JavascriptMethod the stack layout is:
//      [Return Address] [function] [callInfo] [arg0] [arg1] ...
//
#define DECLARE_ARGS_VARARRAY_N(va, n)                              \
    Js::Var* va = _get_va(_AddressOfReturnAddress(), n);            \
    Assert(*reinterpret_cast<Js::CallInfo*>(va - 1) == callInfo)

#define DECLARE_ARGS_VARARRAY(va, ...)                              \
    DECLARE_ARGS_VARARRAY_N(va, _count_args(__VA_ARGS__))

inline Js::Var* _get_va(void* addrOfReturnAddress, int n)
{TRACE_IT(45605);
    // All args are right after ReturnAddress by custom calling convention
    Js::Var* pArgs = reinterpret_cast<Js::Var*>(addrOfReturnAddress) + 1;
#ifdef _ARM_
    n += 2; // ip + fp
#endif
    return pArgs + n;
}

inline int _count_args(Js::CallInfo callInfo)
{TRACE_IT(45606);
    // This is to support typical runtime "ARGUMENTS(args, callInfo)" usage.
    // Only "callInfo" listed, but we have 2 known args "function, callInfo".
    return 2;
}
template <class T1>
inline int _count_args(const T1&, Js::CallInfo callInfo)
{TRACE_IT(45607);
    return 2;
}
template <class T1, class T2>
inline int _count_args(const T1&, const T2&, Js::CallInfo callInfo)
{TRACE_IT(45608);
    return 3;
}
template <class T1, class T2, class T3>
inline int _count_args(const T1&, const T2&, const T3&, Js::CallInfo callInfo)
{TRACE_IT(45609);
    return 4;
}
template <class T1, class T2, class T3, class T4>
inline int _count_args(const T1&, const T2&, const T3&, const T4&, Js::CallInfo callInfo)
{TRACE_IT(45610);
    return 5;
}
#endif


#ifdef _WIN32
#define CALL_ENTRYPOINT(entryPoint, function, callInfo, ...) \
    entryPoint(function, callInfo, ##__VA_ARGS__)
#elif defined(_M_X64) || defined(_M_IX86)
// Call an entryPoint (JavascriptMethod) with custom calling convention.
//  RDI == function, RSI == callInfo, (RDX/RCX/R8/R9==null/unused),
//  all parameters on stack.
#define CALL_ENTRYPOINT(entryPoint, function, callInfo, ...) \
    entryPoint(function, callInfo, nullptr, nullptr, nullptr, nullptr, \
               function, callInfo, ##__VA_ARGS__)
#elif defined(_ARM_)
// xplat-todo: fix me ARM
#define CALL_ENTRYPOINT(entryPoint, function, callInfo, ...) \
    entryPoint(function, callInfo, ##__VA_ARGS__)
#else
#error CALL_ENTRYPOINT not yet implemented
#endif

#define CALL_FUNCTION(function, callInfo, ...) \
    CALL_ENTRYPOINT(function->GetEntryPoint(), \
                    function, callInfo, ##__VA_ARGS__)


/*
 * RUNTIME_ARGUMENTS is a simple wrapper around the variadic calling convention
 * used by JavaScript functions. It is a low level macro that does not try to
 * differentiate between script usable Vars and runtime data structures.
 * To be able to access only script usable args use the ARGUMENTS macro instead.
 *
 * The ... list must be
 *  * "callInfo", typically for JsMethod that has only 2 known args
 *    "function, callInfo";
 *  * or the full known args list ending with "callInfo" (for some runtime
 *    helpers).
 */
#define RUNTIME_ARGUMENTS(n, ...)                       \
    DECLARE_ARGS_VARARRAY(_argsVarArray, __VA_ARGS__);  \
    Js::Arguments n(callInfo, _argsVarArray);

#define ARGUMENTS(n, ...)                               \
    DECLARE_ARGS_VARARRAY(_argsVarArray, __VA_ARGS__);  \
    Js::ArgumentReader n(&callInfo, _argsVarArray);

namespace Js
{
    struct Arguments
    {
    public:
        Arguments(CallInfo callInfo, Var* values) :
            Info(callInfo), Values(values) {TRACE_IT(45611);}

        Arguments(ushort count, Var* values) :
            Info(count), Values(values) {TRACE_IT(45612);}

        Arguments(VirtualTableInfoCtorEnum v) : Info(v) {TRACE_IT(45613);}

        Arguments(const Arguments& other) : Info(other.Info), Values(other.Values) {TRACE_IT(45614);}

        Var operator [](int idxArg) {TRACE_IT(45615); return const_cast<Var>(static_cast<const Arguments&>(*this)[idxArg]); }
        const Var operator [](int idxArg) const
        {TRACE_IT(45616);
            AssertMsg((idxArg < (int)Info.Count) && (idxArg >= 0), "Ensure a valid argument index");
            return Values[idxArg];
        }

        // swb: Arguments is mostly used on stack and does not need write barrier.
        // It is recycler allocated with ES6 generators. We handle that specially.
        FieldNoBarrier(CallInfo) Info;
        FieldNoBarrier(Var*) Values;

        static uint32 GetCallInfoOffset() {TRACE_IT(45617); return offsetof(Arguments, Info); }
        static uint32 GetValuesOffset() {TRACE_IT(45618); return offsetof(Arguments, Values); }

        // Prevent heap/recycler allocation, so we don't need write barrier for this
        static void* operator new   (size_t)    = delete;
        static void* operator new[] (size_t)    = delete;
        static void  operator delete   (void*)  = delete;
        static void  operator delete[] (void*)  = delete;
    };

    struct ArgumentReader : public Arguments
    {
        ArgumentReader(CallInfo *callInfo, Var* values)
            : Arguments(*callInfo, values)
        {TRACE_IT(45619);
            AdjustArguments(callInfo);
        }

    private:
        void AdjustArguments(CallInfo *callInfo)
        {TRACE_IT(45620);
            AssertMsg(!(Info.Flags & Js::CallFlags_NewTarget) || (Info.Flags & Js::CallFlags_ExtraArg), "NewTarget flag must be used together with ExtraArg.");
            if (Info.Flags & Js::CallFlags_ExtraArg)
            {TRACE_IT(45621);
                // If "calling eval" is set, then the last param is the frame display, which only
                // the eval built-in should see.
                Assert(Info.Count > 0);
                // The local version should be consistent. On the other hand, lots of code throughout
                // jscript uses the callInfo from stack to get argument list etc. We'll need
                // to change all the caller to be aware of the id or somehow make sure they don't use
                // the stack version. Both seem risky. It would be safer and more robust to just
                // change the stack version.
                Info.Flags = (CallFlags)(Info.Flags & ~Js::CallFlags_ExtraArg);
                Info.Count--;
                callInfo->Flags = (CallFlags)(callInfo->Flags & ~Js::CallFlags_ExtraArg);
                callInfo->Count--;
            }
        }
    };
}
