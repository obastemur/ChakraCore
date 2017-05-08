//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once


#ifdef PROPERTY_RECORD_TRACE
#define PropertyRecordTrace(...) \
    if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::PropertyRecordPhase)) \
    {TRACE_IT(35818); \
        Output::Print(__VA_ARGS__); \
    }
#else
#define PropertyRecordTrace(...)
#endif

class ThreadContext;
class ServerThreadContext;

namespace Js
{
    class PropertyRecord : public FinalizableObject
    {
        friend class ::ThreadContext;
        friend class ::ServerThreadContext;
        template <int LEN>
        friend struct BuiltInPropertyRecord;
        friend class InternalPropertyRecords;
        friend class BuiltInPropertyRecords;
        friend class DOMBuiltInPropertyRecords;

    private:
        Field(PropertyId) pid;
        //Made this mutable so that we can set it for Built-In js property records when we are adding it.
        //If we try to set it when initializing; we get extra code added for each built in; and thus increasing the size of chakracore
        mutable Field(uint) hash;
        Field(bool) isNumeric;
        Field(bool) isBound;
        Field(bool) isSymbol;
        // Have the length before the buffer so that the buffer would have a BSTR format
        Field(DWORD) byteCount;

        PropertyRecord(DWORD bytelength, bool isNumeric, uint hash, bool isSymbol);
        PropertyRecord(PropertyId pid, uint hash, bool isNumeric, DWORD byteCount, bool isSymbol);
        PropertyRecord() {TRACE_IT(35819); Assert(false); } // never used, needed by compiler for BuiltInPropertyRecord

        static bool IsPropertyNameNumeric(const char16* str, int length, uint32* intVal);
    public:
#ifdef DEBUG
        static bool IsPropertyNameNumeric(const char16* str, int length);
#endif

        static PropertyAttributes DefaultAttributesForPropertyId(PropertyId propertyId, bool __proto__AsDeleted);

        PropertyId GetPropertyId() const {TRACE_IT(35820); return pid; }
        uint GetHashCode() const {TRACE_IT(35821); return hash; }

        charcount_t GetLength() const
        {TRACE_IT(35822);
            return byteCount / sizeof(char16);
        }

        const char16* GetBuffer() const
        {TRACE_IT(35823);
            return (const char16 *)(this + 1);
        }

        bool IsNumeric() const {TRACE_IT(35824); return isNumeric; }
        uint32 GetNumericValue() const;

        bool IsBound() const {TRACE_IT(35825); return isBound; }
        bool IsSymbol() const {TRACE_IT(35826); return isSymbol; }

        void SetHash(uint hash) const
        {TRACE_IT(35827);
            this->hash = hash;
        }

        bool Equals(JsUtil::CharacterBuffer<WCHAR> const & str) const
        {TRACE_IT(35828);
            return (this->GetLength() == str.GetLength() && !Js::IsInternalPropertyId(this->GetPropertyId()) &&
                JsUtil::CharacterBuffer<WCHAR>::StaticEquals(this->GetBuffer(), str.GetBuffer(), this->GetLength()));
        }

        bool Equals(PropertyRecord const & propertyRecord) const
        {TRACE_IT(35829);
            return (this->GetLength() == propertyRecord.GetLength() &&
                Js::IsInternalPropertyId(this->GetPropertyId()) == Js::IsInternalPropertyId(propertyRecord.GetPropertyId()) &&
                JsUtil::CharacterBuffer<WCHAR>::StaticEquals(this->GetBuffer(), propertyRecord.GetBuffer(), this->GetLength()));
        }

    public:
        // Finalizable support
        virtual void Finalize(bool isShutdown);

        virtual void Dispose(bool isShutdown)
        {TRACE_IT(35830);
        }

        virtual void Mark(Recycler *recycler) override { AssertMsg(false, "Mark called on object that isn't TrackableObject"); }
    };

    // This struct maps to the layout of runtime allocated PropertyRecord. Used for creating built-in PropertyRecords statically.
    template <int LEN>
    struct BuiltInPropertyRecord
    {
        PropertyRecord propertyRecord;
        char16 buffer[LEN];

        operator const PropertyRecord*() const
        {TRACE_IT(35831);
            return &propertyRecord;
        }

        bool Equals(JsUtil::CharacterBuffer<WCHAR> const & str) const
        {TRACE_IT(35832);
            return (LEN - 1 == str.GetLength() &&
                JsUtil::CharacterBuffer<WCHAR>::StaticEquals(buffer, str.GetBuffer(), LEN - 1));
        }
    };

    // Internal PropertyRecords mapping to InternalPropertyIds. Property names of internal PropertyRecords are not used
    // and set to empty string.
    class InternalPropertyRecords
    {
    public:
#define INTERNALPROPERTY(n) const static BuiltInPropertyRecord<1> n;
#include "InternalPropertyList.h"

        static const PropertyRecord* GetInternalPropertyName(PropertyId propertyId);
    };

    // Built-in PropertyRecords. Created statically with known PropertyIds.
    class BuiltInPropertyRecords
    {
    public:
        const static BuiltInPropertyRecord<1> EMPTY;
#define ENTRY_INTERNAL_SYMBOL(n) const static BuiltInPropertyRecord<ARRAYSIZE(_u("<") _u(#n) _u(">"))> n;
#define ENTRY_SYMBOL(n, d) const static BuiltInPropertyRecord<ARRAYSIZE(d)> n;
#define ENTRY(n) const static BuiltInPropertyRecord<ARRAYSIZE(_u(#n))> n;
#define ENTRY2(n, s) const static BuiltInPropertyRecord<ARRAYSIZE(s)> n;
#include "Base/JnDirectFields.h"
    };

    template <typename TChar>
    class HashedCharacterBuffer : public JsUtil::CharacterBuffer<TChar>
    {
    private:
        hash_t hashCode;

    public:
        HashedCharacterBuffer(TChar const * string, charcount_t len) :
            JsUtil::CharacterBuffer<TChar>(string, len)
        {TRACE_IT(35833);
            this->hashCode = JsUtil::CharacterBuffer<WCHAR>::StaticGetHashCode(string, len);
        }

        hash_t GetHashCode() const {TRACE_IT(35834); return this->hashCode; }
    };

    struct PropertyRecordPointerComparer
    {
        inline static bool Equals(PropertyRecord const * str1, PropertyRecord const * str2)
        {TRACE_IT(35835);
            return (str1->GetLength() == str2->GetLength() &&
                JsUtil::CharacterBuffer<WCHAR>::StaticEquals(str1->GetBuffer(), str2->GetBuffer(), str1->GetLength()));
        }

        inline static bool Equals(PropertyRecord const * str1, JsUtil::CharacterBuffer<WCHAR> const * str2)
        {TRACE_IT(35836);
            return (str1->GetLength() == str2->GetLength() && !Js::IsInternalPropertyId(str1->GetPropertyId()) &&
                JsUtil::CharacterBuffer<WCHAR>::StaticEquals(str1->GetBuffer(), str2->GetBuffer(), str1->GetLength()));
        }

        inline static hash_t GetHashCode(PropertyRecord const * str)
        {TRACE_IT(35837);
            return str->GetHashCode();
        }

        inline static hash_t GetHashCode(JsUtil::CharacterBuffer<WCHAR> const * str)
        {TRACE_IT(35838);
            return JsUtil::CharacterBuffer<WCHAR>::StaticGetHashCode(str->GetBuffer(), str->GetLength());
        }
    };

    template<typename T>
    struct PropertyRecordStringHashComparer
    {
        inline static bool Equals(T str1, T str2)
        {
            static_assert(false, "Unexpected type T; note T == PropertyId not allowed!");
        }

        inline static hash_t GetHashCode(T str)
        {
            // T == PropertyId is not allowed because there is no way to get the string hash
            // from just a PropertyId value, the PropertyRecord is required for that.
            static_assert(false, "Unexpected type T; note T == PropertyId not allowed!");
        }
    };

    template<>
    struct PropertyRecordStringHashComparer<PropertyRecord const *>
    {
        inline static bool Equals(PropertyRecord const * str1, PropertyRecord const * str2)
        {TRACE_IT(35839);
            return str1 == str2;
        }

        inline static bool Equals(PropertyRecord const * str1, JsUtil::CharacterBuffer<WCHAR> const & str2)
        {TRACE_IT(35840);
            return (!str1->IsSymbol() &&
                str1->GetLength() == str2.GetLength() &&
                !Js::IsInternalPropertyId(str1->GetPropertyId()) &&
                JsUtil::CharacterBuffer<WCHAR>::StaticEquals(str1->GetBuffer(), str2.GetBuffer(), str1->GetLength()));
        }

        inline static bool Equals(PropertyRecord const * str1, HashedCharacterBuffer<char16> const & str2)
        {TRACE_IT(35841);
            return (!str1->IsSymbol() &&
                str1->GetHashCode() == str2.GetHashCode() &&
                str1->GetLength() == str2.GetLength() &&
                !Js::IsInternalPropertyId(str1->GetPropertyId()) &&
                JsUtil::CharacterBuffer<char16>::StaticEquals(str1->GetBuffer(), str2.GetBuffer(), str1->GetLength()));
        }

        inline static bool Equals(PropertyRecord const * str1, JavascriptString * str2);

        inline static hash_t GetHashCode(const PropertyRecord* str)
        {TRACE_IT(35842);
            return str->GetHashCode();
        }
    };

    template<>
    struct PropertyRecordStringHashComparer<JsUtil::CharacterBuffer<WCHAR>>
    {
        inline static bool Equals(JsUtil::CharacterBuffer<WCHAR> const & str1, JsUtil::CharacterBuffer<WCHAR> const & str2)
        {TRACE_IT(35843);
            return (str1.GetLength() == str2.GetLength() &&
                JsUtil::CharacterBuffer<WCHAR>::StaticEquals(str1.GetBuffer(), str2.GetBuffer(), str1.GetLength()));
        }

        inline static hash_t GetHashCode(JsUtil::CharacterBuffer<WCHAR> const & str)
        {TRACE_IT(35844);
            return JsUtil::CharacterBuffer<WCHAR>::StaticGetHashCode(str.GetBuffer(), str.GetLength());
        }
    };

    template<>
    struct PropertyRecordStringHashComparer<HashedCharacterBuffer<char16>>
    {
        inline static hash_t GetHashCode(HashedCharacterBuffer<char16> const & str)
        {TRACE_IT(35845);
            return str.GetHashCode();
        }
    };

    class CaseInvariantPropertyListWithHashCode: public JsUtil::List<const RecyclerWeakReference<Js::PropertyRecord const>*>
    {
    public:
        CaseInvariantPropertyListWithHashCode(Recycler* recycler, int increment):
          JsUtil::List<const RecyclerWeakReference<Js::PropertyRecord const>*>(recycler, increment),
          caseInvariantHashCode(0)
          {TRACE_IT(35846);
          }

        Field(uint) caseInvariantHashCode;
    };
}

// Hash and lookup by PropertyId
template <>
struct DefaultComparer<const Js::PropertyRecord*>
{
    inline static hash_t GetHashCode(const Js::PropertyRecord* str)
    {TRACE_IT(35847);
        return DefaultComparer<Js::PropertyId>::GetHashCode(str->GetPropertyId());
    }

    inline static bool Equals(const Js::PropertyRecord* str, Js::PropertyId propertyId)
    {TRACE_IT(35848);
        return str->GetPropertyId() == propertyId;
    }

    inline static bool Equals(const Js::PropertyRecord* str1, const Js::PropertyRecord* str2)
    {TRACE_IT(35849);
        return str1 == str2;
    }
};

namespace JsUtil
{
    template<>
    struct NoCaseComparer<Js::CaseInvariantPropertyListWithHashCode*>
    {
        static bool Equals(_In_ Js::CaseInvariantPropertyListWithHashCode* list1, _In_ Js::CaseInvariantPropertyListWithHashCode* list2);
        static bool Equals(_In_ Js::CaseInvariantPropertyListWithHashCode* list, JsUtil::CharacterBuffer<WCHAR> const& str);
        static hash_t GetHashCode(_In_ Js::CaseInvariantPropertyListWithHashCode* list);
    };

}
