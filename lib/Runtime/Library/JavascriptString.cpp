//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

#include "DataStructures/BigInt.h"
#include "Library/EngineInterfaceObject.h"
#include "Library/IntlEngineInterfaceExtensionObject.h"

#if ENABLE_NATIVE_CODEGEN
#include "../Backend/JITRecyclableObject.h"
#endif

namespace Js
{
    // White Space characters are defined in ES 2017 Section 11.2 #sec-white-space
    // There are 25 white space characters we need to correctly class.
    // - 6 of these are explicitly specified in ES 2017 Section 11.2 #sec-white-space
    // - 15 of these are Unicode category "Zs" ("Space_Separator") and not explicitly specified above.
    //   - Note: In total, 17 of these are Unicode category "Zs".
    // - 4 of these are actually LineTerminator characters.
    //   - Note: for various reasons it is convenient to group LineTerminator with Whitespace
    //     in the definition of IsWhiteSpaceCharacter.
    //     This does not cause problems because of the syntactic nature of LineTerminators
    //     and their meaning of ending a line in RegExp.
    //   - See: #sec-string.prototype.trim "The definition of white space is the union of WhiteSpace and LineTerminator."
    // Note: ES intentionally excludes characters which have Unicode property "White_Space" but which are not "Zs".
    // See http://www.unicode.org/Public/9.0.0/ucd/UnicodeData.txt for character classes.
    // The 25 white space characters are:
    //0x0009 // <TAB>
    //0x000a // <LF> LineTerminator (LINE FEED)
    //0x000b // <VT>
    //0x000c // <FF>
    //0x000d // <CR> LineTerminator (CARRIAGE RETURN)
    //0x0020 // <SP>
    //0x00a0 // <NBSP>
    //0x1680
    //0x2000
    //0x2001
    //0x2002
    //0x2003
    //0x2004
    //0x2005
    //0x2006
    //0x2007
    //0x2008
    //0x2009
    //0x200a
    //0x2028 // <LS> LineTerminator (LINE SEPARATOR)
    //0x2029 // <PS> LineTerminator (PARAGRAPH SEPARATOR)
    //0x202f
    //0x205f
    //0x3000
    //0xfeff // <ZWNBSP>
    bool IsWhiteSpaceCharacter(char16 ch)
    {LOGMEIN("JavascriptString.cpp] 56\n");
        return ch >= 0x9 &&
            (ch <= 0xd ||
                (ch <= 0x200a &&
                    (ch >= 0x2000 || ch == 0x20 || ch == 0xa0 || ch == 0x1680)
                ) ||
                (ch >= 0x2028 &&
                    (ch <= 0x2029 || ch == 0x202f || ch == 0x205f || ch == 0x3000 || ch == 0xfeff)
                )
            );
    }

    template <typename T, bool copyBuffer>
    JavascriptString* JavascriptString::NewWithBufferT(const char16 * content, charcount_t cchUseLength, ScriptContext * scriptContext)
    {LOGMEIN("JavascriptString.cpp] 70\n");
        AssertMsg(content != nullptr, "NULL value passed to JavascriptString::New");
        AssertMsg(IsValidCharCount(cchUseLength), "String length will overflow an int");
        switch (cchUseLength)
        {LOGMEIN("JavascriptString.cpp] 74\n");
        case 0:
            return scriptContext->GetLibrary()->GetEmptyString();

        case 1:
            return scriptContext->GetLibrary()->GetCharStringCache().GetStringForChar(*content);

        default:
            break;
        }

        Recycler* recycler = scriptContext->GetRecycler();
        StaticType * stringTypeStatic = scriptContext->GetLibrary()->GetStringTypeStatic();
        char16 const * buffer = content;

        charcount_t cchUseBoundLength = static_cast<charcount_t>(cchUseLength);
        if (copyBuffer)
        {LOGMEIN("JavascriptString.cpp] 91\n");
             buffer = JavascriptString::AllocateLeafAndCopySz(recycler, content, cchUseBoundLength);
        }

        return T::New(stringTypeStatic, buffer, cchUseBoundLength, recycler);
    }

    JavascriptString* JavascriptString::NewWithSz(__in_z const char16 * content, ScriptContext * scriptContext)
    {LOGMEIN("JavascriptString.cpp] 99\n");
        AssertMsg(content != nullptr, "NULL value passed to JavascriptString::New");
        return NewWithBuffer(content, GetBufferLength(content), scriptContext);
    }

    JavascriptString* JavascriptString::NewWithArenaSz(__in_z const char16 * content, ScriptContext * scriptContext)
    {LOGMEIN("JavascriptString.cpp] 105\n");
        AssertMsg(content != nullptr, "NULL value passed to JavascriptString::New");
        return NewWithArenaBuffer(content, GetBufferLength(content), scriptContext);
    }

    JavascriptString* JavascriptString::NewWithBuffer(__in_ecount(cchUseLength) const char16 * content, charcount_t cchUseLength, ScriptContext * scriptContext)
    {LOGMEIN("JavascriptString.cpp] 111\n");
        return NewWithBufferT<LiteralString, false>(content, cchUseLength, scriptContext);
    }

    JavascriptString* JavascriptString::NewWithArenaBuffer(__in_ecount(cchUseLength) const char16* content, charcount_t cchUseLength, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptString.cpp] 116\n");
        return NewWithBufferT<ArenaLiteralString, false>(content, cchUseLength, scriptContext);
    }

    JavascriptString* JavascriptString::NewCopySz(__in_z const char16* content, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptString.cpp] 121\n");
        return NewCopyBuffer(content, GetBufferLength(content), scriptContext);
    }

    JavascriptString* JavascriptString::NewCopyBuffer(__in_ecount(cchUseLength) const char16* content, charcount_t cchUseLength, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptString.cpp] 126\n");
        return NewWithBufferT<LiteralString, true>(content, cchUseLength, scriptContext);
    }

    JavascriptString* JavascriptString::NewCopySzFromArena(__in_z const char16* content, ScriptContext* scriptContext, ArenaAllocator *arena)
    {LOGMEIN("JavascriptString.cpp] 131\n");
        AssertMsg(content != nullptr, "NULL value passed to JavascriptString::New");

        charcount_t cchUseLength = JavascriptString::GetBufferLength(content);
        char16* buffer = JavascriptString::AllocateAndCopySz(arena, content, cchUseLength);
        return ArenaLiteralString::New(scriptContext->GetLibrary()->GetStringTypeStatic(),
            buffer, cchUseLength, arena);
    }

    Var JavascriptString::NewInstance(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Negative argument count");

        // SkipDefaultNewObject function flag should have prevented the default object from
        // being created, except when call true a host dispatch.
        Var newTarget = callInfo.Flags & CallFlags_NewTarget ? args.Values[args.Info.Count] : args[0];
        bool isCtorSuperCall = (callInfo.Flags & CallFlags_New) && newTarget != nullptr && !JavascriptOperators::IsUndefined(newTarget);
        Assert(isCtorSuperCall || !(callInfo.Flags & CallFlags_New) || args[0] == nullptr
            || JavascriptOperators::GetTypeId(args[0]) == TypeIds_HostDispatch);

        JavascriptString* str;
        Var result;

        if (args.Info.Count > 1)
        {LOGMEIN("JavascriptString.cpp] 160\n");
            if (JavascriptSymbol::Is(args[1]) && !(callInfo.Flags & CallFlags_New))
            {LOGMEIN("JavascriptString.cpp] 162\n");
                // By ES2015 21.1.1.1 step 2, calling the String constructor directly results in an explicit ToString, which does not throw.
                return JavascriptSymbol::ToString(JavascriptSymbol::FromVar(args[1])->GetValue(), scriptContext);
                // Calling with new is an implicit ToString on the Symbol, resulting in a throw. For this case we can let JavascriptConversion handle the call.
            }
            str = JavascriptConversion::ToString(args[1], scriptContext);
        }
        else
        {
            str = scriptContext->GetLibrary()->GetEmptyString();
        }

        if (callInfo.Flags & CallFlags_New)
        {LOGMEIN("JavascriptString.cpp] 175\n");
            result = scriptContext->GetLibrary()->CreateStringObject(str);
        }
        else
        {
            result = str;
        }

        return isCtorSuperCall ?
            JavascriptOperators::OrdinaryCreateFromConstructor(RecyclableObject::FromVar(newTarget), RecyclableObject::FromVar(result), nullptr, scriptContext) :
            result;
    }

    // static
    bool IsValidCharCount(size_t charCount)
    {LOGMEIN("JavascriptString.cpp] 190\n");
        return charCount <= JavascriptString::MaxCharLength;
    }

    JavascriptString::JavascriptString(StaticType * type)
        : RecyclableObject(type), m_charLength(0), m_pszValue(nullptr)
    {LOGMEIN("JavascriptString.cpp] 196\n");
        Assert(type->GetTypeId() == TypeIds_String);
    }

    JavascriptString::JavascriptString(StaticType * type, charcount_t charLength, const char16* szValue)
        : RecyclableObject(type), m_charLength(charLength), m_pszValue(szValue)
    {LOGMEIN("JavascriptString.cpp] 202\n");
        Assert(type->GetTypeId() == TypeIds_String);
        AssertMsg(IsValidCharCount(charLength), "String length is out of range");
    }

    _Ret_range_(m_charLength, m_charLength)
    charcount_t JavascriptString::GetLength() const
    {LOGMEIN("JavascriptString.cpp] 209\n");
        return m_charLength;
    }

    int JavascriptString::GetLengthAsSignedInt() const
    {LOGMEIN("JavascriptString.cpp] 214\n");
        Assert(IsValidCharCount(m_charLength));
        return static_cast<int>(m_charLength);
    }

    const char16* JavascriptString::UnsafeGetBuffer() const
    {LOGMEIN("JavascriptString.cpp] 220\n");
        return m_pszValue;
    }

    void JavascriptString::SetLength(charcount_t newLength)
    {LOGMEIN("JavascriptString.cpp] 225\n");
        if (!IsValidCharCount(newLength))
        {LOGMEIN("JavascriptString.cpp] 227\n");
            JavascriptExceptionOperators::ThrowOutOfMemory(this->GetScriptContext());
        }
        m_charLength = newLength;
    }

    void JavascriptString::SetBuffer(const char16* buffer)
    {LOGMEIN("JavascriptString.cpp] 234\n");
        m_pszValue = buffer;
    }

    bool JavascriptString::IsValidIndexValue(charcount_t idx) const
    {LOGMEIN("JavascriptString.cpp] 239\n");
        return IsValidCharCount(idx) && idx < GetLength();
    }

    bool JavascriptString::Is(Var aValue)
    {LOGMEIN("JavascriptString.cpp] 244\n");
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_String;
    }

    JavascriptString* JavascriptString::FromVar(Var aValue)
    {
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptString'");

        return static_cast<JavascriptString *>(RecyclableObject::FromVar(aValue));
    }

    charcount_t
    JavascriptString::GetBufferLength(const char16 * content)
    {LOGMEIN("JavascriptString.cpp] 257\n");
        size_t cchActual = wcslen(content);

#if defined(_M_X64_OR_ARM64)
        if (!IsValidCharCount(cchActual))
        {LOGMEIN("JavascriptString.cpp] 262\n");
            // Limit javascript string to 31-bit length
            Js::Throw::OutOfMemory();
        }
#else
        // There shouldn't be enough memory to have UINT_MAX character.
        // INT_MAX is the upper bound for 32-bit;
        Assert(IsValidCharCount(cchActual));
#endif
        return static_cast<charcount_t>(cchActual);
    }

    charcount_t
    JavascriptString::GetBufferLength(
        const char16 * content,                     // Value to examine
        int charLengthOrMinusOne)                    // Optional length, in characters
    {LOGMEIN("JavascriptString.cpp] 278\n");
        //
        // Determine the actual length, in characters, not including a terminating '\0':
        // - If a length was not specified (charLength < 0), search for a terminating '\0'.
        //

        charcount_t cchActual;
        if (charLengthOrMinusOne < 0)
        {LOGMEIN("JavascriptString.cpp] 286\n");
            AssertMsg(charLengthOrMinusOne == -1, "The only negative value allowed is -1");
            cchActual = GetBufferLength(content);
        }
        else
        {
            cchActual = static_cast<charcount_t>(charLengthOrMinusOne);
        }
#ifdef CHECK_STRING
        // removed this to accommodate much larger string constant in regex-dna.js
        if (cchActual > 64 * 1024)
        {LOGMEIN("JavascriptString.cpp] 297\n");
            //
            // String was probably not '\0' terminated:
            // - We need to validate that the string's contents always fit within 1 GB to avoid
            //   overflow checking on 32-bit when using 'int' for 'byte *' pointer operations.
            //

            Throw::OutOfMemory();  // TODO: determine argument error
        }
#endif
        return cchActual;
    }

    template< size_t N >
    Var JavascriptString::StringBracketHelper(Arguments args, ScriptContext *scriptContext, const char16(&tag)[N])
    {LOGMEIN("JavascriptString.cpp] 312\n");
        CompileAssert(0 < N && N <= JavascriptString::MaxCharLength);
        return StringBracketHelper(args, scriptContext, tag, static_cast<charcount_t>(N - 1), nullptr, 0);
    }

    template< size_t N1, size_t N2 >
    Var JavascriptString::StringBracketHelper(Arguments args, ScriptContext *scriptContext, const char16(&tag)[N1], const char16(&prop)[N2])
    {LOGMEIN("JavascriptString.cpp] 319\n");
        CompileAssert(0 < N1 && N1 <= JavascriptString::MaxCharLength);
        CompileAssert(0 < N2 && N2 <= JavascriptString::MaxCharLength);
        return StringBracketHelper(args, scriptContext, tag, static_cast<charcount_t>(N1 - 1), prop, static_cast<charcount_t>(N2 - 1));
    }

    BOOL JavascriptString::BufferEquals(__in_ecount(otherLength) LPCWSTR otherBuffer, __in charcount_t otherLength)
    {LOGMEIN("JavascriptString.cpp] 326\n");
        return otherLength == this->GetLength() &&
            JsUtil::CharacterBuffer<WCHAR>::StaticEquals(this->GetString(), otherBuffer, otherLength);
    }

    BOOL JavascriptString::HasItemAt(charcount_t index)
    {LOGMEIN("JavascriptString.cpp] 332\n");
        return IsValidIndexValue(index);
    }

    BOOL JavascriptString::GetItemAt(charcount_t index, Var* value)
    {LOGMEIN("JavascriptString.cpp] 337\n");
        if (!IsValidIndexValue(index))
        {LOGMEIN("JavascriptString.cpp] 339\n");
            return false;
        }

        char16 character = GetItem(index);

        *value = this->GetLibrary()->GetCharStringCache().GetStringForChar(character);

        return true;
    }

    char16 JavascriptString::GetItem(charcount_t index)
    {
        AssertMsg( IsValidIndexValue(index), "Must specify valid character");

        const char16 *str = this->GetString();
        return str[index];
    }

    void JavascriptString::CopyHelper(__out_ecount(countNeeded) char16 *dst, __in_ecount(countNeeded) const char16 * str, charcount_t countNeeded)
    {LOGMEIN("JavascriptString.cpp] 359\n");
        switch(countNeeded)
        {LOGMEIN("JavascriptString.cpp] 361\n");
        case 0:
            return;
        case 1:
            dst[0] = str[0];
            break;
        case 3:
            dst[2] = str[2];
            goto case_2;
        case 5:
            dst[4] = str[4];
            goto case_4;
        case 7:
            dst[6] = str[6];
            goto case_6;
        case 9:
            dst[8] = str[8];
            goto case_8;

        case 10:
            *(uint32 *)(dst+8) = *(uint32*)(str+8);
            // FALLTHROUGH
        case 8:
case_8:
            *(uint32 *)(dst+6) = *(uint32*)(str+6);
            // FALLTHROUGH
        case 6:
case_6:
            *(uint32 *)(dst+4) = *(uint32*)(str+4);
            // FALLTHROUGH
        case 4:
case_4:
            *(uint32 *)(dst+2) = *(uint32*)(str+2);
            // FALLTHROUGH
        case 2:
case_2:
            *(uint32 *)(dst) = *(uint32*)str;
            break;

        default:
            js_memcpy_s(dst, sizeof(char16) * countNeeded, str, sizeof(char16) * countNeeded);
        }
    }

    JavascriptString* JavascriptString::ConcatDestructive(JavascriptString* pstRight)
    {LOGMEIN("JavascriptString.cpp] 406\n");
        Assert(pstRight);

        if(!IsFinalized())
        {LOGMEIN("JavascriptString.cpp] 410\n");
            if(CompoundString::Is(this))
            {LOGMEIN("JavascriptString.cpp] 412\n");
                return ConcatDestructive_Compound(pstRight);
            }

            if(VirtualTableInfo<ConcatString>::HasVirtualTable(this))
            {LOGMEIN("JavascriptString.cpp] 417\n");
                JavascriptString *const s = ConcatDestructive_ConcatToCompound(pstRight);
                if(s)
                {LOGMEIN("JavascriptString.cpp] 420\n");
                    return s;
                }
            }
        }
        else
        {
            const CharCount leftLength = GetLength();
            const CharCount rightLength = pstRight->GetLength();
            if(leftLength == 0 || rightLength == 0)
            {LOGMEIN("JavascriptString.cpp] 430\n");
                return ConcatDestructive_OneEmpty(pstRight);
            }

            if(CompoundString::ShouldAppendChars(leftLength) && CompoundString::ShouldAppendChars(rightLength))
            {LOGMEIN("JavascriptString.cpp] 435\n");
                return ConcatDestructive_CompoundAppendChars(pstRight);
            }
        }

#ifdef PROFILE_STRINGS
        StringProfiler::RecordConcatenation(GetScriptContext(), GetLength(), pstRight->GetLength(), ConcatType_ConcatTree);
#endif
        if(PHASE_TRACE_StringConcat)
        {LOGMEIN("JavascriptString.cpp] 444\n");
            Output::Print(
                _u("JavascriptString::ConcatDestructive(\"%.8s%s\") - creating ConcatString\n"),
                pstRight->IsFinalized() ? pstRight->GetString() : _u(""),
                !pstRight->IsFinalized() || pstRight->GetLength() > 8 ? _u("...") : _u(""));
            Output::Flush();
        }

        return ConcatString::New(this, pstRight);
    }

    JavascriptString* JavascriptString::ConcatDestructive_Compound(JavascriptString* pstRight)
    {LOGMEIN("JavascriptString.cpp] 456\n");
        Assert(CompoundString::Is(this));
        Assert(pstRight);

#ifdef PROFILE_STRINGS
        StringProfiler::RecordConcatenation(GetScriptContext(), GetLength(), pstRight->GetLength(), ConcatType_CompoundString);
#endif
        if(PHASE_TRACE_StringConcat)
        {LOGMEIN("JavascriptString.cpp] 464\n");
            Output::Print(
                _u("JavascriptString::ConcatDestructive(\"%.8s%s\") - appending to CompoundString\n"),
                pstRight->IsFinalized() ? pstRight->GetString() : _u(""),
                !pstRight->IsFinalized() || pstRight->GetLength() > 8 ? _u("...") : _u(""));
            Output::Flush();
        }

        CompoundString *const leftCs = CompoundString::FromVar(this);
        leftCs->PrepareForAppend();
        leftCs->Append(pstRight);
        return this;
    }

    JavascriptString* JavascriptString::ConcatDestructive_ConcatToCompound(JavascriptString* pstRight)
    {LOGMEIN("JavascriptString.cpp] 479\n");
        Assert(VirtualTableInfo<ConcatString>::HasVirtualTable(this));
        Assert(pstRight);

        const ConcatString *const leftConcatString = static_cast<const ConcatString *>(this);
        JavascriptString *const leftLeftString = leftConcatString->LeftString();
        if(VirtualTableInfo<ConcatString>::HasVirtualTable(leftLeftString))
        {LOGMEIN("JavascriptString.cpp] 486\n");
#ifdef PROFILE_STRINGS
            StringProfiler::RecordConcatenation(GetScriptContext(), GetLength(), pstRight->GetLength(), ConcatType_CompoundString);
#endif
            if(PHASE_TRACE_StringConcat)
            {LOGMEIN("JavascriptString.cpp] 491\n");
                Output::Print(
                    _u("JavascriptString::ConcatDestructive(\"%.8s%s\") - converting ConcatString to CompoundString\n"),
                    pstRight->IsFinalized() ? pstRight->GetString() : _u(""),
                    !pstRight->IsFinalized() || pstRight->GetLength() > 8 ? _u("...") : _u(""));
                Output::Flush();
            }

            const ConcatString *const leftLeftConcatString = static_cast<const ConcatString *>(leftConcatString->LeftString());
            CompoundString *const cs = CompoundString::NewWithPointerCapacity(8, GetLibrary());
            cs->Append(leftLeftConcatString->LeftString());
            cs->Append(leftLeftConcatString->RightString());
            cs->Append(leftConcatString->RightString());
            cs->Append(pstRight);
            return cs;
        }
        return nullptr;
    }

    JavascriptString* JavascriptString::ConcatDestructive_OneEmpty(JavascriptString* pstRight)
    {LOGMEIN("JavascriptString.cpp] 511\n");
        Assert(pstRight);
        Assert(GetLength() == 0 || pstRight->GetLength() == 0);

#ifdef PROFILE_STRINGS
        StringProfiler::RecordConcatenation(GetScriptContext(), GetLength(), pstRight->GetLength());
#endif
        if(PHASE_TRACE_StringConcat)
        {LOGMEIN("JavascriptString.cpp] 519\n");
            Output::Print(
                _u("JavascriptString::ConcatDestructive(\"%.8s%s\") - one side empty, using other side\n"),
                pstRight->IsFinalized() ? pstRight->GetString() : _u(""),
                !pstRight->IsFinalized() || pstRight->GetLength() > 8 ? _u("...") : _u(""));
            Output::Flush();
        }

        if(GetLength() == 0)
        {LOGMEIN("JavascriptString.cpp] 528\n");
            return CompoundString::GetImmutableOrScriptUnreferencedString(pstRight);
        }
        Assert(CompoundString::GetImmutableOrScriptUnreferencedString(this) == this);
        return this;
    }

    JavascriptString* JavascriptString::ConcatDestructive_CompoundAppendChars(JavascriptString* pstRight)
    {LOGMEIN("JavascriptString.cpp] 536\n");
        Assert(pstRight);
        Assert(
            GetLength() != 0 &&
            pstRight->GetLength() != 0 &&
            (CompoundString::ShouldAppendChars(GetLength()) || CompoundString::ShouldAppendChars(pstRight->GetLength())));

#ifdef PROFILE_STRINGS
        StringProfiler::RecordConcatenation(GetScriptContext(), GetLength(), pstRight->GetLength(), ConcatType_CompoundString);
#endif
        if(PHASE_TRACE_StringConcat)
        {LOGMEIN("JavascriptString.cpp] 547\n");
            Output::Print(
                _u("JavascriptString::ConcatDestructive(\"%.8s%s\") - creating CompoundString, appending chars\n"),
                pstRight->IsFinalized() ? pstRight->GetString() : _u(""),
                !pstRight->IsFinalized() || pstRight->GetLength() > 8 ? _u("...") : _u(""));
            Output::Flush();
        }

        CompoundString *const cs = CompoundString::NewWithPointerCapacity(4, GetLibrary());
        cs->AppendChars(this);
        cs->AppendChars(pstRight);
        return cs;
    }

    JavascriptString* JavascriptString::Concat(JavascriptString* pstLeft, JavascriptString* pstRight)
    {LOGMEIN("JavascriptString.cpp] 562\n");
        AssertMsg(pstLeft != nullptr, "Must have a valid left string");
        AssertMsg(pstRight != nullptr, "Must have a valid right string");

        if(!pstLeft->IsFinalized())
        {LOGMEIN("JavascriptString.cpp] 567\n");
            if(CompoundString::Is(pstLeft))
            {LOGMEIN("JavascriptString.cpp] 569\n");
                return Concat_Compound(pstLeft, pstRight);
            }

            if(VirtualTableInfo<ConcatString>::HasVirtualTable(pstLeft))
            {LOGMEIN("JavascriptString.cpp] 574\n");
                return Concat_ConcatToCompound(pstLeft, pstRight);
            }
        }
        else if(pstLeft->GetLength() == 0 || pstRight->GetLength() == 0)
        {LOGMEIN("JavascriptString.cpp] 579\n");
            return Concat_OneEmpty(pstLeft, pstRight);
        }

        if(pstLeft->GetLength() != 1 || pstRight->GetLength() != 1)
        {LOGMEIN("JavascriptString.cpp] 584\n");
#ifdef PROFILE_STRINGS
            StringProfiler::RecordConcatenation(pstLeft->GetScriptContext(), pstLeft->GetLength(), pstRight->GetLength(), ConcatType_ConcatTree);
#endif
            if(PHASE_TRACE_StringConcat)
            {LOGMEIN("JavascriptString.cpp] 589\n");
                Output::Print(
                    _u("JavascriptString::Concat(\"%.8s%s\") - creating ConcatString\n"),
                    pstRight->IsFinalized() ? pstRight->GetString() : _u(""),
                    !pstRight->IsFinalized() || pstRight->GetLength() > 8 ? _u("...") : _u(""));
                Output::Flush();
            }

            return ConcatString::New(pstLeft, pstRight);
        }

        return Concat_BothOneChar(pstLeft, pstRight);
    }

    JavascriptString* JavascriptString::Concat_Compound(JavascriptString * pstLeft, JavascriptString * pstRight)
    {LOGMEIN("JavascriptString.cpp] 604\n");
        Assert(pstLeft);
        Assert(CompoundString::Is(pstLeft));
        Assert(pstRight);

#ifdef PROFILE_STRINGS
        StringProfiler::RecordConcatenation(pstLeft->GetScriptContext(), pstLeft->GetLength(), pstRight->GetLength(), ConcatType_CompoundString);
#endif
        if(PHASE_TRACE_StringConcat)
        {LOGMEIN("JavascriptString.cpp] 613\n");
            Output::Print(
                _u("JavascriptString::Concat(\"%.8s%s\") - cloning CompoundString, appending to clone\n"),
                pstRight->IsFinalized() ? pstRight->GetString() : _u(""),
                !pstRight->IsFinalized() || pstRight->GetLength() > 8 ? _u("...") : _u(""));
            Output::Flush();
        }

        // This is not a left-dead concat, but we can reuse available space in the left string
        // because it may be accessible by script code, append to a clone.
        const bool needAppend = pstRight->GetLength() != 0;
        CompoundString *const leftCs = CompoundString::FromVar(pstLeft)->Clone(needAppend);
        if(needAppend)
        {LOGMEIN("JavascriptString.cpp] 626\n");
            leftCs->Append(pstRight);
        }
        return leftCs;
    }

    JavascriptString* JavascriptString::Concat_ConcatToCompound(JavascriptString * pstLeft, JavascriptString * pstRight)
    {LOGMEIN("JavascriptString.cpp] 633\n");
        Assert(pstLeft);
        Assert(VirtualTableInfo<ConcatString>::HasVirtualTable(pstLeft));
        Assert(pstRight);

#ifdef PROFILE_STRINGS
        StringProfiler::RecordConcatenation(pstLeft->GetScriptContext(), pstLeft->GetLength(), pstRight->GetLength(), ConcatType_CompoundString);
#endif
        if(PHASE_TRACE_StringConcat)
        {LOGMEIN("JavascriptString.cpp] 642\n");
            Output::Print(
                _u("JavascriptString::Concat(\"%.8s%s\") - converting ConcatString to CompoundString\n"),
                pstRight->IsFinalized() ? pstRight->GetString() : _u(""),
                !pstRight->IsFinalized() || pstRight->GetLength() > 8 ? _u("...") : _u(""));
            Output::Flush();
        }

        const ConcatString *const leftConcatString = static_cast<const ConcatString *>(pstLeft);
        CompoundString *const cs = CompoundString::NewWithPointerCapacity(8, pstLeft->GetLibrary());
        cs->Append(leftConcatString->LeftString());
        cs->Append(leftConcatString->RightString());
        cs->Append(pstRight);
        return cs;
    }

    JavascriptString* JavascriptString::Concat_OneEmpty(JavascriptString * pstLeft, JavascriptString * pstRight)
    {LOGMEIN("JavascriptString.cpp] 659\n");
        Assert(pstLeft);
        Assert(pstRight);
        Assert(pstLeft->GetLength() == 0 || pstRight->GetLength() == 0);

#ifdef PROFILE_STRINGS
        StringProfiler::RecordConcatenation(pstLeft->GetScriptContext(), pstLeft->GetLength(), pstRight->GetLength());
#endif
        if(PHASE_TRACE_StringConcat)
        {LOGMEIN("JavascriptString.cpp] 668\n");
            Output::Print(
                _u("JavascriptString::Concat(\"%.8s%s\") - one side empty, using other side\n"),
                pstRight->IsFinalized() ? pstRight->GetString() : _u(""),
                !pstRight->IsFinalized() || pstRight->GetLength() > 8 ? _u("...") : _u(""));
            Output::Flush();
        }

        if(pstLeft->GetLength() == 0)
        {LOGMEIN("JavascriptString.cpp] 677\n");
            return CompoundString::GetImmutableOrScriptUnreferencedString(pstRight);
        }
        Assert(CompoundString::GetImmutableOrScriptUnreferencedString(pstLeft) == pstLeft);
        return pstLeft;
    }

    JavascriptString* JavascriptString::Concat_BothOneChar(JavascriptString * pstLeft, JavascriptString * pstRight)
    {LOGMEIN("JavascriptString.cpp] 685\n");
        Assert(pstLeft);
        Assert(pstLeft->GetLength() == 1);
        Assert(pstRight);
        Assert(pstRight->GetLength() == 1);

#ifdef PROFILE_STRINGS
        StringProfiler::RecordConcatenation(pstLeft->GetScriptContext(), pstLeft->GetLength(), pstRight->GetLength(), ConcatType_BufferString);
#endif
        if(PHASE_TRACE_StringConcat)
        {LOGMEIN("JavascriptString.cpp] 695\n");
            Output::Print(
                _u("JavascriptString::Concat(\"%.8s%s\") - both sides length 1, creating BufferStringBuilder::WritableString\n"),
                pstRight->IsFinalized() ? pstRight->GetString() : _u(""),
                !pstRight->IsFinalized() || pstRight->GetLength() > 8 ? _u("...") : _u(""));
            Output::Flush();
        }

        ScriptContext* scriptContext = pstLeft->GetScriptContext();
        BufferStringBuilder builder(2, scriptContext);
        char16 * stringBuffer = builder.DangerousGetWritableBuffer();
        stringBuffer[0] = *pstLeft->GetString();
        stringBuffer[1] = *pstRight->GetString();
        return builder.ToString();
    }

    Var JavascriptString::EntryCharAt(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        //
        // General algorithm:
        //  1.  Call CheckObjectCoercible passing the this value as its argument.
        //  2.  Let S be the result of calling ToString, giving it the this value as its argument.
        //  3.  Let position be ToInteger(pos).
        //  4.  Let size be the number of characters in S.
        //  5.  If position < 0 or position = size, return the empty string.
        //  6.  Return a string of length 1, containing one character from S, where the first (leftmost) character in S is considered to be at position 0, the next one at position 1, and so on.
        //  NOTE
        //  The charAt function is intentionally generic; it does not require that its this value be a String object. Therefore, it can be transferred to other kinds of objects for use as a method.
        //

        JavascriptString * pThis = nullptr;
        GetThisStringArgument(args, scriptContext, _u("String.prototype.charAt"), &pThis);

        charcount_t idxPosition = 0;
        if (args.Info.Count > 1)
        {LOGMEIN("JavascriptString.cpp] 737\n");
            idxPosition = ConvertToIndex(args[1], scriptContext);
        }

        //
        // Get the character at the specified position.
        //

        Var value;
        if (pThis->GetItemAt(idxPosition, &value))
        {LOGMEIN("JavascriptString.cpp] 747\n");
            return value;
        }
        else
        {
            return scriptContext->GetLibrary()->GetEmptyString();
        }
    }


    Var JavascriptString::EntryCharCodeAt(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        //
        // General algorithm:
        // 1.  Call CheckObjectCoercible passing the this value as its argument.
        // 2.  Let S be the result of calling ToString, giving it the this value as its argument.
        // 3.  Let position be ToInteger(pos).
        // 4.  Let size be the number of characters in S.
        // 5.  If position < 0 or position = size, return NaN.
        // 6.  Return a value of Number type, whose value is the code unit value of the character at that position in the string S, where the first (leftmost) character in S is considered to be at position 0, the next one at position 1, and so on.
        // NOTE
        // The charCodeAt function is intentionally generic; it does not require that its this value be a String object. Therefore it can be transferred to other kinds of objects for use as a method.
        //
        JavascriptString * pThis = nullptr;
        GetThisStringArgument(args, scriptContext, _u("String.prototype.charCodeAt"), &pThis);

        charcount_t idxPosition = 0;
        if (args.Info.Count > 1)
        {LOGMEIN("JavascriptString.cpp] 782\n");
            idxPosition = ConvertToIndex(args[1], scriptContext);
        }

        //
        // Get the character at the specified position.
        //

        charcount_t charLength = pThis->GetLength();
        if (idxPosition >= charLength)
        {LOGMEIN("JavascriptString.cpp] 792\n");
            return scriptContext->GetLibrary()->GetNaN();
        }

        return TaggedInt::ToVarUnchecked(pThis->GetItem(idxPosition));
    }

    Var JavascriptString::EntryCodePointAt(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        JavascriptString * pThis = nullptr;
        GetThisStringArgument(args, scriptContext, _u("String.prototype.codePointAt"), &pThis);

        charcount_t idxPosition = 0;
        if (args.Info.Count > 1)
        {LOGMEIN("JavascriptString.cpp] 813\n");
            idxPosition = ConvertToIndex(args[1], scriptContext);
        }

        charcount_t charLength = pThis->GetLength();
        if (idxPosition >= charLength)
        {LOGMEIN("JavascriptString.cpp] 819\n");
            return scriptContext->GetLibrary()->GetUndefined();
        }

        // A surrogate pair consists of two characters, a lower part and a higher part.
        // Lower part is in range [0xD800 - 0xDBFF], while the higher is [0xDC00 - 0xDFFF].
        char16 first = pThis->GetItem(idxPosition);
        if (first >= 0xD800u && first < 0xDC00u && (uint)(idxPosition + 1) < pThis->GetLength())
        {LOGMEIN("JavascriptString.cpp] 827\n");
            char16 second = pThis->GetItem(idxPosition + 1);
            if (second >= 0xDC00 && second < 0xE000)
            {LOGMEIN("JavascriptString.cpp] 830\n");
                return TaggedInt::ToVarUnchecked(NumberUtilities::SurrogatePairAsCodePoint(first, second));
            }
        }
        return TaggedInt::ToVarUnchecked(first);
    }

    Var JavascriptString::EntryConcat(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        //
        // General algorithm:
        // 1.  Call CheckObjectCoercible passing the this value as its argument.
        // 2.  Let S be the result of calling ToString, giving it the this value as its argument.
        // 3.  Let args be an internal list that is a copy of the argument list passed to this function.
        // 4.  Let R be S.
        // 5.  Repeat, while args is not empty
        //     Remove the first element from args and let next be the value of that element.
        //     Let R be the string value consisting of the characters in the previous value of R followed by the characters of ToString(next).
        // 6.  Return R.
        //
        // NOTE
        // The concat function is intentionally generic; it does not require that its this value be a String object. Therefore it can be transferred to other kinds of objects for use as a method.
        //

        if(args.Info.Count == 0)
        {LOGMEIN("JavascriptString.cpp] 862\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedString, _u("String.prototype.concat"));
        }
        AssertMsg(args.Info.Count > 0, "Negative argument count");
        if (!JavascriptConversion::CheckObjectCoercible(args[0], scriptContext))
        {LOGMEIN("JavascriptString.cpp] 867\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("String.prototype.concat"));
        }

        JavascriptString* pstr = nullptr;
        JavascriptString* accum = nullptr;
        for (uint index = 0; index < args.Info.Count; index++)
        {LOGMEIN("JavascriptString.cpp] 874\n");
            if (JavascriptString::Is(args[index]))
            {LOGMEIN("JavascriptString.cpp] 876\n");
                pstr = JavascriptString::FromVar(args[index]);
            }
            else
            {
                pstr = JavascriptConversion::ToString(args[index], scriptContext);
            }

            if (index == 0)
            {LOGMEIN("JavascriptString.cpp] 885\n");
                accum = pstr;
            }
            else
            {
                accum = Concat(accum,pstr);
            }

        }

        return accum;
    }

    Var JavascriptString::EntryFromCharCode(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        //
        // Construct a new string instance to contain all of the explicit parameters:
        // - Don't include the 'this' parameter.
        //
        if(args.Info.Count == 0)
        {LOGMEIN("JavascriptString.cpp] 912\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedString, _u("String.fromCharCode"));
        }
        AssertMsg(args.Info.Count > 0, "Negative argument count");

        int charLength = args.Info.Count - 1;

        // Special case for single char
        if( charLength == 1 )
        {LOGMEIN("JavascriptString.cpp] 921\n");
            char16 ch = JavascriptConversion::ToUInt16(args[1], scriptContext);
            return scriptContext->GetLibrary()->GetCharStringCache().GetStringForChar(ch);
        }

        BufferStringBuilder builder(charLength,scriptContext);
        char16 * stringBuffer = builder.DangerousGetWritableBuffer();

        //
        // Call ToUInt16 for each parameter, storing the character at the appropriate position.
        //

        for (uint idxArg = 1; idxArg < args.Info.Count; idxArg++)
        {LOGMEIN("JavascriptString.cpp] 934\n");
            *stringBuffer++ = JavascriptConversion::ToUInt16(args[idxArg], scriptContext);
        }

        //
        // Return the new string instance.
        //
        return builder.ToString();
    }

    Var JavascriptString::EntryFromCodePoint(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        AssertMsg(args.Info.Count > 0, "Negative argument count");

        if (args.Info.Count <= 1)
        {LOGMEIN("JavascriptString.cpp] 956\n");
            return scriptContext->GetLibrary()->GetEmptyString();
        }
        else if (args.Info.Count == 2)
        {LOGMEIN("JavascriptString.cpp] 960\n");
            // Special case for a single char string formed from only code point in range [0x0, 0xFFFF]
            double num = JavascriptConversion::ToNumber(args[1], scriptContext);

            if (!NumberUtilities::IsFinite(num))
            {LOGMEIN("JavascriptString.cpp] 965\n");
                JavascriptError::ThrowRangeError(scriptContext, JSERR_InvalidCodePoint);
            }

            if (num < 0 || num > 0x10FFFF || floor(num) != num)
            {LOGMEIN("JavascriptString.cpp] 970\n");
                JavascriptError::ThrowRangeErrorVar(scriptContext, JSERR_InvalidCodePoint, Js::JavascriptConversion::ToString(args[1], scriptContext)->GetSz());
            }

            if (num < 0x10000)
            {LOGMEIN("JavascriptString.cpp] 975\n");
                return scriptContext->GetLibrary()->GetCharStringCache().GetStringForChar((uint16)num);
            }
        }

        BEGIN_TEMP_ALLOCATOR(tempAllocator, scriptContext, _u("fromCodePoint"));
        // Create a temporary buffer that is double the arguments count (in case all are surrogate pairs)
        size_t bufferLength = (args.Info.Count - 1) * 2;
        char16 *tempBuffer = AnewArray(tempAllocator, char16, bufferLength);
        uint32 count = 0;

        for (uint i = 1; i < args.Info.Count; i++)
        {LOGMEIN("JavascriptString.cpp] 987\n");
            double num = JavascriptConversion::ToNumber(args[i], scriptContext);

            if (!NumberUtilities::IsFinite(num))
            {LOGMEIN("JavascriptString.cpp] 991\n");
                JavascriptError::ThrowRangeError(scriptContext, JSERR_InvalidCodePoint);
            }

            if (num < 0 || num > 0x10FFFF || floor(num) != num)
            {LOGMEIN("JavascriptString.cpp] 996\n");
                JavascriptError::ThrowRangeErrorVar(scriptContext, JSERR_InvalidCodePoint, Js::JavascriptConversion::ToString(args[i], scriptContext)->GetSz());
            }

            if (num < 0x10000)
            {LOGMEIN("JavascriptString.cpp] 1001\n");
                __analysis_assume(count < bufferLength);
                Assert(count < bufferLength);
#pragma prefast(suppress: 22102, "I have an assert in place to guard against overflow. Even though this should never happen.")
                tempBuffer[count] = (char16)num;
                count++;
            }
            else
            {
                __analysis_assume(count + 1 < bufferLength);
                Assert(count  + 1 < bufferLength);
                NumberUtilities::CodePointAsSurrogatePair((codepoint_t)num, (tempBuffer + count), (tempBuffer + count + 1));
                count += 2;
            }
        }
        // Create a string of appropriate length
        __analysis_assume(count <= bufferLength);
        Assert(count <= bufferLength);
        JavascriptString *toReturn = JavascriptString::NewCopyBuffer(tempBuffer, count, scriptContext);


        END_TEMP_ALLOCATOR(tempAllocator, scriptContext);
        return toReturn;
    }

    Var JavascriptString::EntryIndexOf(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        return JavascriptNumber::ToVar(IndexOf(args, scriptContext, _u("String.prototype.indexOf"), true), scriptContext);
    }

    int JavascriptString::IndexOf(ArgumentReader& args, ScriptContext* scriptContext, const char16* apiNameForErrorMsg, bool isRegExpAnAllowedArg)
    {LOGMEIN("JavascriptString.cpp] 1040\n");
        // The algorithm steps in the spec are the same between String.prototype.indexOf and
        // String.prototype.includes, except that includes returns true if an index is found,
        // false otherwise.  Share the implementation between these two APIs.
        //
        // 1.  Call CheckObjectCoercible passing the this value as its argument.
        // 2.  Let S be the result of calling ToString, giving it the this value as its argument.
        // 3.  Let searchStr be ToString(searchString).
        // 4.  Let pos be ToInteger(position). (If position is undefined, this step produces the value 0).
        // 5.  Let len be the number of characters in S.
        // 6.  Let start be min(max(pos, 0), len).
        // 7.  Let searchLen be the number of characters in searchStr.
        // 8.  Return the smallest possible integer k not smaller than start such that k+ searchLen is not greater than len, and for all nonnegative integers j less than searchLen, the character at position k+j of S is the same as the character at position j of searchStr); but if there is no such integer k, then return the value -1.
        // NOTE
        // The indexOf function is intentionally generic; it does not require that its this value be a String object. Therefore, it can be transferred to other kinds of objects for use as a method.
        //

        JavascriptString * pThis;
        JavascriptString * searchString;

        GetThisAndSearchStringArguments(args, scriptContext, apiNameForErrorMsg, &pThis, &searchString, isRegExpAnAllowedArg);

        int len = pThis->GetLength();
        int searchLen = searchString->GetLength();

        int position = 0;

        if (args.Info.Count > 2)
        {LOGMEIN("JavascriptString.cpp] 1068\n");
            if (JavascriptOperators::IsUndefinedObject(args[2], scriptContext))
            {LOGMEIN("JavascriptString.cpp] 1070\n");
                position = 0;
            }
            else
            {
                position = ConvertToIndex(args[2], scriptContext); // this is to adjust corner cases like MAX_VALUE
                position = min(max(position, 0), len);  // adjust position within string limits
            }
        }

        // Zero length search strings are always found at the current search position
        if (searchLen == 0)
        {LOGMEIN("JavascriptString.cpp] 1082\n");
            return position;
        }

        int result = -1;

        if (position < pThis->GetLengthAsSignedInt())
        {LOGMEIN("JavascriptString.cpp] 1089\n");
            const char16* searchStr = searchString->GetString();
            const char16* inputStr = pThis->GetString();
            if (searchLen == 1)
            {LOGMEIN("JavascriptString.cpp] 1093\n");
                int i = position;
                for(; i < len && inputStr[i] != *searchStr ; i++);
                if (i < len)
                {LOGMEIN("JavascriptString.cpp] 1097\n");
                    result = i;
                }
            }
            else
            {
                JmpTable jmpTable;
                bool fAsciiJumpTable = BuildLastCharForwardBoyerMooreTable(jmpTable, searchStr, searchLen);
                if (!fAsciiJumpTable)
                {LOGMEIN("JavascriptString.cpp] 1106\n");
                    result = JavascriptString::strstr(pThis, searchString, false, position);
                }
                else
                {
                    result = IndexOfUsingJmpTable(jmpTable, inputStr, len, searchStr, searchLen, position);
                }
            }
        }
        return result;
    }

    Var JavascriptString::EntryLastIndexOf(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        // ES #sec-string.prototype.lastindexof
        // 21.1.3.9  String.prototype.lastIndexOf(searchString[, position])
        //
        // 1. Let O be ? RequireObjectCoercible(this value).
        // 2. Let S be ? ToString(O).
        // 3. Let searchStr be ? ToString(searchString).

        JavascriptString * pThis = nullptr;
        GetThisStringArgument(args, scriptContext, _u("String.prototype.lastIndexOf"), &pThis);

        // default search string if the search argument is not provided
        JavascriptString * searchArg;
        if(args.Info.Count > 1)
        {LOGMEIN("JavascriptString.cpp] 1140\n");
            if (JavascriptString::Is(args[1]))
            {LOGMEIN("JavascriptString.cpp] 1142\n");
                searchArg = JavascriptString::FromVar(args[1]);
            }
            else
            {
                searchArg = JavascriptConversion::ToString(args[1], scriptContext);
            }
        }
        else
        {
            searchArg = scriptContext->GetLibrary()->GetUndefinedDisplayString();
        }

        const char16* const inputStr = pThis->GetString();
        const char16 * const searchStr = searchArg->GetString();

        // 4. Let numPos be ? ToNumber(position). (If position is undefined, this step produces the value NaN.)
        // 5. If numPos is NaN, let pos be +infinity; otherwise, let pos be ToInteger(numPos).
        // 6. Let len be the number of elements in S.
        // 7. Let start be min(max(pos, 0), len).

        const charcount_t inputLen = pThis->GetLength();
        const charcount_t searchLen = searchArg->GetLength();
        charcount_t position = inputLen;
        const char16* const searchLowerBound = inputStr;

        // Determine if the main string can't contain the search string by length
        if (searchLen > inputLen)
        {LOGMEIN("JavascriptString.cpp] 1170\n");
            return JavascriptNumber::ToVar(-1, scriptContext);
        }

        if (args.Info.Count > 2)
        {LOGMEIN("JavascriptString.cpp] 1175\n");
            double pos = JavascriptConversion::ToNumber(args[2], scriptContext);
            if (!JavascriptNumber::IsNan(pos))
            {LOGMEIN("JavascriptString.cpp] 1178\n");
                pos = JavascriptConversion::ToInteger(pos);
                if (pos > inputLen - searchLen)
                {LOGMEIN("JavascriptString.cpp] 1181\n");
                    // No point searching beyond the possible end point.
                    pos = inputLen - searchLen;
                }
                position = (charcount_t)min(max(pos, (double)0), (double)inputLen); // adjust position within string limits
            }
        }

        if (position > inputLen - searchLen)
        {LOGMEIN("JavascriptString.cpp] 1190\n");
            // No point searching beyond the possible end point.
            position = inputLen - searchLen;
        }
        const char16* const searchUpperBound = searchLowerBound + min(position, inputLen - 1);

        // 8. Let searchLen be the number of elements in searchStr.
        // 9. Return the largest possible nonnegative integer k not larger than start such that k + searchLen is
        //    not greater than len, and for all nonnegative integers j less than searchLen, the code unit at
        //    index k + j of S is the same as the code unit at index j of searchStr; but if there is no such
        //    integer k, return the value - 1.
        // Note: The lastIndexOf function is intentionally generic; it does not require that its this value be a
        //    String object. Therefore, it can be transferred to other kinds of objects for use as a method.

        // Zero length search strings are always found at the current search position
        if (searchLen == 0)
        {LOGMEIN("JavascriptString.cpp] 1206\n");
            return JavascriptNumber::ToVar(position, scriptContext);
        }
        else if (searchLen == 1)
        {LOGMEIN("JavascriptString.cpp] 1210\n");
            char16 const * current = searchUpperBound;
            while (*current != *searchStr)
            {LOGMEIN("JavascriptString.cpp] 1213\n");
                current--;
                if (current < inputStr)
                {LOGMEIN("JavascriptString.cpp] 1216\n");
                    return JavascriptNumber::ToVar(-1, scriptContext);
                }
            }
            return JavascriptNumber::ToVar(current - inputStr, scriptContext);
        }

        // Structure for a partial ASCII Boyer-Moore
        JmpTable jmpTable;
        if (BuildFirstCharBackwardBoyerMooreTable(jmpTable, searchStr, searchLen))
        {LOGMEIN("JavascriptString.cpp] 1226\n");
            int result = LastIndexOfUsingJmpTable(jmpTable, inputStr, inputLen, searchStr, searchLen, position);
            return JavascriptNumber::ToVar(result, scriptContext);
        }

        // Revert to slow search if we decided not to do Boyer-Moore.
        char16 const * currentPos = searchUpperBound;
        Assert(currentPos - searchLowerBound + searchLen <= inputLen);
        while (currentPos >= searchLowerBound)
        {LOGMEIN("JavascriptString.cpp] 1235\n");
            if (*currentPos == *searchStr)
            {
                // Quick start char chec
                if (wmemcmp(currentPos, searchStr, searchLen) == 0)
                {LOGMEIN("JavascriptString.cpp] 1240\n");
                    return JavascriptNumber::ToVar(currentPos - searchLowerBound, scriptContext);
                }
            }
            --currentPos;
        }
        return JavascriptNumber::ToVar(-1, scriptContext);
    }

    // Performs common ES spec steps for getting this argument in string form:
    // 1. Let O be CHeckObjectCoercible(this value).
    // 2. Let S be ToString(O).
    // 3. ReturnIfAbrupt(S).
    void JavascriptString::GetThisStringArgument(ArgumentReader& args, ScriptContext* scriptContext, const char16* apiNameForErrorMsg, JavascriptString** ppThis)
    {LOGMEIN("JavascriptString.cpp] 1254\n");
        if (args.Info.Count == 0)
        {LOGMEIN("JavascriptString.cpp] 1256\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedString, apiNameForErrorMsg);
        }
        AssertMsg(args.Info.Count > 0, "Negative argument count");

        JavascriptString * pThis;
        if (JavascriptString::Is(args[0]))
        {LOGMEIN("JavascriptString.cpp] 1263\n");
            pThis = JavascriptString::FromVar(args[0]);
        }
        else
        {

            pThis = JavascriptConversion::CoerseString(args[0], scriptContext , apiNameForErrorMsg);

        }

        *ppThis = pThis;
    }

    // Performs common ES spec steps for getting this and first parameter arguments in string form:
    // 1. Let O be CHeckObjectCoercible(this value).
    // 2. Let S be ToString(O).
    // 3. ReturnIfAbrupt(S).
    // 4. Let otherStr be ToString(firstArg).
    // 5. ReturnIfAbrupt(otherStr).
    void JavascriptString::GetThisAndSearchStringArguments(ArgumentReader& args, ScriptContext* scriptContext, const char16* apiNameForErrorMsg, JavascriptString** ppThis, JavascriptString** ppSearch, bool isRegExpAnAllowedArg)
    {
        GetThisStringArgument(args, scriptContext, apiNameForErrorMsg, ppThis);

        JavascriptString * pSearch = scriptContext->GetLibrary()->GetUndefinedDisplayString();
        if (args.Info.Count > 1)
        {LOGMEIN("JavascriptString.cpp] 1288\n");
            if (!isRegExpAnAllowedArg && JavascriptRegExp::IsRegExpLike(args[1], scriptContext))
            {LOGMEIN("JavascriptString.cpp] 1290\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_FirstCannotBeRegExp, apiNameForErrorMsg);
            }
            else if (JavascriptString::Is(args[1]))
            {LOGMEIN("JavascriptString.cpp] 1294\n");
                pSearch = JavascriptString::FromVar(args[1]);
            }
            else
            {
                pSearch = JavascriptConversion::ToString(args[1], scriptContext);
            }
        }

        *ppSearch = pSearch;
    }

    Var JavascriptString::EntryLocaleCompare(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if(args.Info.Count == 0)
        {LOGMEIN("JavascriptString.cpp] 1316\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedString, _u("String.prototype.localeCompare"));
        }
        AssertMsg(args.Info.Count > 0, "Negative argument count");

        JavascriptString * pThis;
        JavascriptString * pThat;

        GetThisAndSearchStringArguments(args, scriptContext, _u("String.prototype.localeCompare"), &pThis, &pThat, true);

#ifdef ENABLE_INTL_OBJECT
        if (CONFIG_FLAG(IntlBuiltIns) && scriptContext->IsIntlEnabled())
        {LOGMEIN("JavascriptString.cpp] 1328\n");
            EngineInterfaceObject* nativeEngineInterfaceObj = scriptContext->GetLibrary()->GetEngineInterfaceObject();
            if (nativeEngineInterfaceObj)
            {LOGMEIN("JavascriptString.cpp] 1331\n");
                IntlEngineInterfaceExtensionObject* intlExtensionObject = static_cast<IntlEngineInterfaceExtensionObject*>(nativeEngineInterfaceObj->GetEngineExtension(EngineInterfaceExtensionKind_Intl));
                if (args.Info.Count == 2)
                {LOGMEIN("JavascriptString.cpp] 1334\n");
                    auto undefined = scriptContext->GetLibrary()->GetUndefined();
                    CallInfo toPass(callInfo.Flags, 7);
                    return intlExtensionObject->EntryIntl_CompareString(function, toPass, undefined, pThis, pThat, undefined, undefined, undefined, undefined);
                }
                else
                {
                    JavascriptFunction* func = intlExtensionObject->GetStringLocaleCompare();
                    if (func)
                    {LOGMEIN("JavascriptString.cpp] 1343\n");
                        return func->CallFunction(args);
                    }
                    // Initialize String.prototype.toLocaleCompare
                    scriptContext->GetLibrary()->InitializeIntlForStringPrototype();
                    func = intlExtensionObject->GetStringLocaleCompare();
                    if (func)
                    {LOGMEIN("JavascriptString.cpp] 1350\n");
                        return func->CallFunction(args);
                    }
                }
            }
        }
#endif

        const char16* pThisStr = pThis->GetString();
        int thisStrCount = pThis->GetLength();

        const char16* pThatStr = pThat->GetString();
        int thatStrCount = pThat->GetLength();

        // xplat-todo: doing a locale-insensitive compare here
        // but need to move locale-specific string comparison to
        // platform agnostic interface
#ifdef ENABLE_GLOBALIZATION
        LCID lcid = GetUserDefaultLCID();
        int result = CompareStringW(lcid, NULL, pThisStr, thisStrCount, pThatStr, thatStrCount );
        if (result == 0)
        {LOGMEIN("JavascriptString.cpp] 1371\n");
            // TODO there is no spec on the error thrown here.
            // When the support for HR errors is implemented replace this with the same error reported by v5.8
            JavascriptError::ThrowRangeError(function->GetScriptContext(),
                VBSERR_InternalError /* TODO-ERROR: _u("Failed compare operation")*/ );
        }
        return JavascriptNumber::ToVar(result-2, scriptContext);
#else // !ENABLE_GLOBALIZATION
        // no ICU / or external support for localization. Use c-lib
        const int result = wcscmp(pThisStr, pThatStr);
        return JavascriptNumber::ToVar(result > 0 ? 1 : result == 0 ? 0 : -1, scriptContext);
#endif
    }


    Var JavascriptString::EntryMatch(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        PCWSTR const varName = _u("String.prototype.match");

        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, varName);

        auto fallback = [&](JavascriptString* stringObj)
        {LOGMEIN("JavascriptString.cpp] 1400\n");
            Var regExp = (args.Info.Count > 1) ? args[1] : scriptContext->GetLibrary()->GetUndefined();

            if (!scriptContext->GetConfig()->IsES6RegExSymbolsEnabled())
            {LOGMEIN("JavascriptString.cpp] 1404\n");
                JavascriptRegExp * regExObj = JavascriptRegExp::CreateRegEx(regExp, nullptr, scriptContext);
                return RegexHelper::RegexMatch(
                    scriptContext,
                    regExObj,
                    stringObj,
                    RegexHelper::IsResultNotUsed(callInfo.Flags));
            }
            else
            {
                JavascriptRegExp * regExObj = JavascriptRegExp::CreateRegExNoCoerce(regExp, nullptr, scriptContext);
                Var symbolFn = GetRegExSymbolFunction(regExObj, PropertyIds::_symbolMatch, scriptContext);
                return CallRegExSymbolFunction<1>(symbolFn, regExObj, args, varName, scriptContext);
            }
        };
        return DelegateToRegExSymbolFunction<1>(args, PropertyIds::_symbolMatch, fallback, varName, scriptContext);
    }

    Var JavascriptString::EntryNormalize(RecyclableObject* function, CallInfo callInfo, ...)
    {
        using namespace PlatformAgnostic;

        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        JavascriptString *pThis = nullptr;
        GetThisStringArgument(args, scriptContext, _u("String.prototype.normalize"), &pThis);

        UnicodeText::NormalizationForm form = UnicodeText::NormalizationForm::C;

        if (args.Info.Count >= 2 && !(JavascriptOperators::IsUndefinedObject(args.Values[1])))
        {LOGMEIN("JavascriptString.cpp] 1439\n");
            JavascriptString *formStr = nullptr;
            if (JavascriptString::Is(args[1]))
            {LOGMEIN("JavascriptString.cpp] 1442\n");
                formStr = JavascriptString::FromVar(args[1]);
            }
            else
            {
                formStr = JavascriptConversion::ToString(args[1], scriptContext);
            }

            if (formStr->BufferEquals(_u("NFD"), 3))
            {LOGMEIN("JavascriptString.cpp] 1451\n");
                form = UnicodeText::NormalizationForm::D;
            }
            else if (formStr->BufferEquals(_u("NFKC"), 4))
            {LOGMEIN("JavascriptString.cpp] 1455\n");
                form = UnicodeText::NormalizationForm::KC;
            }
            else if (formStr->BufferEquals(_u("NFKD"), 4))
            {LOGMEIN("JavascriptString.cpp] 1459\n");
                form = UnicodeText::NormalizationForm::KD;
            }
            else if (!formStr->BufferEquals(_u("NFC"), 3))
            {LOGMEIN("JavascriptString.cpp] 1463\n");
                JavascriptError::ThrowRangeErrorVar(scriptContext, JSERR_InvalidNormalizationForm, formStr->GetString());
            }
        }

        if (UnicodeText::IsNormalizedString(form, pThis->GetSz(), pThis->GetLength()))
        {LOGMEIN("JavascriptString.cpp] 1469\n");
            return pThis;
        }

        BEGIN_TEMP_ALLOCATOR(tempAllocator, scriptContext, _u("normalize"));

        charcount_t sizeEstimate = 0;
        char16* buffer = pThis->GetNormalizedString(form, tempAllocator, sizeEstimate);
        JavascriptString * retVal;
        if (buffer == nullptr)
        {LOGMEIN("JavascriptString.cpp] 1479\n");
            Assert(sizeEstimate == 0);
            retVal = scriptContext->GetLibrary()->GetEmptyString();
        }
        else
        {
            retVal = JavascriptString::NewCopyBuffer(buffer, sizeEstimate, scriptContext);
        }

        END_TEMP_ALLOCATOR(tempAllocator, scriptContext);
        return retVal;
    }

    ///----------------------------------------------------------------------------
    /// String.raw(), as described in (ES6.0 (Draft 18): S21.1.2.4).
    ///----------------------------------------------------------------------------
    Var JavascriptString::EntryRaw(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count < 2)
        {LOGMEIN("JavascriptString.cpp] 1505\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedObject, _u("String.raw"));
        }

        RecyclableObject* callSite;
        RecyclableObject* raw;
        Var rawVar;

        // Call ToObject on the first argument to get the callSite (which is also cooked string array)
        // ToObject returns false if the parameter is null or undefined
        if (!JavascriptConversion::ToObject(args[1], scriptContext, &callSite))
        {LOGMEIN("JavascriptString.cpp] 1516\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedObject, _u("String.raw"));
        }

        // Get the raw property from the callSite object
        if (!callSite->GetProperty(callSite, Js::PropertyIds::raw, &rawVar, nullptr, scriptContext))
        {LOGMEIN("JavascriptString.cpp] 1522\n");
            rawVar = scriptContext->GetLibrary()->GetUndefined();
        }

        if (!JavascriptConversion::ToObject(rawVar, scriptContext, &raw))
        {LOGMEIN("JavascriptString.cpp] 1527\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedObject, _u("String.raw"));
        }

        int64 length = JavascriptConversion::ToLength(JavascriptOperators::OP_GetLength(raw, scriptContext), scriptContext);

        // If there are no raw strings (somehow), return empty string
        if (length <= 0)
        {LOGMEIN("JavascriptString.cpp] 1535\n");
            return scriptContext->GetLibrary()->GetEmptyString();
        }

        // Get the first raw string
        Var var = JavascriptOperators::OP_GetElementI_UInt32(raw, 0, scriptContext);
        JavascriptString* string = JavascriptConversion::ToString(var, scriptContext);

        // If there is only one raw string, just return that one raw string (doesn't matter if there are replacements)
        if (length == 1)
        {LOGMEIN("JavascriptString.cpp] 1545\n");
            return string;
        }

        // We aren't going to bail early so let's create a StringBuilder and put the first raw string in there
        CompoundString::Builder<64 * sizeof(void *) / sizeof(char16)> stringBuilder(scriptContext);
        stringBuilder.Append(string);

        // Each raw string is followed by a substitution expression except for the last one
        // We will always have one more string constant than substitution expression
        // `strcon1 ${expr1} strcon2 ${expr2} strcon3` = strcon1 + expr1 + strcon2 + expr2 + strcon3
        //
        // strcon1 --- step 1 (above)
        // expr1   \__ step 2
        // strcon2 /
        // expr2   \__ step 3
        // strcon3 /
        for (uint32 i = 1; i < length; ++i)
        {LOGMEIN("JavascriptString.cpp] 1563\n");
            // First append the next substitution expression
            // If we have an arg at [i+1] use that one, otherwise empty string (which is nop)
            if (i+1 < args.Info.Count)
            {LOGMEIN("JavascriptString.cpp] 1567\n");
                string = JavascriptConversion::ToString(args[i+1], scriptContext);

                stringBuilder.Append(string);
            }

            // Then append the next string (this will also cover the final string case)
            var = JavascriptOperators::OP_GetElementI_UInt32(raw, i, scriptContext);
            string = JavascriptConversion::ToString(var, scriptContext);

            stringBuilder.Append(string);
        }

        // CompoundString::Builder has saved our lives
        return stringBuilder.ToString();
    }

    Var JavascriptString::EntryReplace(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        PCWSTR const varName = _u("String.prototype.replace");

        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, varName);

        Assert(!(callInfo.Flags & CallFlags_New));

        auto fallback = [&](JavascriptString* stringObj)
        {LOGMEIN("JavascriptString.cpp] 1598\n");
            return DoStringReplace(args, callInfo, stringObj, scriptContext);
        };
        return DelegateToRegExSymbolFunction<2>(args, PropertyIds::_symbolReplace, fallback, varName, scriptContext);
    }

    Var JavascriptString::DoStringReplace(Arguments& args, CallInfo& callInfo, JavascriptString* input, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptString.cpp] 1605\n");
        //
        // TODO: Move argument processing into DirectCall with proper handling.
        //

        JavascriptRegExp * pRegEx = nullptr;
        JavascriptString * pMatch = nullptr;

        JavascriptString * pReplace = nullptr;
        JavascriptFunction* replacefn = nullptr;

        SearchValueHelper(scriptContext, ((args.Info.Count > 1)?args[1]:scriptContext->GetLibrary()->GetNull()), &pRegEx, &pMatch);
        ReplaceValueHelper(scriptContext, ((args.Info.Count > 2) ? args[2] : scriptContext->GetLibrary()->GetUndefined()), &replacefn, &pReplace);

        if (pRegEx != nullptr)
        {LOGMEIN("JavascriptString.cpp] 1620\n");
            if (replacefn != nullptr)
            {LOGMEIN("JavascriptString.cpp] 1622\n");
                return RegexHelper::RegexReplaceFunction(scriptContext, pRegEx, input, replacefn);
            }
            else
            {
                return RegexHelper::RegexReplace(scriptContext, pRegEx, input, pReplace, RegexHelper::IsResultNotUsed(callInfo.Flags));
            }
        }

        AssertMsg(pMatch != nullptr, "Match string shouldn't be null");
        if (replacefn != nullptr)
        {LOGMEIN("JavascriptString.cpp] 1633\n");
            return RegexHelper::StringReplace(pMatch, input, replacefn);
        }
        else
        {
            if (callInfo.Flags & CallFlags_NotUsed)
            {LOGMEIN("JavascriptString.cpp] 1639\n");
                return scriptContext->GetLibrary()->GetEmptyString();
            }
            return RegexHelper::StringReplace(pMatch, input, pReplace);
        }
    }

    void JavascriptString::SearchValueHelper(ScriptContext* scriptContext, Var aValue, JavascriptRegExp ** ppSearchRegEx, JavascriptString ** ppSearchString)
    {LOGMEIN("JavascriptString.cpp] 1647\n");
        *ppSearchRegEx = nullptr;
        *ppSearchString = nullptr;

        // When the config is enabled, the operation is handled by a Symbol function (e.g. Symbol.replace).
        if (!scriptContext->GetConfig()->IsES6RegExSymbolsEnabled()
            && JavascriptRegExp::Is(aValue))
        {LOGMEIN("JavascriptString.cpp] 1654\n");
            *ppSearchRegEx = JavascriptRegExp::FromVar(aValue);
        }
        else if (JavascriptString::Is(aValue))
        {LOGMEIN("JavascriptString.cpp] 1658\n");
            *ppSearchString = JavascriptString::FromVar(aValue);
        }
        else
        {
            *ppSearchString = JavascriptConversion::ToString(aValue, scriptContext);
        }
    }

    void JavascriptString::ReplaceValueHelper(ScriptContext* scriptContext, Var aValue, JavascriptFunction ** ppReplaceFn, JavascriptString ** ppReplaceString)
    {LOGMEIN("JavascriptString.cpp] 1668\n");
        *ppReplaceFn = nullptr;
        *ppReplaceString = nullptr;

        if (JavascriptFunction::Is(aValue))
        {LOGMEIN("JavascriptString.cpp] 1673\n");
            *ppReplaceFn = JavascriptFunction::FromVar(aValue);
        }
        else if (JavascriptString::Is(aValue))
        {LOGMEIN("JavascriptString.cpp] 1677\n");
            *ppReplaceString = JavascriptString::FromVar(aValue);
        }
        else
        {
            *ppReplaceString = JavascriptConversion::ToString(aValue, scriptContext);
        }
    }

    Var JavascriptString::EntrySearch(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        PCWSTR const varName = _u("String.prototype.search");

        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, varName);

        auto fallback = [&](JavascriptString* stringObj)
        {LOGMEIN("JavascriptString.cpp] 1700\n");
            Var regExp = (args.Info.Count > 1) ? args[1] : scriptContext->GetLibrary()->GetUndefined();

            if (!scriptContext->GetConfig()->IsES6RegExSymbolsEnabled())
            {LOGMEIN("JavascriptString.cpp] 1704\n");
                JavascriptRegExp * regExObj = JavascriptRegExp::CreateRegEx(regExp, nullptr, scriptContext);
                return RegexHelper::RegexSearch(scriptContext, regExObj, stringObj);
            }
            else
            {
                JavascriptRegExp * regExObj = JavascriptRegExp::CreateRegExNoCoerce(regExp, nullptr, scriptContext);
                Var symbolFn = GetRegExSymbolFunction(regExObj, PropertyIds::_symbolSearch, scriptContext);
                return CallRegExSymbolFunction<1>(symbolFn, regExObj, args, varName, scriptContext);
            }
        };
        return DelegateToRegExSymbolFunction<1>(args, PropertyIds::_symbolSearch, fallback, varName, scriptContext);
    }

    template<int argCount, typename FallbackFn>
    Var JavascriptString::DelegateToRegExSymbolFunction(ArgumentReader &args, PropertyId symbolPropertyId, FallbackFn fallback, PCWSTR varName, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptString.cpp] 1720\n");
        if (scriptContext->GetConfig()->IsES6RegExSymbolsEnabled())
        {LOGMEIN("JavascriptString.cpp] 1722\n");
            if (args.Info.Count == 0 || !JavascriptConversion::CheckObjectCoercible(args[0], scriptContext))
            {LOGMEIN("JavascriptString.cpp] 1724\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, varName);
            }

            if (args.Info.Count >= 2 && !JavascriptOperators::IsUndefinedOrNull(args[1]))
            {LOGMEIN("JavascriptString.cpp] 1729\n");
                Var regExp = args[1];
                Var symbolFn = GetRegExSymbolFunction(regExp, symbolPropertyId, scriptContext);
                if (!JavascriptOperators::IsUndefinedOrNull(symbolFn))
                {LOGMEIN("JavascriptString.cpp] 1733\n");
                    return CallRegExSymbolFunction<argCount>(symbolFn, regExp, args, varName, scriptContext);
                }
            }
        }

        JavascriptString * pThis = nullptr;
        GetThisStringArgument(args, scriptContext, varName, &pThis);
        return fallback(pThis);
    }

    Var JavascriptString::GetRegExSymbolFunction(Var regExp, PropertyId propertyId, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptString.cpp] 1745\n");
        return JavascriptOperators::GetProperty(
            RecyclableObject::FromVar(JavascriptOperators::ToObject(regExp, scriptContext)),
            propertyId,
            scriptContext);
    }

    template<int argCount>
    Var JavascriptString::CallRegExSymbolFunction(Var fn, Var regExp, Arguments& args, PCWSTR const varName, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptString.cpp] 1754\n");
        if (!JavascriptConversion::IsCallable(fn))
        {LOGMEIN("JavascriptString.cpp] 1756\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_Invalid, varName);
        }

        RecyclableObject* fnObj = RecyclableObject::FromVar(fn);
        return CallRegExFunction<argCount>(fnObj, regExp, args);
    }

    template<>
    Var JavascriptString::CallRegExFunction<1>(RecyclableObject* fnObj, Var regExp, Arguments& args)
    {LOGMEIN("JavascriptString.cpp] 1766\n");
        // args[0]: String
        return CALL_FUNCTION(fnObj, CallInfo(CallFlags_Value, 2), regExp, args[0]);
    }

    template<>
    Var JavascriptString::CallRegExFunction<2>(RecyclableObject* fnObj, Var regExp, Arguments& args)
    {LOGMEIN("JavascriptString.cpp] 1773\n");
        // args[0]: String
        // args[1]: RegExp (ignored since we need to create one when the argument is "undefined")
        // args[2]: Var

        if (args.Info.Count < 3)
        {LOGMEIN("JavascriptString.cpp] 1779\n");
            return CallRegExFunction<1>(fnObj, regExp, args);
        }

        return CALL_FUNCTION(fnObj, CallInfo(CallFlags_Value, 3), regExp, args[0], args[2]);
    }

    Var JavascriptString::EntrySlice(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        JavascriptString * pThis = nullptr;
        GetThisStringArgument(args, scriptContext, _u("String.prototype.slice"), &pThis);

        int len = pThis->GetLength();

        int idxStart = 0;
        int idxEnd = len;

        if (args.Info.Count > 1)
        {LOGMEIN("JavascriptString.cpp] 1804\n");
            idxStart = JavascriptOperators::IsUndefinedObject(args[1], scriptContext) ? 0 : ConvertToIndex(args[1], scriptContext);
            if (args.Info.Count > 2)
            {LOGMEIN("JavascriptString.cpp] 1807\n");
                idxEnd = JavascriptOperators::IsUndefinedObject(args[2], scriptContext) ? len : ConvertToIndex(args[2], scriptContext);
            }
        }

        if (idxStart < 0)
        {LOGMEIN("JavascriptString.cpp] 1813\n");
            idxStart = max(len + idxStart, 0);
        }
        else if (idxStart > len)
        {LOGMEIN("JavascriptString.cpp] 1817\n");
            idxStart = len;
        }

        if (idxEnd < 0)
        {LOGMEIN("JavascriptString.cpp] 1822\n");
            idxEnd = max(len + idxEnd, 0);
        }
        else if (idxEnd > len )
        {LOGMEIN("JavascriptString.cpp] 1826\n");
            idxEnd = len;
        }

        if (idxEnd < idxStart)
        {LOGMEIN("JavascriptString.cpp] 1831\n");
            idxEnd = idxStart;
        }
        return SubstringCore(pThis, idxStart, idxEnd - idxStart, scriptContext);
    }

    Var JavascriptString::EntrySplit(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        PCWSTR const varName = _u("String.prototype.split");

        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, varName);

        auto fallback = [&](JavascriptString* stringObj)
        {LOGMEIN("JavascriptString.cpp] 1851\n");
            return DoStringSplit(args, callInfo, stringObj, scriptContext);
        };
        return DelegateToRegExSymbolFunction<2>(args, PropertyIds::_symbolSplit, fallback, varName, scriptContext);
    }

    Var JavascriptString::DoStringSplit(Arguments& args, CallInfo& callInfo, JavascriptString* input, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptString.cpp] 1858\n");
        if (args.Info.Count == 1)
        {LOGMEIN("JavascriptString.cpp] 1860\n");
            JavascriptArray* ary = scriptContext->GetLibrary()->CreateArray(1);
            ary->DirectSetItemAt(0, input);
            return ary;
        }
        else
        {
            uint32 limit;
            if (args.Info.Count < 3 || JavascriptOperators::IsUndefinedObject(args[2], scriptContext))
            {LOGMEIN("JavascriptString.cpp] 1869\n");
                limit = UINT_MAX;
            }
            else
            {
                limit = JavascriptConversion::ToUInt32(args[2], scriptContext);
            }

            // When the config is enabled, the operation is handled by RegExp.prototype[@@split].
            if (!scriptContext->GetConfig()->IsES6RegExSymbolsEnabled()
                && JavascriptRegExp::Is(args[1]))
            {LOGMEIN("JavascriptString.cpp] 1880\n");
                return RegexHelper::RegexSplit(scriptContext, JavascriptRegExp::FromVar(args[1]), input, limit,
                    RegexHelper::IsResultNotUsed(callInfo.Flags));
            }
            else
            {
                JavascriptString* separator = JavascriptConversion::ToString(args[1], scriptContext);

                if (callInfo.Flags & CallFlags_NotUsed)
                {LOGMEIN("JavascriptString.cpp] 1889\n");
                    return scriptContext->GetLibrary()->GetNull();
                }

                if (!limit)
                {LOGMEIN("JavascriptString.cpp] 1894\n");
                    JavascriptArray* ary = scriptContext->GetLibrary()->CreateArray(0);
                    return ary;
                }

                if (JavascriptOperators::GetTypeId(args[1]) == TypeIds_Undefined)
                {LOGMEIN("JavascriptString.cpp] 1900\n");
                    JavascriptArray* ary = scriptContext->GetLibrary()->CreateArray(1);
                    ary->DirectSetItemAt(0, input);
                    return ary;
                }

                return RegexHelper::StringSplit(separator, input, limit);
            }
        }
    }

    Var JavascriptString::EntrySubstring(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        JavascriptString * pThis = nullptr;
        GetThisStringArgument(args, scriptContext, _u("String.prototype.substring"), &pThis);

        int len = pThis->GetLength();

        int idxStart = 0;
        int idxEnd = len;

        if (args.Info.Count > 1)
        {LOGMEIN("JavascriptString.cpp] 1929\n");
            idxStart = JavascriptOperators::IsUndefinedObject(args[1], scriptContext) ? 0 : ConvertToIndex(args[1], scriptContext);
            if (args.Info.Count > 2)
            {LOGMEIN("JavascriptString.cpp] 1932\n");
                idxEnd = JavascriptOperators::IsUndefinedObject(args[2], scriptContext) ? len : ConvertToIndex(args[2], scriptContext);
            }
        }

        idxStart = min(max(idxStart, 0), len);
        idxEnd = min(max(idxEnd, 0), len);
        if(idxEnd < idxStart)
        {LOGMEIN("JavascriptString.cpp] 1940\n");
            //swap
            idxStart ^= idxEnd;
            idxEnd ^= idxStart;
            idxStart ^= idxEnd;
        }

        if (idxStart == 0 && idxEnd == len)
        {LOGMEIN("JavascriptString.cpp] 1948\n");
            //return the string if we need to substring entire span
            return pThis;
        }

        return SubstringCore(pThis, idxStart, idxEnd - idxStart, scriptContext);
    }

    Var JavascriptString::EntrySubstr(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        JavascriptString * pThis = nullptr;
        GetThisStringArgument(args, scriptContext, _u("String.prototype.substr"), &pThis);

        int len = pThis->GetLength();

        int idxStart = 0;
        int idxEnd = len;

        if (args.Info.Count > 1)
        {LOGMEIN("JavascriptString.cpp] 1974\n");
            idxStart = JavascriptOperators::IsUndefinedObject(args[1], scriptContext) ? 0 : ConvertToIndex(args[1], scriptContext);
            if (args.Info.Count > 2)
            {LOGMEIN("JavascriptString.cpp] 1977\n");
                idxEnd = JavascriptOperators::IsUndefinedObject(args[2], scriptContext) ? len : ConvertToIndex(args[2], scriptContext);
            }
        }
        if (idxStart < 0)
        {LOGMEIN("JavascriptString.cpp] 1982\n");
            idxStart = max(len + idxStart, 0);
        }
        else if (idxStart > len)
        {LOGMEIN("JavascriptString.cpp] 1986\n");
            idxStart = len;
        }

        if (idxEnd < 0)
        {LOGMEIN("JavascriptString.cpp] 1991\n");
            idxEnd = idxStart;
        }
        else if (idxEnd > len - idxStart)
        {LOGMEIN("JavascriptString.cpp] 1995\n");
            idxEnd = len;
        }
        else
        {
            idxEnd += idxStart;
        }

        if (idxStart == 0 && idxEnd == len)
        {LOGMEIN("JavascriptString.cpp] 2004\n");
            //return the string if we need to substr entire span
            return pThis;
        }

        Assert(0 <= idxStart && idxStart <= idxEnd && idxEnd <= len);
        return SubstringCore(pThis, idxStart, idxEnd - idxStart, scriptContext);
    }

    Var JavascriptString::SubstringCore(JavascriptString* pThis, int idxStart, int span, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptString.cpp] 2014\n");
        return SubString::New(pThis, idxStart, span);
    }

    Var JavascriptString::EntryPadStart(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(String_Prototype_padStart);

        JavascriptString * pThis = nullptr;
        GetThisStringArgument(args, scriptContext, _u("String.prototype.padStart"), &pThis);

        return PadCore(args, pThis, true /*isPadStart*/, scriptContext);
    }

    Var JavascriptString::EntryPadEnd(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(String_Prototype_padEnd);

        JavascriptString * pThis = nullptr;
        GetThisStringArgument(args, scriptContext, _u("String.prototype.padEnd"), &pThis);

        return PadCore(args, pThis, false /*isPadStart*/, scriptContext);
    }

    JavascriptString* JavascriptString::PadCore(ArgumentReader& args, JavascriptString *mainString, bool isPadStart, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptString.cpp] 2051\n");
        Assert(mainString != nullptr);
        Assert(args.Info.Count > 0);

        if (args.Info.Count == 1)
        {LOGMEIN("JavascriptString.cpp] 2056\n");
            return mainString;
        }

        int64 maxLength = JavascriptConversion::ToLength(args[1], scriptContext);
        charcount_t currentLength = mainString->GetLength();
        if (maxLength <= currentLength)
        {LOGMEIN("JavascriptString.cpp] 2063\n");
            return mainString;
        }

        if (maxLength > JavascriptString::MaxCharLength)
        {LOGMEIN("JavascriptString.cpp] 2068\n");
            Throw::OutOfMemory();
        }

        JavascriptString * fillerString = nullptr;
        if (args.Info.Count > 2 && !JavascriptOperators::IsUndefinedObject(args[2], scriptContext))
        {LOGMEIN("JavascriptString.cpp] 2074\n");
            JavascriptString *argStr = JavascriptConversion::ToString(args[2], scriptContext);
            if (argStr->GetLength() > 0)
            {LOGMEIN("JavascriptString.cpp] 2077\n");
                fillerString = argStr;
            }
            else
            {
                return mainString;
            }
        }

        if (fillerString == nullptr)
        {LOGMEIN("JavascriptString.cpp] 2087\n");
            fillerString = NewWithBuffer(_u(" "), 1, scriptContext);
        }

        Assert(fillerString->GetLength() > 0);

        charcount_t fillLength = (charcount_t)(maxLength - currentLength);
        charcount_t count = fillLength / fillerString->GetLength();
        JavascriptString * finalPad = scriptContext->GetLibrary()->GetEmptyString();
        if (count > 0)
        {LOGMEIN("JavascriptString.cpp] 2097\n");
            finalPad = RepeatCore(fillerString, count, scriptContext);
            fillLength -= (count * fillerString->GetLength());
        }

        if (fillLength > 0)
        {LOGMEIN("JavascriptString.cpp] 2103\n");
            finalPad = Concat(finalPad, SubString::New(fillerString, 0, fillLength));
        }

        return isPadStart ? Concat(finalPad, mainString) : Concat(mainString, finalPad);
    }

    Var JavascriptString::EntryToLocaleLowerCase(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        return ToLocaleCaseHelper(args[0], false, scriptContext);
    }

    Var JavascriptString::EntryToLocaleUpperCase(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        return ToLocaleCaseHelper(args[0], true, scriptContext);
    }

    Var JavascriptString::EntryToLowerCase(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        JavascriptString * pThis = nullptr;
        GetThisStringArgument(args, scriptContext, _u("String.prototype.toLowerCase"), &pThis);

        // Fast path for one character strings
        if (pThis->GetLength() == 1)
        {LOGMEIN("JavascriptString.cpp] 2148\n");
            char16 inChar = pThis->GetString()[0];
            char16 outChar = inChar;
#if DBG
            DWORD converted =
#endif
                PlatformAgnostic::UnicodeText::ChangeStringCaseInPlace(
                    PlatformAgnostic::UnicodeText::CaseFlags::CaseFlagsLower, &outChar, 1);

            Assert(converted == 1);

            return (inChar == outChar) ? pThis : scriptContext->GetLibrary()->GetCharStringCache().GetStringForChar(outChar);
        }

        return ToCaseCore(pThis, ToLower);
    }

    Var JavascriptString::EntryToString(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if(args.Info.Count == 0)
        {LOGMEIN("JavascriptString.cpp] 2175\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedString, _u("String.prototype.toString"));
        }
        AssertMsg(args.Info.Count > 0, "Negative argument count");

        JavascriptString* str = nullptr;
        if (!GetThisValueVar(args[0], &str, scriptContext))
        {LOGMEIN("JavascriptString.cpp] 2182\n");
            if (JavascriptOperators::GetTypeId(args[0]) == TypeIds_HostDispatch)
            {LOGMEIN("JavascriptString.cpp] 2184\n");
                Var result;
                if (RecyclableObject::FromVar(args[0])->InvokeBuiltInOperationRemotely(EntryToString, args, &result))
                {LOGMEIN("JavascriptString.cpp] 2187\n");
                    return result;
                }
            }

            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedString, _u("String.prototype.toString"));
        }

        return str;
    }

    Var JavascriptString::EntryToUpperCase(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        JavascriptString* pThis = nullptr;
        GetThisStringArgument(args, scriptContext, _u("String.prototype.toUpperCase"), &pThis);

        // Fast path for one character strings
        if (pThis->GetLength() == 1)
        {LOGMEIN("JavascriptString.cpp] 2212\n");
            char16 inChar = pThis->GetString()[0];
            char16 outChar = inChar;
#if DBG
            DWORD converted =
#endif
                PlatformAgnostic::UnicodeText::ChangeStringCaseInPlace(
                    PlatformAgnostic::UnicodeText::CaseFlags::CaseFlagsUpper, &outChar, 1);

            Assert(converted == 1);

            return (inChar == outChar) ? pThis : scriptContext->GetLibrary()->GetCharStringCache().GetStringForChar(outChar);
        }

        return ToCaseCore(pThis, ToUpper);
    }

    Var JavascriptString::ToCaseCore(JavascriptString* pThis, ToCase toCase)
    {LOGMEIN("JavascriptString.cpp] 2230\n");
        charcount_t count = pThis->GetLength();

        const char16* inStr = pThis->GetString();
        const char16* inStrLim = inStr + count;
        const char16* i = inStr;

        // Try to find out the chars that do not need casing (in the ASCII range)
        if (toCase == ToUpper)
        {LOGMEIN("JavascriptString.cpp] 2239\n");
            while (i < inStrLim)
            {LOGMEIN("JavascriptString.cpp] 2241\n");
                // first range of ascii lower-case (97-122)
                // second range of ascii lower-case (223-255)
                // non-ascii chars (255+)
                if (*i >= 'a')
                {LOGMEIN("JavascriptString.cpp] 2246\n");
                    if (*i <= 'z') {LOGMEIN("JavascriptString.cpp] 2247\n"); break; }
                    if (*i >= 223) {LOGMEIN("JavascriptString.cpp] 2248\n"); break; }
                }
                i++;
            }
        }
        else
        {
            Assert(toCase == ToLower);
            while (i < inStrLim)
            {LOGMEIN("JavascriptString.cpp] 2257\n");
                // first range of ascii uppercase (65-90)
                // second range of ascii uppercase (192-222)
                // non-ascii chars (255+)
                if (*i >= 'A')
                {LOGMEIN("JavascriptString.cpp] 2262\n");
                    if (*i <= 'Z') {LOGMEIN("JavascriptString.cpp] 2263\n"); break; }
                    if (*i >= 192)
                    {LOGMEIN("JavascriptString.cpp] 2265\n");
                        if (*i < 223) {LOGMEIN("JavascriptString.cpp] 2266\n"); break; }
                        if (*i >= 255) {LOGMEIN("JavascriptString.cpp] 2267\n"); break; }
                    }
                }
                i++;
            }
        }

        // If no char needs casing, return immediately
        if (i == inStrLim) {LOGMEIN("JavascriptString.cpp] 2275\n"); return pThis; }

        // Otherwise, copy the string and start casing
        charcount_t countToCase = (charcount_t)(inStrLim - i);
        BufferStringBuilder builder(count, pThis->type->GetScriptContext());
        char16 *outStr = builder.DangerousGetWritableBuffer();

        char16* outStrLim = outStr + count;
        char16 *o = outStr;

        while (o < outStrLim)
        {LOGMEIN("JavascriptString.cpp] 2286\n");
            *o++ = *inStr++;
        }

        if(toCase == ToUpper)
        {LOGMEIN("JavascriptString.cpp] 2291\n");
#if DBG
            DWORD converted =
#endif
                PlatformAgnostic::UnicodeText::ChangeStringCaseInPlace(
                    PlatformAgnostic::UnicodeText::CaseFlags::CaseFlagsUpper, outStrLim - countToCase, countToCase);

            Assert(converted == countToCase);
        }
        else
        {
            Assert(toCase == ToLower);
#if DBG
            DWORD converted =
#endif
                PlatformAgnostic::UnicodeText::ChangeStringCaseInPlace(
                    PlatformAgnostic::UnicodeText::CaseFlags::CaseFlagsLower, outStrLim - countToCase, countToCase);

            Assert(converted == countToCase);
        }

        return builder.ToString();
    }

    Var JavascriptString::EntryTrim(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(String_Prototype_trim);

        Assert(!(callInfo.Flags & CallFlags_New));

        //15.5.4.20      The following steps are taken:
        //1.    Call CheckObjectCoercible passing the this value as its argument.
        //2.    Let S be the result of calling ToString, giving it the this value as its argument.
        //3.    Let T be a string value that is a copy of S with both leading and trailing white space removed. The definition of white space is the union of WhiteSpace and LineTerminator.
        //4.    Return T.

        JavascriptString* pThis = nullptr;
        GetThisStringArgument(args, scriptContext, _u("String.prototype.trim"), &pThis);
        return TrimLeftRightHelper<true /*trimLeft*/, true /*trimRight*/>(pThis, scriptContext);
    }

    Var JavascriptString::EntryTrimLeft(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        // 1.Let O be  CheckObjectCoercible(this value) .
        // 2.Let S be  ToString(O) .
        // 3.ReturnIfAbrupt(S).
        // 4.Let T be a String value that is a copy of S with leading white space removed. The definition of white space is the union of WhiteSpace and )LineTerminator.
        //   When determining whether a Unicode code point is in Unicode general category "Zs", code unit sequences are interpreted as UTF-16 encoded code point sequences as specified in 6.1.4.
        // 5.Return T.

        JavascriptString* pThis = nullptr;
        GetThisStringArgument(args, scriptContext, _u("String.prototype.trimLeft"), &pThis);
        return TrimLeftRightHelper< true /*trimLeft*/, false /*trimRight*/>(pThis, scriptContext);
    }


    Var JavascriptString::EntryTrimRight(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        // 1.Let O be  CheckObjectCoercible(this value) .
        // 2.Let S be  ToString(O) .
        // 3.ReturnIfAbrupt(S).
        // 4.Let T be a String value that is a copy of S with trailing white space removed.The definition of white space is the union of WhiteSpace and )LineTerminator.
        //   When determining whether a Unicode code point is in Unicode general category "Zs", code unit sequences are interpreted as UTF - 16 encoded code point sequences as specified in 6.1.4.
        // 5.Return T.

        JavascriptString* pThis = nullptr;
        GetThisStringArgument(args, scriptContext, _u("String.prototype.trimRight"), &pThis);
        return TrimLeftRightHelper<false /*trimLeft*/, true /*trimRight*/>(pThis, scriptContext);
    }

    template <bool trimLeft, bool trimRight>
    Var JavascriptString::TrimLeftRightHelper(JavascriptString* arg, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptString.cpp] 2381\n");
        static_assert(trimLeft || trimRight, "bad template instance of TrimLeftRightHelper()");

        int len = arg->GetLength();
        const char16 *string = arg->GetString();

        int idxStart = 0;
        if (trimLeft)
        {LOGMEIN("JavascriptString.cpp] 2389\n");
            for (; idxStart < len; idxStart++)
            {LOGMEIN("JavascriptString.cpp] 2391\n");
                char16 ch = string[idxStart];
                if (IsWhiteSpaceCharacter(ch))
                {LOGMEIN("JavascriptString.cpp] 2394\n");
                    continue;
                }
                break;
            }

            if (len == idxStart)
            {LOGMEIN("JavascriptString.cpp] 2401\n");
                return (scriptContext->GetLibrary()->GetEmptyString());
            }
        }

        int idxEnd = len - 1;
        if (trimRight)
        {LOGMEIN("JavascriptString.cpp] 2408\n");
            for (; idxEnd >= 0; idxEnd--)
            {LOGMEIN("JavascriptString.cpp] 2410\n");
                char16 ch = string[idxEnd];
                if (IsWhiteSpaceCharacter(ch))
                {LOGMEIN("JavascriptString.cpp] 2413\n");
                    continue;
                }
                break;
            }

            if (!trimLeft)
            {LOGMEIN("JavascriptString.cpp] 2420\n");
                if (idxEnd < 0)
                {LOGMEIN("JavascriptString.cpp] 2422\n");
                    Assert(idxEnd == -1);
                    return (scriptContext->GetLibrary()->GetEmptyString());
                }
            }
            else
            {
                Assert(idxEnd >= 0);
            }
        }
        return SubstringCore(arg, idxStart, idxEnd - idxStart + 1, scriptContext);
    }

    ///----------------------------------------------------------------------------
    /// Repeat() returns a new string equal to the toString(this) repeated n times,
    /// as described in (ES6.0: S21.1.3.13).
    ///----------------------------------------------------------------------------
    Var JavascriptString::EntryRepeat(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(String_Prototype_repeat);

        JavascriptString* pThis = nullptr;
        GetThisStringArgument(args, scriptContext, _u("String.prototype.repeat"), &pThis);

        charcount_t count = 0;

        if (args.Info.Count > 1)
        {LOGMEIN("JavascriptString.cpp] 2456\n");
            if (!JavascriptOperators::IsUndefinedObject(args[1], scriptContext))
            {LOGMEIN("JavascriptString.cpp] 2458\n");
                double countDbl = JavascriptConversion::ToInteger(args[1], scriptContext);
                if (JavascriptNumber::IsPosInf(countDbl) || countDbl < 0.0)
                {LOGMEIN("JavascriptString.cpp] 2461\n");
                    JavascriptError::ThrowRangeError(scriptContext, JSERR_ArgumentOutOfRange, _u("String.prototype.repeat"));
                }

                count = NumberUtilities::LuFromDblNearest(countDbl);
            }
        }

        if (count == 0 || pThis->GetLength() == 0)
        {LOGMEIN("JavascriptString.cpp] 2470\n");
            return scriptContext->GetLibrary()->GetEmptyString();
        }
        else if (count == 1)
        {LOGMEIN("JavascriptString.cpp] 2474\n");
            return pThis;
        }

        return RepeatCore(pThis, count, scriptContext);
    }

    JavascriptString* JavascriptString::RepeatCore(JavascriptString* currentString, charcount_t count, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptString.cpp] 2482\n");
        Assert(currentString != nullptr);
        Assert(currentString->GetLength() > 0);
        Assert(count > 0);

        const char16* currentRawString = currentString->GetString();
        charcount_t currentLength = currentString->GetLength();

        charcount_t finalBufferCount = UInt32Math::Add(UInt32Math::Mul(count, currentLength), 1);
        char16* buffer = RecyclerNewArrayLeaf(scriptContext->GetRecycler(), char16, finalBufferCount);

        if (currentLength == 1)
        {
            wmemset(buffer, currentRawString[0], finalBufferCount - 1);
            buffer[finalBufferCount - 1] = '\0';
        }
        else
        {
            char16* bufferDst = buffer;
            size_t bufferDstSize = finalBufferCount;
            AnalysisAssert(bufferDstSize > currentLength);

            for (charcount_t i = 0; i < count; i += 1)
            {
                js_wmemcpy_s(bufferDst, bufferDstSize, currentRawString, currentLength);
                bufferDst += currentLength;
                bufferDstSize -= currentLength;
            }
            Assert(bufferDstSize == 1);
            *bufferDst = '\0';
        }

        return JavascriptString::NewWithBuffer(buffer, finalBufferCount - 1, scriptContext);
    }

    ///----------------------------------------------------------------------------
    /// StartsWith() returns true if the given string matches the beginning of the
    /// substring starting at the given position in toString(this), as described
    /// in (ES6.0: S21.1.3.18).
    ///----------------------------------------------------------------------------
    Var JavascriptString::EntryStartsWith(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(String_Prototype_startsWith);

        JavascriptString * pThis;
        JavascriptString * pSearch;

        GetThisAndSearchStringArguments(args, scriptContext, _u("String.prototype.startsWith"), &pThis, &pSearch, false);

        const char16* thisStr = pThis->GetString();
        int thisStrLen = pThis->GetLength();

        const char16* searchStr = pSearch->GetString();
        int searchStrLen = pSearch->GetLength();

        int startPosition = 0;

        if (args.Info.Count > 2)
        {LOGMEIN("JavascriptString.cpp] 2547\n");
            if (!JavascriptOperators::IsUndefinedObject(args[2], scriptContext))
            {LOGMEIN("JavascriptString.cpp] 2549\n");
                startPosition = ConvertToIndex(args[2], scriptContext); // this is to adjust corner cases like MAX_VALUE
                startPosition = min(max(startPosition, 0), thisStrLen);
            }
        }

        // Avoid signed 32-bit int overflow if startPosition is large by subtracting searchStrLen from thisStrLen instead of
        // adding searchStrLen and startPosition.  The subtraction cannot underflow because maximum string length is
        // MaxCharCount == INT_MAX-1.  I.e. the RHS can be == 0 - (INT_MAX-1) == 1 - INT_MAX which would not underflow.
        if (startPosition <= thisStrLen - searchStrLen)
        {LOGMEIN("JavascriptString.cpp] 2559\n");
            Assert(searchStrLen <= thisStrLen - startPosition);
            if (wmemcmp(thisStr + startPosition, searchStr, searchStrLen) == 0)
            {LOGMEIN("JavascriptString.cpp] 2562\n");
                return scriptContext->GetLibrary()->GetTrue();
            }
        }

        return scriptContext->GetLibrary()->GetFalse();
    }

    ///----------------------------------------------------------------------------
    /// EndsWith() returns true if the given string matches the end of the
    /// substring ending at the given position in toString(this), as described
    /// in (ES6.0: S21.1.3.7).
    ///----------------------------------------------------------------------------
    Var JavascriptString::EntryEndsWith(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(String_Prototype_endsWith);

        JavascriptString * pThis;
        JavascriptString * pSearch;

        GetThisAndSearchStringArguments(args, scriptContext, _u("String.prototype.endsWith"), &pThis, &pSearch, false);

        const char16* thisStr = pThis->GetString();
        int thisStrLen = pThis->GetLength();

        const char16* searchStr = pSearch->GetString();
        int searchStrLen = pSearch->GetLength();

        int endPosition = thisStrLen;

        if (args.Info.Count > 2)
        {LOGMEIN("JavascriptString.cpp] 2600\n");
            if (!JavascriptOperators::IsUndefinedObject(args[2], scriptContext))
            {LOGMEIN("JavascriptString.cpp] 2602\n");
                endPosition = ConvertToIndex(args[2], scriptContext); // this is to adjust corner cases like MAX_VALUE
                endPosition = min(max(endPosition, 0), thisStrLen);
            }
        }

        int startPosition = endPosition - searchStrLen;

        if (startPosition >= 0)
        {LOGMEIN("JavascriptString.cpp] 2611\n");
            Assert(startPosition <= thisStrLen);
            Assert(searchStrLen <= thisStrLen - startPosition);
            if (wmemcmp(thisStr + startPosition, searchStr, searchStrLen) == 0)
            {LOGMEIN("JavascriptString.cpp] 2615\n");
                return scriptContext->GetLibrary()->GetTrue();
            }
        }

        return scriptContext->GetLibrary()->GetFalse();
    }

    ///----------------------------------------------------------------------------
    /// Includes() returns true if the given string matches any substring (of the
    /// same length) of the substring starting at the given position in
    /// toString(this), as described in (ES6.0 (draft 33): S21.1.3.7).
    ///----------------------------------------------------------------------------
    Var JavascriptString::EntryIncludes(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(String_Prototype_contains);

        return JavascriptBoolean::ToVar(IndexOf(args, scriptContext, _u("String.prototype.includes"), false) != -1, scriptContext);
    }

    Var JavascriptString::EntryValueOf(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if(args.Info.Count == 0)
        {LOGMEIN("JavascriptString.cpp] 2652\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedString, _u("String.prototype.valueOf"));
        }
        AssertMsg(args.Info.Count > 0, "Negative argument count");

        JavascriptString* str = nullptr;
        if (!GetThisValueVar(args[0], &str, scriptContext))
        {LOGMEIN("JavascriptString.cpp] 2659\n");
            if (JavascriptOperators::GetTypeId(args[0]) == TypeIds_HostDispatch)
            {LOGMEIN("JavascriptString.cpp] 2661\n");
                Var result;
                if (RecyclableObject::FromVar(args[0])->InvokeBuiltInOperationRemotely(EntryValueOf, args, &result))
                {LOGMEIN("JavascriptString.cpp] 2664\n");
                    return result;
                }
            }

            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedString, _u("String.prototype.valueOf"));
        }

        return str;
    }

    Var JavascriptString::EntrySymbolIterator(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0)
        {LOGMEIN("JavascriptString.cpp] 2685\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedString, _u("String.prototype[Symbol.iterator]"));
        }
        AssertMsg(args.Info.Count > 0, "Negative argument count");

        if (!JavascriptConversion::CheckObjectCoercible(args[0], scriptContext))
        {LOGMEIN("JavascriptString.cpp] 2691\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("String.prototype[Symbol.iterator]"));
        }

        JavascriptString* str = JavascriptConversion::ToString(args[0], scriptContext);

        return scriptContext->GetLibrary()->CreateStringIterator(str);
    }

    const char16 * JavascriptString::GetSz()
    {LOGMEIN("JavascriptString.cpp] 2701\n");
        Assert(m_pszValue[m_charLength] == _u('\0'));
        return m_pszValue;
    }

    const char16 * JavascriptString::GetString()
    {LOGMEIN("JavascriptString.cpp] 2707\n");
        if (!this->IsFinalized())
        {LOGMEIN("JavascriptString.cpp] 2709\n");
            this->GetSz();
            Assert(m_pszValue);
        }
        return m_pszValue;
    }

    void const * JavascriptString::GetOriginalStringReference()
    {LOGMEIN("JavascriptString.cpp] 2717\n");
        // Just return the string buffer
        return GetString();
    }

    size_t JavascriptString::GetAllocatedByteCount() const
    {LOGMEIN("JavascriptString.cpp] 2723\n");
        if (!this->IsFinalized())
        {LOGMEIN("JavascriptString.cpp] 2725\n");
            return 0;
        }
        return this->m_charLength * sizeof(WCHAR);
    }

    bool JavascriptString::IsSubstring() const
    {LOGMEIN("JavascriptString.cpp] 2732\n");
        return false;
    }

    bool JavascriptString::IsNegZero(JavascriptString *string)
    {LOGMEIN("JavascriptString.cpp] 2737\n");
        return string->GetLength() == 2 && wmemcmp(string->GetString(), _u("-0"), 2) == 0;
    }

    void JavascriptString::FinishCopy(__inout_xcount(m_charLength) char16 *const buffer, StringCopyInfoStack &nestedStringTreeCopyInfos)
    {LOGMEIN("JavascriptString.cpp] 2742\n");
        while (!nestedStringTreeCopyInfos.IsEmpty())
        {LOGMEIN("JavascriptString.cpp] 2744\n");
            const StringCopyInfo copyInfo(nestedStringTreeCopyInfos.Pop());
            Assert(copyInfo.SourceString()->GetLength() <= GetLength());
            Assert(copyInfo.DestinationBuffer() >= buffer);
            Assert(copyInfo.DestinationBuffer() <= buffer + (GetLength() - copyInfo.SourceString()->GetLength()));
            copyInfo.SourceString()->Copy(copyInfo.DestinationBuffer(), nestedStringTreeCopyInfos, 0);
        }
    }

    void JavascriptString::CopyVirtual(
        _Out_writes_(m_charLength) char16 *const buffer,
        StringCopyInfoStack &nestedStringTreeCopyInfos,
        const byte recursionDepth)
    {LOGMEIN("JavascriptString.cpp] 2757\n");
        Assert(buffer);
        Assert(!this->IsFinalized());   // CopyVirtual should only be called for unfinalized buffers
        CopyHelper(buffer, GetString(), GetLength());
    }

    char16* JavascriptString::GetSzCopy()
    {LOGMEIN("JavascriptString.cpp] 2764\n");
        return AllocateLeafAndCopySz(this->GetScriptContext()->GetRecycler(), GetString(), GetLength());
    }

    LPCWSTR JavascriptString::GetSzCopy(ArenaAllocator* alloc)
    {LOGMEIN("JavascriptString.cpp] 2769\n");
        return AllocateAndCopySz(alloc, GetString(), GetLength());
    }

    /*
    Table generated using the following:

    var invalidValue = 37;

    function toStringTable()
    {
        var stringTable = new Array(128);
        for(var i = 0; i < 128; i++)
        {
            var ch = i;
            if ('0'.charCodeAt(0) <= ch && '9'.charCodeAt(0) >= ch)
                ch -= '0'.charCodeAt(0);
            else if ('A'.charCodeAt(0)  <= ch && 'Z'.charCodeAt(0)  >= ch)
                ch -= 'A'.charCodeAt(0) - 10;
            else if ('a'.charCodeAt(0) <= ch && 'z'.charCodeAt(0) >= ch)
                ch -= 'a'.charCodeAt(0) - 10;
            else
                ch = 37;
            stringTable[i] = ch;
        }
        WScript.Echo("{" + stringTable + "}");
    }
    toStringTable();*/
    const char JavascriptString::stringToIntegerMap[] = {
        37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37
        ,37,37,37,37,37,37,37,37,0,1,2,3,4,5,6,7,8,9,37,37,37,37,37,37,37,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,
        28,29,30,31,32,33,34,35,37,37,37,37,37,37,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,
        37,37,37,37,37};

    /*
    Table generated using the following:
    function logMaxUintTable()
    {
        var MAX_UINT = 4294967295;
        var logTable = new Array(37);
        logTable[0] = 0;
        logTable[1] = 0;
        for(var i = 2; i < logTable.length; i++)
        {
            logTable[i] = Math.floor(Math.log(MAX_UINT + 1) / Math.log(i));
        }
        WScript.Echo("{" + logTable + "}");
    }
    logMaxUintTable();
    */
    const uint8 JavascriptString::maxUintStringLengthTable[] =
        { 0,0,32,20,16,13,12,11,10,10,9,9,8,8,8,8,8,7,7,7,7,7,7,7,6,6,6,6,6,6,6,6,6,6,6,6,6 };

    // NumberUtil::FIntRadStrToDbl and parts of GlobalObject::EntryParseInt were refactored into ToInteger
    Var JavascriptString::ToInteger(int radix)
    {LOGMEIN("JavascriptString.cpp] 2824\n");
        AssertMsg(radix == 0 || radix >= 2 && radix <= 36, "'radix' is invalid");
        const char16* pchStart = GetString();
        const char16* pchEnd =  pchStart + m_charLength;
        const char16 *pch = this->GetScriptContext()->GetCharClassifier()->SkipWhiteSpace(pchStart, pchEnd);
        bool isNegative = false;
        switch (*pch)
        {LOGMEIN("JavascriptString.cpp] 2831\n");
        case '-':
            isNegative = true;
            // Fall through.
        case '+':
            if(pch < pchEnd)
            {LOGMEIN("JavascriptString.cpp] 2837\n");
                pch++;
            }
            break;
        }

        if (0 == radix)
        {LOGMEIN("JavascriptString.cpp] 2844\n");
            if (pch < pchEnd && '0' != pch[0])
            {LOGMEIN("JavascriptString.cpp] 2846\n");
                radix = 10;
            }
            else if (('x' == pch[1] || 'X' == pch[1]) && pchEnd - pch >= 2)
            {LOGMEIN("JavascriptString.cpp] 2850\n");
                radix = 16;
                pch += 2;
            }
            else
            {
                 // ES5's 'parseInt' does not allow treating a string beginning with a '0' as an octal value. ES3 does not specify a
                 // behavior
                 radix = 10;
            }
        }
        else if (16 == radix)
        {LOGMEIN("JavascriptString.cpp] 2862\n");
            if('0' == pch[0] && ('x' == pch[1] || 'X' == pch[1]) && pchEnd - pch >= 2)
            {LOGMEIN("JavascriptString.cpp] 2864\n");
                pch += 2;
            }
        }

        Assert(radix <= _countof(maxUintStringLengthTable));
        Assert(pchEnd >= pch);
        size_t length = pchEnd - pch;
        const char16 *const pchMin = pch;
        __analysis_assume(radix < _countof(maxUintStringLengthTable));
        if(length <= maxUintStringLengthTable[radix])
        {LOGMEIN("JavascriptString.cpp] 2875\n");
            // Use uint32 as integer being parsed - much faster than BigInt
            uint32 value = 0;
            for ( ; pch < pchEnd ; pch++)
            {LOGMEIN("JavascriptString.cpp] 2879\n");
                char16 ch = *pch;

                if(ch >= _countof(stringToIntegerMap) || (ch = stringToIntegerMap[ch]) >= radix)
                {LOGMEIN("JavascriptString.cpp] 2883\n");
                    break;
                }
                uint32 beforeValue = value;
                value = value * radix + ch;
                AssertMsg(value >= beforeValue, "uint overflow");
            }

            if(pchMin == pch)
            {LOGMEIN("JavascriptString.cpp] 2892\n");
                return GetScriptContext()->GetLibrary()->GetNaN();
            }

            if(isNegative)
            {LOGMEIN("JavascriptString.cpp] 2897\n");
                // negative zero can only be represented by doubles
                if(value <= INT_MAX && value != 0)
                {LOGMEIN("JavascriptString.cpp] 2900\n");
                    int32 result = -((int32)value);
                    return JavascriptNumber::ToVar(result, this->GetScriptContext());
                }
                double result = -((double)(value));
                return JavascriptNumber::New(result, this->GetScriptContext());
            }
            return JavascriptNumber::ToVar(value, this->GetScriptContext());
        }

        BigInt bi;
        for ( ; pch < pchEnd ; pch++)
        {LOGMEIN("JavascriptString.cpp] 2912\n");
            char16 ch = *pch;

            if(ch >= _countof(stringToIntegerMap) || (ch = stringToIntegerMap[ch]) >= radix)
            {LOGMEIN("JavascriptString.cpp] 2916\n");
                break;
            }
            if (!bi.FMulAdd(radix, ch))
            {LOGMEIN("JavascriptString.cpp] 2920\n");
                //Mimic IE8 which threw an OutOfMemory exception in this case.
                JavascriptError::ThrowOutOfMemoryError(GetScriptContext());
            }
            // If we ever have more than 32 ulongs, the result must be infinite.
            if (bi.Clu() > 32)
            {LOGMEIN("JavascriptString.cpp] 2926\n");
                Var result = isNegative ?
                    GetScriptContext()->GetLibrary()->GetNegativeInfinite() :
                    GetScriptContext()->GetLibrary()->GetPositiveInfinite();
                return result;
            }
        }

        if (pchMin == pch)
        {LOGMEIN("JavascriptString.cpp] 2935\n");
            return GetScriptContext()->GetLibrary()->GetNaN();
        }

        // Convert to a double.
        double result = bi.GetDbl();
        if(isNegative)
        {LOGMEIN("JavascriptString.cpp] 2942\n");
            result = -result;
        }

        return Js::JavascriptNumber::ToVarIntCheck(result, GetScriptContext());
    }

    bool JavascriptString::ToDouble(double * result)
    {LOGMEIN("JavascriptString.cpp] 2950\n");
        const char16* pch;
        int32 len = this->m_charLength;
        if (0 == len)
        {LOGMEIN("JavascriptString.cpp] 2954\n");
            *result = 0;
            return true;
        }

        if (1 == len && NumberUtilities::IsDigit(this->GetString()[0]))
        {LOGMEIN("JavascriptString.cpp] 2960\n");
            *result = (double)(this->GetString()[0] - '0');
            return true;
        }

        // TODO: Use GetString here instead of GetSz (need to modify DblFromHex and StrToDbl to take a length)
        for (pch = this->GetSz(); IsWhiteSpaceCharacter(*pch); pch++)
            ;
        if (0 == *pch)
        {LOGMEIN("JavascriptString.cpp] 2969\n");
            *result = 0;
            return true;
        }

        bool isNumericLiteral = false;
        if (*pch == '0')
        {LOGMEIN("JavascriptString.cpp] 2976\n");
            const char16 *pchT = pch + 2;
            switch (pch[1])
            {LOGMEIN("JavascriptString.cpp] 2979\n");
            case 'x':
            case 'X':

                *result = NumberUtilities::DblFromHex(pchT, &pch);
                isNumericLiteral = true;
                break;
            case 'o':
            case 'O':
                *result = NumberUtilities::DblFromOctal(pchT, &pch);
                isNumericLiteral = true;
                break;
            case 'b':
            case 'B':
                *result = NumberUtilities::DblFromBinary(pchT, &pch);
                isNumericLiteral = true;
                break;
            }
            if (pchT == pch && isNumericLiteral)
            {LOGMEIN("JavascriptString.cpp] 2998\n");
                *result = JavascriptNumber::NaN;
                return false;
            }
        }
        if (!isNumericLiteral)
        {LOGMEIN("JavascriptString.cpp] 3004\n");
            *result = NumberUtilities::StrToDbl(pch, &pch, GetScriptContext());
        }

        while (IsWhiteSpaceCharacter(*pch))
            pch++;
        if (pch != this->m_pszValue + len)
        {LOGMEIN("JavascriptString.cpp] 3011\n");
            *result = JavascriptNumber::NaN;
            return false;
        }
        return true;
    }

    double JavascriptString::ToDouble()
    {LOGMEIN("JavascriptString.cpp] 3019\n");
        double result;
        this->ToDouble(&result);
        return result;
    }

    bool JavascriptString::Equals(Var aLeft, Var aRight)
    {LOGMEIN("JavascriptString.cpp] 3026\n");
        return JavascriptStringHelpers<JavascriptString>::Equals(aLeft, aRight);
    }

    //
    // LessThan implements algorithm of ES5 11.8.5 step 4
    // returns false for same string pattern
    //
    bool JavascriptString::LessThan(Var aLeft, Var aRight)
    {LOGMEIN("JavascriptString.cpp] 3035\n");
        AssertMsg(JavascriptString::Is(aLeft) && JavascriptString::Is(aRight), "string LessThan");

        JavascriptString *leftString  = JavascriptString::FromVar(aLeft);
        JavascriptString *rightString = JavascriptString::FromVar(aRight);

        if (JavascriptString::strcmp(leftString, rightString) < 0)
        {LOGMEIN("JavascriptString.cpp] 3042\n");
            return true;
        }
        return false;
    }

    // thisStringValue(value) abstract operation as defined in ES6.0 (Draft 25) Section 21.1.3
    BOOL JavascriptString::GetThisValueVar(Var aValue, JavascriptString** pString, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptString.cpp] 3050\n");
        Assert(pString);

        // 1. If Type(value) is String, return value.
        if (JavascriptString::Is(aValue))
        {LOGMEIN("JavascriptString.cpp] 3055\n");
            *pString = JavascriptString::FromVar(aValue);
            return TRUE;
        }
        // 2. If Type(value) is Object and value has a [[StringData]] internal slot
        else if ( JavascriptStringObject::Is(aValue))
        {LOGMEIN("JavascriptString.cpp] 3061\n");
            JavascriptStringObject* pStringObj = JavascriptStringObject::FromVar(aValue);

            // a. Let s be the value of value's [[StringData]] internal slot.
            // b. If s is not undefined, then return s.
            *pString = JavascriptString::FromVar(CrossSite::MarshalVar(scriptContext, pStringObj->Unwrap()));
            return TRUE;
        }

        // 3. Throw a TypeError exception.
        // Note: We don't throw a TypeError here, choosing to return FALSE and let the caller throw the error
        return FALSE;
    }

#ifdef TAGENTRY
#undef TAGENTRY
#endif
#define TAGENTRY(name, ...) \
        Var JavascriptString::Entry##name(RecyclableObject* function, CallInfo callInfo, ...)   \
        {                                                                                       \
            PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);          \
                                                                                                \
            ARGUMENTS(args, callInfo);                                                          \
            ScriptContext* scriptContext = function->GetScriptContext();                        \
                                                                                                \
            Assert(!(callInfo.Flags & CallFlags_New));                                          \
                                                                                                \
            return StringBracketHelper(args, scriptContext, __VA_ARGS__);                       \
        }
#include "JavascriptStringTagEntries.h"
#undef TAGENTRY

    Var JavascriptString::StringBracketHelper(Arguments args, ScriptContext *scriptContext, __in_ecount(cchTag) char16 const *pszTag,
                                                charcount_t cchTag, __in_ecount_opt(cchProp) char16 const *pszProp, charcount_t cchProp)
    {LOGMEIN("JavascriptString.cpp] 3095\n");
        charcount_t cchThis;
        charcount_t cchPropertyValue;
        charcount_t cchTotalChars;
        charcount_t ich;
        JavascriptString * pThis;
        JavascriptString * pPropertyValue = nullptr;
        const char16 * propertyValueStr = nullptr;
        uint quotesCount = 0;
        const char16 quotStr[] = _u("&quot;");
        const charcount_t quotStrLen = _countof(quotStr) - 1;
        bool ES6FixesEnabled = scriptContext->GetConfig()->IsES6StringPrototypeFixEnabled();

        // Assemble the component pieces of a string tag function (ex: String.prototype.link).
        // In the general case, result is as below:
        //
        // pszProp = _u("href");
        // pszTag = _u("a");
        // pThis = JavascriptString::FromVar(args[0]);
        // pPropertyValue = JavascriptString::FromVar(args[1]);
        //
        // pResult = _u("<a href=\"[[pPropertyValue]]\">[[pThis]]</a>");
        //
        // cchTotalChars = 5                    // <></>
        //                 + cchTag * 2         // a
        //                 + cchProp            // href
        //                 + 4                  // _=""
        //                 + cchPropertyValue
        //                 + cchThis;
        //
        // Note: With ES6FixesEnabled, we need to escape quote characters (_u('"')) in pPropertyValue.
        // Note: Without ES6FixesEnabled, the tag and prop strings should be capitalized.

        if(args.Info.Count == 0)
        {LOGMEIN("JavascriptString.cpp] 3129\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedString);
        }

        if (ES6FixesEnabled)
        {LOGMEIN("JavascriptString.cpp] 3134\n");
            if (!JavascriptConversion::CheckObjectCoercible(args[0], scriptContext))
            {LOGMEIN("JavascriptString.cpp] 3136\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, pszTag);
            }
        }

        if (JavascriptString::Is(args[0]))
        {LOGMEIN("JavascriptString.cpp] 3142\n");
            pThis = JavascriptString::FromVar(args[0]);
        }
        else
        {
            pThis = JavascriptConversion::ToString(args[0], scriptContext);
        }

        cchThis = pThis->GetLength();
        cchTotalChars = UInt32Math::Add(cchTag, cchTag);

        // 5 is for the <></> characters
        cchTotalChars = UInt32Math::Add(cchTotalChars, 5);

        if (nullptr != pszProp)
        {LOGMEIN("JavascriptString.cpp] 3157\n");
            // Need one string argument.
            if (args.Info.Count >= 2)
            {LOGMEIN("JavascriptString.cpp] 3160\n");
                if (JavascriptString::Is(args[1]))
                {LOGMEIN("JavascriptString.cpp] 3162\n");
                    pPropertyValue = JavascriptString::FromVar(args[1]);
                }
                else
                {
                    pPropertyValue = JavascriptConversion::ToString(args[1], scriptContext);
                }
            }
            else
            {
                pPropertyValue = scriptContext->GetLibrary()->GetUndefinedDisplayString();
            }

            cchPropertyValue = pPropertyValue->GetLength();
            propertyValueStr = pPropertyValue->GetString();

            if (ES6FixesEnabled)
            {LOGMEIN("JavascriptString.cpp] 3179\n");
                // Count the number of " characters we need to escape.
                for (ich = 0; ich < cchPropertyValue; ich++)
                {LOGMEIN("JavascriptString.cpp] 3182\n");
                    if (propertyValueStr[ich] == _u('"'))
                    {LOGMEIN("JavascriptString.cpp] 3184\n");
                        ++quotesCount;
                    }
                }
            }

            cchTotalChars = UInt32Math::Add(cchTotalChars, cchProp);

            // 4 is for the _="" characters
            cchTotalChars = UInt32Math::Add(cchTotalChars, 4);

            if (ES6FixesEnabled)
            {LOGMEIN("JavascriptString.cpp] 3196\n");
                // Account for the " escaping (&quot;)
                cchTotalChars = UInt32Math::Add(cchTotalChars, UInt32Math::Mul(quotesCount, quotStrLen)) - quotesCount;
            }
        }
        else
        {
            cchPropertyValue = 0;
            cchProp = 0;
        }
        cchTotalChars = UInt32Math::Add(cchTotalChars, cchThis);
        cchTotalChars = UInt32Math::Add(cchTotalChars, cchPropertyValue);
        if (!IsValidCharCount(cchTotalChars) || cchTotalChars < cchThis || cchTotalChars < cchPropertyValue)
        {LOGMEIN("JavascriptString.cpp] 3209\n");
            Js::JavascriptError::ThrowOutOfMemoryError(scriptContext);
        }

        BufferStringBuilder builder(cchTotalChars, scriptContext);
        char16 *pResult = builder.DangerousGetWritableBuffer();

        *pResult++ = _u('<');
        for (ich = 0; ich < cchTag; ich++)
        {LOGMEIN("JavascriptString.cpp] 3218\n");
            *pResult++ = ES6FixesEnabled ? pszTag[ich] : towupper(pszTag[ich]);
        }
        if (nullptr != pszProp)
        {LOGMEIN("JavascriptString.cpp] 3222\n");
            *pResult++ = _u(' ');
            for (ich = 0; ich < cchProp; ich++)
            {LOGMEIN("JavascriptString.cpp] 3225\n");
                *pResult++ = ES6FixesEnabled ? pszProp[ich] : towupper(pszProp[ich]);
            }
            *pResult++ = _u('=');
            *pResult++ = _u('"');

            Assert(propertyValueStr != nullptr);

            if (!ES6FixesEnabled || quotesCount == 0)
            {
                js_wmemcpy_s(pResult,
                    cchTotalChars - (pResult - builder.DangerousGetWritableBuffer() + 1),
                    propertyValueStr,
                    cchPropertyValue);

                pResult += cchPropertyValue;
            }
            else {
                for (ich = 0; ich < cchPropertyValue; ich++)
                {LOGMEIN("JavascriptString.cpp] 3244\n");
                    if (propertyValueStr[ich] == _u('"'))
                    {LOGMEIN("JavascriptString.cpp] 3246\n");
                        charcount_t destLengthLeft = (cchTotalChars - (charcount_t)(pResult - builder.DangerousGetWritableBuffer() + 1));

                        // Copy the quote string into result beginning at the index where the quote would appear
                        js_wmemcpy_s(pResult,
                            destLengthLeft,
                            quotStr,
                            quotStrLen);

                        // Move result ahead by the length of the quote string
                        pResult += quotStrLen;
                        // We ate one of the quotes
                        quotesCount--;

                        // We only need to check to see if we have no more quotes after eating a quote
                        if (quotesCount == 0)
                        {LOGMEIN("JavascriptString.cpp] 3262\n");
                            // Skip the quote character.
                            // Note: If ich is currently the last character (cchPropertyValue-1), it becomes cchPropertyValue after incrementing.
                            // At that point, cchPropertyValue - ich == 0 so we will not increment pResult and will call memcpy for zero bytes.
                            ich++;

                            // Copy the rest from the property value string starting at the index after the last quote
                            js_wmemcpy_s(pResult,
                                destLengthLeft - quotStrLen,
                                propertyValueStr + ich,
                                cchPropertyValue - ich);

                            // Move result ahead by the length of the rest of the property string
                            pResult += (cchPropertyValue - ich);
                            break;
                        }
                    }
                    else
                    {
                        // Each non-quote character just gets copied into result string
                        *pResult++ = propertyValueStr[ich];
                    }
                }
            }

            *pResult++ = _u('"');
        }
        *pResult++ = _u('>');

        const char16 *pThisString = pThis->GetString();
        js_wmemcpy_s(pResult, cchTotalChars - (pResult - builder.DangerousGetWritableBuffer() + 1), pThisString, cchThis);
        pResult += cchThis;

        *pResult++ = _u('<');
        *pResult++ = _u('/');
        for (ich = 0; ich < cchTag; ich++)
        {LOGMEIN("JavascriptString.cpp] 3298\n");
            *pResult++ = ES6FixesEnabled ? pszTag[ich] : towupper(pszTag[ich]);
        }
        *pResult++ = _u('>');

        // Assert we ended at the right place.
        AssertMsg((charcount_t)(pResult - builder.DangerousGetWritableBuffer()) == cchTotalChars, "Exceeded allocated string limit");

        return builder.ToString();
    }
    Var JavascriptString::ToLocaleCaseHelper(Var thisObj, bool toUpper, ScriptContext *scriptContext)
    {LOGMEIN("JavascriptString.cpp] 3309\n");
        using namespace PlatformAgnostic::UnicodeText;

        JavascriptString * pThis;

        if (JavascriptString::Is(thisObj))
        {LOGMEIN("JavascriptString.cpp] 3315\n");
            pThis = JavascriptString::FromVar(thisObj);
        }
        else
        {
            pThis = JavascriptConversion::ToString(thisObj, scriptContext);
        }

        uint32 strLength = pThis->GetLength();
        if (strLength == 0)
        {LOGMEIN("JavascriptString.cpp] 3325\n");
            return pThis;
        }

        // Get the number of chars in the mapped string.
        CaseFlags caseFlags = (toUpper ? CaseFlags::CaseFlagsUpper : CaseFlags::CaseFlagsLower);
        const char16* str = pThis->GetString();
        ApiError err = ApiError::NoError;
        int32 count = PlatformAgnostic::UnicodeText::ChangeStringLinguisticCase(caseFlags, str, strLength, nullptr, 0, &err);

        if (count <= 0)
        {LOGMEIN("JavascriptString.cpp] 3336\n");
            AssertMsg(err != ApiError::NoError, "LCMapString failed");
            Throw::InternalError();
        }

        BufferStringBuilder builder(count, scriptContext);
        char16* stringBuffer = builder.DangerousGetWritableBuffer();

        int count1 = PlatformAgnostic::UnicodeText::ChangeStringLinguisticCase(caseFlags, str, count, stringBuffer, count, &err);

        if (count1 <= 0)
        {LOGMEIN("JavascriptString.cpp] 3347\n");
            AssertMsg(err != ApiError::NoError, "LCMapString failed");
            Throw::InternalError();
        }

        return builder.ToString();
    }

    int JavascriptString::IndexOfUsingJmpTable(JmpTable jmpTable, const char16* inputStr, int len, const char16* searchStr, int searchLen, int position)
    {LOGMEIN("JavascriptString.cpp] 3356\n");
        int result = -1;

        const char16 searchLast = searchStr[searchLen-1];

        uint32 lMatchedJump = searchLen;
        if (jmpTable[searchLast].shift > 0)
        {LOGMEIN("JavascriptString.cpp] 3363\n");
            lMatchedJump = jmpTable[searchLast].shift;
        }

        char16 const * p = inputStr + position + searchLen-1;
        WCHAR c;
        while(p < inputStr + len)
        {LOGMEIN("JavascriptString.cpp] 3370\n");
            // first character match, keep checking
            if (*p == searchLast)
            {LOGMEIN("JavascriptString.cpp] 3373\n");
                if ( wmemcmp(p-searchLen+1, searchStr, searchLen) == 0 )
                {LOGMEIN("JavascriptString.cpp] 3375\n");
                    break;
                }
                p += lMatchedJump;
            }
            else
            {
                c = *p;
                if ( 0 == ( c & ~0x7f ) && jmpTable[c].shift != 0 )
                {LOGMEIN("JavascriptString.cpp] 3384\n");
                    p += jmpTable[c].shift;
                }
                else
                {
                    p += searchLen;
                }
            }
        }

        if (p >= inputStr+position && p < inputStr + len)
        {LOGMEIN("JavascriptString.cpp] 3395\n");
            result = (int)(p - inputStr) - searchLen + 1;
        }

        return result;
    }

    int JavascriptString::LastIndexOfUsingJmpTable(JmpTable jmpTable, const char16* inputStr, int len, const char16* searchStr, int searchLen, int position)
    {LOGMEIN("JavascriptString.cpp] 3403\n");
        const char16 searchFirst = searchStr[0];
        uint32 lMatchedJump = searchLen;
        if (jmpTable[searchFirst].shift > 0)
        {LOGMEIN("JavascriptString.cpp] 3407\n");
            lMatchedJump = jmpTable[searchFirst].shift;
        }
        WCHAR c;
        char16 const * p = inputStr + min(len - searchLen, position);
        while(p >= inputStr)
        {LOGMEIN("JavascriptString.cpp] 3413\n");
            // first character match, keep checking
            if (*p == searchFirst)
            {
                if ( wmemcmp(p, searchStr, searchLen) == 0 )
                {LOGMEIN("JavascriptString.cpp] 3418\n");
                    break;
                }
                p -= lMatchedJump;
            }
            else
            {
                c = *p;
                if ( 0 == ( c & ~0x7f ) && jmpTable[c].shift != 0 )
                {LOGMEIN("JavascriptString.cpp] 3427\n");
                    p -= jmpTable[c].shift;
                }
                else
                {
                    p -= searchLen;
                }
            }
        }
        return ((p >= inputStr) ? (int)(p - inputStr) : -1);
    }

    bool JavascriptString::BuildLastCharForwardBoyerMooreTable(JmpTable jmpTable, const char16* searchStr, int searchLen)
    {LOGMEIN("JavascriptString.cpp] 3440\n");
        AssertMsg(searchLen >= 1, "Table for non-empty string");
        memset(jmpTable, 0, sizeof(JmpTable));

        const char16 * p2 = searchStr + searchLen - 1;
        const char16 * const begin = searchStr;

        // Determine if we can do a partial ASCII Boyer-Moore
        while (p2 >= begin)
        {LOGMEIN("JavascriptString.cpp] 3449\n");
            WCHAR c = *p2;
            if ( 0 == ( c & ~0x7f ))
            {LOGMEIN("JavascriptString.cpp] 3452\n");
                if ( jmpTable[c].shift == 0 )
                {LOGMEIN("JavascriptString.cpp] 3454\n");
                    jmpTable[c].shift = (uint32)(searchStr + searchLen - 1 - p2);
                }
            }
            else
            {
                return false;
            }
            p2--;
        }

        return true;
    }

    bool JavascriptString::BuildFirstCharBackwardBoyerMooreTable(JmpTable jmpTable, const char16* searchStr, int searchLen)
    {LOGMEIN("JavascriptString.cpp] 3469\n");
        AssertMsg(searchLen >= 1, "Table for non-empty string");
        memset(jmpTable, 0, sizeof(JmpTable));

        const char16 * p2 = searchStr;
        const char16 * const end = searchStr + searchLen;

        // Determine if we can do a partial ASCII Boyer-Moore
        while (p2 < end)
        {LOGMEIN("JavascriptString.cpp] 3478\n");
            WCHAR c = *p2;
            if ( 0 == ( c & ~0x7f ))
            {LOGMEIN("JavascriptString.cpp] 3481\n");
                if ( jmpTable[c].shift == 0 )
                {LOGMEIN("JavascriptString.cpp] 3483\n");
                    jmpTable[c].shift = (uint32)(p2 - searchStr);
                }
            }
            else
            {
                return false;
            }
            p2++;
        }

        return true;
    }

    uint JavascriptString::strstr(JavascriptString *string, JavascriptString *substring, bool useBoyerMoore, uint start)
    {LOGMEIN("JavascriptString.cpp] 3498\n");
        uint i;

        const char16 *stringOrig = string->GetString();
        uint stringLenOrig = string->GetLength();
        const char16 *stringSz = stringOrig + start;
        const char16 *substringSz = substring->GetString();
        uint stringLen = stringLenOrig - start;
        uint substringLen = substring->GetLength();

        if (useBoyerMoore && substringLen > 2)
        {LOGMEIN("JavascriptString.cpp] 3509\n");
            JmpTable jmpTable;
            bool fAsciiJumpTable = BuildLastCharForwardBoyerMooreTable(jmpTable, substringSz, substringLen);
            if (fAsciiJumpTable)
            {LOGMEIN("JavascriptString.cpp] 3513\n");
                int result = IndexOfUsingJmpTable(jmpTable, stringOrig, stringLenOrig, substringSz, substringLen, start);
                if (result != -1)
                {LOGMEIN("JavascriptString.cpp] 3516\n");
                    return result;
                }
                else
                {
                    return (uint)-1;
                }
            }
        }

        if (stringLen >= substringLen)
        {LOGMEIN("JavascriptString.cpp] 3527\n");
            // If substring is empty, it matches anything...
            if (substringLen == 0)
            {LOGMEIN("JavascriptString.cpp] 3530\n");
                return 0;
            }
            for (i = 0; i <= stringLen - substringLen; i++)
            {LOGMEIN("JavascriptString.cpp] 3534\n");
                // Quick check for first character.
                if (stringSz[i] == substringSz[0])
                {LOGMEIN("JavascriptString.cpp] 3537\n");
                    if (substringLen == 1 || memcmp(stringSz+i+1, substringSz+1, (substringLen-1)*sizeof(char16)) == 0)
                    {LOGMEIN("JavascriptString.cpp] 3539\n");
                        return i + start;
                    }
                }
            }
        }

        return (uint)-1;
    }

    int JavascriptString::strcmp(JavascriptString *string1, JavascriptString *string2)
    {LOGMEIN("JavascriptString.cpp] 3550\n");
        uint string1Len = string1->GetLength();
        uint string2Len = string2->GetLength();

        int result = wmemcmp(string1->GetString(), string2->GetString(), min(string1Len, string2Len));

        return (result == 0) ? (int)(string1Len - string2Len) : result;
    }

    /*static*/ charcount_t JavascriptString::SafeSzSize(charcount_t cch)
    {LOGMEIN("JavascriptString.cpp] 3560\n");
        // JavascriptString::MaxCharLength is valid; however, we are incrementing below by 1 and want to make sure we aren't overflowing
        // Nor going outside of valid range.
        if (cch >= JavascriptString::MaxCharLength)
        {LOGMEIN("JavascriptString.cpp] 3564\n");
            Throw::OutOfMemory();
        }

        // Compute cch + 1, checking for overflow
        ++cch;

        return cch;
    }

    charcount_t JavascriptString::SafeSzSize() const
    {LOGMEIN("JavascriptString.cpp] 3575\n");
        return SafeSzSize(GetLength());
    }

    /*static*/ __ecount(length+1) char16* JavascriptString::AllocateLeafAndCopySz(__in Recycler* recycler, __in_ecount(length) const char16* content, charcount_t length)
    {LOGMEIN("JavascriptString.cpp] 3580\n");
        // Note: Intentionally not using SafeSzSize nor hoisting common
        // sub-expression "length + 1" into a local variable otherwise
        // Prefast gets confused and cannot track buffer's length.

        // JavascriptString::MaxCharLength is valid; however, we are incrementing below by 1 and want to make sure we aren't overflowing
        // Nor going outside of valid range.
        if (length >= JavascriptString::MaxCharLength)
        {LOGMEIN("JavascriptString.cpp] 3588\n");
            Throw::OutOfMemory();
        }

        charcount_t bufLen = length + 1;
        // Allocate recycler memory to store the string plus a terminating NUL
        char16* buffer = RecyclerNewArrayLeaf(recycler, char16, bufLen);
        js_wmemcpy_s(buffer, bufLen, content, length);
        buffer[length] = _u('\0');

        return buffer;
    }

    /*static*/ __ecount(length+1) char16* JavascriptString::AllocateAndCopySz(__in ArenaAllocator* arena, __in_ecount(length) const char16* content, charcount_t length)
    {LOGMEIN("JavascriptString.cpp] 3602\n");
        // Note: Intentionally not using SafeSzSize nor hoisting common
        // sub-expression "length + 1" into a local variable otherwise
        // Prefast gets confused and cannot track buffer's length.

        // JavascriptString::MaxCharLength is valid; however, we are incrementing below by 1 and want to make sure we aren't overflowing
        // Nor going outside of valid range.
        if (length >= JavascriptString::MaxCharLength)
        {LOGMEIN("JavascriptString.cpp] 3610\n");
            Throw::OutOfMemory();
        }

        // Allocate arena memory to store the string plus a terminating NUL
        char16* buffer = AnewArray(arena, char16, length + 1);
        js_wmemcpy_s(buffer, length + 1, content, length);
        buffer[length] = _u('\0');

        return buffer;
    }

    RecyclableObject * JavascriptString::CloneToScriptContext(ScriptContext* requestContext)
    {LOGMEIN("JavascriptString.cpp] 3623\n");
        return JavascriptString::NewWithBuffer(this->GetSz(), this->GetLength(), requestContext);
    }

    charcount_t JavascriptString::ConvertToIndex(Var varIndex, ScriptContext *scriptContext)
    {LOGMEIN("JavascriptString.cpp] 3628\n");
        if (TaggedInt::Is(varIndex))
        {LOGMEIN("JavascriptString.cpp] 3630\n");
            return TaggedInt::ToInt32(varIndex);
        }
        return NumberUtilities::LwFromDblNearest(JavascriptConversion::ToInteger(varIndex, scriptContext));
    }

    char16* JavascriptString::GetNormalizedString(PlatformAgnostic::UnicodeText::NormalizationForm form, ArenaAllocator* tempAllocator, charcount_t& sizeOfNormalizedStringWithoutNullTerminator)
    {LOGMEIN("JavascriptString.cpp] 3637\n");
        using namespace PlatformAgnostic;

        ScriptContext* scriptContext = this->GetScriptContext();
        if (this->GetLength() == 0)
        {LOGMEIN("JavascriptString.cpp] 3642\n");
            sizeOfNormalizedStringWithoutNullTerminator = 0;
            return nullptr;
        }

        // IMPORTANT: Implementation Notes
        // Normalize string estimates the required size of the buffer based on averages and other data.
        // It is very hard to get a precise size from an input string without expanding/contracting it on the buffer.
        // It is estimated that the maximum size the string after an NFC is 6x the input length, and 18x for NFD. This approach isn't very feasible as well.
        // The approach taken is based on the simple example in the MSDN article.
        //  - Loop until the return value is either an error (apart from insufficient buffer size), or success.
        //  - Each time recreate a temporary buffer based on the last guess.
        //  - When creating the JS string, use the positive return value and copy the buffer across.
        // Design choice for "guesses" comes from data Windows collected; and in most cases the loop will not iterate more than 2 times.

        Assert(!UnicodeText::IsNormalizedString(form, this->GetSz(), this->GetLength()));

        //Get the first size estimate
        UnicodeText::ApiError error;
        int32 sizeEstimate = UnicodeText::NormalizeString(form, this->GetSz(), this->GetLength() + 1, nullptr, 0, &error);
        char16 *tmpBuffer = nullptr;
        //Loop while the size estimate is bigger than 0
        while (error == UnicodeText::ApiError::InsufficientBuffer)
        {LOGMEIN("JavascriptString.cpp] 3665\n");
            tmpBuffer = AnewArray(tempAllocator, char16, sizeEstimate);
            sizeEstimate = UnicodeText::NormalizeString(form, this->GetSz(), this->GetLength() + 1, tmpBuffer, sizeEstimate, &error);

            // Success, sizeEstimate is the exact size including the null terminator
            if (sizeEstimate > 0)
            {LOGMEIN("JavascriptString.cpp] 3671\n");
                sizeOfNormalizedStringWithoutNullTerminator = sizeEstimate - 1;
                return tmpBuffer;
            }

            // Anything less than 0, we have an error, flip sizeEstimate now. As both times we need to use it, we need positive anyways.
            sizeEstimate *= -1;

        }

        switch (error)
        {LOGMEIN("JavascriptString.cpp] 3682\n");
            case UnicodeText::ApiError::InvalidParameter:
                //some invalid parameter, coding error
                AssertMsg(false, "Invalid Parameter- check pointers passed to NormalizeString");
                JavascriptError::ThrowRangeError(scriptContext, JSERR_FailedToNormalize);
                break;
            case UnicodeText::ApiError::InvalidUnicodeText:
                //the value returned is the negative index of an invalid unicode character
                JavascriptError::ThrowRangeErrorVar(scriptContext, JSERR_InvalidUnicodeCharacter, sizeEstimate);
                break;
            case UnicodeText::ApiError::NoError:
                //The actual size of the output string is zero.
                //Theoretically only empty input string should produce this, which is handled above, thus the code path should not be hit.
                AssertMsg(false, "This code path should not be hit, empty string case is handled above. Perhaps a false error (sizeEstimate <= 0; but lastError == 0; ERROR_SUCCESS and NO_ERRROR == 0)");
                sizeOfNormalizedStringWithoutNullTerminator = 0;
                return nullptr; // scriptContext->GetLibrary()->GetEmptyString();
                break;
            default:
                AssertMsg(false, "Unknown error");
                JavascriptError::ThrowRangeError(scriptContext, JSERR_FailedToNormalize);
                break;
        }
    }

    void JavascriptString::InstantiateForceInlinedMembers()
    {LOGMEIN("JavascriptString.cpp] 3707\n");
        // Force-inlined functions defined in a translation unit need a reference from an extern non-force-inlined function in
        // the same translation unit to force an instantiation of the force-inlined function. Otherwise, if the force-inlined
        // function is not referenced in the same translation unit, it will not be generated and the linker is not able to find
        // the definition to inline the function in other translation units.
        Assert(false);

        JavascriptString *const s = nullptr;

        s->ConcatDestructive(nullptr);
    }

    JavascriptString *
    JavascriptString::Concat3(JavascriptString * pstLeft, JavascriptString * pstCenter, JavascriptString * pstRight)
    {LOGMEIN("JavascriptString.cpp] 3721\n");
        ConcatStringMulti * concatString = ConcatStringMulti::New(3, pstLeft, pstCenter, pstLeft->GetScriptContext());
        concatString->SetItem(2, pstRight);
        return concatString;
    }

    BOOL JavascriptString::HasProperty(PropertyId propertyId)
    {LOGMEIN("JavascriptString.cpp] 3728\n");
        if (propertyId == PropertyIds::length)
        {LOGMEIN("JavascriptString.cpp] 3730\n");
            return true;
        }
        ScriptContext* scriptContext = GetScriptContext();
        charcount_t index;
        if (scriptContext->IsNumericPropertyId(propertyId, &index))
        {LOGMEIN("JavascriptString.cpp] 3736\n");
            if (index < this->GetLength())
            {LOGMEIN("JavascriptString.cpp] 3738\n");
                return true;
            }
        }
        return false;
    }

    BOOL JavascriptString::IsEnumerable(PropertyId propertyId)
    {LOGMEIN("JavascriptString.cpp] 3746\n");
        ScriptContext* scriptContext = GetScriptContext();
        charcount_t index;
        if (scriptContext->IsNumericPropertyId(propertyId, &index))
        {LOGMEIN("JavascriptString.cpp] 3750\n");
            if (index < this->GetLength())
            {LOGMEIN("JavascriptString.cpp] 3752\n");
                return true;
            }
        }
        return false;
    }

    BOOL JavascriptString::GetProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("JavascriptString.cpp] 3760\n");
        return GetPropertyBuiltIns(propertyId, value, requestContext);
    }
    BOOL JavascriptString::GetProperty(Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("JavascriptString.cpp] 3764\n");
        PropertyRecord const* propertyRecord;
        this->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);

        if (propertyRecord != nullptr && GetPropertyBuiltIns(propertyRecord->GetPropertyId(), value, requestContext))
        {LOGMEIN("JavascriptString.cpp] 3769\n");
            return true;
        }

        *value = requestContext->GetMissingPropertyResult();
        return false;
    }
    bool JavascriptString::GetPropertyBuiltIns(PropertyId propertyId, Var* value, ScriptContext* requestContext)
    {LOGMEIN("JavascriptString.cpp] 3777\n");
        if (propertyId == PropertyIds::length)
        {LOGMEIN("JavascriptString.cpp] 3779\n");
            *value = JavascriptNumber::ToVar(this->GetLength(), requestContext);
            return true;
        }

        *value = requestContext->GetMissingPropertyResult();
        return false;
    }
    BOOL JavascriptString::GetPropertyReference(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("JavascriptString.cpp] 3788\n");
        return JavascriptString::GetProperty(originalInstance, propertyId, value, info, requestContext);
    }

    BOOL JavascriptString::SetItem(uint32 index, Var value, PropertyOperationFlags propertyOperationFlags)
    {LOGMEIN("JavascriptString.cpp] 3793\n");
        if (this->HasItemAt(index))
        {LOGMEIN("JavascriptString.cpp] 3795\n");
            JavascriptError::ThrowCantAssignIfStrictMode(propertyOperationFlags, this->GetScriptContext());

            return FALSE;
        }

        return __super::SetItem(index, value, propertyOperationFlags);
    }

    BOOL JavascriptString::DeleteItem(uint32 index, PropertyOperationFlags propertyOperationFlags)
    {LOGMEIN("JavascriptString.cpp] 3805\n");
        if (this->HasItemAt(index))
        {LOGMEIN("JavascriptString.cpp] 3807\n");
            JavascriptError::ThrowCantDeleteIfStrictMode(propertyOperationFlags, this->GetScriptContext(), TaggedInt::ToString(index, this->GetScriptContext())->GetString());

            return FALSE;
        }

        return __super::DeleteItem(index, propertyOperationFlags);
    }

    BOOL JavascriptString::HasItem(uint32 index)
    {LOGMEIN("JavascriptString.cpp] 3817\n");
        return this->HasItemAt(index);
    }

    BOOL JavascriptString::GetItem(Var originalInstance, uint32 index, Var* value, ScriptContext* requestContext)
    {LOGMEIN("JavascriptString.cpp] 3822\n");
        // String should always be marshalled to the current context
        Assert(requestContext == this->GetScriptContext());
        return this->GetItemAt(index, value);
    }

    BOOL JavascriptString::GetItemReference(Var originalInstance, uint32 index, Var* value, ScriptContext* requestContext)
    {LOGMEIN("JavascriptString.cpp] 3829\n");
        // String should always be marshalled to the current context
        return this->GetItemAt(index, value);
    }

    BOOL JavascriptString::GetEnumerator(JavascriptStaticEnumerator * enumerator, EnumeratorFlags flags, ScriptContext* requestContext, ForInCache * forInCache)
    {LOGMEIN("JavascriptString.cpp] 3835\n");
        return enumerator->Initialize(
            RecyclerNew(GetScriptContext()->GetRecycler(), JavascriptStringEnumerator, this, requestContext),
            nullptr, nullptr, flags, requestContext, forInCache);
    }

    BOOL JavascriptString::DeleteProperty(PropertyId propertyId, PropertyOperationFlags propertyOperationFlags)
    {LOGMEIN("JavascriptString.cpp] 3842\n");
        if (propertyId == PropertyIds::length)
        {LOGMEIN("JavascriptString.cpp] 3844\n");
            JavascriptError::ThrowCantDeleteIfStrictMode(propertyOperationFlags, this->GetScriptContext(), this->GetScriptContext()->GetPropertyName(propertyId)->GetBuffer());

            return FALSE;
        }
        return __super::DeleteProperty(propertyId, propertyOperationFlags);
    }

    BOOL JavascriptString::DeleteProperty(JavascriptString *propertyNameString, PropertyOperationFlags propertyOperationFlags)
    {LOGMEIN("JavascriptString.cpp] 3853\n");
        JsUtil::CharacterBuffer<WCHAR> propertyName(propertyNameString->GetString(), propertyNameString->GetLength());
        if (BuiltInPropertyRecords::length.Equals(propertyName))
        {LOGMEIN("JavascriptString.cpp] 3856\n");
            JavascriptError::ThrowCantDeleteIfStrictMode(propertyOperationFlags, this->GetScriptContext(), propertyNameString->GetString());

            return FALSE;
        }
        return __super::DeleteProperty(propertyNameString, propertyOperationFlags);
    }

    BOOL JavascriptString::GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {LOGMEIN("JavascriptString.cpp] 3865\n");
        stringBuilder->Append(_u('"'));
        stringBuilder->Append(this->GetString(), this->GetLength());
        stringBuilder->Append(_u('"'));
        return TRUE;
    }

    BOOL JavascriptString::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {LOGMEIN("JavascriptString.cpp] 3873\n");
        stringBuilder->AppendCppLiteral(_u("String"));
        return TRUE;
    }

    RecyclableObject* JavascriptString::ToObject(ScriptContext * requestContext)
    {LOGMEIN("JavascriptString.cpp] 3879\n");
        return requestContext->GetLibrary()->CreateStringObject(this);
    }

    Var JavascriptString::GetTypeOfString(ScriptContext * requestContext)
    {LOGMEIN("JavascriptString.cpp] 3884\n");
        return requestContext->GetLibrary()->GetStringTypeDisplayString();
    }

    /* static */
    template <typename T>
    bool JavascriptStringHelpers<T>::Equals(Var aLeft, Var aRight)
    {LOGMEIN("JavascriptString.cpp] 3891\n");
        AssertMsg(T::Is(aLeft) && T::Is(aRight), "string comparison");

        T *leftString = T::FromVar(aLeft);
        T *rightString = T::FromVar(aRight);

        if (leftString->GetLength() != rightString->GetLength())
        {LOGMEIN("JavascriptString.cpp] 3898\n");
            return false;
        }

        if (wmemcmp(leftString->GetString(), rightString->GetString(), leftString->GetLength()) == 0)
        {LOGMEIN("JavascriptString.cpp] 3903\n");
            return true;
        }
        return false;
    }

#if ENABLE_NATIVE_CODEGEN
    template bool JavascriptStringHelpers<JITJavascriptString>::Equals(Var aLeft, Var aRight);
#endif

}
