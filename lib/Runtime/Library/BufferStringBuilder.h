//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    // Used when the final string has a known final length
    class BufferStringBuilder
    {
    public:
        BufferStringBuilder(charcount_t charLength, ScriptContext* scriptContext)
            : m_string( WritableString::New(charLength, scriptContext) )
        {
        }

        JavascriptString* ToString();

        void DbgAssertNotFrozen() const
        {
            AssertMsg(this->m_string != nullptr, "Do not call BufferStringBuilder methods after ToString() has been called.");
        }

        void SetContent(
            const CHAR_T* prefix, charcount_t cchPrefix,
            const CHAR_T* content, charcount_t cchContent,
            const CHAR_T* suffix, charcount_t cchSuffix)
        {
            DbgAssertNotFrozen();
            this->m_string->SetContent(prefix, cchPrefix, content, cchContent, suffix, cchSuffix);
        }

        // Caution: Do not retain the writable buffer after ToString has been called
        CHAR_T* DangerousGetWritableBuffer()
        {
            DbgAssertNotFrozen();
            return this->m_string->GetWritableBuffer();
        }

        class WritableString sealed : public JavascriptString
        {
        public:
            static WritableString* New(charcount_t length, ScriptContext* scriptContext);
            CHAR_T* GetWritableBuffer() const
            {
                return const_cast< CHAR_T* >( this->UnsafeGetBuffer() );
            }

            void SetContent(const CHAR_T* content, charcount_t offset, charcount_t length);
            void SetContent(const CHAR_T* prefix, charcount_t cchPrefix,
                            const CHAR_T* content, charcount_t cchContent,
                            const CHAR_T* suffix, charcount_t cchSuffix);

        protected:
            DEFINE_VTABLE_CTOR(WritableString, JavascriptString);

        private:
            WritableString(StaticType * type, charcount_t length, const CHAR_T* szValue)
                : JavascriptString(type, length, szValue)
            {
            }

            static CHAR_T* SafeCopyAndAdvancePtr(__out_ecount(cchDst) CHAR_T* dst, charcount_t& cchDst, __in_ecount(cch) const CHAR_T* ptr, charcount_t cch);
        };

    private:
        WritableString* m_string;
#if DBG
        static const CHAR_T k_dbgFill = _u('\xCDCD');
#endif
    };

    // Needed by diagnostics vtable access
    typedef BufferStringBuilder::WritableString BufferStringBuilderWritableString;

} // namespace Js
