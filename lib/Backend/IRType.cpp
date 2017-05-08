//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "Backend.h"

int const TySize[] = {
#define IRTYPE(ucname, baseType, size, bitSize, enRegOk, dname) size,
#include "IRTypeList.h"
#undef IRTYPE
};

enum IRBaseTypes : BYTE {
    IRBaseType_Illegal,
    IRBaseType_Int,
    IRBaseType_Uint,
    IRBaseType_Float,
    IRBaseType_Simd,
    IRBaseType_Var,
    IRBaseType_Condcode,
    IRBaseType_Misc
};

int const TyBaseType[] = {
#define IRTYPE(ucname, baseType, size, bitSize, enRegOk, dname) IRBaseType_ ## baseType,
#include "IRTypeList.h"
#undef IRTYPE
};

const char16 * const TyDumpName[] = {
#define IRTYPE(ucname, baseType, size, bitSize, enRegOk, dname) _u(#dname),
#include "IRTypeList.h"
#undef IRTYPE
};

bool IRType_IsSignedInt(IRType type) {TRACE_IT(8717); return TyBaseType[type] == IRBaseType_Int; }
bool IRType_IsUnsignedInt(IRType type) {TRACE_IT(8718); return TyBaseType[type] == IRBaseType_Uint; }
bool IRType_IsFloat(IRType type) {TRACE_IT(8719); return TyBaseType[type] == IRBaseType_Float; }
bool IRType_IsNative(IRType type)
{TRACE_IT(8720);
    return TyBaseType[type] > IRBaseType_Illegal && TyBaseType[type] < IRBaseType_Var;
}
bool IRType_IsNativeInt(IRType type)
{TRACE_IT(8721);
    return TyBaseType[type] > IRBaseType_Illegal && TyBaseType[type] < IRBaseType_Float;
}
bool IRType_IsInt64(IRType type) {TRACE_IT(8722); return type == TyInt64 || type == TyUint64; }

bool IRType_IsSimd(IRType type)
{TRACE_IT(8723);
    return TyBaseType[type] == IRBaseType_Simd;
}

bool IRType_IsSimd128(IRType type)
{TRACE_IT(8724);
    return type >= TySimd128F4 && type <= TySimd128D2;
}

#if DBG_DUMP || defined(ENABLE_IR_VIEWER)
void IRType_Dump(IRType type)
{TRACE_IT(8725);
    Output::Print(TyDumpName[type]);
}
#endif
