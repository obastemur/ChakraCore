//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#pragma once

class JITRecyclableObject
{
private:
    intptr_t remoteVTable;
    Js::TypeId * typeId;
public:
    static bool Is(Js::Var var)
    {LOGMEIN("JITRecyclableObject.h] 14\n");
        return !Js::TaggedNumber::Is(var);
    }

    Js::TypeId GetTypeId() const
    {LOGMEIN("JITRecyclableObject.h] 19\n");
        return *typeId;
    }
};

class JITJavascriptString : JITRecyclableObject
{
private:
    const char16* m_pszValue;
    charcount_t m_charLength;
public:
    const char16* GetString() const
    {LOGMEIN("JITRecyclableObject.h] 31\n");
        return m_pszValue;
    }

    charcount_t GetLength() const
    {LOGMEIN("JITRecyclableObject.h] 36\n");
        return m_charLength;
    }

    static bool Equals(Js::Var aLeft, Js::Var aRight)
    {LOGMEIN("JITRecyclableObject.h] 41\n");
        return Js::JavascriptStringHelpers<JITJavascriptString>::Equals(aLeft, aRight);
    }

    static bool Is(Js::Var var)
    {LOGMEIN("JITRecyclableObject.h] 46\n");
        if (!JITRecyclableObject::Is(var))
        {LOGMEIN("JITRecyclableObject.h] 48\n");
            return false;
        }
        JITRecyclableObject * jitObj = reinterpret_cast<JITRecyclableObject*>(var);
        return jitObj->GetTypeId() == Js::TypeIds_String;
    }

    static JITJavascriptString * FromVar(Js::Var var)
    {
        Assert(offsetof(JITJavascriptString, m_pszValue) == Js::JavascriptString::GetOffsetOfpszValue());
        Assert(offsetof(JITJavascriptString, m_charLength) == Js::JavascriptString::GetOffsetOfcharLength());
        Assert(Is(var));

        return reinterpret_cast<JITJavascriptString*>(var);
    }
};

class JITJavascriptNumber : JITRecyclableObject
{
private:
    double value;

};

template <>
struct DefaultComparer<JITJavascriptString*>
{
    inline static bool Equals(JITJavascriptString * x, JITJavascriptString * y)
    {LOGMEIN("JITRecyclableObject.h] 76\n");
        return Js::JavascriptStringHelpers<JITJavascriptString>::Equals(x, y);
    }

    inline static uint GetHashCode(JITJavascriptString * pStr)
    {LOGMEIN("JITRecyclableObject.h] 81\n");
        return JsUtil::CharacterBuffer<char16>::StaticGetHashCode(pStr->GetString(), pStr->GetLength());
    }
};
